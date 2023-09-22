// vim: set nowrap
// vim: set tabstop=4

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <dlfcn.h>
#include <assert.h>
#include <errno.h>

#define likely(x)      __builtin_expect(!!(x), 1) 
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define IS_TINY(x)     ((x) <= 64)
#define IS_SMALL(x)    (!IS_TINY(x) && (x) <= 4096)
#define IS_LARGE(x)    (!IS_SMALL(x) && (x) <= 2*1024*1024)
#define IS_HUGE(x)     (!IS_LARGE(x))

typedef enum sizes_e {
	TINY = 0,
	SMALL,
	LARGE,
	HUGE,
	N_SIZES
} size_et;

static bool Destructed = false;

static unsigned long long get_clock (void)
{
	struct timespec t;
	clock_gettime (CLOCK_MONOTONIC, &t);
	return (unsigned long long) t.tv_sec * 1000ULL * 1000ULL * 1000ULL + t.tv_nsec;
}
static unsigned long long initial_clock;

static long long estimated_living_data_objects = 0;
static long long max_estimated_living_data_objects = 0;

static bool debug = false;

/*
 * malloc -- allocate a block of size bytes
 */
static unsigned long long n_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_malloc = 0;
static void* (*real_malloc)(size_t) = nullptr;
extern "C" void * malloc(size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: malloc (%ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_malloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_malloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_malloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_malloc == nullptr))
		real_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");

	if (real_malloc != nullptr)
		return real_malloc (size);
	else
		return nullptr;
}

/*
 * kmp_malloc -- allocate a block of size bytes
 */
static unsigned long long n_kmp_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_kmp_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_kmp_malloc = 0;
static void* (*real_kmp_malloc)(size_t) = nullptr;
extern "C" void * kmp_malloc(size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: kmp_malloc (%ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_kmp_malloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_kmp_malloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_kmp_malloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_kmp_malloc == nullptr))
		real_kmp_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "kmp_malloc");

	if (real_kmp_malloc != nullptr)
		return real_kmp_malloc (size);
	else
		return nullptr;
}

// NEW operator from C++ -- same code as malloc
// REFERENCE: /usr/src/debug/gcc-7.3.1-6.fc27.x86_64/libstdc++-v3/libsupc++/new_op.cc
// We rely on regular mallocs
static unsigned long long n_new_sizes[N_SIZES] = { 0, };
static unsigned long long t_new_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_new = 0;
void * operator new (size_t size)
{
	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: new (%ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_new_sizes[idx], 1);
	__sync_fetch_and_add (&t_new_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_new, t - previous_time);
	previous_time = t;

	if (unlikely(real_malloc == nullptr))
		real_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
	if (real_malloc != nullptr)
		return real_malloc (size);
	else
		return nullptr;
}

/*
 * calloc -- allocate a block of nmemb * size bytes and set its contents to zero
 */
#define _CALLOC_INIT_BUFFER     1024*1024
static char     __calloc_buffer[_CALLOC_INIT_BUFFER] = { 0 };
static unsigned __calloc_buffer_last_pos = 0;

static unsigned long long n_calloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_calloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_calloc = 0;
static void * (*real_calloc)(size_t,size_t) = nullptr;
extern "C" void * calloc(size_t nmemb, size_t size)
{
	static unsigned calloc_depth = 0;
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(nmemb*size))
		idx = TINY;
	else if (IS_SMALL(nmemb*size))
		idx = SMALL;
	else if (IS_LARGE(nmemb*size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: calloc (%ld, %ld) -> idx = %d\n", nmemb, size, idx);

	__sync_fetch_and_add (&n_calloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_calloc_sizes[idx], nmemb*size);

	if (nmemb*size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_calloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_calloc == nullptr))
	{
		if (calloc_depth == 0)
		{
			calloc_depth++;
			real_calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "calloc");
			calloc_depth--;
		}
		else
		{
			assert (nmemb * size + __calloc_buffer_last_pos < _CALLOC_INIT_BUFFER);
			void * res = &__calloc_buffer[__calloc_buffer_last_pos];
			__calloc_buffer_last_pos += nmemb * size;
			return res;
		}
	}

	return real_calloc (nmemb, size);
}

/*
 * kmp_calloc -- allocate a block of nmemb * size bytes and set its contents to zero
 */
