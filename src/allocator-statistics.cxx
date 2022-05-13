// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <limits.h>
#include <string.h>
#include "allocator-statistics.hxx"
#include "common.hxx"

AllocatorStatistics::AllocatorStatistics () :
	high_water_mark (0),
	current_water_mark (0),
	malloc_total_allocated_size (0),
	malloc_min_allocated_size (LONG_MAX),
	malloc_max_allocated_size (0),
	calloc_total_allocated_size (0),
	calloc_min_allocated_size (LONG_MAX),
	calloc_max_allocated_size (0),
	aligned_malloc_total_allocated_size (0),
	aligned_malloc_min_allocated_size (LONG_MAX),
	aligned_malloc_max_allocated_size (0),
	realloc_total_allocated_size (0),
	realloc_min_allocated_size (LONG_MAX),
	realloc_max_allocated_size (0),
	n_malloc_calls (0),
	n_calloc_calls (0),
	n_aligned_malloc_calls (0),
	n_realloc_calls (0),
	n_free_calls (0),
	n_unfitted_malloc_calls (0), unfitted_malloc_calls_size(0),
	n_unfitted_calloc_calls (0), unfitted_calloc_calls_size (0),
	n_unfitted_aligned_malloc_calls (0), unfitted_aligned_malloc_calls_size (0),
	n_unfitted_realloc_calls (0), unfitted_realloc_calls_size (0),
	n_source_realloc (0), source_realloc_size (0),
	n_target_realloc (0), target_realloc_size (0),
	n_self_realloc (0), self_realloc_size (0),
	n_realloc_fwd_malloc (0)
{
}

AllocatorStatistics::~AllocatorStatistics ()
{
}

void AllocatorStatistics::record_malloc (size_t s)
{
	n_malloc_calls++;
	malloc_total_allocated_size += s;
	malloc_min_allocated_size =
	  s > malloc_min_allocated_size ? malloc_min_allocated_size : s;
	malloc_max_allocated_size =
	  s < malloc_max_allocated_size ? malloc_max_allocated_size : s;

	current_water_mark += s;
	if (current_water_mark > high_water_mark)
		high_water_mark = current_water_mark;
}

void AllocatorStatistics::record_calloc (size_t s)
{
	n_calloc_calls++;
	calloc_total_allocated_size += s;
	calloc_min_allocated_size =
	  s > calloc_min_allocated_size ? calloc_min_allocated_size : s;
	calloc_max_allocated_size =
	  s < calloc_max_allocated_size ? calloc_max_allocated_size : s;

	current_water_mark += s;
	if (current_water_mark > high_water_mark)
		high_water_mark = current_water_mark;
}

void AllocatorStatistics::record_aligned_malloc (size_t s)
{
	n_aligned_malloc_calls++;
	aligned_malloc_total_allocated_size += s;
	aligned_malloc_min_allocated_size =
	  s > aligned_malloc_min_allocated_size ? aligned_malloc_min_allocated_size : s;
	aligned_malloc_max_allocated_size =
	  s < aligned_malloc_max_allocated_size ? aligned_malloc_max_allocated_size : s;

	current_water_mark += s;
	if (current_water_mark > high_water_mark)
		high_water_mark = current_water_mark;
}

void AllocatorStatistics::record_realloc (size_t size, size_t prev_size)
{
	n_realloc_calls++;
	realloc_total_allocated_size += size;
	realloc_min_allocated_size =
	  size > realloc_min_allocated_size ? realloc_min_allocated_size : size;
	realloc_max_allocated_size =
	  size < realloc_max_allocated_size ? realloc_max_allocated_size : size;

	if (current_water_mark > prev_size)
		current_water_mark -= prev_size;
	else
		current_water_mark = 0; // This should not happen

	current_water_mark += size;
	if (current_water_mark > high_water_mark)
		high_water_mark = current_water_mark;
}

void AllocatorStatistics::record_free (size_t s)
{
	n_free_calls++;

	if (current_water_mark > s)
		current_water_mark -= s;
	else
		current_water_mark = 0; // This should not happen
}

void AllocatorStatistics::record_source_realloc (size_t s)
{
	n_source_realloc++;
	source_realloc_size += s;
}

void AllocatorStatistics::record_target_realloc (size_t s)
{
	n_target_realloc++;
	target_realloc_size += s;
}

void AllocatorStatistics::record_self_realloc (size_t s)
{
	n_self_realloc++;
	self_realloc_size += s;
}

void AllocatorStatistics::record_unfitted_malloc (size_t s)
{
	n_unfitted_malloc_calls++;
	unfitted_malloc_calls_size += s;
}

void AllocatorStatistics::record_unfitted_calloc (size_t s)
{
	n_unfitted_calloc_calls++;
	unfitted_calloc_calls_size += s;
}

void AllocatorStatistics::record_unfitted_aligned_malloc (size_t s)
{
	n_unfitted_aligned_malloc_calls++;
	unfitted_aligned_malloc_calls_size += s;
}

void AllocatorStatistics::record_unfitted_realloc (size_t s)
{
	n_unfitted_realloc_calls++;
	unfitted_realloc_calls_size += s;
}

