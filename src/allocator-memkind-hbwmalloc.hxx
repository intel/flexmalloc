// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

#include "allocator.hxx"

class AllocatorMemkindHBWMalloc final : public Allocator
{
	private:
	AllocatorStatistics _stats;

	public:
	AllocatorMemkindHBWMalloc (allocation_functions_t &);
	~AllocatorMemkindHBWMalloc();

	void*  malloc (size_t);
	void*  calloc (size_t, size_t);
	int    posix_memalign (void **, size_t, size_t);
	void   free (void *);
	void*  realloc (void *, size_t);
	size_t malloc_usable_size (void*);

	void   configure (const char *);
	const char * name (void) const;
	const char * description (void) const;
	void show_statistics (void) const;

	void *memcpy (void *dest, const void *src, size_t n)
	  { return ::memcpy (dest, src, n); }

	bool fits (size_t s) const;
	size_t hwm (void) const
	  { return _stats.water_mark(); }
	void record_unfitted_malloc (size_t s)
	  { _stats.record_unfitted_malloc (s); } ;
	void record_unfitted_calloc (size_t s)
	  { _stats.record_unfitted_calloc (s); } ;
	void record_unfitted_aligned_malloc (size_t s)
	  { _stats.record_unfitted_aligned_malloc (s); } ;
	void record_unfitted_realloc (size_t s)
	  { _stats.record_unfitted_realloc (s); } ;

	void record_source_realloc (size_t s)
	  { _stats.record_source_realloc (s); };
	void record_target_realloc (size_t s)
	  { _stats.record_target_realloc (s); };
	void record_self_realloc (size_t s)
	  { _stats.record_self_realloc (s); };

	void record_realloc_forward_malloc (void)
	  { _stats.record_realloc_forward_malloc (); }
};
