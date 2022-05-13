// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine
//
#pragma once

#include <assert.h>
#include "common.hxx"
#include "allocator-statistics.hxx"

class Allocator
{
	public:
	// Internal header structure to hold information around the allocator used and other data.
	// Contains:
	// 	- (pos-1) Allocator pointer that should handle
	// 	- (pos-2) Original allocated pointer -- for free, realloc. Needed as there may be a gap
	// 	          when issuing aligned allocations
	// 	- (pos-3) Requested size
	// 	- (pos-4) Auxiliary field
	typedef struct Header_st {
		Allocator *allocator;
		void      *base_ptr;
		size_t    size;
		union
		{
			uint64_t u64[1];
			uint32_t u32[2];
		} aux;
	} Header_t;

	enum ALLOCATOR_HEADER_AUX_IDX { ALLOCATOR_HEADER_AUX_PMEM_NODE = 0, ALLOCATOR_HEADER_AUX_CALLSTACKID };

	private:
	static const size_t ALLOCATOR_HEADER_SZ = sizeof(Header_t);

	protected:
	const allocation_functions_t _af;
	bool _has_size;
	size_t _size;
	bool _used;
	Allocator * _fallback;
	
	public:
	Allocator (allocation_functions_t &);
	virtual ~Allocator ();

	static Header_t * getAllocatorHeader (void * ptr);
	static void * generateAllocatorHeader (void *ptr, Allocator *a, size_t s);
	static void * generateAllocatorHeader (void *ptr, size_t extrabytes, Allocator *a, size_t s);
	static void * generateAllocatorHeaderOnAligned (void *ptr, size_t align, Allocator *a, size_t s);
	static size_t getTotalSize (size_t size);
	static uintptr_t getExtraSize (Header_t *hdr);

	static void codeLocation (void *ptr, uint32_t codelocation);
	static uint32_t codeLocation (void *ptr, bool& valid);
	static void pmemNode (void *ptr, uint32_t node);
	static uint32_t pmemNode (void *ptr, bool& valid);

	virtual const char * name (void) const = 0;
	virtual const char * description (void) const = 0;

	virtual void*  malloc (size_t) = 0;
	virtual void*  calloc (size_t, size_t) = 0;
	virtual int    posix_memalign (void **, size_t, size_t) = 0;
	virtual void   free (void *) = 0;
	virtual void*  realloc (void *, size_t) = 0;
	virtual size_t malloc_usable_size (void*) = 0;

	virtual void *memcpy (void *dest, const void *src, size_t n) = 0;

	virtual void   configure (const char *) = 0;
	void size (size_t s) { _size = s; _has_size = s > 0; };
	size_t size (void) const { return _size; };
	bool has_size (void) const { return _has_size; };
	virtual void show_statistics (void) const = 0;

	bool used (void) const { return _used; };
	void used (bool b) { _used = b; };

	virtual bool fits (size_t s) const = 0;
	virtual size_t hwm (void) const = 0;
	virtual void record_unfitted_malloc (size_t) = 0;
	virtual void record_unfitted_calloc (size_t) = 0;
	virtual void record_unfitted_aligned_malloc (size_t) = 0;
	virtual void record_unfitted_realloc (size_t) = 0;

	virtual void record_source_realloc (size_t) = 0;
	virtual void record_target_realloc (size_t) = 0;
	virtual void record_self_realloc (size_t) = 0;

	virtual void record_realloc_forward_malloc (void) = 0;
};

