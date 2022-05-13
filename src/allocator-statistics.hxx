// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

#include <stdlib.h>

class AllocatorStatistics
{
	private:
	size_t high_water_mark;
	size_t current_water_mark;
	size_t malloc_total_allocated_size;
	size_t malloc_min_allocated_size;
	size_t malloc_max_allocated_size;
	size_t calloc_total_allocated_size;
	size_t calloc_min_allocated_size;
	size_t calloc_max_allocated_size;
	size_t aligned_malloc_total_allocated_size;
	size_t aligned_malloc_min_allocated_size;
	size_t aligned_malloc_max_allocated_size;
	size_t realloc_total_allocated_size;
	size_t realloc_min_allocated_size;
	size_t realloc_max_allocated_size;
	unsigned n_malloc_calls;
	unsigned n_calloc_calls;
	unsigned n_aligned_malloc_calls;
	unsigned n_realloc_calls;
	unsigned n_free_calls;
	unsigned n_unfitted_malloc_calls;
	size_t unfitted_malloc_calls_size;
	unsigned n_unfitted_calloc_calls;
	size_t unfitted_calloc_calls_size;
	unsigned n_unfitted_aligned_malloc_calls;
	size_t unfitted_aligned_malloc_calls_size;
	unsigned n_unfitted_realloc_calls;
	size_t unfitted_realloc_calls_size;
	unsigned n_source_realloc;
	size_t source_realloc_size;
	unsigned n_target_realloc;
	size_t target_realloc_size;
	unsigned n_self_realloc;
	size_t self_realloc_size;
	unsigned n_realloc_fwd_malloc; // number of realloc(null, X) forwarded to malloc (X)

	public:
	AllocatorStatistics ();
	~AllocatorStatistics ();

	void record_malloc (size_t);
	void record_calloc (size_t);
	void record_aligned_malloc (size_t);
	void record_realloc (size_t size, size_t prev_size);
	void record_free (size_t);

	void record_source_realloc (size_t s);
	void record_target_realloc (size_t s);
	void record_self_realloc (size_t s);

	void record_unfitted_malloc (size_t);
	void record_unfitted_calloc (size_t);
	void record_unfitted_aligned_malloc (size_t);
	void record_unfitted_realloc (size_t);

	void record_realloc_forward_malloc (void);

	void show_statistics (const char * allocator_name,
	  bool show_high_water_mark, const char *extra_name = nullptr) const;

	size_t water_mark (void) const { return current_water_mark; };
};

