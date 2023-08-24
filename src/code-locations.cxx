// Authors: Harald Servat <harald.servat@intel.com>
// Date: 02/02/2017
// Author: Clement Foyer <clement.foyer@univ-reims.fr>
// Date: 24/08/2023
// License: 

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <algorithm>

#include "common.hxx"
#include "utils.hxx"
#include "code-locations.hxx"

CodeLocations::CodeLocations (allocation_functions_t &af, Allocators *a)
	: _fast_indexes_frames(nullptr), _af(af), _allocators(a), _locations(nullptr),
	  _nlocations(0), _min_nframes(UINT_MAX), _max_nframes(0)
{
}

CodeLocations::~CodeLocations()
{
}

bool CodeLocations::comparator_by_ID (const location_t &lhs, const location_t &rhs)
{
	return lhs.id < rhs.id;
}

bool CodeLocations::comparator_by_NumberOfFrames (const location_t &lhs, const location_t &rhs)
{
	return lhs.nframes < rhs.nframes;
}

char * CodeLocations::find_and_set_allocator (char *location_txt, location_t * location, const char * fallback_allocator_name)
{
	// Make sure it contains a @ symbol
	char *allocator_marker = strchr (location_txt, '@');
	if (allocator_marker == nullptr)
	{
		VERBOSE_MSG(0, "Error! Cannot find allocator marker.\n");
		return nullptr;
	}

	// Identify allocator first
	char allocator[PATH_MAX] = {0};
	size_t allocator_len = std::min(strcspn(allocator_marker+2, "\n"), (size_t) PATH_MAX-1);
	memcpy (allocator, allocator_marker+2, allocator_len);
	// +2 because we skip @ && the following space
	allocator[allocator_len] = '\0';

	location->allocator = _allocators->get (allocator);
	if (location->allocator == nullptr)
	{
		VERBOSE_MSG (0, "Error! Given allocator '%s' does not exist in given memory definitions file.\n", allocator);
		return nullptr;
	}

	// Mark the allocator as used
	location->allocator->used (true);

	if (strcmp(allocator, fallback_allocator_name) == 0 && options.ignoreIfFallbackAllocator())
	{
		DBG("Refusing this location as it is already handled by the fallback allocator (%s).\n",
		  fallback_allocator_name);
		return nullptr;
	}

	return allocator_marker;
}

size_t CodeLocations::count_frames (char *location_txt, location_t * location, const char * const allocator_marker, char marker)
{
	location->nframes = 0;

	// Count number of frames for this location
	// Process the line with frames until the allocator marker is found
	for (char * frame = strchr (location_txt, marker);
			frame != nullptr && frame < allocator_marker;
			frame = strchr (frame+1, marker))
	{
		location->nframes++;
	}

	DBG("Location contains %u frames.\n", location->nframes);

	return location->nframes;
}

void CodeLocations::clean_source_location (location_t * l)
{
#define UNRESOLVED        "Unresolved" // Paraver label for unresolved symbols
#define UNRESOLVED_LINENO 0

#define NOT_FOUND         "_NOT_Found"
#define NOT_FOUND_LINENO  0

	unsigned new_height = 0;
	bool has_new_height = false;

	for (unsigned d = l->nframes-1; d > 0; d--)
	{
		if (strcmp (l->frames.source[d].file, UNRESOLVED) == 0 &&
		    l->frames.source[d].line == UNRESOLVED_LINENO)
		{
			new_height = d;
			has_new_height = true;
		}
		else if (strcmp (l->frames.source[d].file, NOT_FOUND) == 0 &&
		    l->frames.source[d].line == NOT_FOUND_LINENO)
		{
			new_height = d;
			has_new_height = true;
		}
		else
			break;
	}
	if (has_new_height)
		l->nframes = new_height;
}

