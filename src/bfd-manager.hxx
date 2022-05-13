// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once 

#include <bfd.h>

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