static unsigned long long n_kmp_calloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_kmp_calloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_kmp_calloc = 0;
static void * (*real_kmp_calloc)(size_t,size_t) = nullptr;
extern "C" void * kmp_calloc(size_t nmemb, size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(nmemb*size))
		idx = TINY;
	else if (IS_SMALL(nmemb*size))
		idx = SMALL;
	else if (IS_LARGE(nmemb*size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: kmp_calloc (%ld, %ld) -> idx = %d\n", nmemb, size, idx);

	__sync_fetch_and_add (&n_kmp_calloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_kmp_calloc_sizes[idx], nmemb*size);

	if (nmemb*size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_kmp_calloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_kmp_calloc == nullptr))
		real_kmp_calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "kmp_calloc");

	if (real_kmp_calloc != nullptr)
		return real_kmp_calloc (nmemb, size);
	else
		return nullptr;
}

/*
 * realloc -- resize a block previously allocated by malloc
 */
static unsigned long long n_realloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_realloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_realloc = 0;
static void *(*real_realloc)(void*,size_t) = nullptr;
extern "C" void * realloc(void *ptr, size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: realloc (.., %ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_realloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_realloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	else if (size == 0)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_realloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_realloc == nullptr))
		real_realloc = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "realloc");
	if (real_realloc != nullptr)
		return real_realloc (ptr, size);
	else
		return nullptr;
}

/*
 * kmp_realloc -- resize a block previously allocated by kmp_malloc
 */
static unsigned long long n_kmp_realloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_kmp_realloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_kmp_realloc = 0;
static void *(*real_kmp_realloc)(void*,size_t) = nullptr;
extern "C" void * kmp_realloc(void *ptr, size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: kmp_realloc (.., %ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_kmp_realloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_kmp_realloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	else if (size == 0)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_kmp_realloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_kmp_realloc == nullptr))
		real_kmp_realloc = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "kmp_realloc");

	if (real_kmp_realloc != nullptr)
		return real_kmp_realloc (ptr, size);
	else
		return nullptr;
}

/*
 * free -- free a block previously allocated by malloc
 */
static unsigned long long n_free = 0;
static void (*real_free)(void*) = nullptr;
extern "C" void free(void *ptr)
{
	if (unlikely(Destructed))
		return;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: free (..)\n");

	if (ptr != nullptr)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);

	__sync_fetch_and_add (&n_free, 1);
	if (unlikely(real_free == nullptr))
		real_free = (void (*)(void*)) dlsym (RTLD_NEXT, "free");

	if (real_free != nullptr)
		real_free (ptr);
}

/*
 * kmp_free -- free a block previously allocated by kmp_malloc
 */
static unsigned long long n_kmp_free = 0;
static void (*real_kmp_free)(void*) = nullptr;
extern "C" void kmp_free(void *ptr)
{
	if (unlikely(Destructed))
		return;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: kmp_free (..)\n");

	if (ptr != nullptr)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);

	__sync_fetch_and_add (&n_kmp_free, 1);
	if (unlikely(real_kmp_free == nullptr))
		real_kmp_free = (void (*)(void*)) dlsym (RTLD_NEXT, "kmp_free");

	if (real_kmp_free != nullptr)
		real_kmp_free (ptr);
}

// DELETE operator from C++ -- same code as malloc
// REFERENCE: /usr/src/debug/gcc-7.3.1-6.fc27.x86_64/libstdc++-v3/libsupc++/del_op.cc
// We rely on regular frees
static unsigned long long n_delete = 0;
void operator delete(void* ptr) 
{
	if (unlikely(Destructed))
		return;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: delete (..)\n");

	if (ptr != nullptr)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);

	__sync_fetch_and_add (&n_delete, 1);
	if (unlikely(real_free == nullptr))
		real_free = (void (*)(void*)) dlsym (RTLD_NEXT, "free");

	if (real_free != nullptr)
		real_free (ptr);
}

/*
 * _mm_free -- Free aligned memory that was allocated with _mm_malloc.
 *
 */
static unsigned long long n_mm_free = 0;
static void (*real_mm_free)(void*) = nullptr;
extern "C" void _mm_free(void *ptr)
{
	if (unlikely(Destructed))
		return;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: _mm_free (..)\n");

	if (ptr != nullptr)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);

	__sync_fetch_and_add (&n_mm_free, 1);
	if (unlikely(real_mm_free == nullptr))
		real_mm_free = (void (*)(void*)) dlsym (RTLD_NEXT, "_mm_free");

	if (real_mm_free != nullptr)
		real_mm_free (ptr);
}

