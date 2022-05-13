// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <new>

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

	char allocatorname[256];
	char configureline[256];
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
	char *defs = index ((char*)p, '#');
	while (defs != nullptr)
	{
		const char * MEMORYCONFIG_ALLOCATOR = "# Memory configuration for allocator ";
		bool has_memoryconfig_allocator = false;

		// Have we found an entry for a memory allocator configuration?
		if (strncmp (defs, MEMORYCONFIG_ALLOCATOR, strlen(MEMORYCONFIG_ALLOCATOR)) == 0)
		{
			const char *in = &defs[strlen(MEMORYCONFIG_ALLOCATOR)];
			memset (allocatorname, 0, sizeof(allocatorname));
			size_t count = 0;
			while (in[count] != ((char)0) && in[count] != '\n' && count+1 < sizeof(allocatorname))
			{
				allocatorname[count] = in[count];
				count++;
			}
			// If we have read something, then we have a new allocator
			has_memoryconfig_allocator = (count > 0 && count < sizeof(allocatorname));
		}

		if (has_memoryconfig_allocator)
		{
			Allocator * allocator = get (allocatorname);
			if (allocator != nullptr)
			{
				defs = index (defs+1, '\n');
				defs++;
				// Set in configureline the next line, except the trailing LF
				size_t i = 0;
				// Fully clean the input buffer
				memset (configureline, 0, sizeof(configureline));
				while (defs[i] != (char)0)
				{
					// Have we found the end of the line?
					if (defs[i] == '\n')
					{
						break;
					}
					// Have we reached the limit of the dest buffer ?
					else if (i >= sizeof(configureline))
					{
						configureline[sizeof(configureline)-1] = (char)0;
						break;
					}
					configureline[i] = defs[i];
					i++;
				}
				VERBOSE_MSG(1, "Configuring allocator %s with \"%s\"\n", allocatorname, configureline);
				allocator->configure (configureline);
			}
			else
			{
				VERBOSE_MSG(0, "Allocator %s is not registered.\n", allocatorname);
				exit (0);
			}
		}
		defs = index (defs+1, '#'); // Search for next # Memory configuration
	}
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

