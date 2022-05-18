// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once 

#include <bfd.h>

#define bfd_get_section_size(ptr) ((ptr)->size)
#define bfd_get_section_vma(bfd, ptr) ((void) bfd, (ptr)->vma)
#define bfd_get_section_flags(bfd, ptr) ((void) bfd, (ptr)->flags)

class BFDManager
{
	private:
	bfd *BFDImage;
	asymbol **BFDSymbols;

	public:
	BFDManager();
	~BFDManager();

	bool load_binary (const char *file);
	bool translate_address (const void *address, const char **function, char **file, unsigned *line);
};
