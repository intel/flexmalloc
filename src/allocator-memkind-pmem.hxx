// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// Author: Clement Foyer <clement.foyer@univ-reims.fr>
// Date: Jan 18, 2024
// License: To determine

#pragma once

#include <memkind.h>

#if defined(PMDK_SUPPORTED)
# include <libpmem.h>
#else
# include <string.h>
#endif

#include "allocator.hxx"

class AllocatorMemkindPMEM final : public Allocator
{
	private:
	memkind_t *_kind;
	AllocatorStatistics *_stats;
	short *_cpu_2_NUMA;
	int _num_NUMA_nodes;

	public:
	AllocatorMemkindPMEM (allocation_functions_t &af);
	~AllocatorMemkindPMEM();

	void*  malloc (size_t);
	void*  calloc (size_t,size_t);
	int    posix_memalign (void **, size_t, size_t);
	void   free (void *);
	void*  realloc (void *, size_t);
	size_t malloc_usable_size (void*);

	void   configure (const char *);
	const char * name (void) const;
	const char * description (void) const;
	void show_statistics (void) const override;

	void *memcpy (void *dest, const void *src, size_t n) override
	  {
#if defined(PMDK_SUPPORTED)
	    return ::pmem_memcpy (dest, src, n, 0);
#else
	    return Allocator::memcpy(dest, src, n);
#endif
	  }

	bool fits (size_t s) const override
	  { return fits(s, get_local_numa ()); }
	size_t hwm (void) const override
	  { return hwm(get_local_numa ()); }

	void record_malloc (size_t s) override
	  { return record_malloc (s, get_local_numa ()); }
	void record_calloc (size_t s) override
	  { return record_calloc (s, get_local_numa ()); }
	void record_aligned_malloc (size_t s) override
	  { return record_aligned_malloc (s, get_local_numa ()); }
	void record_realloc (size_t size, size_t prev_size) override
	  { return record_realloc (size, prev_size, get_local_numa ()); }
	void record_free (size_t s) override
	  { return record_free (s, get_local_numa ()); }

	void record_source_realloc (size_t s) override
	  { return record_source_realloc (s, get_local_numa ()); }
	void record_target_realloc (size_t s) override
	  { return record_target_realloc (s, get_local_numa ()); }
	void record_self_realloc (size_t s) override
	  { return record_self_realloc (s, get_local_numa ()); }

	void record_unfitted_malloc (size_t s) override
	  { return record_unfitted_malloc (s, get_local_numa ()); }
	void record_unfitted_calloc (size_t s) override
	  { return record_unfitted_calloc (s, get_local_numa ()); }
	void record_unfitted_aligned_malloc (size_t s) override
	  { return record_unfitted_aligned_malloc (s, get_local_numa ()); }
	void record_unfitted_realloc (size_t s) override
	  { return record_unfitted_realloc (s, get_local_numa ()); }

	void record_realloc_forward_malloc (void) override
	  { return record_realloc_forward_malloc (get_local_numa ()); }

	private:
	int get_current_cpu (void) const;
	short get_local_numa (void) const;

	bool fits (size_t,short) const;
	size_t hwm (short) const;

	void record_malloc (size_t,short);
	void record_calloc (size_t,short);
	void record_aligned_malloc (size_t,short);
	void record_realloc (size_t, size_t,short);
	void record_free (size_t,short);

	void record_source_realloc (size_t,short);
	void record_target_realloc (size_t,short);
	void record_self_realloc (size_t,short);

	void record_unfitted_malloc (size_t,short);
	void record_unfitted_calloc (size_t,short);
	void record_unfitted_aligned_malloc (size_t,short);
	void record_unfitted_realloc (size_t,short);

	void record_realloc_forward_malloc (short);
};
