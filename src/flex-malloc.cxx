
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>

#include "common.hxx"
#include "utils.hxx"
#include "flex-malloc.hxx"
#include "allocator.hxx"

static AllocatorStatistics _uninitialized_stats;

FlexMalloc::FlexMalloc (allocation_functions_t &af, Allocator * f, CodeLocations *cl)
  : _af(af), _fallback(f), _allocators (cl->allocators()), _modules(nullptr),
    _nmodules(0), _cl(cl)
{
	assert (_fallback != nullptr);

	if (options.sourceFrames())
		parse_map_files();
}

FlexMalloc::~FlexMalloc ()
{
}

// FlexMalloc::uninitialized_malloc
//   performs a malloc when the FlexMalloc library has not been fully initialized or
//   when specifically requesting memory from regular posix calls. This routine adds
//   the necessary information to the created buffer (i.e. header)
void * FlexMalloc::uninitialized_malloc (size_t size)
{
	void *res = nullptr;
	void* (*tmp_malloc)(size_t) = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
	if (tmp_malloc == nullptr)
		return nullptr;

	DBG("(size = %lu)\n", size);
	void * baseptr = tmp_malloc (Allocator::getTotalSize (size));
	if (baseptr != nullptr)
	{
		res = Allocator::generateAllocatorHeader (baseptr, nullptr, size);
		DBG("returning %p\n", res);
		if (res != nullptr)
			_uninitialized_stats.record_malloc (size);
	}
	return res;
}

// FlexMalloc::uninitialized_posix_memalign
//   performs a posix_memalign when the FlexMalloc library has not been fully initialized
//   or when specifically requesting memory from regular posix calls. This routine adds
//   the necessary information to the created buffer (i.e. header)
//    - invokes uninitialized_malloc
int FlexMalloc::uninitialized_posix_memalign (void **ptr, size_t align, size_t size)
{
	void *res = nullptr;
	void* (*tmp_malloc)(size_t) = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
	if (tmp_malloc == nullptr)
		return ENOMEM;

	DBG("(ptr = %p, align = %lu, size = %lu)\n", ptr, align, size);
	void * baseptr = tmp_malloc (Allocator::getTotalSize(size + align));
	if (baseptr != nullptr)
	{
		res = Allocator::generateAllocatorHeaderOnAligned (baseptr, align, nullptr, size);
	}
	DBG("returning ptr %p\n", res);
	if (ptr != nullptr)
	{
		*ptr = res;
		_uninitialized_stats.record_aligned_malloc (size);
		return 0;
	}
	else
		return ENOMEM;
}

// FlexMalloc::uninitialized_realloc
//   performs a realloc when the FlexMalloc library has not been fully initialized
//   or when specifically requesting memory from regular posix calls. This routine adds
//   the necessary information to the created buffer (i.e. header)
void * FlexMalloc::uninitialized_realloc (void *ptr, size_t size)
{
	void *res = nullptr;

	// If given an allocated pointer, then we handle it normally as a realloc
	if (ptr)
	{
		void *new_baseptr = nullptr;

		// Given this pointer - we check whether the pointer was allocated by any allocator from
		//   flexmalloc or by "default" calls (in _af structure).
		Allocator::Header_t *prev_hdr = Allocator::getAllocatorHeader (ptr);
		Allocator *prev_allocator = prev_hdr->allocator;
		size_t prev_size  = prev_hdr->size;
		void * prev_base  = prev_hdr->base_ptr;
		uintptr_t extra_size = Allocator::getExtraSize (prev_hdr);

		DBG("with prev_allocator = %p and prev_size = %lu\n", prev_allocator, prev_size);

		// Only act if requested buffer is larger
		if (prev_size < size)
		{
			// Unhandled allocator
			if (prev_allocator == nullptr)
			{
				void* (*tmp_realloc)(void*, size_t) = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "realloc");
				if (tmp_realloc == nullptr)
					return nullptr;

				new_baseptr = tmp_realloc (prev_base, Allocator::getTotalSize (size + extra_size));

				res = Allocator::generateAllocatorHeader (new_baseptr, extra_size, nullptr, size);

				DBG("Reallocated (%ld->%ld [extra bytes = %lu]) from %p (base at %p, header at %p) into %p (base at %p, header at %p)\n", prev_size, size, extra_size, ptr, prev_base, prev_hdr, res, new_baseptr, Allocator::getAllocatorHeader (res));

				_uninitialized_stats.record_realloc (prev_size, size);
			}
			// Pointer handled by an allocator? Same allocator will handle it then
			else if (prev_allocator != nullptr)
			{
				res = prev_allocator->realloc (ptr, size);
			}

		}
		else
			res = ptr;

		DBG("returning ptr %p\n", res);
	}
	// If we're given a null pointer, pass to malloc
	else
	{
		DBG("realloc forwarding to malloc%s\n", "");
		res = uninitialized_malloc (size);
	}

	return res;
}