/*
 * cfree -- free a block previously allocated by calloc
 *
 * the implementation is identical to free()
 *
 * XXX Not supported on FreeBSD, but we define it anyway
 */
static unsigned long long n_cfree = 0;
static void (*real_cfree)(void*) = nullptr;
extern "C" void cfree(void *ptr)
{
	if (unlikely(Destructed))
		return;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: cfree (..)\n");

	if (ptr != nullptr)
		__sync_fetch_and_add (&estimated_living_data_objects, -1);

	__sync_fetch_and_add (&n_cfree, 1);
	if (unlikely(real_cfree == nullptr))
		real_cfree = (void (*)(void*)) dlsym (RTLD_NEXT, "cfree");

	if (real_cfree != nullptr)
		real_cfree (ptr);
}

/*
 * memalign -- allocate a block of size bytes, starting on an address
 * that is a multiple of boundary
 *
 * XXX Not supported on FreeBSD, but we define it anyway
 */
static unsigned long long n_memalign_sizes[N_SIZES] = { 0, };
static unsigned long long t_memalign_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_memalign = 0;
static void * (*real_memalign)(size_t,size_t) = nullptr;
extern "C" void * memalign(size_t boundary, size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: memalign (.., %ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_memalign_sizes[idx], 1);
	__sync_fetch_and_add (&t_memalign_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_memalign, t - previous_time);
	previous_time = t;

	if (unlikely(real_memalign == nullptr))
		real_memalign = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "memalign");

	if (real_memalign != nullptr)
		return real_memalign (boundary, size);
	else
		return nullptr;
}

/*
 * aligned_alloc -- allocate a block of size bytes, starting on an address
 * that is a multiple of alignment
 *
 * size must be a multiple of alignment
 */
static unsigned long long n_aligned_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_aligned_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_aligned_malloc = 0;
static void * (*real_aligned_malloc)(size_t,size_t) = nullptr;
extern "C" void * aligned_alloc(size_t alignment, size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: aligned_malloc (.., %ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_aligned_malloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_aligned_malloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_aligned_malloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_aligned_malloc == nullptr))
		real_aligned_malloc = (void *(*)(size_t,size_t)) dlsym (RTLD_NEXT, "aligned_malloc");

	if (real_aligned_malloc != nullptr)
		return real_aligned_malloc (alignment, size);
	else
		return nullptr;
}

/*
 * kmp_aligned_alloc -- allocate a block of size bytes, starting on an address
 * that is a multiple of alignment
 *
 * size must be a multiple of alignment
 */
static unsigned long long n_kmp_aligned_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_kmp_aligned_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_kmp_aligned_malloc = 0;
static void * (*real_kmp_aligned_malloc)(size_t,size_t) = nullptr;
extern "C" void * kmp_aligned_malloc(size_t size, size_t alignment)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: kmp_aligned_malloc (%ld, ..) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_kmp_aligned_malloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_kmp_aligned_malloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_kmp_aligned_malloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_kmp_aligned_malloc == nullptr))
		real_kmp_aligned_malloc = (void *(*)(size_t,size_t)) dlsym (RTLD_NEXT, "kmp_aligned_malloc");

	if (real_kmp_aligned_malloc != nullptr)
		return real_kmp_aligned_malloc (size, alignment);
	else
		return nullptr;
}

/*
 * posix_memalign -- allocate a block of size bytes, starting on an address
 * that is a multiple of alignment
 */
static unsigned long long n_posix_memalign_sizes[N_SIZES] = { 0, };
static unsigned long long t_posix_memalign_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_posix_memalign = 0;
static int (*real_posix_memalign)(void**,size_t,size_t) = nullptr;
extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	if (unlikely(Destructed))
		return ENOMEM;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: posix_memalign (.., %ld, ..) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_posix_memalign_sizes[idx], 1);
	__sync_fetch_and_add (&t_posix_memalign_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_posix_memalign, t - previous_time);
	previous_time = t;

	if (unlikely(real_posix_memalign == nullptr))
		real_posix_memalign = (int (*)(void**,size_t,size_t)) dlsym (RTLD_NEXT, "posix_memalign");

	if (real_posix_memalign != nullptr)
		return real_posix_memalign (memptr, alignment, size);
	else
		return ENOMEM;
}

