// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <stdlib.h>
#include <hbwmalloc.h>
#include <memkind.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "common.hxx"
#include "allocator-memkind-hbwmalloc.hxx"

#define ALLOCATOR_NAME "memkind/hbwmalloc"

AllocatorMemkindHBWMalloc::AllocatorMemkindHBWMalloc (allocation_functions_t &af)
  : Allocator (af)
{
	hbw_set_policy (HBW_POLICY_PREFERRED);
}

AllocatorMemkindHBWMalloc::~AllocatorMemkindHBWMalloc ()
{
}

void * AllocatorMemkindHBWMalloc::malloc (size_t size)
{
	// Forward memory request to real malloc and reserve some space for the header
	void * baseptr = hbw_malloc (Allocator::getTotalSize (size));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeader (baseptr, this, size);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr & base at %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), name(), this);
		_stats.record_malloc (size);
	}

	return res;
}

void * AllocatorMemkindHBWMalloc::calloc (size_t nmemb, size_t size)
{
	// Forward memory request to real malloc and request additional space to store
	// the allocator and the basepointer
	void * baseptr = hbw_malloc (Allocator::getTotalSize (nmemb * size));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeader (baseptr, this, nmemb * size);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr & base %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), name(), this);
		_stats.record_calloc (nmemb * size);
	}

	return res;
}

int AllocatorMemkindHBWMalloc::posix_memalign (void **ptr, size_t align, size_t size)
{
	assert (ptr != nullptr);

	// Forward memory request to real malloc and request additional space to
	// store the allocator and the basepointer
	void * baseptr = hbw_malloc (Allocator::getTotalSize (size + align));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeaderOnAligned (baseptr, align, this, size);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr %p, base %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), baseptr, name(), this);
		_stats.record_aligned_malloc (size + align);

		*ptr = res;
		return 0;
	}
	else
		return ENOMEM;
}

void AllocatorMemkindHBWMalloc::free (void *ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);

	// When freeing the memory, need to free the base pointe
	VERBOSE_MSG(3, ALLOCATOR_NAME": Freeing up pointer %p (hdr %p) w/ size - %lu (base pointer located in %p)\n", ptr, hdr, hdr->size, hdr->base_ptr);
	
	_stats.record_free (hdr->size);
	hbw_free (hdr->base_ptr);
}

void * AllocatorMemkindHBWMalloc::realloc (void *ptr, size_t size)
{
	// If previous pointer is not null, behave normally. otherwise, behave like a malloc but
	// without calling information
	if (ptr)
	{
		// Search for previous allocation size through the header
		Allocator::Header_t *prev_hdr = Allocator::getAllocatorHeader (ptr);
		size_t prev_size = prev_hdr->size;
		void * prev_baseptr = prev_hdr->base_ptr;
		uintptr_t extra_size = Allocator::getExtraSize (prev_hdr);

		if (prev_size < size)
		{
			// Reallocate, from base pointer to fit the new size plus a new header
			void *new_baseptr = hbw_realloc (prev_baseptr, Allocator::getTotalSize (size + extra_size));
			void *res = nullptr;

			// If realloc was successful, it also copied the data from previous allocation.
			// We need to update the header.
			if (new_baseptr)
			{
				// res points to the space where the user can store their data
				res = Allocator::generateAllocatorHeader (new_baseptr, extra_size, this, size);
				DBG("Reallocated (%ld->%ld [extra bytes = %lu]) from %p (base at %p, header at %p) into %p (base at %p, header at %p) w/ allocator %s (%p)\n", prev_size, size, extra_size, ptr, prev_baseptr, prev_hdr, res, new_baseptr, Allocator::getAllocatorHeader (res), name(), this);
			}

			_stats.record_realloc (size, prev_size);

			return res;
		}
		else
		{
			DBG("Reallocated (%ld->%ld) from %p but not touching as new size is smaller w/ allocator %s (%p)\n", prev_size, size, ptr, name(), this);
			return ptr;
		}
	}
	else
	{
		VERBOSE_MSG(3, ALLOCATOR_NAME": realloc (NULL, ...) forwarded to malloc\n");
		_stats.record_realloc_forward_malloc();

		return this->malloc (size);
	}
}

size_t AllocatorMemkindHBWMalloc::malloc_usable_size (void *ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);

	// When checking for the usable size, return the size we requested originally, no matter
	// what the underlying library did. This may alter execution behaviors, though.
	VERBOSE_MSG(3, ALLOCATOR_NAME": Checking usable size on pointer %p w/ size - %lu (but base pointer located in %p)\n",
	  ptr, hdr->size, hdr->base_ptr);

	return hdr->size;
}

void AllocatorMemkindHBWMalloc::configure (const char *config)
{
	const char * MEMORYCONFIG_SIZE = "Size ";
	const char * MEMORYCONFIG_MBYTES_SUFFIX = " MBytes";

	if (strncmp (config, MEMORYCONFIG_SIZE, strlen(MEMORYCONFIG_SIZE)) == 0)
	{
		// Get given size after the Size marker
		char *pEnd = nullptr;
		long long s_size = strtoll (&config[strlen(MEMORYCONFIG_SIZE)], &pEnd, 10);
		// Was text converted into s? If so, now look for suffix
		if (pEnd != &config[strlen(MEMORYCONFIG_SIZE)])
		{
			if (strncmp (pEnd, MEMORYCONFIG_MBYTES_SUFFIX, strlen(MEMORYCONFIG_MBYTES_SUFFIX)) == 0)
			{
				size_t s;
				if (s_size < 0)
				{
					VERBOSE_MSG(1, ALLOCATOR_NAME": Invalid given size.\n");
					exit (1);
				}
				else
					s = s_size;
				VERBOSE_MSG(1, ALLOCATOR_NAME": Setting up size %lu MBytes.\n", s);
				size (s << 20);
			}
			else
			{
				VERBOSE_MSG(0, ALLOCATOR_NAME": Invalid size suffix.\n");
				exit (1);
			}
		}
		else
		{
			VERBOSE_MSG(0, ALLOCATOR_NAME": Could not parse given size.\n");
			exit (1);
		}
	}
	else
	{
		VERBOSE_MSG(0, ALLOCATOR_NAME": Wrong configuration for the allocator. Available options include:\n"
		               " Size <NUM> MBytes\n");
		exit (1);
	}
}

const char * AllocatorMemkindHBWMalloc::name (void) const
{
	return ALLOCATOR_NAME;
}

const char * AllocatorMemkindHBWMalloc::description (void) const
{
	return "Allocator based on hbwmalloc on top of memkind";
}

void AllocatorMemkindHBWMalloc::show_statistics (void) const
{
	_stats.show_statistics (ALLOCATOR_NAME, true);
}

bool AllocatorMemkindHBWMalloc::fits (size_t s) const
{
	return _stats.water_mark() + s <= this->size();
}