bool CodeLocations::process_source_location (char *location_txt, location_t * location, const char *fallback_allocator_name)
{
	/* Example of parsing line:
	   /home/harald/src/intel-tools.git/mini-tools/auto-hbwmalloc/src/tests/libtester.c:6 > /home/harald/src/intel-tools.git/mini-tools/auto-hbwmalloc/src/tests/hello-world-libtester.c:9 @ posix
	   (final EoL is required)
	*/

	DBG("Processing '%s'\n", location_txt);

	// Find and set allocator in location
	char *allocator_marker = find_and_set_allocator (location_txt, location, fallback_allocator_name);
	if (allocator_marker == nullptr) // forward error
		return false;

	count_frames (location_txt, location, allocator_marker, ':');
	assert(location->nframes > 0);

	location->frames.source = (source_frame_t*) _af.realloc (nullptr, sizeof(source_frame_t)*location->nframes);
	assert (location->frames.source != nullptr);

	// Process the frames for this location
	char * prev_frame = location_txt;
	char * frame = strchr (location_txt, ':');
	for (size_t f = 0; f < location->nframes && frame != nullptr; ++f)
	{
		char file[PATH_MAX] = {0};
		size_t file_len = std::min (frame-prev_frame, (std::ptrdiff_t) PATH_MAX-1);
		memcpy (file, prev_frame, file_len);
		file[file_len] = '\0';
		if (options.compareWholePath())
			snprintf (location->frames.source[f].file, PATH_MAX, "%s", file);
		else
			snprintf (location->frames.source[f].file, PATH_MAX, "%s", basename(file));

		char *endptr;
		location->frames.source[f].line = (unsigned) strtol (frame+1, &endptr, 10);

		// Frames is valid only if it is not unresolved / not found and line
		// makes sense (not 0)
		location->frames.source[f].valid =
		    strcmp (location->frames.source[f].file, UNRESOLVED) != 0 &&
		    strcmp (location->frames.source[f].file, NOT_FOUND) != 0 &&
		    location->frames.source[f].line >= 0;;

		DBG("Frame %zu - File = '%s' Line = %u Valid = %d\n", f,
		  location->frames.source[f].file, location->frames.source[f].line, location->frames.source[f].valid );

		prev_frame = strchr (endptr, '>') + 2;
		frame = strchr (endptr, ':');
	}

	return true;
}

long CodeLocations::base_address_for_library (const char *lib)
{
	long baseAddress = 0;

	FILE * mapsfile = fopen ("/proc/self/maps", "r");

	if (mapsfile == nullptr)
	{
		VERBOSE_MSG (1, "Warning! Could not open /proc/self/maps\n");
		return 0;
	}

	DBG("Opened /proc/self/maps to learn about application symbols\n%s", "");

	#define LINE_SIZE 2048
	char line[LINE_SIZE+1];
	unsigned line_no = 0;

	while (!feof (mapsfile))
		if (fgets (line, LINE_SIZE, mapsfile) != nullptr)
		{
			unsigned long start, end, offset;
			char module[LINE_SIZE+1] = {0};
			char permissions[5];

			bool entry_parsed = parse_proc_self_maps_entry (line, &start, &end,
			  sizeof(permissions), permissions, &offset, LINE_SIZE, module);
			if (module[LINE_SIZE] != (char)0)
				break;

			if (entry_parsed && strlen(module) > 0)
			{
				// Check for execution bits, ignore the rest
				if (strcmp (permissions, "r-xp") == 0 ||
					strcmp (permissions, "rwxp") == 0)
				{
					char module_buf[PATH_MAX] = {0};
					const char * p_module = module_buf;
					if (realpath(module, module_buf) == nullptr) {
						VERBOSE_MSG (1, "Warning! Could not get realpath of %s\n", module);
						p_module = module;
					}
					char lib_buf[PATH_MAX] = {0};
					const char * p_lib = lib_buf;
					if (realpath(lib, lib_buf) == nullptr) {
						VERBOSE_MSG (1, "Warning! Could not get realpath of %s\n", lib);
						p_lib = lib;
					}

					// Check if library matches
					if (strcmp (p_module, p_lib) == 0)
					{
						// Base address for main binary is 0
						if (line_no == 0)
							baseAddress = 0;
						else
							baseAddress = start;
						// Stop iterating
						break;
					}
				}
			}
			line_no++;
		}

	fclose (mapsfile);

	DBG("Base address for library (%s) -> %lx\n", lib, baseAddress);

	return baseAddress;
}

