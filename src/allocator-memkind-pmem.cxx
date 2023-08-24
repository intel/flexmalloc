// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// Author: Clement Foyer <clement.foyer@univ-reims.fr>
// Date: Aug 24, 2023
// License: To determine

#include <stdlib.h>
#include <sched.h>
#include <assert.h>
#include <string.h>
#include <numa.h>
#include <dirent.h>
#include <errno.h>
#include <new>

#include "flex-malloc.hxx"

#include "common.hxx"
#include "allocator-memkind-pmem.hxx"

#define ALLOCATOR_NAME "memkind/pmem"

AllocatorMemkindPMEM::AllocatorMemkindPMEM (allocation_functions_t &af)
  : Allocator (af)
{
	if (numa_available() == -1)
	{
		VERBOSE_MSG(0, "Error! NUMA is not supported in this machine!\n");
		exit (-1);
	}
	_num_NUMA_nodes = numa_num_configured_nodes();
	int _num_CPUs = numa_num_configured_cpus();

	_cpu_2_NUMA = (short*) _af.malloc (sizeof(short)*_num_CPUs);
	assert (_cpu_2_NUMA != nullptr);
	for (int c = 0; c < _num_CPUs; ++c)
		_cpu_2_NUMA[c] = numa_node_of_cpu(c);

	_kind = (memkind_t*) _af.malloc (sizeof(memkind_t)*_num_NUMA_nodes);
	assert (_kind != nullptr);

	_stats = (AllocatorStatistics*) _af.malloc (_num_NUMA_nodes * sizeof(AllocatorStatistics));
	assert (_stats != nullptr);
	new (_stats) AllocatorStatistics[_num_NUMA_nodes];
}

AllocatorMemkindPMEM::~AllocatorMemkindPMEM ()
{
	_af.free (_stats);
	_af.free (_kind);
	_af.free (_cpu_2_NUMA);
}

void * AllocatorMemkindPMEM::malloc (size_t size)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	assert (0 <= n && n < _num_NUMA_nodes);

	DBG("Running on CPU %d - NUMA node %ld\n", cpu, n);

	// Forward memory request to real malloc and reserve some space for the header
	void * baseptr = memkind_malloc (_kind[n], Allocator::getTotalSize (size));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeader (baseptr, this, size);
		Allocator::pmemNode (res, n);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr & base at %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), name(), this);
		_stats[n].record_malloc (size);
	}

	return res;
}

void * AllocatorMemkindPMEM::calloc (size_t nmemb, size_t size)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	assert (0 <= n && n < _num_NUMA_nodes);

	DBG("Running on CPU %d - NUMA node %ld\n", cpu, n);

	// Forward memory request to real malloc and request additional space to store
	// the allocator and the basepointer
	void * baseptr = memkind_malloc (_kind[n], Allocator::getTotalSize (nmemb * size));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeader (baseptr, this, nmemb * size);
		Allocator::pmemNode (res, n);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr & base %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), name(), this);
		_stats[n].record_calloc (nmemb * size);
	}

	return res;
}

int AllocatorMemkindPMEM::posix_memalign (void **ptr, size_t align, size_t size)
{
	assert (ptr != nullptr);

	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	assert (0 <= n && n < _num_NUMA_nodes);

	DBG("Running on CPU %d - NUMA node %ld\n", cpu, n);

	// Forward memory request to real malloc and request additional space to
	// store the allocator and the basepointer
	void * baseptr = memkind_malloc (_kind[n], Allocator::getTotalSize (size + align));
	void * res = nullptr;

	// If malloc succeded, then forge a header and the pointer points to the 
	// data space after the header
	if (baseptr)
	{
		res = Allocator::generateAllocatorHeaderOnAligned (baseptr, align, this, size);
		Allocator::pmemNode (res, n);

		// Verbosity and emit statistics
		VERBOSE_MSG(3, ALLOCATOR_NAME": Allocated %lu bytes in %p (hdr %p, base %p) w/ allocator %s (%p)\n", size, res, Allocator::getAllocatorHeader (res), baseptr, name(), this);
		_stats[n].record_aligned_malloc (size + align);

		*ptr = res;
		return 0;
	}
	else
		return ENOMEM;
}