void FlexMalloc::uninitialized_free (void *ptr)
{
	void (*tmp_free)(void*) = (void (*)(void*)) dlsym (RTLD_NEXT, "free");
	if (tmp_free == nullptr)
		return;

	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);
	_uninitialized_stats.record_free (hdr->size);
	tmp_free (hdr->base_ptr);
}

size_t FlexMalloc::uninitialized_malloc_usable_size (void *ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);
	return hdr->size;
}

static const char * __flexmalloc_excluded_libraries [] = {
	"/libnuma.so",
	"/libmemkind.so",
	"/libbfd-",
	"/libgcc_s-",
	// "/libc-",  // Omit this! prevents libc-start!
	"/libm-",
	"/librt-",
	"/libdl-",
	"/ld-",
	"/libpthread-",
	"/libflexmalloc.so",
	"/libmpi.so",
	"/libmpicxx.so",
	"/libmpifort.so",
	nullptr
};

bool FlexMalloc::excluded_library (const char *library)
{
	unsigned u = 0;
	while (__flexmalloc_excluded_libraries[u] != nullptr)
	{
		if (strstr (library, __flexmalloc_excluded_libraries[u]) != nullptr )
			return true;
		u++;
	}
	return false;
}


void FlexMalloc::parse_map_files (void)
{
	FILE * mapsfile = fopen ("/proc/self/maps", "r");

	if (mapsfile == nullptr)
	{
		VERBOSE_MSG (1, "Warning! Could not open /proc/self/maps\n");
		return;
	}

	VERBOSE_MSG(1, "Opened /proc/self/maps to learn about application symbols\n");

	#define LINE_SIZE 2048
	char line[LINE_SIZE+1];
	unsigned line_no = 0;

	while (!feof (mapsfile))
		if (fgets (line, LINE_SIZE, mapsfile) != nullptr)
		{
			unsigned long start, end, offset;
			char module[LINE_SIZE+1];
			char permissions[5];

			line_no++;

			memset (module, 0, (LINE_SIZE+1)*sizeof(char));
			bool entry_parsed = parse_proc_self_maps_entry (line, &start, &end,
			  sizeof(permissions), permissions, &offset, LINE_SIZE, module);
			if (module[LINE_SIZE] != (char)0)
				break;

			bool excluded = true;
			if (entry_parsed && strlen(module) > 0)
				excluded = excluded_library (module);

			if (entry_parsed && !excluded)
			{
				// Check for execution bits, ignore the rest
				if (strcmp (permissions, "r-xp") == 0 ||
				    strcmp (permissions, "rwxp") == 0)
					// avoid [vdso], [syscall] and others
					if (strlen (module) > 0 && module[0] != '[') 
					{
						DBG("Processing line %u, %s [0x%08lx-0x%08lx] and tentatively inserting into index %u\n",
						  line_no, module, start, end, _nmodules);

						_modules = (module_t*) _af.realloc (_modules, (_nmodules+1)*sizeof(module_t));
						assert (_modules != nullptr);

						_modules[_nmodules].name = strdup (module);
						assert (_modules[_nmodules].name != nullptr);
						_modules[_nmodules].startAddress = start;
						_modules[_nmodules].endAddress = end;
						_modules[_nmodules].bfd = new BFDManager;
						_modules[_nmodules].symbolsLoaded =
						  _modules[_nmodules].bfd->load_binary (_modules[_nmodules].name);

						// If BFD failed to load the binary/symbols then ignore this
						// recently created entry
						if (_modules[_nmodules].symbolsLoaded)
						{
							VERBOSE_MSG(1, "Successfully loaded symbols from %s into index %u\n",
							  _modules[_nmodules].name, _nmodules);
							_nmodules++;
						}
						else
						{
							VERBOSE_MSG(2, "Could not load symbols from %s\n",
							  _modules[_nmodules].name);
						}
					}
			}
			else if (entry_parsed && excluded)
			{
				// Check for execution bits, ignore the rest
				if (strcmp (permissions, "r-xp") == 0 ||
				    strcmp (permissions, "rwxp") == 0)
					VERBOSE_MSG(2, "Excluding the analysis of %s\n", module);
			}
		}

	VERBOSE_MSG(1, "Loaded symbols from %u libraries\n", _nmodules);

	fclose (mapsfile);
}

