// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <time.h>
#include <limits.h>
#include <strings.h>
#include <cmath>
#include "common.hxx"

Options options;

#define CHECK_ENABLED(x) \
	(strncasecmp (s, "yes", 3) == 0 || \
	 strncasecmp (s, "enabled", 7) == 0 || \
	 strncasecmp (s, "true", 4) == 0 || \
	 strncasecmp (s, "1", 1) == 0)
	 
#define CHECK_DISABLED(x) \
	(strncasecmp (s, "no", 2) == 0 || \
	 strncasecmp (s, "disabled", 8) == 0 || \
	 strncasecmp (s, "false", 5) == 0 || \
	 strncasecmp (s, "0", 1) == 0)

#define COMPARE_WHOLE_PATH_DEFAULT          false
#define DEBUG_DEFAULT                       false
#define DEBUG_MESSAGES_ON_STDERR_DEFAULT    true
#define CALLSTACK_MINUS1_DEFAULT            false
#define CALLSTACK_STOP_AT_MAIN_DEFAULT      false
#define CALLSTACK_SHORTEN_FRAMES_DEFAULT    true
#define MATCH_ONLY_ON_MAIN_BINARY_DEFAULT   false
#define SOURCE_FRAMES_DEFAULT               true
#define IGNORE_IF_FALLBACK_ALLOCATOR_DEFAULT true
#define READ_OFFSET_BASE_DEFAULT            16

#define PROCESS_ENVVAR(envvar,var,defvalue) \
    do { \
    const char *s = getenv (envvar); \
    if (s != nullptr) \
    { \
        if (CHECK_ENABLED(s)) \
            var = true; \
        else if (CHECK_DISABLED(s)) \
            var = false; \
        else \
            var = defvalue; \
    } \
    else \
        var = defvalue;\
    } while (0)

Options::Options ()
	: _minSize(0), _max_depth(100)
{
	const char *verbose = getenv(TOOL_VERBOSE);
	if (verbose != nullptr)
	{
		_VerboseLvl = atoi(verbose);
		if (_VerboseLvl < 0)
		{
			VERBOSE_MSG(0, "Wrong value for environment variable %s. Setting it to 0.\n",
				TOOL_VERBOSE);
			_VerboseLvl = 0;
		}
	}
	else
		_VerboseLvl = 0;

	PROCESS_ENVVAR(TOOL_COMPARE_WHOLE_PATH, _comparewholepath, COMPARE_WHOLE_PATH_DEFAULT);
	PROCESS_ENVVAR(TOOL_DEBUG, _debug, DEBUG_DEFAULT);
	PROCESS_ENVVAR(TOOL_MESSAGES_ON_STDERR, _messages_on_stderr, DEBUG_MESSAGES_ON_STDERR_DEFAULT);
	PROCESS_ENVVAR(TOOL_CALLSTACK_MINUS1, _callstack_minus_1, CALLSTACK_MINUS1_DEFAULT);
	PROCESS_ENVVAR(TOOL_CALLSTACK_STOP_AT_MAIN, _stopAtMain, CALLSTACK_STOP_AT_MAIN_DEFAULT);
	PROCESS_ENVVAR(TOOL_SHORTEN_FRAMES, _shorten_frames, CALLSTACK_SHORTEN_FRAMES_DEFAULT);
	PROCESS_ENVVAR(TOOL_IGNORE_IF_FALLBACK_ALLOCATOR, _ignoreIfFallbackAllocator, IGNORE_IF_FALLBACK_ALLOCATOR_DEFAULT);
	if (getenv (TOOL_SOURCE_FRAMES))
	{
		PROCESS_ENVVAR(TOOL_SOURCE_FRAMES, _sourceFrames, SOURCE_FRAMES_DEFAULT);
		_sourceFramesSet = true;
	}
	else
	{
		_sourceFramesSet = false;
		_sourceFrames = false;
	}

	int msize = 0;
	const char *msize_threshold = getenv(TOOL_MINSIZE_THRESHOLD);
	if (msize_threshold != nullptr)
		msize = atoi (msize_threshold);
	if (msize < 0)
	{
		msize = 0;
		VERBOSE_MSG(0, "Wrong value for environment variable %s. Setting it to %d.\n",
		  TOOL_MINSIZE_THRESHOLD, msize);
	}
	_minSize = msize;

	const char *def_file = getenv (TOOL_DEFINITIONS_FILE);
	if (def_file == nullptr)
	{
		VERBOSE_MSG(0, "Did not find " TOOL_DEFINITIONS_FILE " environment variable. Finishing...\n");
		_exit (2);
	}
	_definitions_filename = def_file;

	const char *loc_file = getenv (TOOL_LOCATIONS_FILE);
	if (loc_file == nullptr)
	{
		VERBOSE_MSG(0, "Did not find " TOOL_LOCATIONS_FILE " environment variable. Finishing...\n");
		_exit (2);
	}
	_locations_filename = loc_file;

	int offset_base = READ_OFFSET_BASE_DEFAULT;
	const char *read_offset_base = getenv(TOOL_READ_OFFSET_BASE);
	if (read_offset_base != nullptr)
		offset_base = atoi (read_offset_base);
	if (offset_base < 0)
	{
		offset_base = READ_OFFSET_BASE_DEFAULT;
		VERBOSE_MSG(0, "Wrong value for environment variable %s. Setting it to %d.\n",
		  TOOL_READ_OFFSET_BASE, offset_base);
	}
	_read_offset_base = offset_base;
	// At most, we have 16 digits long hexadecimal addresses in /proc/<id>/maps entries
	_max_offset_digits = static_cast<size_t>(ceil(std::log(std::pow(16.0,16.0))/std::log(static_cast<double>(offset_base))));

	// Get fallback allocator, if given from environment.
	// If not, we use the regular "posix" allocators as fallback.
	const char *fallback_alloc_name = getenv (TOOL_FALLBACK_ALLOCATOR);
	if (fallback_alloc_name == nullptr)
	{
		fallback_alloc_name = "posix";
		VERBOSE_MSG(0, "No fallback allocator's name provided. Using the default one: \"%s\".\n",
		  fallback_alloc_name);
	}
	_fallback_allocator_name = fallback_alloc_name;

	const char *small_alloc_fallback_name = getenv (TOOL_MINSIZE_THRESHOLD_ALLOCATOR);
	if (small_alloc_fallback_name == nullptr)
	{
		small_alloc_fallback_name = _fallback_allocator_name;
		VERBOSE_MSG(0, "No fallback allocator's name provided for small allocations. Using the default one: \"%s\".\n",
		  small_alloc_fallback_name);
	}
	_small_allocation_falback_allocator_name = small_alloc_fallback_name;

	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	_initial_time = ((uint64_t) ts.tv_sec * 1000000000ULL + ts.tv_nsec);
}

Options::~Options ()
{
}

uint64_t Options::getTime (void) const
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	uint64_t current_time = ((uint64_t) ts.tv_sec * 1000000000ULL + ts.tv_nsec);
	return current_time - _initial_time;
}

