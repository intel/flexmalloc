// Author: Clément Foyer <clement.foyer@univ-reims.fr>
// Date: Jul 06, 2023
// License: To determine

#pragma once

#include "allocator.hxx"
#include "allocator-statistics.hxx"

class StatisticsRecorderAllocator : public Allocator
{
	private:
	AllocatorStatistics _stats;

	public:
	StatisticsRecorderAllocator (allocation_functions_t &af)
	  : Allocator(af) {}
	~StatisticsRecorderAllocator() = default;

	void show_statistics (void) const override
	  { _stats.show_statistics (this->name(), true); }

	size_t hwm (void) const override
	  { return _stats.water_mark(); }
	bool fits (size_t s) const override
	  { return this->hwm() + s <= this->size(); }

	void record_malloc (size_t s) override
	  { _stats.record_malloc (s); }
	void record_calloc (size_t s) override
	  { _stats.record_calloc (s); }
	void record_aligned_malloc (size_t s) override
	  { _stats.record_aligned_malloc (s); }
	void record_realloc (size_t size, size_t prev_size) override
	  { _stats.record_realloc (size, prev_size); }
	void record_free (size_t s) override
	  { _stats.record_free (s); }

	void record_source_realloc (size_t s) override
	  { _stats.record_source_realloc (s); }
	void record_target_realloc (size_t s) override
	  { _stats.record_target_realloc (s); }
	void record_self_realloc (size_t s) override
	  { _stats.record_self_realloc (s); }

	void record_unfitted_malloc (size_t s) override
	  { _stats.record_unfitted_malloc (s); }
	void record_unfitted_calloc (size_t s) override
	  { _stats.record_unfitted_calloc (s); }
	void record_unfitted_aligned_malloc (size_t s) override
	  { _stats.record_unfitted_aligned_malloc (s); }
	void record_unfitted_realloc (size_t s) override
	  { _stats.record_unfitted_realloc (s); }

	void record_realloc_forward_malloc (void) override
	  { _stats.record_realloc_forward_malloc (); }
};
