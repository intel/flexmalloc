// Author: Clément Foyer <clement.foyer@univ-reims.fr>
// Date: Oct 31, 2023
// License: To determine

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "common.hxx"
#include "allocator.hxx"
#include "allocator-debug.hxx"
#include "allocator-posix.hxx"

#define ALLOCATOR_NAME "debug"

AllocatorDebug::AllocatorDebug (allocation_functions_t &af, AllocatorPOSIX &allocator)
  : Allocator (af), _allocator (allocator)
{
}

AllocatorDebug::~AllocatorDebug ()
{
}

bool AllocatorDebug::is_ready (void) const
{
	return _allocator.is_ready ();
}

void AllocatorDebug::configure (const char *)
{
}

const char * AllocatorDebug::name (void) const
{
	return ALLOCATOR_NAME;
}

const char * AllocatorDebug::description (void) const
{
	return "Fake allocator based on regular POSIX allocators";
}

void AllocatorDebug::show_statistics (void) const
{
	_stats.show_statistics (ALLOCATOR_NAME, true);
}