void AllocatorMemkindPMEM::free (void *ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);

	// When freeing the memory, need to free the base pointe
	VERBOSE_MSG(3, ALLOCATOR_NAME": Freeing up pointer %p (hdr %p) w/ size - %lu (base pointer located in %p)\n", ptr, hdr, hdr->size, hdr->base_ptr);
	
	// Recover memkind kind from AUX field and pass base pointer
	bool gotnode;
	int n = Allocator::pmemNode (ptr, gotnode);
	assert (gotnode);
	assert (0 <= n && n < _num_NUMA_nodes);

	_stats[n].record_free (hdr->size);
	memkind_free (_kind[n], hdr->base_ptr);
}

void * AllocatorMemkindPMEM::realloc (void *ptr, size_t size)
{
	// If previous pointer is not null, behave normally. otherwise, behave like a malloc but
	// without calling information
	if (ptr)
	{
		// Search for previous allocation size through the header
		Allocator::Header_t *prev_hdr = Allocator::getAllocatorHeader (ptr);
		size_t prev_size = prev_hdr->size;
		void * prev_baseptr = prev_hdr->base_ptr;
		// Recover memkind kind from AUX field and pass base pointer
		bool gotnode;
		int n = Allocator::pmemNode (ptr, gotnode);
		assert (gotnode);
		assert (0 <= n && n < _num_NUMA_nodes);
		uintptr_t extra_size = Allocator::getExtraSize (prev_hdr);

		if (prev_size < size)
		{
			// Reallocate, from base pointer to fit the new size plus a new header
			void *new_baseptr = memkind_realloc (_kind[n], prev_baseptr, Allocator::getTotalSize (size + extra_size));
			void *res = nullptr;

			// If realloc was successful, it also copied the data from previous allocation.
			// We need to update the header.
			if (new_baseptr)
			{
				res = Allocator::generateAllocatorHeader (new_baseptr, extra_size, this, size);
				Allocator::pmemNode (res, n);

				DBG("Reallocated (%ld->%ld [extra bytes = %lu]) from %p (base at %p, header at %p) into %p (base at %p, header at %p) w/ allocator %s (%p) on node %d\n", prev_size, size, extra_size, ptr, prev_baseptr, prev_hdr, res, new_baseptr, Allocator::getAllocatorHeader (res), name(), this, n);
			}

			_stats[n].record_realloc (size, prev_size);

			return res;
		}
		else
		{
			DBG("Reallocated (%ld->%ld) from %p but not touching as new size is smaller w/ allocator %s (%p)\n", prev_size, size, ptr, name(), this);
			return ptr;
		}
	}
	else
	{
		VERBOSE_MSG(3, ALLOCATOR_NAME": realloc (NULL, ...) forwarded to malloc\n");
		int cpu = sched_getcpu();
		long n = _cpu_2_NUMA[cpu];
		_stats[n].record_realloc_forward_malloc();

		return this->malloc (size);
	}
}

size_t AllocatorMemkindPMEM::malloc_usable_size (void * ptr)
{
	Allocator::Header_t *hdr = Allocator::getAllocatorHeader (ptr);

	// When checking for the usable size, return the size we requested originally, no matter
	// what the underlying library did. This may alter execution behaviors, though.
	VERBOSE_MSG(3, ALLOCATOR_NAME": Checking usable size on pointer %p w/ size - %lu (but base pointer located in %p)\n",
	  ptr, hdr->size, hdr->base_ptr);

	return hdr->size;
}

