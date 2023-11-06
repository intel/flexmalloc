// vim: set nowrap
// vim: set tabstop=4

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <execinfo.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <errno.h>
#include <unistd.h>
#include <new>
#include <malloc.h>
#include <dlfcn.h>

#if defined(HWC)
# include <papi.h>
#endif

#include "common.hxx"
#include "code-locations.hxx"
#include "allocators.hxx"
#include "flex-malloc.hxx"

static allocation_functions_t real_allocation_functions;
static Allocator * fallback = nullptr;
static Allocator * fallback_smallAllocation = nullptr;
static Allocator * posix_allocator = nullptr;

static unsigned long long _n_malloc = 0;
static unsigned long long _n_malloc_small = 0;
static unsigned long long _n_valloc = 0;
static unsigned long long _n_pvalloc = 0;
static unsigned long long _n_calloc = 0;
static unsigned long long _n_free = 0;
static unsigned long long _n_cfree = 0;
static unsigned long long _n_realloc = 0;
static unsigned long long _n_posix_memalign = 0;
static unsigned long long _n_aligned_alloc = 0;
static unsigned long long _n_memalign = 0;
static unsigned long long _n_malloc_usable_size = 0;

static unsigned inside = 0;
static bool malloc_interposer_started = false;
static pthread_mutex_t mtx_malloc_interposer;
static pthread_mutexattr_t mtx_attr_malloc_interposer;

static Allocators *allocators = nullptr;
static CodeLocations *codelocations = nullptr;
static FlexMalloc *flexmalloc = nullptr;

#if defined(HWC)
static pthread_mutex_t pthread_create_mutex;
static int EventSet;
static uint8_t CountersEnabled[8];

static void counters_init (void)
{
	int rc = PAPI_library_init(PAPI_VER_CURRENT);
	if (rc != PAPI_VER_CURRENT && rc > 0)
	{
		fprintf (stderr, "PAPI library version mismatch!\n");
		exit(1);
	}
	if (rc < 0)
	{
		fprintf (stderr, "PAPI library init error (%d)!\n", rc);
		exit(1);
	}
	rc = PAPI_thread_init(pthread_self); assert (rc == PAPI_OK);
	rc = PAPI_set_domain (PAPI_DOM_ALL); assert (rc == PAPI_OK);
}

static void counters_prepare (int *EventSet)
{
	*EventSet = PAPI_NULL;

	int rc = PAPI_create_eventset(EventSet);
	if (rc != PAPI_OK)
	{
		fprintf (stderr, "Could not create PAPI eventset.\n");
		exit (1);
	}

	CountersEnabled[0] = PAPI_add_named_event (*EventSet, "PAPI_TOT_INS") == PAPI_OK; assert (CountersEnabled[0]);
	CountersEnabled[1] = PAPI_add_named_event (*EventSet, "PAPI_TOT_CYC") == PAPI_OK; assert (CountersEnabled[1]);
	CountersEnabled[2] = PAPI_add_named_event (*EventSet, "PAPI_REF_CYC") == PAPI_OK; assert (CountersEnabled[2]);
	CountersEnabled[3] = PAPI_add_named_event (*EventSet, "PAPI_L1_DCM") == PAPI_OK;
	CountersEnabled[4] = PAPI_add_named_event (*EventSet, "PAPI_L2_TCM") == PAPI_OK;
	CountersEnabled[5] = PAPI_add_named_event (*EventSet, "PAPI_L3_TCM") == PAPI_OK;
	CountersEnabled[6] = PAPI_add_named_event (*EventSet, "PAPI_BR_INS") == PAPI_OK;
	CountersEnabled[7] = PAPI_add_named_event (*EventSet, "PAPI_BR_MSP") == PAPI_OK;
}

static void counters_start (void)
{
	PAPI_start (EventSet);
}

static void counters_stop (long long values[8])
{
	PAPI_stop (EventSet, values);
}

static void counters_accum (long long values[8])
{
	PAPI_stop (EventSet, values);
}

struct pthread_create_info
{
	void *(*routine)(void*);
	void *arg;
	