void AllocatorStatistics::record_realloc_forward_malloc (void)
{
	n_realloc_fwd_malloc++;
}

void AllocatorStatistics::show_statistics (const char * allocator_name,
	bool show_high_water_mark, const char *extra_name) const
{
	size_t s;
	if (extra_name)
		s = strlen(allocator_name) + strlen(extra_name) + 2 + 1;
	else
		s = strlen(allocator_name) + 1;
	char full_name[s];
	memset (full_name, 0, sizeof(char)*s);
	if (extra_name)
		snprintf (full_name, s, "%s(%s)", allocator_name, extra_name);
	else
		snprintf (full_name, s, "%s", allocator_name);

	VERBOSE_MSG(1, "%s|Number of malloc calls: %u\n", full_name, n_malloc_calls);
	if (n_malloc_calls > 0)
	{
		VERBOSE_MSG(1, "%s|Malloc total allocated size = %lu bytes\n",
		  full_name, malloc_total_allocated_size);
		VERBOSE_MSG(1, "%s|Malloc min allocated size = %lu bytes\n",
		  full_name, malloc_min_allocated_size);
		VERBOSE_MSG(1 ,"%s|Malloc max allocated size = %lu bytes\n",
		  full_name, malloc_max_allocated_size);
		VERBOSE_MSG(1, "%s|%u not fitted malloc calls = %lu bytes.\n",
		  full_name, n_unfitted_malloc_calls, unfitted_malloc_calls_size);
	}
	if (n_realloc_fwd_malloc > 0)
	{
		VERBOSE_MSG(1, "%s|%u realloc calls were forwarded to malloc because of NULL pointer.\n",
		  full_name, n_realloc_fwd_malloc);
	}

	VERBOSE_MSG(1, "%s|Number of calloc calls: %u\n", full_name, n_calloc_calls);
	if (n_calloc_calls > 0)
	{
		VERBOSE_MSG(1, "%s|Calloc total allocated size = %lu bytes\n",
		  full_name, calloc_total_allocated_size);
		VERBOSE_MSG(1, "%s|Calloc min allocated size = %lu bytes\n",
		  full_name, calloc_min_allocated_size);
		VERBOSE_MSG(1 ,"%s|Calloc max allocated size = %lu bytes\n",
		  full_name, calloc_max_allocated_size);
		VERBOSE_MSG(1, "%s|%u not fitted calloc calls = %lu bytes.\n",
		  full_name, n_unfitted_calloc_calls, unfitted_calloc_calls_size);
	}

	VERBOSE_MSG(1, "%s|Number of aligned malloc calls: %u\n", full_name, n_aligned_malloc_calls);
	if (n_aligned_malloc_calls > 0)
	{
		VERBOSE_MSG(1, "%s|Aligned malloc total allocated size = %lu bytes\n",
		  full_name, aligned_malloc_total_allocated_size);
		VERBOSE_MSG(1, "%s|Aligned malloc min allocated size = %lu bytes\n",
		  full_name, aligned_malloc_min_allocated_size);
		VERBOSE_MSG(1 ,"%s|Aligned malloc max allocated size = %lu bytes\n",
		  full_name, aligned_malloc_max_allocated_size);
		VERBOSE_MSG(1, "%s|%u not fitted aligned malloc calls = %lu bytes.\n",
		  full_name, n_unfitted_aligned_malloc_calls, unfitted_aligned_malloc_calls_size);
	}

	VERBOSE_MSG(1, "%s|Number of realloc calls: %u\n", full_name, n_realloc_calls);
	if (n_realloc_calls > 0)
	{
		if (n_self_realloc > 0)
		{
			VERBOSE_MSG(1, "%s| - %u realloc on same allocator for a total of %lu copied bytes.\n",
			  full_name, n_self_realloc, self_realloc_size);
		}
		if (n_source_realloc > 0)
		{
			VERBOSE_MSG(1, "%s| - %u realloc as source allocator for a total of %lu copied bytes.\n",
			  full_name, n_source_realloc, source_realloc_size);
		}
		if (n_target_realloc > 0)
		{
			VERBOSE_MSG(1, "%s| - %u realloc as target allocator for a total of %lu copied bytes.\n",
			  full_name, n_target_realloc, target_realloc_size);
		}
		VERBOSE_MSG(1, "%s|Realloc total allocated size = %lu bytes\n",
		  full_name, realloc_total_allocated_size);
		VERBOSE_MSG(1, "%s|Realloc min allocated size = %lu bytes\n",
		  full_name, realloc_min_allocated_size);
		VERBOSE_MSG(1, "%s|Realloc max allocated size = %lu bytes\n",
		  full_name, realloc_max_allocated_size);
		VERBOSE_MSG(1, "%s|%u not fitted realloc calls = %lu bytes.\n",
		  full_name, n_unfitted_realloc_calls, unfitted_realloc_calls_size);
	}

	VERBOSE_MSG(1, "%s|Number of free calls: %u\n", full_name, n_free_calls);

	if (show_high_water_mark)
	{
		VERBOSE_MSG(1 ,"%s|High-water mark = %lu bytes\n", full_name, high_water_mark);
	}
}