void AllocatorMemkindPMEM::configure (const char *config)
{
	int nnodes = 0;

#warning This casting is necessary for gcc 7.3.1/fedora 27
	for (char *tmp = strchr ((char*)config, '@');
			tmp != nullptr && nnodes < _num_NUMA_nodes;
			tmp = strchr (tmp+1, '@'))
	{
		bool has_path = false;
		char path[PATH_MAX] = {0};
		{
			// Copy into path the given path for PMEM -- skip every empty char after @
			constexpr char blankchars[] = " \n\r\t\f\v";
			size_t count = strspn(tmp, blankchars); // skip blank chars
			const char* in = &tmp[count];
			count = strcspn(in, blankchars); // count valid characters
			if (count == 0 || *in == '\0')
			{
				VERBOSE_MSG(0, "Error! pmem configuration line poorly formatted: nothing found after '@'.\n");
				exit (-1);
			}
			const size_t path_len = std::min(count, sizeof(path)-1);
			memcpy(path, in, path_len);
			path[path_len] = '\0';

			// If we have read something, then we have a path (not necessarily absolute)
			// Also check for untruncated path
			has_path = (count > 0 && path_len == count);
		}

		if (has_path)
		{
			DBG("Checking for directory %s for PMEM allocator.\n", path);
			struct stat statbuf;
			if (stat(path, &statbuf) == 0)
			{
				if (! S_ISDIR(statbuf.st_mode) )
				{
					VERBOSE_MSG(0, "Error! Given path (%s) is not a directory.\n", path);
					exit (-1);
				}
			}
			else
			{
				VERBOSE_MSG(0, "Error! Cannot stat path (%s). Does it exist?\n", path);
				exit (-1);
			}
			DIR *dir = opendir (path);
			if (dir != nullptr)
			{
				closedir (dir);
			}
			else if (ENOENT == errno)
			{
				VERBOSE_MSG(0, "Error! Given path (%s) for PMEM allocator is not found.\n", path);
				exit (-1);
			}
			else
			{
				VERBOSE_MSG(0, "Error! An unknown error when opening given path (%s) for PMEM allocator.\n", path);
				exit (-1);
			}

			if (nnodes >= _num_NUMA_nodes)
			{
				VERBOSE_MSG(0, "Error! Number of given PMEM nodes is larger than the number of NUMA nodes (%d).\n", _num_NUMA_nodes);
				exit (-1);
			}

			int err = memkind_create_pmem (path, 0, &_kind[nnodes]);
			if (err)
			{
				VERBOSE_MSG(0, "Error! An error ocurred during memkind_create_pmem invocation for path %s.\n", path);
				char msg[1024];
				memkind_error_message(err, msg, sizeof(msg));
				VERBOSE_MSG_NOPREFIX(0, "%s", msg);
				exit (-1);
			}
			else
				VERBOSE_MSG(0, "* Successfully created PMEM on top of %s for NUMA node %d.\n", path, nnodes);

			nnodes++;
		}
	}

	if (nnodes != _num_NUMA_nodes)
	{
		VERBOSE_MSG(0, "Error! Incorrect number of PMEM nodes for PMEM allocator (%d). Should be equal to number of NUMA nodes (%d).\n", nnodes, _num_NUMA_nodes);
		exit (-1);
	}

	_is_ready = true;
}

const char * AllocatorMemkindPMEM::name (void) const
{
	return ALLOCATOR_NAME;
}

const char * AllocatorMemkindPMEM::description (void) const
{
	return "Allocator based on pmem on top of memkind";
}

void AllocatorMemkindPMEM::show_statistics (void) const
{
	char node[32];

	for (long n = 0; n < _num_NUMA_nodes; ++n)
	{
		memset (node, 0, sizeof(node));
		snprintf (node, sizeof(node), "node%ld", n);
		_stats[n].show_statistics (ALLOCATOR_NAME, true, node);
	}
}

bool AllocatorMemkindPMEM::fits (size_t) const
{
	return true;
}

size_t AllocatorMemkindPMEM::hwm (void) const
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	return _stats[n].water_mark ();
}

void AllocatorMemkindPMEM::record_unfitted_malloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_unfitted_malloc (s);
}

void AllocatorMemkindPMEM::record_unfitted_calloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_unfitted_calloc (s);
}

void AllocatorMemkindPMEM::record_unfitted_aligned_malloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_unfitted_aligned_malloc (s);
}

void AllocatorMemkindPMEM::record_unfitted_realloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_unfitted_realloc (s);
}

void AllocatorMemkindPMEM::record_source_realloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_source_realloc (s);
}

void AllocatorMemkindPMEM::record_target_realloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_target_realloc (s);
}

void AllocatorMemkindPMEM::record_self_realloc (size_t s)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_self_realloc (s);
}

void AllocatorMemkindPMEM::record_realloc_forward_malloc (void)
{
	int cpu = sched_getcpu();
	long n = _cpu_2_NUMA[cpu];
	_stats[n].record_realloc_forward_malloc ();
}