inline Allocator * FlexMalloc::allocatorForCallstack (unsigned nptrs, void **callstack, size_t size, bool& fits, uint32_t& CL)
{
	if (options.sourceFrames())
		return allocatorForCallstack_source (nptrs, callstack, size, fits, CL);
	else
		return allocatorForCallstack_raw (nptrs, callstack, size, fits, CL);
}

Allocator * FlexMalloc::allocatorForCallstack_source (unsigned nptrs, void **callstack, size_t size, bool& fits, uint32_t& CL)
{
	Allocator *a = nullptr;
	bool _c_hit = _c_cache.match (nptrs, callstack, a, CL);
	if (! _c_hit )
	{
		// Process each callstack frame. Check on which module it resides, compute effective address
		// and then translate it using BFD (if possible)

		translated_frame_t tf[nptrs];
		unsigned n_translated_frames = 0;
		unsigned highest_translated_frame = 0;
		bool any_translated = false;

		// Initialize data structure
		memset (tf, 0, sizeof(translated_frame_t)*nptrs);

		for (unsigned frame = 0; frame < nptrs; frame++)
		{
			DBG("Frame %u (out of %u) points to %p\n", frame, nptrs, callstack[frame]);

			const char *fname = nullptr;
			char *file = nullptr;
			long lptr = (long) callstack[frame];
			void *effective_address = nullptr;

			tf[frame].translated = false;
			for (unsigned m = 0; m < _nmodules; ++m)
				if (_modules[m].startAddress <= lptr && lptr <= _modules[m].endAddress && _modules[m].symbolsLoaded)
				{
					if (m != 0) // If we're looking into a module, substract its base address
						effective_address = (void*) ((long) callstack[frame] - _modules[m].startAddress);
					else
						effective_address = callstack[frame];

					DBG("Frame %d hit module #%d (%s) and effective address is %p\n", frame, m+1, _modules[m].name, effective_address);

					tf[frame].translated =
					  _modules[m].bfd->translate_address (effective_address, &fname, &file, &tf[frame].line);
					break;
				}

			if (tf[frame].translated && file != nullptr && fname != nullptr)
			{
				any_translated = true;
				highest_translated_frame = frame;

#warning Do we need strdup() here?
				if (options.compareWholePath())
					tf[frame].file = file;
				else
					tf[frame].file = basename(file);
	
				DBG("Frame %d (%p) translated into: %s [%s:%d].\n",
				  frame, effective_address, fname, tf[frame].file, tf[frame].line);

				if (options.stopAtMain())
					/* Stop parsing backtrace at main -- avoid start symbol, for instance */
					if (strncmp (fname, "main", 4) == 0 || strncmp (fname, "MAIN__", 6) == 0)
						break;
			}
			else
			{
				tf[frame].file = nullptr;
				tf[frame].line = 0;
				DBG("Frame %d (%p) was not translated.\n", frame, effective_address);
			}

			// Stop translating once we have translated more frames than the max of frames
			// seen in the code-locations.
			if (any_translated)
			{
				n_translated_frames++;
				if (n_translated_frames >= _cl->max_nframes())
				{
					DBG("Breaking call-stack translation because translated max frames (%u) in code locations.\n",
					  _cl->max_nframes());
					break;
				}
			}
		}

		// Make sure that we have translated a portion of the call-stack
		if (any_translated)
		{
			// Ignore the initial frame (the malloc routine call itself) and the prefix of the
			// the callstack that has not been translated
			unsigned initial_frame = 0;
			while (initial_frame < nptrs)
			{
				if (tf[initial_frame].translated)
					break;
				initial_frame++;
			}

			unsigned nframes = highest_translated_frame - initial_frame + 1;
			DBG("Number of used frames: %u = %u - %u + 1 \n", nframes, highest_translated_frame, initial_frame);
			if (nframes > 0)
				a = _cl->match (nframes, &tf[initial_frame], CL);
			DBG("Cache - adding match - a = %p CL = %u\n", a, CL);
			_c_cache.add_match (nptrs, callstack, a, CL); // Record the original call-stack
		}
	}

	if (nullptr != a)
	{
		fits = a->fits(size);
		DBG("Location %u hit = %d fits = %d\n", CL, _c_hit, fits);
		_cl->record_location (CL, fits, _c_hit);
	}
	else
	{
		fits = true;
		DBG("Location is not registered to any allocator.%s\n", "");
	}

	return a;
}

