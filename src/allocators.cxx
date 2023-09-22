// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// Author: Clement Foyer <clement.foyer@univ-reims.fr>
// Date: Aug 24, 2023
// License: To determine

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <new>
#include <algorithm>

#include "common.hxx"

#include "allocators.hxx"
#include "allocator-posix.hxx"
#if defined(MEMKIND_SUPPORTED)
# include "allocator-memkind-hbwmalloc.hxx"
# include "allocator-memkind-pmem.hxx"
#endif

Allocators::Allocators (allocation_functions_t &af, const char *definitions)
{
	unsigned indx = 0;
#if defined(MEMKIND_SUPPORTED)
	void *a_memkind_hbwmalloc = (AllocatorMemkindHBWMalloc*) malloc (sizeof(AllocatorMemkindHBWMalloc));
	void *a_memkind_pmem = (AllocatorMemkindPMEM*) malloc (sizeof(AllocatorMemkindPMEM));
	allocators[indx++] = new (a_memkind_hbwmalloc) AllocatorMemkindHBWMalloc(af);
	allocators[indx++] = new (a_memkind_pmem) AllocatorMemkindPMEM(af);
#endif
	void *a_posix = (AllocatorPOSIX*) malloc (sizeof(AllocatorPOSIX));
	allocators[indx++] = new (a_posix) AllocatorPOSIX(af);
	allocators[indx]   = nullptr;

	// Objects have been already initialized when constructing (allocating them) -- just use
	// this function to list them
	unsigned u = 0;
	VERBOSE_MSG(1, "Initializing allocators:\n");

	while (allocators[u] != nullptr)
	{
		VERBOSE_MSG(1, "* %s\n", allocators[u]->name());
		u++;
	}

	struct stat sb;
	int fd;

	VERBOSE_MSG(0, "Parsing %s\n", definitions);

	fd = open(definitions, O_RDONLY);
	if (fd == -1)
	{
		VERBOSE_MSG(0, "Warning! Could not open file %s\n", definitions);
		perror("open");
		return;
	}
	if (fstat(fd, &sb) == -1)
	{
		VERBOSE_MSG(0, "Warning! fstat failed on file %s\n", definitions);
		perror("fstat");
		close (fd);
		return;
	}
	if (!S_ISREG(sb.st_mode))
	{
		VERBOSE_MSG(0, "Warning! Given file (%s) is not a regular file\n", definitions);
		close (fd);
		return;
	}
	if (sb.st_size == 0)
	{
		VERBOSE_MSG(0, "Warning! Given file (%s) is empty\n", definitions);
		close (fd);
		return;
	}
	void *p = mmap (nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED)
	{
		VERBOSE_MSG(0, "Warning! Could not mmap file %s\n", definitions);
		perror("mmap");
		close (fd);
		return;
	}
	close(fd);

	// Process memory definitions
	char *defs = strchr ((char*)p, '#');
	while (defs != nullptr)
	{
		char allocatorname[256] = {0};
		const char blankchars[] = "\n\r\t\f\v";	// only allow single space as blank in the allocator's name
		const char * MEMORYCONFIG_ALLOCATOR = "# Memory configuration for allocator ";
		bool has_memoryconfig_allocator = false;
		size_t count = 0;

		// Have we found an entry for a memory allocator configuration?
		if (strncmp (defs, MEMORYCONFIG_ALLOCATOR, strlen (MEMORYCONFIG_ALLOCATOR)) == 0)
		{
			const char *in = &defs[strlen(MEMORYCONFIG_ALLOCATOR)];
			count = std::min (strcspn (in, blankchars), sizeof (allocatorname)-1);
			memcpy (allocatorname, in, count);
			allocatorname[count] = '\0';

			// If we have read something, then we have a new allocator
			has_memoryconfig_allocator = (count > 0 && count < sizeof (allocatorname));
		}

		if (has_memoryconfig_allocator)
		{
			char configureline[256] = {0};
			Allocator * allocator = get (allocatorname);
			if (allocator != nullptr)
			{
				// Set in configureline the next line, except the trailing LF
				defs += strlen (MEMORYCONFIG_ALLOCATOR) + count + 1;
				count = std::min (strcspn (defs, blankchars), sizeof (configureline)-1);
				memcpy (configureline, defs, count);
				configureline[count] = '\0';
				VERBOSE_MSG(1, "Configuring allocator %s with \"%s\"\n", allocatorname, configureline);
				allocator->configure (configureline);
			}
			else
			{
				VERBOSE_MSG(0, "Allocator %s is not registered.\n", allocatorname);
				exit (0);
			}
		}
		defs = strchr (defs+count+1, '#'); // Search for next # Memory configuration
	}
	munmap(p, sb.st_size);
}

Allocators::~Allocators (void)
{
}

Allocator * Allocators::get (const char *name)
{
	unsigned u = 0;
	while (allocators[u] != nullptr)
	{
		if (strcasecmp (name, allocators[u]->name()) == 0)
			return allocators[u];
		u++;
	}
	return nullptr;
}

Allocator ** Allocators::get (void)
{
	return allocators;
}

void Allocators::show_statistics (void) const
{
	unsigned u = 0;

	while (allocators[u] != nullptr)
	{
		if (allocators[u]->used())
			allocators[u]->show_statistics();
		u++;
	}
}