bool CodeLocations::process_raw_location (char *location_txt, location_t * location, const char *fallback_allocator_name)
{
	/* Example of parsing line:
	   beefdead!offset1 > deadbeef!offset2 > beefbeef!offset3 @ posix
	   (final EoL is required)
	*/

	DBG("Processing '%s'\n", location_txt);

	// Find and set allocator in location
	char *allocator_marker = find_and_set_allocator (location_txt, location, fallback_allocator_name);
	if (allocator_marker == nullptr) // forward error
		return false;

	count_frames (location_txt, location, allocator_marker, '!');
	assert(location->nframes > 0);

	location->frames.raw = (raw_frame_t*) _af.realloc (nullptr, sizeof(raw_frame_t)*location->nframes);
	assert (location->frames.raw != nullptr);

	// Process the frames for this location
	char * prev_frame = location_txt;
	char * frame = strchr (location_txt, '!');
	for (size_t f = 0; f < location->nframes && frame != nullptr; ++f)
	{
		char module[PATH_MAX] = {0};
		size_t module_len = std::min(frame-prev_frame, (std::ptrdiff_t) PATH_MAX-1);
		memcpy (module, prev_frame, module_len);
		module[module_len] = '\0';

		char *endptr;
		long address = strtoul (frame+1, &endptr, 16);
		assert (endptr <= frame+1+16);

		long base_address = base_address_for_library (module);

		DBG("Address %lx in library %s gets relocated to %lx.\n",
		  address, module, address+base_address);

		location->frames.raw[f].frame = address+base_address;

		prev_frame = strchr (endptr, '>') + 2;
		frame = strchr (endptr, '!');
	}

	return true;
}