	pthread_cond_t wait;
	pthread_mutex_t lock;
};

static void * pthread_create_hook (void *p1)
{
	struct pthread_create_info *i = (struct pthread_create_info*) p1;
	void *(*routine)(void*) = i->routine;
	void *arg = i->arg;
	void *res = nullptr;

	/* Notify the calling thread */
	pthread_mutex_lock (&(i->lock));
	pthread_cond_signal (&(i->wait));
	pthread_mutex_unlock (&(i->lock));

	DBG("pthread %lx is alive!\n", pthread_self());

	int EventSet;
	counters_prepare (&EventSet);

	DBG("pthread %lx starting routine %p.\n", pthread_self(), arg);
	res = routine (arg);

	long long v[8];
	counters_stop (v);

	DBG("pthread %lx finished routine %p. Metrics = { %lld, %lld, %lld, %lld, %lld, %lld, %lld, %lld }.\n",
	  pthread_self(), arg, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);

	return res;
}

int pthread_create (
	pthread_t* p1,
	const pthread_attr_t* p2,
	void *(*p3) (void *),
	void* p4)
{
	static int (*pthread_create_real)(pthread_t*,const pthread_attr_t*,void *(*) (void *),void*) = nullptr;
	static int pthread_library_depth = 0;
	int res = 0;
	struct pthread_create_info i;

	if (pthread_create_real == nullptr)
	{
		pthread_create_real =
		(int(*)(pthread_t*,const pthread_attr_t*,void *(*) (void *),void*))
			dlsym (RTLD_NEXT, "pthread_create");
		pthread_mutex_init (&pthread_create_mutex, nullptr);
	}
	assert (pthread_create_real != nullptr);

	/* Protect creation, just one at a time */
	pthread_mutex_lock (&pthread_create_mutex);

	if (0 == pthread_library_depth)
	{
		pthread_library_depth++;

		DBG("Creating a thread for function %p with data at %p.\n", p3, p4);

		pthread_cond_init (&(i.wait), nullptr);
		pthread_mutex_init (&(i.lock), nullptr);
		pthread_mutex_lock (&(i.lock));

		i.routine = p3;
		i.arg = p4;
		res = pthread_create_real (p1, p2, pthread_create_hook, (void*) &i);

		if (0 == res)
		{
			/* if succeded, wait for a completion on copy the info */
			pthread_cond_wait (&(i.wait), &(i.lock));
		}

		DBG("Returned from pthread_create function with result %d.\n", res);

		pthread_mutex_unlock (&(i.lock));
		pthread_mutex_destroy (&(i.lock));
		pthread_cond_destroy (&(i.wait));

		pthread_library_depth--;
	}
	else
		res = pthread_create_real (p1, p2, p3, p4);

	/* Stop protecting the region, more pthread creations can enter */
	pthread_mutex_unlock (&pthread_create_mutex);

	return res;
}
#endif // HWC

void* dlopen(const char *file, int mode)
{
	static void* (*o_dlopen) ( const char *file, int mode )=0;
	o_dlopen = (void*(*)(const char *file, int mode)) dlsym(RTLD_NEXT,"dlopen");
	void* res = (*o_dlopen)( file, mode );

	if (LIKELY(malloc_interposer_started) && !options.sourceFrames())
	{
		if (file != NULL)
		{
			codelocations->translate_pending_frames(file);
		}
	}
	return res;
}

