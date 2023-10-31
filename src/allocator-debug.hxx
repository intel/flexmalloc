// Author: Clément Foyer <clement.foyer@univ-reims.fr>
// Date: Oct 31, 2023
// License: To determine

#pragma once

#include "allocator.hxx"
#include "allocator-posix.hxx"

class AllocatorDebug : public Allocator
{
	private:
	AllocatorStatistics _stats;
	AllocatorPOSIX &_allocator;

	public:
	AllocatorDebug(allocation_functions_t &, AllocatorPOSIX &);
	~AllocatorDebug();

	void configure (const char *);
	const char * name (void) const;
	const char * description (void) const;
	void show_statistics (void) const;
	bool is_ready (void) const override;

	void* malloc (size_t s)
	{
		_stats.record_malloc (s);
		void *ptr = _allocator.malloc (s);
		Header_t *hdr = getAllocatorHeader (ptr);
		hdr->allocator = this;
		return ptr;
	}
	void* calloc (size_t n, size_t s)
	{
		_stats.record_calloc (n * s);
		void *ptr = _allocator.calloc (n, s);
		Header_t *hdr = getAllocatorHeader (ptr);
		hdr->allocator = this;
		return ptr;
	}
	int posix_memalign (void **p, size_t s, size_t a)
	{
		_stats.record_aligned_malloc (s + a);
		int ret = _allocator.posix_memalign (p, s, a);
		Header_t *hdr = getAllocatorHeader (*p);
		hdr->allocator = this;
		return ret;
	}
	void free (void *p)
	{
		Header_t *hdr = getAllocatorHeader (p);
		_stats.record_free (hdr->size);
		_allocator.free (p);
	}
	void* realloc (void *ptr, size_t size)
	{
		Header_t *hdr = nullptr;
		if (ptr)
		{
			hdr = getAllocatorHeader (ptr);
			size_t prev_size = hdr->size;
			if (prev_size < size)
			{
				_stats.record_realloc (size, prev_size);
				ptr = _allocator.realloc (ptr, size);
			}
		}
		else
		{
			_stats.record_realloc_forward_malloc();
			ptr = _allocator.realloc (ptr, size);
			_stats.record_malloc (size); // otherwise skipped once we enter _allocator
		}
		hdr = getAllocatorHeader (ptr);
		hdr->allocator = this;
		return ptr;
	}
	size_t malloc_usable_size (void *ptr)
	{
		return _allocator.malloc_usable_size (ptr);
	}

	void *memcpy (void *dest, const void *src, size_t n)
	{ return _allocator.memcpy (dest, src, n); }

	bool fits (size_t s) const
	{ return _allocator.fits (s); }
	size_t hwm (void) const
	{ return _allocator.hwm (); }

	void record_unfitted_malloc (size_t s)
	{
		_stats.record_unfitted_malloc (s);
		_allocator.record_unfitted_malloc (s);
	}
	void record_unfitted_calloc (size_t s)
	{
		_stats.record_unfitted_calloc (s);
		_allocator.record_unfitted_calloc (s);
	}
	void record_unfitted_aligned_malloc (size_t s)
	{
		_stats.record_unfitted_aligned_malloc (s);
		_allocator.record_unfitted_aligned_malloc (s);
	}
	void record_unfitted_realloc (size_t s)
	{
		_stats.record_unfitted_realloc (s);
		_allocator.record_unfitted_realloc (s);
	}

	void record_source_realloc (size_t s)
	{
		_stats.record_source_realloc (s);
		_allocator.record_source_realloc (s);
	}
	void record_target_realloc (size_t s)
	{
		_stats.record_target_realloc (s);
		_allocator.record_target_realloc (s);
	}
	void record_self_realloc (size_t s)
	{
		_stats.record_self_realloc (s);
		_allocator.record_self_realloc (s);
	}

	void record_realloc_forward_malloc (void)
	{
		_stats.record_realloc_forward_malloc ();
		_allocator.record_realloc_forward_malloc ();
	}
};
