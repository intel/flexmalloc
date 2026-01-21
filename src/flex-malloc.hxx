#pragma once

#include <stdlib.h>

#include "allocator.hxx"
#include "code-locations.hxx"
#include "bfd-manager.hxx"
#include "cache-callstack.hxx"

class FlexMalloc
{
	private:
	const allocation_functions_t _af;
	Allocator * _fallback;
	const Allocators * _allocators; 

	CacheCallstacks _c_cache;

	typedef struct module_st
	{
		BFDManager *bfd;
		char *name;
		long startAddress;
		long endAddress;
		long offset;
		bool symbolsLoaded;
		bool do_not_backtrace;
	} module_t;

	module_t   *_modules;
	unsigned   _nmodules;
	void parse_map_files (void);
	CodeLocations * const _cl;

	bool excluded_library (const char *library);
	Allocator * allocatorForCallstack_source (unsigned nptrs, void **callstack, size_t sz, bool &fits, uint32_t& codelocation);
	Allocator * allocatorForCallstack_raw    (unsigned nptrs, void **callstack, size_t sz, bool &fits, uint32_t& codelocation);
	Allocator * allocatorForCallstack (unsigned nptrs, void **callstack, size_t sz, bool &fits, uint32_t& codelocation);

	public:
	FlexMalloc (allocation_functions_t &, Allocator *, CodeLocations *);
	~FlexMalloc ();

	void * malloc (unsigned nptrs, void **callstack, size_t sz);
	void * calloc (unsigned nptrs, void **callstack, size_t nmemb, size_t sz);
	int posix_memalign (unsigned nptrs, void **callstack, void ** memptr, size_t alignment, size_t sz);
	void * realloc (unsigned nptrs, void **callstack, void *ptr, size_t sz);
	void free (void *ptr);
	size_t malloc_usable_size (void *ptr) const;

	void show_statistics (void) const;

	////// Static methods - to be called before FlexMalloc has been fully initiliazed

	static void * uninitialized_malloc (size_t s);
	static int    uninitialized_posix_memalign (void **ptr, size_t align, size_t s);
	static void * uninitialized_realloc (void *ptr, size_t s);
	static void   uninitialized_free (void *ptr);
	static size_t uninitialized_malloc_usable_size (void *ptr);
};