/*
 * valloc -- allocate a block of size bytes, starting on a page boundary
 */
static unsigned long long n_valloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_valloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_valloc = 0;
static void * (*real_valloc)(size_t) = nullptr;
extern "C" void * valloc(size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: valloc (%ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_valloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_valloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_valloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_valloc == nullptr))
		real_valloc = (void*(*)(size_t)) dlsym (RTLD_NEXT, "valloc");

	if (real_valloc != nullptr)
		return real_valloc (size);
	else
		return nullptr;
}

/*
 * pvalloc -- allocate a block of size bytes, starting on a page boundary
 *
 * Requested size is also aligned to page boundary.
 *
 * XXX Not supported on FreeBSD, but we define it anyway.
 */
static unsigned long long n_pvalloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_pvalloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_pvalloc = 0;
static void * (*real_pvalloc)(size_t) = nullptr;
extern "C" void * pvalloc(size_t size)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: pvalloc (%ld) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_pvalloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_pvalloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_pvalloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_pvalloc == nullptr))
		real_pvalloc = (void*(*)(size_t)) dlsym (RTLD_NEXT, "pvalloc");

	if (real_pvalloc != nullptr)
		return real_pvalloc (size);
	else
		return nullptr;
}

/*
 * _mm_malloc --
 * Allocate size bytes of memory, aligned to the alignment specified in align,
 * and return a pointer to the allocated memory. _mm_free should be used to free
 * memory that is allocated with _mm_malloc.
 *
 * See https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_malloc
 */
static unsigned long long n_mm_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long t_mm_malloc_sizes[N_SIZES] = { 0, };
static unsigned long long total_interval_mm_malloc = 0;
static void * (*real_mm_malloc)(size_t,size_t) = nullptr;
extern "C" void * _mm_malloc(size_t size, size_t align)
{
	if (unlikely(Destructed))
		return nullptr;

	int idx = HUGE;
	if (IS_TINY(size))
		idx = TINY;
	else if (IS_SMALL(size))
		idx = SMALL;
	else if (IS_LARGE(size))
		idx = LARGE;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: _mm_malloc (%ld,..) -> idx = %d\n", size, idx);

	__sync_fetch_and_add (&n_mm_malloc_sizes[idx], 1);
	__sync_fetch_and_add (&t_mm_malloc_sizes[idx], size);

	if (size > 0)
		__sync_fetch_and_add (&estimated_living_data_objects, 1);
	max_estimated_living_data_objects = MAX(estimated_living_data_objects, max_estimated_living_data_objects);

	static unsigned long long previous_time = 0;
	unsigned long long t = get_clock ();
	if (previous_time)
		__sync_fetch_and_add (&total_interval_mm_malloc, t - previous_time);
	previous_time = t;

	if (unlikely(real_mm_malloc == nullptr))
		real_mm_malloc = (void*(*)(size_t,size_t)) dlsym (RTLD_NEXT, "_mm_malloc");

	if (real_mm_malloc != nullptr)
		return real_mm_malloc (size, align);
	else
		return nullptr;
}

/*
 * malloc_usable_size -- get usable size of allocation
 */
static unsigned long long n_malloc_usable_size = 0;
static size_t (*real_malloc_usable_size)(void*) = nullptr;
extern "C" size_t malloc_usable_size(void *ptr)
{
	if (unlikely(Destructed))
		return 0;

	if (debug)
		fprintf (stderr, "[LIBCOUNTER]: malloc_usable_size(..)\n");

	__sync_fetch_and_add (&n_malloc_usable_size, 1);
	if (unlikely(real_malloc_usable_size == nullptr))
		real_malloc_usable_size = (size_t(*)(void*)) dlsym (RTLD_NEXT, "malloc_usable_size");

	if (real_malloc_usable_size != nullptr)
		return real_malloc_usable_size (ptr);
	else
		return 0;
}