Allocator * FlexMalloc::allocatorForCallstack_raw (unsigned nptrs, void **callstack, size_t size, bool& fits, uint32_t& CL)
{
	Allocator *a = _cl->match (nptrs, callstack, CL);

	if (nullptr != a)
	{
		fits = a->fits(size);
		DBG("Location %u fits = %d\n", CL, fits);
		_cl->record_location (CL, fits);
	}
	else
	{
		fits = true;
		DBG("Location is not registered to any allocator.%s\n", "");
	}

	return a;
}

void * FlexMalloc::malloc (unsigned nptrs, void **callstack, size_t size)
{
	DBG("(nptrs = %u callstack = %p size = %lu)\n", nptrs, callstack, size);

	bool fits;
	bool save_CL = false;
	uint32_t CL;
	Allocator *a = allocatorForCallstack (nptrs, callstack, size, fits, CL);
	if (!fits)
	{
		DBG("Willing to allocate %lu bytes using allocator '%s' but it does not fit. Using fallback allocator.\n",
		    size, a->name());
		a->record_unfitted_malloc (size);
		a = _fallback;
	}
	if (nullptr == a)
		a = _fallback;
	else
		save_CL = true;

	DBG("Allocating %lu bytes using allocator '%s'\n", size, a->name());
	void * res = a->malloc(size);
	DBG("Data allocated in %p\n", res);

	// Save code location to quantify HWM per location
	if (save_CL)
	{
		Allocator::codeLocation (res, CL);
		_cl->record_location_add_memory (CL, size, !fits);
	}

	return res;
}

void * FlexMalloc::calloc (unsigned nptrs, void **callstack, size_t nmemb, size_t size)
{
	DBG("(nptrs = %u callstack = %p nmemb = %lu size = %lu)\n", nptrs, callstack, nmemb, size);

	bool fits;
	bool save_CL = false;
	uint32_t CL;
	Allocator *a = allocatorForCallstack (nptrs, callstack, size, fits, CL);
	if (!fits)
	{
		DBG("Willing to allocate %lu bytes using allocator '%s' but it does not fit. Using fallback allocator.\n",
		    size, a->name());
		a->record_unfitted_calloc (size);
		a = _fallback;
	}
	if (nullptr == a)
		a = _fallback;
	else
		save_CL = true;

	DBG("Allocating %lu bytes using allocator '%s'\n", size, a->name());
	void * res = a->calloc(nmemb, size);
	DBG("Data allocated in %p\n", res);

	// Save code location to quantify HWM per location
	if (save_CL)
	{
		Allocator::codeLocation (res, CL);
		_cl->record_location_add_memory (CL, nmemb * size, !fits);
	}

	return res;
}