void * malloc (size_t size)
{
	if (UNLIKELY(!malloc_interposer_started))
	{
		DBG("uninit size %lu\n", size);
		return FlexMalloc::uninitialized_malloc (size);
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	void * res = nullptr;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			_n_malloc++;

			DBG("IN (size = %lu)\n", size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);
			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			res = flexmalloc->malloc (nptrs-1, &callstack_ptrs[1], size); // Skip top function -- this malloc routine
			DBG("returning IN %p\n", res);
		}
		else
		{
			DBG("OUT (size = %lu) calling to fallback allocator (%s)\n", size, fallback->name());
			res = fallback->malloc (size);
			DBG("returning OUT %p\n", res);
		}
	}
	else
	{
		_n_malloc_small++;

		DBG("IN_small (size = %lu)\n", size);
		if (inside == 1)
			res = fallback_smallAllocation->malloc (size);
		else
			res = posix_allocator->malloc(size);
		DBG("returning IN_small %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}


#define _CALLOC_INIT_BUFFER     1024*1024
static char     __calloc_buffer[_CALLOC_INIT_BUFFER] = { 0 };
static unsigned __calloc_buffer_last_pos = 0;

void * calloc (size_t nmemb, size_t size)
{
	static void* (*tmp_calloc)(size_t,size_t) = nullptr;
	static void* (*tmp_malloc)(size_t) = nullptr;
	static unsigned calloc_init_depth = 0;
	void * res = nullptr;
 	if (UNLIKELY(!malloc_interposer_started))
	{
		if (tmp_calloc == nullptr && calloc_init_depth == 0)
		{
			calloc_init_depth++;
			tmp_calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "calloc");
			tmp_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
			calloc_init_depth--;
			DBG("PRE0 (size = %lu)\n", size);
			void *res = tmp_calloc (nmemb, size);
			DBG("returning %p\n", res);
			return res;
		}
		else if (tmp_calloc == nullptr && calloc_init_depth > 0)
		{
			assert (nmemb * size + __calloc_buffer_last_pos < _CALLOC_INIT_BUFFER);
			res = &__calloc_buffer[__calloc_buffer_last_pos];
			__calloc_buffer_last_pos += nmemb * size;
			DBG("PRE0b (size = %lu)\n", size);
			DBG("returning %p\n", res);
			return res;
		}
		else if (tmp_malloc != nullptr)
		{
			DBG("uninit size %lu * %lu\n", nmemb, size);
			void * res = FlexMalloc::uninitialized_malloc (nmemb * size);
			memset (res, 0, nmemb * size);
			return res;
		}
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (nmemb * size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			_n_calloc++;

			DBG("IN (size = %lu)\n", size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			res = flexmalloc->calloc (nptrs-1, &callstack_ptrs[1], nmemb, size); // Skip top function -- this calloc routine
			DBG("returning IN %p\n", res);
		}
		else
		{
			DBG("OUT (nmemb = %lu, size = %lu)\n", nmemb, size);
			res = fallback->calloc (nmemb, size);
			DBG("returning OUT %p\n", res);
		}
	}
	else
	{
		_n_calloc++;

		DBG("IN_small (nmemb = %lu, size = %lu)\n", nmemb, size);
		if (inside == 1)
			res = fallback_smallAllocation->calloc (nmemb, size);
		else
			res = posix_allocator->calloc (nmemb, size);
		DBG("returning IN_small %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	// Set memory to 0s, as calloc semantics requires
	memset (res, 0, nmemb * size);

	return res;
}

void *realloc (void *ptr, size_t size)
{
	if (UNLIKELY((void*) &__calloc_buffer[0] <= ptr && ptr < (void*) &__calloc_buffer[_CALLOC_INIT_BUFFER]))
	{
		assert (false);
		return nullptr;
	}

	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu ptr %p\n", size, ptr);
		return FlexMalloc::uninitialized_realloc (ptr, size);
	}

	// We cannot discriminate according to the given size because we need to honor
	// the allocator previously used.

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	void * res = nullptr;
	if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
	{
		_n_realloc++;

		DBG("IN ptr = %p, size = %lu\n", ptr, size);
		unsigned MF = codelocations->max_nframes();
		void *callstack_ptrs[1+MF];
		unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
		assert (nptrs <= 1+MF);

		if (options.callstackMinus1())
			for (unsigned u = 1; u < nptrs; ++u) // Skip top function
				callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
		res = flexmalloc->realloc (nptrs-1, &callstack_ptrs[1], ptr, size); // Skip top function -- this malloc routine
		DBG("returning IN %p\n", res);
	}
	else
	{
		DBG("OUT ptr = %p size = %lu calling to fallback allocator (%s)\n", ptr, size, fallback->name());
		// flexmalloc should handle the realloc since calling it in the context of the
		// fallback allocator could move the region into another allocator, which is wrong
		// res = fallback->realloc (ptr, size);
		res = nullptr;
		if (flexmalloc != nullptr)
			res = flexmalloc->realloc (0, nullptr, ptr, size);
		DBG("returning OUT %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

int posix_memalign (void **memptr, size_t alignment, size_t size)
{
	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu\n", size);
		return FlexMalloc::uninitialized_posix_memalign (memptr, alignment, size);
	}

	int res = 0; 
	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			_n_posix_memalign++;

			DBG("IN (alignment = %lu, size = %lu)\n", alignment, size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			res = flexmalloc->posix_memalign (nptrs-1, &callstack_ptrs[1], memptr, alignment, size); // Skip top function -- this very same routine
			DBG("returning %d (memptr %p)\n", res, *memptr);
		}
		else
		{
			DBG("OUT (alignment = %lu, size = %lu)\n", alignment, size);
			res = fallback->posix_memalign (memptr, alignment, size);
			DBG("returning %d (memptr %p)\n", res, *memptr);
		}
	}
	else
	{
		_n_posix_memalign++;

		DBG("IN_small (alignment = %lu, size = %lu)\n", alignment, size);
		if (inside == 1)
			res = fallback_smallAllocation->posix_memalign (memptr, alignment, size);
		else
			res = posix_allocator->posix_memalign (memptr, alignment, size);
		DBG("returning %d (memptr %p)\n", res, *memptr);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

// aligned_alloc relies on top of posix_memalign
void * aligned_alloc (size_t alignment, size_t size)
{
	void * res = nullptr;

	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu\n", size);
		if (FlexMalloc::uninitialized_posix_memalign (&res, alignment, size) == 0)
			return res;
		else
			return nullptr;
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			DBG("IN (alignment = %lu, size = %lu)\n", alignment, size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			int r = flexmalloc->posix_memalign (nptrs-1, &callstack_ptrs[1], &res, alignment, size); // Skip top function -- this very same routine
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
		else
		{
			DBG("OUT (alignment = %lu, size = %lu)\n", alignment, size);
			int r = fallback->posix_memalign (&res, alignment, size);
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
	}
	else
	{
		_n_aligned_alloc++;

		DBG("IN_small (alignment = %lu, size = %lu)\n", alignment, size);
		int r;
		if (inside == 1)
			r = fallback_smallAllocation->posix_memalign (&res, alignment, size);
		else
			r = posix_allocator->posix_memalign (&res, alignment, size);
		if (r != 0)
			res = nullptr;
		DBG("returning %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

// memalign relies on top of posix_memalign
void * memalign (size_t alignment, size_t size)
{
	void * res = nullptr;

	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu\n", size);
		if (FlexMalloc::uninitialized_posix_memalign (&res, alignment, size) == 0)
			return res;
		else
			return nullptr;
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			DBG("IN (alignment = %lu, size = %lu)\n", alignment, size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			int r = flexmalloc->posix_memalign (nptrs-1, &callstack_ptrs[1], &res, alignment, size); // Skip top function -- this very same routine
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
		else
		{
			DBG("OUT (alignment = %lu, size = %lu)\n", alignment, size);
			int r = fallback->posix_memalign (&res, alignment, size);
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
	}
	else
	{
		_n_memalign++;

		DBG("IN_small (alignment = %lu, size = %lu)\n", alignment, size);
		int r;
		if (inside == 1)
			r = fallback_smallAllocation->posix_memalign (&res, alignment, size);
		else
			r = posix_allocator->posix_memalign (&res, alignment, size);
		if (r != 0)
			res = nullptr;
		DBG("returning %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

// valloc relies on top of posix_memalign
void * valloc (size_t size)
{
	void * res = nullptr;

	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu\n", size);
		if (FlexMalloc::uninitialized_posix_memalign (&res, sysconf(_SC_PAGESIZE), size) == 0)
			return res;
		else
			return nullptr;
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			DBG("IN (size = %lu)\n", size);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			int r = flexmalloc->posix_memalign (nptrs-1, &callstack_ptrs[1], &res, sysconf(_SC_PAGESIZE), size); // Skip top function -- this very same routine
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
		else
		{
			DBG("OUT (size = %lu)\n", size);
			int r = fallback->posix_memalign (&res, sysconf(_SC_PAGESIZE), size);
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
	}
	else
	{
		_n_valloc++;

		DBG("IN_small (size = %lu)\n", size);
		int r = fallback->posix_memalign (&res, sysconf(_SC_PAGESIZE), size);
		if (r != 0)
			res = nullptr;
		DBG("returning %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

// pvalloc relies on top of posix_memalign
void * pvalloc (size_t size)
{
	void * res = nullptr;

	// As per pvalloc definition:
	// 	rounds the size of the allocation up to the next multiple of the system page size
	long pagesize = sysconf(_SC_PAGESIZE);
	size_t nsize = ((size + pagesize - 1) / pagesize) * pagesize;

	if (UNLIKELY(!malloc_interposer_started))
	{
	// This branch occurs when running free before initializing library
		DBG("uninit size %lu (nsize %lu)\n", size, nsize);
		if (FlexMalloc::uninitialized_posix_memalign (&res, sysconf(_SC_PAGESIZE), nsize) == 0)
			return res;
		else
			return nullptr;
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	inside++;
	if (size > options.minSize())
	{
		if (LIKELY(inside == 1 && flexmalloc && codelocations->has_locations()))
		{
			DBG("IN (size = %lu, nsize = %lu)\n", size, nsize);
			unsigned MF = codelocations->max_nframes();
			void *callstack_ptrs[1+MF];
			unsigned nptrs = backtrace (callstack_ptrs, 1+MF); // Careful, this seems to use malloc
			assert (nptrs <= 1+MF);

			if (options.callstackMinus1())
				for (unsigned u = 1; u < nptrs; ++u) // Skip top function
					callstack_ptrs[u] = (void*) ( ( (long) callstack_ptrs[u] ) - 1 );
			int r = flexmalloc->posix_memalign (nptrs-1, &callstack_ptrs[1], &res, sysconf(_SC_PAGESIZE), nsize); // Skip top function -- this very same routine
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
		else
		{
			DBG("OUT (size = %lu, nsize = %lu)\n", size, nsize);
			int r = fallback->posix_memalign (&res, sysconf(_SC_PAGESIZE), nsize);
			if (r != 0)
				res = nullptr;
			DBG("returning %p\n", res);
		}
	}
	else
	{
		_n_pvalloc++;

		DBG("IN_small (size = %lu, nsize = %lu)\n", size, nsize);
		int r = fallback->posix_memalign (&res, sysconf(_SC_PAGESIZE), nsize);
		if (r != 0)
			res = nullptr;
		DBG("returning %p\n", res);
	}
	inside--;
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

void free (void *ptr)
{
	if (UNLIKELY(ptr == nullptr))
		return;

	if (UNLIKELY((void*) &__calloc_buffer[0] <= ptr && ptr < (void*) &__calloc_buffer[_CALLOC_INIT_BUFFER]))
		return;

	if (UNLIKELY(!malloc_interposer_started))
		return;

	pthread_mutex_lock (&mtx_malloc_interposer);
	flexmalloc->free (ptr);
	_n_free++;
	pthread_mutex_unlock (&mtx_malloc_interposer);
}

void cfree (void *ptr)
{
	if (UNLIKELY(ptr == nullptr))
		return;

	if (UNLIKELY((void*) &__calloc_buffer[0] <= ptr && ptr < (void*) &__calloc_buffer[_CALLOC_INIT_BUFFER]))
		return;

	if (UNLIKELY(!malloc_interposer_started))
		return;

	// cfree (ptr) relies on top of free (ptr)
	pthread_mutex_lock (&mtx_malloc_interposer);
	flexmalloc->free (ptr);
	_n_cfree++;
	pthread_mutex_unlock (&mtx_malloc_interposer);
}

size_t malloc_usable_size (void *ptr)
{
	if (UNLIKELY(ptr == nullptr))
		return 0;

	size_t res; 

	if (UNLIKELY(!malloc_interposer_started))
	// This branch occurs when running free before initializing library
	{
		return FlexMalloc::uninitialized_malloc_usable_size (ptr);
	}

	pthread_mutex_lock (&mtx_malloc_interposer);
	res = flexmalloc->malloc_usable_size (ptr);
	pthread_mutex_unlock (&mtx_malloc_interposer);

	return res;
}

void malloc_interposer_start (void) __attribute__((constructor));
void malloc_interposer_start (void)
{
	char *env;

	VERBOSE_MSG(0, "Initializing " TOOL_NAME " " PACKAGE_VERSION "... \n");

	// Create mutex to avoid multiple threads running into the interposer
	pthread_mutexattr_init (&mtx_attr_malloc_interposer);
	pthread_mutexattr_settype (&mtx_attr_malloc_interposer, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init (&mtx_malloc_interposer, &mtx_attr_malloc_interposer);

	// Get the symbols for the default allocator functions
	real_allocation_functions.malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
	real_allocation_functions.calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "calloc");
	real_allocation_functions.free = (void (*)(void*)) dlsym (RTLD_NEXT, "free");
	real_allocation_functions.realloc = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "realloc");
	real_allocation_functions.posix_memalign = (int (*)(void**,size_t,size_t)) dlsym (RTLD_NEXT, "posix_memalign");
	real_allocation_functions.malloc_usable_size = (size_t (*)(void*)) dlsym (RTLD_NEXT, "malloc_usable_size");

	if (real_allocation_functions.malloc == nullptr ||
	    real_allocation_functions.calloc == nullptr ||
	    real_allocation_functions.free == nullptr ||
	    real_allocation_functions.realloc == nullptr ||
	    real_allocation_functions.posix_memalign == nullptr ||
	    real_allocation_functions.malloc_usable_size == nullptr)
	{
		VERBOSE_MSG(0, "Could not find malloc/calloc/free/realloc/posix_memalign/malloc_usable_size symbols in DSOs.");
		_exit (-1);
	}

	// Get memory definitions from environment
	if ((env = getenv (TOOL_DEFINITIONS_FILE)) != nullptr)
	{
		allocators = (Allocators*) real_allocation_functions.malloc (sizeof(Allocators));
		assert (allocators != nullptr);
		new (allocators) Allocators (real_allocation_functions, getenv(TOOL_DEFINITIONS_FILE));
	}
	else
	{
		VERBOSE_MSG(0, "Did not find " TOOL_DEFINITIONS_FILE " environment variable. Finishing...\n");
		_exit (2);
	}

	// Get fallback allocator, if given from environment.
	// If not, we use the regular "posix" allocators as fallback.
	if ((env = getenv (TOOL_FALLBACK_ALLOCATOR)) != nullptr)
	{
		fallback = allocators->get (env);
		if (!fallback)
		{
			VERBOSE_MSG(0, "Did not find allocator \"%s\" to be used as fallback allocator. Exiting!\n", env);
			_exit (2);
		}
	}
	else
	{
		fallback = allocators->get ("posix");
		if (!fallback)
		{
			VERBOSE_MSG(0, "Did not find allocator \"posix\" to be used as fallback allocator. Exiting!\n");
			_exit (2);
		}
	}

	if (! fallback->is_ready()) {
		VERBOSE_MSG(0, "The fallback allocator '%s' is not ready. Check if its parameters in the configuration file are correct. Exiting!\n", fallback->name());
		_exit (2);
	}

	VERBOSE_MSG(0, "Fallback allocator set to '%s'\n", fallback->name());

	if (fallback->has_size())
	{
		if (1024 * 1024 < fallback->size())
		{
			VERBOSE_MSG(0, "WARNING! WARNING! WARNING!\n"
			               " The fallback allocator (%s) has a size limit (%lu Mbytes).\n"
			               " This is likely to be dangerous because fallback allocator is\n"
			               " not checked against memory used and may lead to memory exhaustion\n"
			               " and an application failure.\n"
			               "WARNING! WARNING! WARNING!\n",
			               fallback->name(), fallback->size() >> 20);
		}
		else
		{
			VERBOSE_MSG(0, "WARNING! WARNING! WARNING!\n"
			               " The fallback allocator (%s) has a size limit (%lu bytes).\n"
			               " This is likely to be dangerous because fallback allocator is\n"
			               " not checked against memory used and may lead to memory exhaustion\n"
			               " and an application failure.\n"
			               "WARNING! WARNING! WARNING!\n",
			               fallback->name(), fallback->size());
		}
	}

	posix_allocator = allocators->get ("posix");

	// If user set a min threshold size, we need to configure the allocator
	// that will handle small allocations
	if (options.minSize() > 0)
	{
		if ((env = getenv (TOOL_MINSIZE_THRESHOLD_ALLOCATOR)) != nullptr)
		{
			fallback_smallAllocation = allocators->get (env);
			if (!fallback_smallAllocation)
			{
				VERBOSE_MSG(0, "Did not find allocator \"%s\" to be used as fallback allocator for small allocations. Exiting!\n", env);
				_exit (2);
			}

			if (! fallback_smallAllocation->is_ready()) {
				VERBOSE_MSG(0, "The fallback allocator for small allocations '%s' is not ready. Check if its parameters in the configuration file are correct.\n", fallback_smallAllocation->name());
				_exit (2);
			}
		}
		else
			fallback_smallAllocation = fallback;
		VERBOSE_MSG(0, "Allocator '%s' will handle allocations smaller or equal than %ld bytes\n",
		  fallback_smallAllocation->name(), options.minSize());
	}

	// Mark the fallback allocator as used by default
	fallback->used(true);

	// If the user has given a file pointing to callstacks, parse and enable runtime
	if ((env = getenv (TOOL_LOCATIONS_FILE)) != nullptr)
	{
		codelocations = (CodeLocations*) real_allocation_functions.malloc (sizeof(CodeLocations));
		assert (codelocations != nullptr);
		new (codelocations) CodeLocations (real_allocation_functions, allocators);
		codelocations->readfile (env, fallback->name());
	}
	else
	{
		VERBOSE_MSG(0, "Did not find " TOOL_LOCATIONS_FILE " environment variable. Finishing...\n");
		_exit (2);
	}

	// Allocate main flexmalloc object
	flexmalloc = (FlexMalloc*) real_allocation_functions.malloc (sizeof(FlexMalloc));
	assert (flexmalloc != nullptr);
	new (flexmalloc) FlexMalloc (real_allocation_functions, fallback, codelocations);

#if defined(HWC)
	// Initialize and start performance counters
	counters_init();
	counters_prepare(&EventSet);
	counters_start();
#endif

	malloc_interposer_started = true;

	VERBOSE_MSG(0,"Starting application\n\n");
}

void malloc_interposer_stop  (void) __attribute__((destructor));
void malloc_interposer_stop  (void)
{
	// We've reached a point where the application has finished.
	// Dump multiple metrics.
	
	// Number of invocations
	VERBOSE_MSG_NOPREFIX(0,"\n");
	VERBOSE_MSG(0, "Execution time %llu seconds.\n", options.getTime() / (1000ULL * 1000ULL * 1000ULL));
	if (_n_malloc)
		VERBOSE_MSG(0, "Number of malloc calls: %llu.\n", _n_malloc);
	if (_n_malloc_small)
		VERBOSE_MSG(0, "Number of malloc (small) calls: %llu.\n", _n_malloc_small);
	if (_n_valloc)
		VERBOSE_MSG(0, "Number of valloc calls: %llu.\n", _n_valloc);
	if (_n_pvalloc)
		VERBOSE_MSG(0, "Number of pvalloc calls: %llu.\n", _n_pvalloc);
	if (_n_calloc)
		VERBOSE_MSG(0, "Number of calloc calls: %llu.\n", _n_calloc);
	if (_n_posix_memalign)
		VERBOSE_MSG(0, "Number of posix_memalign calls: %llu.\n", _n_posix_memalign);
	if (_n_aligned_alloc)
		VERBOSE_MSG(0, "Number of aligned_alloc calls: %llu.\n", _n_aligned_alloc);
	if (_n_memalign)
		VERBOSE_MSG(0, "Number of memalign calls: %llu.\n", _n_memalign);
	if (_n_realloc)
		VERBOSE_MSG(0, "Number of realloc calls: %llu.\n", _n_realloc);
	if (_n_free)
		VERBOSE_MSG(0, "Number of free calls: %llu.\n", _n_free);
	if (_n_cfree)
		VERBOSE_MSG(0, "Number of cfree calls: %llu.\n", _n_cfree);
	if (_n_malloc_usable_size)
		VERBOSE_MSG(0, "Number of malloc_usable_size calls: %llu.\n", _n_malloc_usable_size);

#if defined(HWC)
	// Performance counters
	long long counters[8];
	counters_stop (counters);

	VERBOSE_MSG(0, "%lld Minstructions.\n", counters[0]/1000000);
	VERBOSE_MSG(0, "%lld Mcycles (%0.3f ratio wrt nominal cycles).\n", counters[1]/1000000, ((float) counters[1])/((float)counters[2]));
	VERBOSE_MSG(0, "%0.3f instructions per cycle.\n", ((float) counters[0])/((float)counters[1]));

	if (CountersEnabled[3])
		VERBOSE_MSG(0, "%lld L1 data-cache Mmisses (%0.3f L1 data-cache misses per instruction).\n",
		  counters[3]/1000000, ((float) counters[3])/((float)counters[0]));
	if (CountersEnabled[4])
		VERBOSE_MSG(0, "%lld L2 Mmisses (%0.3f L2 misses per instruction).\n",
		  counters[4]/1000000, ((float) counters[4])/((float)counters[0]));
	if (CountersEnabled[5])
		VERBOSE_MSG(0, "%lld L3 Mmisses (%0.3f L3 misses per instruction).\n",
		  counters[5]/1000000, ((float) counters[5])/((float)counters[0]));
	if (CountersEnabled[6])
		VERBOSE_MSG(0, "%lld Mbranches (%0.3f branches per instruction).\n",
		  counters[6]/1000000, ((float) counters[6])/((float)counters[0]));
	if (CountersEnabled[6] && CountersEnabled[7])
		VERBOSE_MSG(0, "%lld Mbranches mispredicted (%0.3f branches mispredicted per branch, %0.3f branches mispredicted per instruction).\n",
		  counters[7]/1000000, ((float) counters[7])/((float)counters[6]), ((float) counters[7])/((float)counters[0]));
#endif

	// Dump internal statistics
	flexmalloc->show_statistics();
	codelocations->show_stats();
	codelocations->show_hmem_visualizer_stats(fallback->name());

	// Dump OS statistics
	struct rusage ru;
	if (0 == getrusage (RUSAGE_SELF, &ru))
	{
		VERBOSE_MSG(0, "Maximum resident size: %lu\n", ru.ru_maxrss);
		VERBOSE_MSG(0, "%lu page faults serviced that did not require I/O activity\n", ru.ru_minflt);
		VERBOSE_MSG(0, "%lu page faults servided that required I/O activity\n", ru.ru_majflt);
		VERBOSE_MSG(0, "%lu voluntary context switches\n", ru.ru_nvcsw);
		VERBOSE_MSG(0, "%lu involuntary context switches\n", ru.ru_nivcsw);
	}

	pthread_mutex_destroy (&mtx_malloc_interposer);

	// Dump OS mantained statuses
	VERBOSE_MSG(0, "Dump of /proc/self/status:\n");
	char buf[1024+1];
	int fd = open ("/proc/self/status", O_RDONLY);
	lseek (fd, 0, SEEK_SET);
	assert (fd >= 0);
	ssize_t r = read (fd, buf, 1024);
	while (r > 0 && r <= 1024)
	{
		write (2, buf, r);
		r = read (fd, buf, 1024);
	}
	close (fd);
	VERBOSE_MSG(0, "End of dump.\n");

	_exit (0);
}
