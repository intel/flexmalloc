// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

#include "statistics-recorder-allocator.hxx"
#include <string.h>

class AllocatorPOSIX final : public StatisticsRecorderAllocator
{
	public:
	AllocatorPOSIX(allocation_functions_t &);
	~AllocatorPOSIX();

	void*  malloc (size_t);
	void*  calloc (size_t, size_t);
	int    posix_memalign (void **, size_t, size_t);
	void   free (void *);
	void*  realloc (void *, size_t);
	size_t malloc_usable_size (void*);

	void   configure (const char *);
	const char * name (void) const;
	const char * description (void) const;
};
