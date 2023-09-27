// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <sys/param.h>

#include "flexmalloc-config.h"

class Options
{
	private:
	int _VerboseLvl;
	bool _comparewholepath;
	size_t _minSize;
	uint64_t _initial_time;
	bool _debug;
	bool _messages_on_stderr;
	unsigned _max_depth;
	bool _callstack_minus_1;
	bool _stopAtMain;
	bool _shorten_frames;
	bool _sourceFrames;
	bool _sourceFramesSet;
	bool _ignoreIfFallbackAllocator;
	unsigned _read_offset_base;
	size_t _max_offset_digits;

	public:
	Options ();
	~Options ();
	int verboseLvl (void) const
	  { return _VerboseLvl; }
	bool compareWholePath (void) const
	  { return _comparewholepath; }
	void minSize (unsigned s)
	  { _minSize = s; }
	size_t minSize (void) const
	  { return _minSize; }
	bool debug (void) const
	  { return _debug; }
	bool messages_on_stderr (void) const
	  { return _messages_on_stderr; }
	size_t maxDepth (void) const
	  { return _max_depth; }
	void callstackMinus1 (bool b)
	  { _callstack_minus_1 = b; }
	bool callstackMinus1 (void) const
	  { return _callstack_minus_1; }
	void stopAtMain (bool b)
	  { _stopAtMain = b; }
	bool stopAtMain (void) const
	  { return _stopAtMain; }
	void shortenFrames (bool b)
	  { _shorten_frames = b; }
	bool shortenFrames (void) const
	  { return _shorten_frames; }
	uint64_t getTime (void) const;
	bool sourceFrames (void) const
	  { return _sourceFrames; }
	void sourceFrames (bool b)
	  { _sourceFrames = b; }
	bool sourceFrame_set (void) const
	  { return _sourceFramesSet; }
	bool ignoreIfFallbackAllocator (void) const
	  { return _ignoreIfFallbackAllocator; }
	unsigned readOffsetBase (void) const
	  { return _read_offset_base; }
	size_t maxOffsetDigits (void) const
	  { return _max_offset_digits; }
};

typedef struct allocation_functions_st
{
	void*  (*malloc)(size_t);
	void*  (*calloc)(size_t,size_t);
	void   (*free)(void*);
	void*  (*realloc)(void*, size_t);
	int    (*posix_memalign)(void**, size_t, size_t);
	size_t (*malloc_usable_size)(void *);
} allocation_functions_t;

extern Options options;

#define TOOL_NAME                         "FLEXMALLOC"
#define TOOL_VERBOSE                      TOOL_NAME"_VERBOSE"
#define TOOL_LOCATIONS_FILE               TOOL_NAME"_LOCATIONS"
#define TOOL_DEFINITIONS_FILE             TOOL_NAME"_DEFINITIONS"
#define TOOL_COMPARE_WHOLE_PATH           TOOL_NAME"_COMPARE_WHOLE_PATH"
#define TOOL_DEBUG                        TOOL_NAME"_DEBUG"
#define TOOL_MESSAGES_ON_STDERR           TOOL_NAME"_MESSAGES_ON_STDERR"
#define TOOL_FALLBACK_ALLOCATOR           TOOL_NAME"_FALLBACK_ALLOCATOR"
#define TOOL_CALLSTACK_MINUS1             TOOL_NAME"_CALLSTACK_MINUS1"
#define TOOL_CALLSTACK_STOP_AT_MAIN       TOOL_NAME"_CALLSTACK_STOP_AT_MAIN"
#define TOOL_MINSIZE_THRESHOLD            TOOL_NAME"_MINSIZE_THRESHOLD"
#define TOOL_MINSIZE_THRESHOLD_ALLOCATOR  TOOL_NAME"_MINSIZE_THRESHOLD_ALLOCATOR"
#define TOOL_SHORTEN_FRAMES               TOOL_NAME"_SHORTEN_FRAMES"
#define TOOL_MATCH_ONLY_ON_MAIN_BINARY    TOOL_NAME"_MATCH_ONLY_ON_MAIN_BINARY"
#define TOOL_SOURCE_FRAMES                TOOL_NAME"_SOURCE_FRAMES"
#define TOOL_IGNORE_IF_FALLBACK_ALLOCATOR TOOL_NAME"_IGNORE_LOCATIONS_ON_FALLBACK_ALLOCATOR"
#define TOOL_READ_OFFSET_BASE             TOOL_NAME"_READ_OFFSET_BASE"

#define VERBOSE_MSG(level,...) \
	do { if (options.verboseLvl() >= level || options.debug()) { fprintf (options.messages_on_stderr() ? stderr : stdout, TOOL_NAME"|" __VA_ARGS__); } } while(0)
#define VERBOSE_MSG_NOPREFIX(level,...) \
	do { if (options.verboseLvl() >= level || options.debug()) { fprintf (options.messages_on_stderr() ? stderr : stdout, __VA_ARGS__); } } while(0)

#if defined(DEBUG)
# define DBG(fmt,...) \
	do { \
		if (options.debug()) \
		{ \
			fprintf (options.messages_on_stderr() ? stderr : stdout, \
			         TOOL_NAME"|DBG|%s (%s:%d)|%luus: " fmt, __func__, __FILE__, __LINE__, options.getTime()/1000, __VA_ARGS__); \
			fflush (options.messages_on_stderr() ? stderr : stdout); \
		} \
	} while(0)
#else
# define DBG(fmt,...)
#endif

#define LIKELY(condition) __builtin_expect(static_cast<bool>(condition), 1)
#define UNLIKELY(condition) __builtin_expect(static_cast<bool>(condition), 0)