int FlexMalloc::posix_memalign (unsigned nptrs, void **callstack, void ** memptr, size_t alignment, size_t size)
{
	assert (powerof2(alignment));
	void *ptr;

	DBG("(nptrs = %u callstack = %p memptr = %p alignment = %lu size = %lu)\n", nptrs, callstack, memptr, alignment, size);

	bool fits;
	bool save_CL = false;
	uint32_t CL;
	Allocator *a = allocatorForCallstack (nptrs, callstack, size, fits, CL);
	if (!fits)
	{
		DBG("Willing to allocate %lu bytes using allocator '%s' but it does not fit. Using fallback allocator.\n",
		    size, a->name());
		a->record_unfitted_aligned_malloc (size);
		a = _fallback;
	}
	if (nullptr == a)
		a = _fallback;
	else
		save_CL = true;

	DBG("Allocating %lu bytes using allocator '%s'\n", size, a->name());
	int res = a->posix_memalign (&ptr, alignment, size);
	DBG("Result %d - data allocated in %p\n", res, *memptr);

	if (memptr != nullptr)
		*memptr = ptr;

	// Save code location to quantify HWM per location
	if (save_CL)
	{
		Allocator::codeLocation (ptr, CL);
		_cl->record_location_add_memory (CL, size, !fits);
	}

	return res;
}

void * FlexMalloc::realloc (unsigned nptrs, void **callstack, void *ptr, size_t new_size)
{
	void *res = nullptr;
	bool fits;
	uint32_t CL;
	bool save_CL = false;
	Allocator *new_allocator = allocatorForCallstack (nptrs, callstack, new_size, fits, CL);
	if (!fits)
	{
		DBG("Willing to allocate %lu bytes using allocator '%s' but it does not fit. Using fallback allocator.\n",
		    new_size, new_allocator->name());
		new_allocator->record_unfitted_realloc (new_size);
		new_allocator = _fallback;
	}
	if (nullptr == new_allocator)
		new_allocator = _fallback;
	else
		save_CL = true;

	if (ptr)
	{
		// Given this pointer - we check whether the pointer was allocated by any allocator from
		//   flexmalloc or by "default" calls (in _af structure).
		Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);
		Allocator *prev_allocator = hdr->allocator;
		size_t prev_size  = hdr->size;
		void * prev_base  = hdr->base_ptr;
		// Extract the previous code-location ID and if it is valid
		bool valid_prev_CL;
		uint32_t prev_CL  = Allocator::codeLocation (ptr, valid_prev_CL);

		if (prev_allocator == nullptr && new_allocator == nullptr)
		{
			// Case in which buffer was allocated by "_af"/backend allocators and no new allocator
			// assigned to this call-site
			DBG("realloc from null-allocator to null-allocator%s\n", "");
			FlexMalloc::uninitialized_realloc (ptr, new_size);
		}
		else if (prev_allocator != new_allocator)
		{
			// Case in which buffer was initially allocated by one allocator but realloc callstack
			// now states to be allocated by a new allocator.
			// As allocators are different, we cannot rely on regular "realloc"
			if (prev_allocator != nullptr && new_allocator != nullptr)
			{
				DBG("realloc from allocator %p (%s) to allocator %p (%s)\n",
				  prev_allocator, prev_allocator->name(),
				  new_allocator, new_allocator->name());
			}
			else
			{
				DBG("realloc from allocator %p to allocator %p\n",
				  prev_allocator, new_allocator);
			}

			// Allocate space in allocator
			if (new_allocator == nullptr)
			{
				res = _af.malloc (new_size);
				if (res == nullptr)
				{
					VERBOSE_MSG (0, "Could not allocate space for memcpy operation on default allocator. Exiting...\n");
					_exit (0);
				}
			}
			else
			{
				res = new_allocator->malloc (new_size);
				if (res == nullptr)
				{
					VERBOSE_MSG (0, "Could not allocate space for memcpy operation on allocator (%s). Exiting...\n", new_allocator->name());
					_exit (0);
				}
			}

			// Copy from one memory region to the other using the specific
			// memcpy from the new allocator
			if (new_allocator == nullptr)
				memcpy (res, ptr, MIN(prev_size, new_size));
			else
				new_allocator->memcpy (res, ptr, MIN(prev_size, new_size));

			// Free old pointer
			if (prev_allocator == nullptr)
				_af.free (prev_base);
			else
				prev_allocator->free (ptr);

			// Annotate statistics for the "moved" data between allocators.
			// In this case, we annotate for both allocators the amount of data
			// transferred (as either source or target) - and the amount of data
			// is the min between the new size and old size
			if (prev_allocator == nullptr)
				_uninitialized_stats.record_source_realloc (MIN(prev_size, new_size));
			else
				prev_allocator->record_source_realloc (MIN(prev_size, new_size));

			if (new_allocator == nullptr)
				_uninitialized_stats.record_target_realloc (MIN(prev_size, new_size));
			else
				new_allocator->record_target_realloc (MIN(prev_size, new_size));
		}
		else if (prev_allocator == new_allocator) // && prev_a != nullptr && new_a != nullptr
		{
			// Case in which the allocator used in previous allocation and allocator
			// used in current allocation match
			DBG("pre/post allocator in realloc are the same allocator %p (%s)\n",
			  prev_allocator, prev_allocator->name());

			res = new_allocator->realloc (ptr, new_size);

			// Need to annotate statistics. Note that the bytes copied are the
			// minimum between new and prev sizes. Also, allocators ignore realloc
			// if new_size is less or equal than prev_size, so we only need to
			// capture metrics when new size is larger -> bytes copies are the
			// prev_size then
			if (new_size > prev_size)
				new_allocator->record_self_realloc (prev_size);
		}

		// Identify whether the previous allocation did fit to substract
		// the amount of memory used. For this, we check if requested
		// allocator matches the used allocator.
		if (valid_prev_CL)
		{
			bool prev_fit = _cl->allocator (prev_CL) == prev_allocator;
			_cl->record_location_sub_memory (prev_CL, prev_size, !prev_fit);
		}
		// Save code location to quantify HWM per location
		if (save_CL)
		{
			Allocator::codeLocation (res, CL);
			_cl->record_location_add_memory (CL, new_size, !fits);
		}
	}
	else
	{
		// Forward to a regular malloc if given ptr was null
		DBG("realloc forwarding to malloc%s\n", "");
		new_allocator->record_realloc_forward_malloc();

		res = new_allocator->malloc (new_size);
	}

	return res;
}