static int exited = false;
__attribute__((destructor(101))) static void counter_library_finalize (void)
{
	if (exited)
		return;

	unsigned long long end_clock = get_clock();
	unsigned long long total_time_s = (end_clock - initial_clock)/(1000ULL*1000ULL*1000ULL);

	unsigned long long n_malloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_malloc += n_malloc_sizes[i];
	if (n_malloc)
		fprintf (stderr,
          "LIBCOUNTER:                                          [  <64b  |  <4Kb  |  <2Mb  |  >=2Mb ]\n"
		  "LIBCOUNTER: Number of malloc calls:                  [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for malloc (Mb)              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of mallocs per second: %0.3lf, Avg Interval between malloc calls: %0.3lf ms\n",
		  n_malloc_sizes[TINY], n_malloc_sizes[SMALL], n_malloc_sizes[LARGE], n_malloc_sizes[HUGE],
		  t_malloc_sizes[TINY]>>20, t_malloc_sizes[SMALL]>>20, t_malloc_sizes[LARGE]>>20, t_malloc_sizes[HUGE]>>20,
		  ((float) n_malloc / (float) total_time_s),
		  ((float) total_interval_malloc / (float) (n_malloc * 1000000)) );
	unsigned long long n_kmp_malloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_kmp_malloc += n_kmp_malloc_sizes[i];
	if (n_kmp_malloc)
		fprintf (stderr,
          "                                                     [  <64b  |  <4Kb  |  <2Mb  |  >=2Mb ]\n"
		  "LIBCOUNTER: Number of kmp_malloc calls:              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for kmp_malloc (Mb)          [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of kmp_mallocs per second: %0.3lf, Avg Interval between kmp_malloc calls: %0.3lf ms\n",
		  n_kmp_malloc_sizes[TINY], n_kmp_malloc_sizes[SMALL], n_kmp_malloc_sizes[LARGE], n_kmp_malloc_sizes[HUGE],
		  t_kmp_malloc_sizes[TINY]>>20, t_kmp_malloc_sizes[SMALL]>>20, t_kmp_malloc_sizes[LARGE]>>20, t_kmp_malloc_sizes[HUGE]>>20,
		  ((float) n_kmp_malloc / (float) total_time_s),
		  ((float) total_interval_kmp_malloc / (float) (n_kmp_malloc * 1000000)) );
	unsigned long long n_new = 0;
	for (int i = 0; i < N_SIZES; ++i) n_new += n_new_sizes[i];
	if (n_new)
		fprintf (stderr,
		  "LIBCOUNTER: Number of new calls:                     [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for new (Mb)                 [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of new per second: %0.3lf, Avg Interval between new calls: %0.3lf ms\n",
		  n_new_sizes[TINY], n_new_sizes[SMALL], n_new_sizes[LARGE], n_new_sizes[HUGE],
		  t_new_sizes[TINY]>>20, t_new_sizes[SMALL]>>20, t_new_sizes[LARGE]>>20, t_new_sizes[HUGE]>>20,
		  ((float) n_new / (float) total_time_s),
		  ((float) total_interval_new / (float) (n_new * 1000000)) );
	unsigned long long n_calloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_calloc += n_calloc_sizes[i];
	if (n_calloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of calloc calls:                  [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for calloc (Mb)              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of calloc per second: %0.3lf, Avg Interval between calloc calls: %0.3lf ms\n",
		  n_calloc_sizes[TINY], n_calloc_sizes[SMALL], n_calloc_sizes[LARGE], n_calloc_sizes[HUGE],
		  t_calloc_sizes[TINY]>>20, t_calloc_sizes[SMALL]>>20, t_calloc_sizes[LARGE]>>20, t_calloc_sizes[HUGE]>>20,
		  ((float) n_calloc / (float) total_time_s),
		  ((float) total_interval_calloc / (float) (n_calloc * 1000000)) );
	unsigned long long n_kmp_calloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_kmp_calloc += n_kmp_calloc_sizes[i];
	if (n_kmp_calloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of kmp_calloc calls:              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for kmp_calloc (Mb)          [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of kmp_calloc per second: %0.3lf, Avg Interval between kmp_calloc calls: %0.3lf ms\n",
		  n_kmp_calloc_sizes[TINY], n_kmp_calloc_sizes[SMALL], n_kmp_calloc_sizes[LARGE], n_kmp_calloc_sizes[HUGE],
		  t_kmp_calloc_sizes[TINY]>>20, t_kmp_calloc_sizes[SMALL]>>20, t_kmp_calloc_sizes[LARGE]>>20, t_kmp_calloc_sizes[HUGE]>>20,
		  ((float) n_kmp_calloc / (float) total_time_s),
		  ((float) total_interval_kmp_calloc / (float) (n_kmp_calloc * 1000000)) );
	unsigned long long n_realloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_realloc += n_realloc_sizes[i];
	if (n_realloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of realloc calls:                 [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for realloc (Mb)             [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER: Number of realloc per second: %0.3lf, Avg Interval between realloc calls: %0.3lf ms\n",
		  n_realloc_sizes[TINY], n_realloc_sizes[SMALL], n_realloc_sizes[LARGE], n_realloc_sizes[HUGE],
		  t_realloc_sizes[TINY]>>20, t_realloc_sizes[SMALL]>>20, t_realloc_sizes[LARGE]>>20, t_realloc_sizes[HUGE]>>20,
		  ((float) n_realloc / (float) total_time_s),
		  ((float) total_interval_realloc / (float) (n_realloc * 1000000)) );
	unsigned long long n_kmp_realloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_kmp_realloc += n_kmp_realloc_sizes[i];
	if (n_kmp_realloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of kmp_realloc calls:             [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for kmp_realloc (Mb)         [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER: Number of kmp_realloc per second: %0.3lf, Avg Interval between kmp_realloc calls: %0.3lf ms\n",
		  n_kmp_realloc_sizes[TINY], n_kmp_realloc_sizes[SMALL], n_kmp_realloc_sizes[LARGE], n_kmp_realloc_sizes[HUGE],
		  t_kmp_realloc_sizes[TINY]>>20, t_kmp_realloc_sizes[SMALL]>>20, t_kmp_realloc_sizes[LARGE]>>20, t_kmp_realloc_sizes[HUGE]>>20,
		  ((float) n_kmp_realloc / (float) total_time_s),
		  ((float) total_interval_kmp_realloc / (float) (n_kmp_realloc * 1000000)) );
	unsigned long long n_memalign = 0;
	for (int i = 0; i < N_SIZES; ++i) n_memalign += n_memalign_sizes[i];
	if (n_memalign)
		fprintf (stderr,
		  "LIBCOUNTER: Number of memalign calls:                [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for memalign (Mb)            [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of memalign per second: %0.3lf, Avg Interval between memalign calls: %0.3lf ms\n",
		  n_memalign_sizes[TINY], n_memalign_sizes[SMALL], n_memalign_sizes[LARGE], n_memalign_sizes[HUGE],
		  t_memalign_sizes[TINY]>>20, t_memalign_sizes[SMALL]>>20, t_memalign_sizes[LARGE]>>20, t_memalign_sizes[HUGE]>>20,
		  ((float) n_memalign / (float) total_time_s),
		  ((float) total_interval_memalign / (float) (n_memalign * 1000000)) );
	unsigned long long n_aligned_malloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_aligned_malloc += n_aligned_malloc_sizes[i];
	if (n_aligned_malloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of aligned_malloc calls:          [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for aligned_malloc (Mb)      [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of aligned_malloc per second: %0.3lf, Avg Interval between aligned_malloc calls: %0.3lf ms\n",
		  n_aligned_malloc_sizes[TINY], n_aligned_malloc_sizes[SMALL], n_aligned_malloc_sizes[LARGE], n_aligned_malloc_sizes[HUGE],
		  t_aligned_malloc_sizes[TINY]>>20, t_aligned_malloc_sizes[SMALL]>>20, t_aligned_malloc_sizes[LARGE]>>20, t_aligned_malloc_sizes[HUGE]>>20,
		  ((float) n_aligned_malloc / (float) total_time_s),
		  ((float) total_interval_aligned_malloc / (float) (n_aligned_malloc * 1000000)) );
	unsigned long long n_kmp_aligned_malloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_kmp_aligned_malloc += n_kmp_aligned_malloc_sizes[i];
	if (n_kmp_aligned_malloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of kmp_aligned_malloc calls:      [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for kmp_aligned_malloc (Mb)  [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of kmp_aligned_malloc per second: %0.3lf, Avg Interval between kmp_aligned_malloc calls: %0.3lf ms\n",
		  n_kmp_aligned_malloc_sizes[TINY], n_kmp_aligned_malloc_sizes[SMALL], n_kmp_aligned_malloc_sizes[LARGE], n_kmp_aligned_malloc_sizes[HUGE],
		  t_kmp_aligned_malloc_sizes[TINY]>>20, t_kmp_aligned_malloc_sizes[SMALL]>>20, t_kmp_aligned_malloc_sizes[LARGE]>>20, t_kmp_aligned_malloc_sizes[HUGE]>>20,
		  ((float) n_kmp_aligned_malloc / (float) total_time_s),
		  ((float) total_interval_kmp_aligned_malloc / (float) (n_kmp_aligned_malloc * 1000000)) );
	unsigned long long n_posix_memalign = 0;
	for (int i = 0; i < N_SIZES; ++i) n_posix_memalign += n_posix_memalign_sizes[i];
	if (n_posix_memalign)
		fprintf (stderr,
		  "LIBCOUNTER: Number of posix_memalign calls:          [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for posix_memalign (Mb)      [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of posix_memalign per second: %0.3lf, Avg Interval between posix_memalign calls: %0.3lf ms\n",
		  n_posix_memalign_sizes[TINY], n_posix_memalign_sizes[SMALL], n_posix_memalign_sizes[LARGE], n_posix_memalign_sizes[HUGE],
		  t_posix_memalign_sizes[TINY]>>20, t_posix_memalign_sizes[SMALL]>>20, t_posix_memalign_sizes[LARGE]>>20, t_posix_memalign_sizes[HUGE]>>20,
		  ((float) n_posix_memalign / (float) total_time_s),
		  ((float) total_interval_posix_memalign / (float) (n_posix_memalign * 1000000)) );
	unsigned long long n_valloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_valloc += n_valloc_sizes[i];
	if (n_valloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of valloc calls:                  [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for valloc (Mb)              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of valloc per second: %0.3lf, Avg Interval between valloc calls: %0.3lf ms\n",
		  n_valloc_sizes[TINY], n_valloc_sizes[SMALL], n_valloc_sizes[LARGE], n_valloc_sizes[HUGE],
		  t_valloc_sizes[TINY]>>20, t_valloc_sizes[SMALL]>>20, t_valloc_sizes[LARGE]>>20, t_valloc_sizes[HUGE]>>20,
		  ((float) n_valloc / (float) total_time_s),
		  ((float) total_interval_valloc / (float) (n_valloc * 1000000)) );
	unsigned long long n_pvalloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_pvalloc += n_pvalloc_sizes[i];
	if (n_pvalloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of pvalloc calls:                 [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for pvalloc (Mb)             [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of pvalloc per second: %0.3lf, Avg Interval between pvalloc calls: %0.3lf ms\n",
		  n_pvalloc_sizes[TINY], n_pvalloc_sizes[SMALL], n_pvalloc_sizes[LARGE], n_pvalloc_sizes[HUGE],
		  t_pvalloc_sizes[TINY]>>20, t_pvalloc_sizes[SMALL]>>20, t_pvalloc_sizes[LARGE]>>20, t_pvalloc_sizes[HUGE]>>20,
		  ((float) n_pvalloc / (float) total_time_s),
		  ((float) total_interval_pvalloc / (float) (n_pvalloc * 1000000)) );
	unsigned long long n_mm_malloc = 0;
	for (int i = 0; i < N_SIZES; ++i) n_mm_malloc += n_mm_malloc_sizes[i];
	if (n_mm_malloc)
		fprintf (stderr,
		  "LIBCOUNTER: Number of _mm_malloc calls:              [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Aggr. size for _mm_malloc (Mb)          [%8llu|%8llu|%8llu|%8llu]\n"
		  "LIBCOUNTER:  Number of _mm_malloc per second: %0.3lf, Avg Interval between _mm_malloc calls: %0.3lf ms\n",
		  n_mm_malloc_sizes[TINY], n_mm_malloc_sizes[SMALL], n_mm_malloc_sizes[LARGE], n_mm_malloc_sizes[HUGE],
		  t_mm_malloc_sizes[TINY]>>20, t_mm_malloc_sizes[SMALL]>>20, t_mm_malloc_sizes[LARGE]>>20, t_mm_malloc_sizes[HUGE]>>20,
		  ((float) n_mm_malloc / (float) total_time_s),
		  ((float) total_interval_mm_malloc / (float) (n_mm_malloc * 1000000)) );

	unsigned long long n_total_alloc = n_mm_malloc + n_pvalloc +
	  n_valloc + n_posix_memalign + n_aligned_malloc + n_kmp_aligned_malloc + n_memalign +
	  n_calloc + n_kmp_calloc + n_new + n_kmp_calloc + n_malloc; // Intentionally ignore n_realloc
	fprintf (stderr,
	  "\nLIBCOUNTER: Number of free calls: %llu\n"
	  "LIBCOUNTER: Number of kmp_free calls: %llu\n"
	  "LIBCOUNTER: Number of delete calls: %llu\n"
	  "LIBCOUNTER: Number of cfree calls: %llu\n"
	  "LIBCOUNTER: Number of _mm_free calls: %llu\n"
	  "LIBCOUNTER: Number of malloc_usable_size calls: %llu\n"
	  "LIBCOUNTER: Estimated number of maximum living data-objects: %lld (ratio %0.3lf)\n"
	  "LIBCOUNTER: Total execution time: %llu seconds\n",
	  n_free, n_kmp_free, n_delete, n_cfree, n_mm_free, n_malloc_usable_size,
	  max_estimated_living_data_objects,
	  ((float) max_estimated_living_data_objects / (float) n_total_alloc),
	  total_time_s);

	fprintf (stderr, "LIBCOUNTER: Dump of /proc/self/status:\n");
	char buf[1024+1];
	int fd = open ("/proc/self/status", O_RDONLY);
	lseek (fd, 0, SEEK_SET);
	assert (fd >= 0);
	ssize_t r = read (fd, buf, 1024);
	while ( r > 0 && r < 1024)
	{
		write (2, buf, r);
		r = read (fd, buf, 1024);
	}
	close (fd);
	fprintf (stderr, "LIBCOUNTER: End of dump.\n");

	exited = true;
}

static void _sigint_handler (int)
{
	counter_library_finalize();
	_exit (0);
}

__attribute__((constructor(101))) static void counter_library_initialize (void)
{
	fprintf (stderr, "LIBCOUNTER: Starting...\n");

	signal(SIGINT, _sigint_handler);
	atexit (counter_library_finalize);

	real_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "malloc");
	real_kmp_malloc = (void* (*)(size_t)) dlsym (RTLD_NEXT, "kmp_malloc");
	real_calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "calloc");
	real_kmp_calloc = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "kmp_calloc");
	real_realloc = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "realloc");
	real_kmp_realloc = (void* (*)(void*,size_t)) dlsym (RTLD_NEXT, "kmp_realloc");
	real_free = (void (*)(void*)) dlsym (RTLD_NEXT, "free");
	real_kmp_free = (void (*)(void*)) dlsym (RTLD_NEXT, "kmp_free");
	real_cfree = (void (*)(void*)) dlsym (RTLD_NEXT, "cfree");
	real_mm_free = (void (*)(void*)) dlsym (RTLD_NEXT, "_mm_free");
	real_memalign = (void* (*)(size_t,size_t)) dlsym (RTLD_NEXT, "memalign");
	real_aligned_malloc = (void *(*)(size_t,size_t)) dlsym (RTLD_NEXT, "aligned_malloc");
	real_kmp_aligned_malloc = (void *(*)(size_t,size_t)) dlsym (RTLD_NEXT, "kmp_aligned_malloc");
	real_posix_memalign = (int (*)(void**,size_t,size_t)) dlsym (RTLD_NEXT, "posix_memalign");
	real_valloc = (void*(*)(size_t)) dlsym (RTLD_NEXT, "valloc");
	real_pvalloc = (void*(*)(size_t)) dlsym (RTLD_NEXT, "pvalloc");
	real_mm_malloc = (void*(*)(size_t,size_t)) dlsym (RTLD_NEXT, "_mm_malloc");
	real_malloc_usable_size = (size_t(*)(void*)) dlsym (RTLD_NEXT, "malloc_usable_size");

	char * s_debug = getenv ("LIBCOUNTER_DEBUG");
	if (s_debug)
		debug =
		    (strncasecmp (s_debug, "1", strlen("1")) == 0 ||
		    strncasecmp (s_debug, "enabled", strlen("enabled")) == 0 ||
		    strncasecmp (s_debug, "yes", strlen("yes")) == 0);

	initial_clock = get_clock();
}

