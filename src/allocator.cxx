// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include "allocator.hxx"

// Macro to align an address to the nearest power of two
#ifndef align_to
# define align_to(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#endif

size_t Allocator::getTotalSize (size_t size)
{
	return size + ALLOCATOR_HEADER_SZ;
}

Allocator::Header_t * Allocator::getAllocatorHeader (void * ptr)
{
	return (Header_t*) ((uintptr_t) ptr - ALLOCATOR_HEADER_SZ);
}

void * Allocator::generateAllocatorHeader (void *ptr, Allocator *a, size_t s)
{
	return generateAllocatorHeader (ptr, 0, a, s);
}

void * Allocator::generateAllocatorHeader (void *ptr, size_t extrabytes, Allocator *a, size_t s)
{
	// Calculate new storage address
	void *res = (void*) (((uintptr_t) ptr) + ALLOCATOR_HEADER_SZ + extrabytes);

	// Record the header contents
	Header_t *hdr = getAllocatorHeader (res);
	hdr->allocator = a;
	hdr->base_ptr = ptr;
	hdr->size = s;
	hdr->aux.u64[0] = 0; // Reset AUX field

	return res;
}

void * Allocator::generateAllocatorHeaderOnAligned (void *ptr, size_t align, Allocator *a, size_t s)
{
	// Calculate new storage address
	void * res = (void*) align_to (((uintptr_t) ptr + ALLOCATOR_HEADER_SZ), align);
	size_t padded_align = (uintptr_t) res - (uintptr_t) ptr;

	// Well-aligned given the requested alignment
	assert ( ( (uintptr_t) res & (align - 1) ) == 0);
	// Ensure enough space for a header between newptr and baseptr
	assert ( padded_align >= ALLOCATOR_HEADER_SZ );

	return generateAllocatorHeader(ptr, padded_align - ALLOCATOR_HEADER_SZ, a, s);
}

Allocator::Allocator (allocation_functions_t &af)
  : _af(af), _has_size (false), _size(0), _used(false)
{
	_fallback = nullptr;
}

Allocator::~Allocator ()
{
}

void Allocator::codeLocation (void *ptr, uint32_t CL)
{
	if (nullptr != ptr)
	{
		Header_t *hdr = getAllocatorHeader (ptr);
		hdr->aux.u32[ALLOCATOR_HEADER_AUX_CALLSTACKID] = CL + 1;
	}
}

uint32_t Allocator::codeLocation (void *ptr, bool& valid)
{
	uint32_t res = 0;
	if (nullptr != ptr)
	{
		Header_t *hdr = getAllocatorHeader (ptr);
		valid = hdr->aux.u32[ALLOCATOR_HEADER_AUX_CALLSTACKID] > 0;
		if (valid)
			res = hdr->aux.u32[ALLOCATOR_HEADER_AUX_CALLSTACKID] - 1;
	}
	else
		valid = false;
	return res;
}

void Allocator::pmemNode (void *ptr, uint32_t node)
{
	if (nullptr != ptr)
	{
		Header_t *hdr = getAllocatorHeader (ptr);
		hdr->aux.u32[ALLOCATOR_HEADER_AUX_PMEM_NODE] = 1+node;
	}
}

uint32_t Allocator::pmemNode (void *ptr, bool& valid)
{
	uint32_t res = 0;
	if (nullptr != ptr)
	{
		Header_t *hdr = getAllocatorHeader (ptr);
		valid = hdr->aux.u32[ALLOCATOR_HEADER_AUX_PMEM_NODE] > 0;
		if (valid)
			res = hdr->aux.u32[ALLOCATOR_HEADER_AUX_PMEM_NODE] - 1;
	}
	else
		valid = false;
	return res;
}


// This is a bit obscure. This extra size comes from aligned mallocs.
//   The alignment bytes need to be reallocated as well to avoid passing to the user
//   some FlexMalloc internal bytes (the header, more precisely) and other bytes.
uintptr_t Allocator::getExtraSize (Header_t *hdr)
{
	assert (hdr != nullptr);
	return (uintptr_t) hdr - ((uintptr_t) hdr->base_ptr);
}

