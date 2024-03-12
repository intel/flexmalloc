// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

#include <stdlib.h>

#include "allocator-statistics.hxx"
#include "allocator.hxx"

#if defined(MEMKIND_SUPPORTED)
# define NUM_ALLOCATORS 3
#else
# define NUM_ALLOCATORS 1
#endif

class Allocators
{
	private:
	Allocator * allocators[NUM_ALLOCATORS+2]; // +1 for null-terminated +1 debug (optionnal)

	public:
	Allocators (allocation_functions_t &, const char * definitions);
	~Allocators ();
	Allocator * get (const char *name);
	Allocator ** get (void);
	void show_statistics (void) const;
};