bool CodeLocations::readfile (const char *f, const char *fallback_allocator_name)
{
	struct stat sb;
	int fd;

	VERBOSE_MSG(0, "Parsing %s\n", f);

	fd = open (f, O_RDONLY);
	if (fd == -1)
	{
		VERBOSE_MSG(0, "Warning! Could not open file %s\n", f);
		perror("open");
		close (fd);
		return false;
	}
	if (fstat(fd, &sb) == -1)
	{
		VERBOSE_MSG(0, "Warning! fstat failed on file %s\n", f);
		perror("fstat");
		close (fd);
		return false;
	}
	if (!S_ISREG(sb.st_mode))
	{
		VERBOSE_MSG(0, "Warning! Given file (%s) is not a regular file\n", f);
		close (fd);
		return false;
	}
	if (sb.st_size == 0)
	{
		VERBOSE_MSG(0, "Warning! Given file (%s) is empty\n", f);
		close (fd);
		return false;
	}
	char * p = (char *) mmap (nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
	{
		VERBOSE_MSG(0, "Warning! Could not mmap file %s\n", f);
		perror("mmap");
		close (fd);
		return false;
	}
	close(fd);

	// If user has not suggested whether locations are in raw or source-code
	// format, then explore the input file and determine based on its contents
	if (!options.sourceFrame_set())
	{
		unsigned library_address_cnt = 0;
		unsigned source_line_cnt     = 0;

		for (char *p_current = p; p_current < &p[sb.st_size]; ++p_current)
		{
			if (*p_current == '!')
				library_address_cnt++;
			else if (*p_current == ':')
				source_line_cnt++;
		}
		if (library_address_cnt > 0 && source_line_cnt == 0)
		{
			VERBOSE_MSG(0, "Detected code locations format: raw references.\n");
			options.sourceFrames (false);
		}
		else if (library_address_cnt == 0 && source_line_cnt > 0)
		{
			VERBOSE_MSG(0, "Detected code locations format: source-code references.\n");
			options.sourceFrames (true);
		}
		else
		{
			VERBOSE_MSG(0, "Error! Cannot determine whether the locations file contains raw or source code references\n");
			exit (-1);
		}
	}

	_nlocations = 0;
	for (char * p_current = p, *prevEOL = p; p_current != nullptr && p_current < &p[sb.st_size];
			// prevEOL needs to be equal to p_current+1 except on the first iteration, as p_current points
			// to the (n-1)-th '\n' at the begining of every iteration (excepted the first one).
			p_current = strchr (p_current, '\n'), prevEOL = p_current + 1)
	{
		// Are there locations -- identified by @ presence
		char *nextAT = strchr (p_current + 1, '@');
		if (nextAT == nullptr)
			// No more locations
			break;

		// If there are locations, make sure that we start in the line that contains the @
		for (char *nextEOL = strchr (prevEOL + 1, '\n');
				nextEOL != nullptr && nextEOL < nextAT;
				nextEOL = strchr (prevEOL, '\n'))
		{
			prevEOL = nextEOL + 1;
		}
		p_current = prevEOL;

		if (p_current[0] == '#')
		{
			// COMMENT
			continue;
		}

		_locations = (location_t*) _af.realloc (_locations, sizeof(location_t)*(_nlocations+1));

		// Process source location and see if it is correctly processed (and not ignored).
		if (options.sourceFrames() &&
				process_source_location (p_current, &_locations[_nlocations], fallback_allocator_name))
		{
			clean_source_location (&_locations[_nlocations]);
			if (_locations[_nlocations].nframes > options.maxDepth())
			{
				VERBOSE_MSG(0, "* Warning! Location #%u was too long (%u frames). Truncated to %u frames.\n",
						_nlocations+1, _locations[_nlocations].nframes, options.maxDepth());
				_locations[_nlocations].nframes = options.maxDepth();
			}
		}
		// Process raw location and see if it is correctly processed (and not ignored).
		else if (!options.sourceFrames() &&
				process_raw_location (p_current, &_locations[_nlocations], fallback_allocator_name))
		{ // Nothing to be done.
		}
		// There has been an error while trying to process a location
		else
		{
			VERBOSE_MSG(0, "* Warning! There has been an error while processing Location #%u. The location is ignored.\n", _nlocations+1);
			continue;
		}

		if (! _locations[_nlocations].allocator->is_ready()) {
			VERBOSE_MSG(0, "The allocator \"%s\" is not available. Check if its parameters in the configuration file are correct.\n", _locations[_nlocations].allocator->name());
			exit (-1);
		}

		_min_nframes = std::min(_min_nframes, _locations[_nlocations].nframes);
		_max_nframes = std::max(_max_nframes, _locations[_nlocations].nframes);
		memset (&_locations[_nlocations].stats, 0, sizeof(location_stats_t));
		_locations[_nlocations].id = _nlocations+1;
		_nlocations++;
	}

	if (_nlocations > 0)
		VERBOSE_MSG(0, "Locations contain min = %u - max = %u frames.\n",
		  _min_nframes, _max_nframes);

	// Dump frames to terminal, for user checks
	// This sort is not necessary since at this point, locations have been already
	// sorted by ID. Kept for exemplification purpose.
	// std::sort (_locations, _locations+_nlocations, comparator_by_ID);
	show_frames ();

	// Keep the locations sorted by the number of frames.
	std::sort (_locations, _locations+_nlocations, comparator_by_NumberOfFrames);

	if (_nlocations > 0)
		create_fast_indexes_for_frames();

	munmap(p, sb.st_size);
	return true;
}

void CodeLocations::create_fast_indexes_for_frames (void)
{
	assert (_min_nframes > 0);
	assert (_max_nframes > 0);
	assert (_min_nframes <= _max_nframes);

	// Calculate fast indices
	//   _fast_indexes[K] -> points to first element in _locations (
	//   when sorted by num frames) that has K frames. If no location has
	//   K frames, then point to the next level of frames

	_fast_indexes_frames = (unsigned*) _af.realloc (
	  _fast_indexes_frames, sizeof(unsigned)*(2+_max_nframes));
	assert (_fast_indexes_frames != nullptr);

	memset (_fast_indexes_frames, 0, sizeof(unsigned)*(2+_max_nframes));

	assert (_nlocations > 0);
	{
		assert (_locations[0].nframes > 0);
		unsigned highest_frame_no = _locations[0].nframes;
		_fast_indexes_frames[highest_frame_no] = 0;
		for (unsigned u = 1; u < _nlocations; ++u)
		{
			// If we observe a new higher frame number, keep its initial
			// index in _fast_indexes_frames
			if (_locations[u].nframes > highest_frame_no)
			{
				// Neeed to fill the positions from previous highest
				// and current highest. Need to iterate in case
				// that frame heights are not consecutive
				for (unsigned uu = highest_frame_no+1; uu <= _locations[u].nframes; ++uu)
					_fast_indexes_frames[uu] = u;
				highest_frame_no = _locations[u].nframes;
			}
			if (_locations[u].nframes == _max_nframes)
				break;
		}
	}
	_fast_indexes_frames[_max_nframes+1] = _nlocations;

#if defined(DEBUG)
	// Extra checks for _fast_indexes
	for (unsigned u = 0; u < 2+_max_nframes; ++u)
		DBG("_fast_indexes_frames[%u] = %u\n", u, _fast_indexes_frames[u]);
	assert (_fast_indexes_frames[0] == 0);
	for (unsigned u = 0; u < 1+_max_nframes; ++u)
	{
		assert (_fast_indexes_frames[u] <= _fast_indexes_frames[u+1]);
	}
	show_frames ();
#endif
}

// Show statistics related to the code locations recorded
void CodeLocations::show_stats (void)
{
	if (_nlocations > 0)
	{
		VERBOSE_MSG(0, "Locations information:\n");
	}
	else
	{
		VERBOSE_MSG(0, "Empty locations file provided.\n");
	}

	for (unsigned l = 0; l < _nlocations; ++l)
	{
		// Show the call-stack in a compact format first
		VERBOSE_MSG(0, "* Location %d on allocator '%s' with %u frames\n",
		  _locations[l].id, allocator(l)->name(), _locations[l].nframes);
		for (unsigned f = 0; f < _locations[l].nframes; f++)
		{
			const source_frame_t * fp = &(_locations[l].frames.source[f]);
			const raw_frame_t    * rf = &(_locations[l].frames.raw[f]);
			if (f == 0)
			{
				if (options.sourceFrames())
				{
					VERBOSE_MSG(0, " - [ %s:%d", fp->file, fp->line);
				}
				else
				{
					VERBOSE_MSG(0, " - [ %08lx", rf->frame);
				}
			}
			else
			{
				if (options.sourceFrames())
				{
					VERBOSE_MSG_NOPREFIX(0, " > %s:%d", fp->file, fp->line);
				}
				else
				{
					VERBOSE_MSG_NOPREFIX(0, " > %08lx", rf->frame);
				}
			}
		}
		VERBOSE_MSG_NOPREFIX(0, " ]\n");

		VERBOSE_MSG(0,
		  " - Max simultaneously living objects: %u\n",
		  _locations[l].stats.n_max_living_objects);
		VERBOSE_MSG(0,
		  " - HWM (in req. allocator / in fallback allocator): %lu / %lu Mbytes\n",
		  _locations[l].stats.HWM >> 20,
		  _locations[l].stats.HWM_fb >> 20);

		// Show the statistics for this code-location
		unsigned c_accesses = _locations[l].stats.n_allocations_in_cache +
		   _locations[l].stats.n_allocations_not_in_cache;
		if (c_accesses > 0)
		{
			float hit_r = ((float) (_locations[l].stats.n_allocations_in_cache) / (float) (c_accesses)) * 100.f;
			unsigned not_fit = _locations[l].stats.n_allocations_not_fit;
			if (not_fit > 0)
			{
				VERBOSE_MSG(0, " - %u matches (%.2f%% hit on callstack cache) and %u not fit.\n",
				  c_accesses, hit_r, not_fit);
			}
			else
			{
				VERBOSE_MSG(0, " - %u matches (%.2f%% hit on callstack cache).\n",
				  c_accesses, hit_r);
			}
		}
		else
		{
			unsigned not_fit = _locations[l].stats.n_allocations_not_fit;
			if (not_fit > 0)
			{
				VERBOSE_MSG(0, " - %u matches and %u not fit.\n",
				  _locations[l].stats.n_allocations, not_fit);
			}
			else
			{
				VERBOSE_MSG(0, " - %u matches.\n", _locations[l].stats.n_allocations);
			}
		}
	}
}


// Show frames for all the recorded locations
void CodeLocations::show_frames (void)
{
	if (_nlocations > 0)
	{
		VERBOSE_MSG(0, "%s locations information:\n",
		  options.sourceFrames() ? "Source code" : "Raw");
	}
	else
	{
		VERBOSE_MSG(0, "Empty locations file provided.\n");
	}

	for (unsigned l = 0; l < _nlocations; ++l)
	{
		VERBOSE_MSG(0, "* Location %d on allocator '%s' with %u frames",
		  _locations[l].id, allocator(l)->name(), _locations[l].nframes);

		if (options.verboseLvl() > 1)
		{
			VERBOSE_MSG_NOPREFIX(2, "\n");
		}
		else
		{
			VERBOSE_MSG_NOPREFIX(0, ": ");
		}
		for (unsigned f = 0; f < _locations[l].nframes; f++)
		{
			const source_frame_t * fp = &(_locations[l].frames.source[f]);
			const raw_frame_t    * rf = &(_locations[l].frames.raw[f]);

			// When verbosity > 0, then we use the extended version which and
			// with verbosity = 0, we use the compact version
			if (options.verboseLvl() > 1)
			{
				if (options.sourceFrames())
				{
					VERBOSE_MSG(2, "  - Frame %d: %s:%d%s\n", f, fp->file, fp->line, fp->valid?"":" (*)");
				}
				else
				{
					VERBOSE_MSG(2, "  - Frame %d: %08lx\n", f, rf->frame);
				}
			}
			else
			{
				if (options.sourceFrames())
				{
					VERBOSE_MSG_NOPREFIX(0, "%s%s:%d%s", (f==0)?"[ ":" > ", fp->file, fp->line, fp->valid?"":" (*)");
				}
				else
				{
					VERBOSE_MSG_NOPREFIX(0, "%s%08lx", (f==0)?"[ ":" > ", rf->frame);
				}
			}
		}
		if (options.verboseLvl() <= 1)
		{
			VERBOSE_MSG_NOPREFIX(0, " ]\n");
		}
	}
}

void CodeLocations::show_hmem_visualizer_stats (const char *fallback_allocator_name)
{
	if (_nlocations > 0)
	{
		VERBOSE_MSG(0, "Locations information:\n");
	}
	else
	{
		VERBOSE_MSG(0, "Empty locations file provided.\n");
	}

	VERBOSE_MSG(0, "-- HMEM visualizer results -- (cut here) --\n");

	// Form of hmem_visualizer is:
	//
	// #vis <mem_name>_capacity = <capacity> (current HEADER)
	// #vis type=mem, name=<mem_name>, capacity=<capacity> (forecoming HEADER)
	unsigned u = 0;
	Allocator **al = _allocators->get();
	while (al[u] != nullptr)
	{
		if (al[u]->has_size() && al[u]->used())
		{
			fprintf (options.messages_on_stderr()?stderr:stdout, "#vis type=mem, name=%s, capacity=%lu\n",
			  al[u]->name(), al[u]->size());
		}
		u++;
	}

	// Additional information for extra fields
	fprintf (options.messages_on_stderr()?stderr:stdout, "#vis extra_field_0 = hatch\n");

	// Data
	// callstack;bytes;weight;memtype[;extrafield1;extrafield2;...]
	for (unsigned l = 0; l < _nlocations; ++l)
	{
		// Decompose this in 2 parts.

		// First -- information on allocations that were served by the requested allocator
		if (_locations[l].stats.HWM > 0)
		{
			//   callstack part
			const source_frame_t * fp = &(_locations[l].frames.source[0]);
			const raw_frame_t    * rf = &(_locations[l].frames.raw[0]);

			if (options.sourceFrames())
				fprintf (options.messages_on_stderr()?stderr:stdout, "%s:%d", fp->file, fp->line);
			else
				fprintf (options.messages_on_stderr()?stderr:stdout, "%08lx", rf->frame);
			for (unsigned f = 1; f < _locations[l].nframes; f++)
			{
				fp = &(_locations[l].frames.source[f]);
				rf = &(_locations[l].frames.raw[f]);
				if (options.sourceFrames())
					fprintf (options.messages_on_stderr()?stderr:stdout, " > %s:%d", fp->file, fp->line);
				else
					fprintf (options.messages_on_stderr()?stderr:stdout, " > %08lx", rf->frame);
			}
			//   bytes part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%lu", _locations[l].stats.HWM);
			//   weight part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%d", 0);
			//   memtype part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%s", allocator(l)->name());
			//   EoL
			fprintf (options.messages_on_stderr()?stderr:stdout, "\n");
		}

		// Second -- information on allocations that were served by the fallback allocator
		if (_locations[l].stats.HWM_fb > 0)
		{
			//   callstack part -- only if fallback memory used
			const source_frame_t * fp = &(_locations[l].frames.source[0]);
			const raw_frame_t    * rf = &(_locations[l].frames.raw[0]);
			if (options.sourceFrames())
				fprintf (options.messages_on_stderr()?stderr:stdout, "%s:%d", fp->file, fp->line);
			else
				fprintf (options.messages_on_stderr()?stderr:stdout, "%08lx", rf->frame);
			for (unsigned f = 1; f < _locations[l].nframes; f++)
			{
				fp = &(_locations[l].frames.source[f]);
				rf = &(_locations[l].frames.raw[f]);
				if (options.sourceFrames())
					fprintf (options.messages_on_stderr()?stderr:stdout, " > %s:%d", fp->file, fp->line);
				else
					fprintf (options.messages_on_stderr()?stderr:stdout, " > %08lx", rf->frame);
			}
			//   bytes part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%lu", _locations[l].stats.HWM_fb);
			//   weight part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%d", 0);
			//   memtype part
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%s", fallback_allocator_name);
			//   extra field
			fprintf (options.messages_on_stderr()?stderr:stdout, ";%d", 1);
			//   EoL
			fprintf (options.messages_on_stderr()?stderr:stdout, "\n");
		}
	}
	VERBOSE_MSG(0, "-- HMEM visualizer results -- (end cut here) --\n");
}

inline unsigned CodeLocations::get_min_index_for_number_of_frames (unsigned nframes) const
{
	if (_min_nframes <= nframes && nframes <= _max_nframes)
		return _fast_indexes_frames[nframes];
	else
		return 0;
}

inline unsigned CodeLocations::get_max_index_for_number_of_frames (unsigned nframes) const
{
	if (_min_nframes <= nframes && nframes <= _max_nframes)
		return _fast_indexes_frames[nframes+1];
	else
		return 0;
}

Allocator * CodeLocations::match (unsigned nframes, void **frames, unsigned & location_id)
{
	if ((nframes < _min_nframes) || (nframes > _max_nframes))
	{
		DBG("Info: Match? %s\n", "no");
		return nullptr;
	}

	unsigned min_idx = get_min_index_for_number_of_frames (nframes);
	unsigned max_idx = get_max_index_for_number_of_frames (nframes);

	DBG("Callstack with %u frames will search in range [%u, %u).\n",
	  nframes, min_idx, max_idx);

	for (unsigned i = min_idx; i < max_idx; i++)
	{
		DBG("Comparing callstack with %u frames against location #%d (idx %u)\n",
		  nframes, _locations[i].id, i);

		// This conditional is likely to be unnecessary after the inclusion
		// of min_idx/max_idx for loop
		if (_locations[i].nframes == nframes)
		{
			bool match = _locations[i].frames.raw[0].frame == (long) frames[0];

			DBG("Comparing frame 0 <%08lx> <%08lx> match? %s\n",
			  _locations[i].frames.raw[0].frame, (long) frames[0],
			  match?"yes":"no");

			if (match)
			{
				unsigned frame = 1;
				unsigned highest_frame = nframes;
				while (frame < highest_frame && match)
				{
					match = _locations[i].frames.raw[frame].frame == (long) frames[frame];

					DBG("Comparing frame %u <%08lx> <%08lx> match? %s\n",
					  frame, _locations[i].frames.raw[frame].frame, (long) frames[frame],
					  match?"yes":"no");

					frame++;
				}

				DBG("Info: Match? %s\n", match?"yes":"no");

				if (match && (frame == highest_frame))
				{
					DBG("Info: Location id = %d, Allocator = %p (%s)\n", _locations[i].id,
					  _locations[i].allocator, _locations[i].allocator->name());
					location_id = i;
					return _locations[i].allocator;
				}
			}
		}
	}

	return nullptr;
}

Allocator * CodeLocations::match (unsigned nframes, const translated_frame_t *tf,
	unsigned & location_id)
{
	DBG("(nframes = %u [min = %u, max = %u], tf = %p)\n", nframes, _min_nframes, _max_nframes, tf);

	for (unsigned i = 0; i < nframes; i++)
		DBG("frame = %u : file %p <%s:%u>\n", i, tf[i].file, tf[i].file != nullptr ? tf[i].file : "", tf[i].line);

	if ((nframes < _min_nframes) || (nframes > _max_nframes))
	{
		DBG("Info: Match? %s\n", "no");
		return nullptr;
	}

	for (unsigned i = 0; i < _nlocations; i++)
	{
		DBG("Comparing callstack with %u frames against location #%d with deep %u\n",
		  nframes, _locations[i].id, _locations[i].nframes);

		if (_locations[i].nframes == nframes)
		{
			bool match = true;
			if (tf[0].file != nullptr && tf[0].line > 0 && _locations[i].frames.source[0].valid)
				match = (strcasecmp (_locations[i].frames.source[0].file, tf[0].file) == 0) &&
				  _locations[i].frames.source[0].line == tf[0].line;

			DBG("Comparing frame 0 <%s:%u> <%s:%u> match? %s\n",
			  _locations[i].frames.source[0].file, _locations[i].frames.source[0].line,
			  tf[0].file, tf[0].line,
			  match?"yes":"no");

			if (match)
			{
				unsigned frame = 1;
				unsigned highest_frame = nframes;
				while (frame < highest_frame && match)
				{
					// We skip frames that have not been properly translated
					// We also skip frames from locations that are not valid
					if (tf[frame].file != nullptr && tf[frame].line > 0 &&
					    _locations[i].frames.source[frame].valid)
						match = strcasecmp (_locations[i].frames.source[frame].file, tf[frame].file) == 0 &&
						  _locations[i].frames.source[frame].line == tf[frame].line;

					DBG("Comparing frame %u <%s:%u> <%s:%u> match? %s\n",
					  frame,
					  _locations[i].frames.source[frame].file, _locations[i].frames.source[frame].line,
					  tf[frame].file, tf[frame].line,
					  match?"yes":"no");

					frame++;
				}

				DBG("Info: Match? %s\n", match?"yes":"no");

				if (match && (frame == highest_frame))
				{
					DBG("Info: Location id = %d, Allocator = %p (%s)\n", _locations[i].id,
					  _locations[i].allocator, _locations[i].allocator->name());
					location_id = i;
					return _locations[i].allocator;
				}
			}
		}
	}

	return nullptr;
}

void CodeLocations::record_location (unsigned lid, bool fits, bool in_cache)
{
	assert (lid < _nlocations);

	_locations[lid].stats.n_allocations++;

	if (in_cache)
		_locations[lid].stats.n_allocations_in_cache++;
	else
		_locations[lid].stats.n_allocations_not_in_cache++;
	if (!fits)
		_locations[lid].stats.n_allocations_not_fit++;
}

void CodeLocations::record_location (unsigned lid, bool fits)
{
	assert (lid < _nlocations);

	_locations[lid].stats.n_allocations++;

	if (!fits)
		_locations[lid].stats.n_allocations_not_fit++;
}

void CodeLocations::record_location_add_memory (unsigned lid, size_t size, bool fallback_allocator)
{
	assert (lid < _nlocations);

	if (!fallback_allocator)
	{
		_locations[lid].stats.current_used_memory += size;
		if (_locations[lid].stats.HWM < _locations[lid].stats.current_used_memory)
			_locations[lid].stats.HWM = _locations[lid].stats.current_used_memory;
	}
	else
	{
		_locations[lid].stats.current_used_memory_fb += size;
		if (_locations[lid].stats.HWM_fb < _locations[lid].stats.current_used_memory_fb)
			_locations[lid].stats.HWM_fb = _locations[lid].stats.current_used_memory_fb;
	}
	_locations[lid].stats.n_living_objects++;
	if (_locations[lid].stats.n_max_living_objects < _locations[lid].stats.n_living_objects)
	  _locations[lid].stats.n_max_living_objects = _locations[lid].stats.n_living_objects;
}

void CodeLocations::record_location_sub_memory (unsigned lid, size_t size, bool fallback_allocator)
{
	assert (lid < _nlocations);

	if (!fallback_allocator)
	{
		assert (_locations[lid].stats.current_used_memory >= size);
		if (_locations[lid].stats.current_used_memory >= size)
			_locations[lid].stats.current_used_memory -= size;
	}
	else
	{
		assert (_locations[lid].stats.current_used_memory_fb >= size);
		if (_locations[lid].stats.current_used_memory_fb >= size)
			_locations[lid].stats.current_used_memory_fb -= size;
	}

	assert(_locations[lid].stats.n_living_objects > 0);
	if (_locations[lid].stats.n_living_objects > 0)
		_locations[lid].stats.n_living_objects--;
}