void FlexMalloc::free (void *ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);
	Allocator *a = hdr->allocator;
	size_t size  = hdr->size;

	// Extract the previous code-location ID and if it is valid
	bool valid_prev_CL;
	uint32_t prev_CL  = Allocator::codeLocation (ptr, valid_prev_CL);

	// Identify whether the previous allocation did fit to substract
	// the amount of memory used. For this, we check if requested
	// allocator matches the used allocator.
	if (valid_prev_CL)
	{
		bool prev_fit = _cl->allocator (prev_CL) == a;
		_cl->record_location_sub_memory (prev_CL, size, !prev_fit);
	}

	// Delegate to the allocator to do the actual free
	if (a == nullptr)
	{
		_af.free (hdr->base_ptr);
		_uninitialized_stats.record_free (size);
	}
	else if (a != nullptr)
	{
		a->free (ptr);
	}
}

size_t FlexMalloc::malloc_usable_size (void *ptr) const
{
	// Given this pointer - we check whether the pointer was allocated by any allocator from
	//   flexmalloc or by "default" calls (in _af structure).
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);
	return hdr->size;
}

void FlexMalloc::show_statistics (void) const
{
	if (options.sourceFrames())
	{
		VERBOSE_MSG(1, "Callstacks cache summary:\n");
		_c_cache.show_statistics();
		VERBOSE_MSG(1, "End of callstacks cache summary.\n");
	}
	VERBOSE_MSG(1, "Allocator statistics:\n");
	_allocators->show_statistics();
	_uninitialized_stats.show_statistics ("out-of-flexmalloc", true);
	VERBOSE_MSG(1, "End of allocator statistics.\n");
	if (options.sourceFrames())
	{
		for (unsigned u = 0; u < _nmodules; ++u)
		{
			struct stat s;
			if (0 != stat (_modules[u].name, &s))
				s.st_size = 0;
			VERBOSE_MSG(1, "Module %u (%s) in range %08lx-%08lx w/ size %lu.\n", 
			  u, _modules[u].name, _modules[u].startAddress, _modules[u].endAddress, s.st_size);
		}
	}
}
