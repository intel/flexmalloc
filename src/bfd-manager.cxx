// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <stdlib.h>
#include <stdio.h>

#include "common.hxx"
#include "bfd-manager.hxx"

static bool __bfd_manager_initialized = false;

BFDManager::BFDManager ()
{
}

BFDManager::~BFDManager ()
{
}

bool BFDManager::load_binary (const char *file)
{
	if ( !__bfd_manager_initialized )
	{
		bfd_init ();
		__bfd_manager_initialized = true;
	}

	/* Open the binary in read-only mode */
	BFDImage = bfd_openr (file, nullptr);
	if (BFDImage == nullptr)
	{
		VERBOSE_MSG(0, "Error! Could not read symbols from file %s\n",
		  file);
		return false;
	}

	/* Check the binary file format */
	if (!bfd_check_format (BFDImage, bfd_object))
	{
		VERBOSE_MSG(0, "Error! Binary file format does not match for file %s\n",
		  file);
		return false;
	}

	/* Load the symbol table */
	if (bfd_get_file_flags (BFDImage) & HAS_SYMS)
	{
		size_t size = bfd_get_symtab_upper_bound (BFDImage);
		if (size > 0)
		{
			BFDSymbols = (asymbol**) malloc (size); /* TODO, fix this call */
			if (BFDSymbols == nullptr)
			{
				VERBOSE_MSG(0, "Error! Could not allocate memory to translate addresses into source code references\n");
				return false;
			}

			// Based from binutils/addr2info.c 
			// If we haven't found symbols even though size > 0, try with dynamic symbols
			if (bfd_canonicalize_symtab (BFDImage, BFDSymbols) == 0)
			{
				free (BFDSymbols);

				size = bfd_get_dynamic_symtab_upper_bound (BFDImage);
				if (size > 0)
				{
					BFDSymbols = (asymbol**) malloc (size); /* TODO, fix this call */
					if (BFDSymbols == nullptr)
					{
						VERBOSE_MSG(0, "Error! Could not allocate memory to translate addresses into source code references\n");
						return false;
					}
					return bfd_canonicalize_dynamic_symtab (BFDImage, BFDSymbols) > 0;
				}
				return false;
			}
			else
				return true;
		}
		else if (size == 0)
		{
			size = bfd_get_dynamic_symtab_upper_bound (BFDImage);
			if (size > 0)
			{
				BFDSymbols = (asymbol**) malloc (size); /* TODO, fix this call */
				if (BFDSymbols == nullptr)
				{
					VERBOSE_MSG(0, "Error! Could not allocate memory to translate addresses into source code references\n");
					return false;
				}
				return bfd_canonicalize_dynamic_symtab (BFDImage, BFDSymbols) > 0;
			}
		}
	}

	return false;
}

typedef struct symbol_information_st
{
	bfd_vma pc;
	asymbol **symbols;
	char *filename;
	const char *function;
	unsigned line;
	bfd_boolean found;
} symbol_information_t;

static void find_address_in_section (bfd *abfd, asection *section, PTR data)
{
	/* TODO fix this */
#define HAVE_BFD_GET_SECTION_SIZE 1

#if HAVE_BFD_GET_SECTION_SIZE || HAVE_BFD_GET_SECTION_SIZE_BEFORE_RELOC
	bfd_size_type size;
#endif
	bfd_vma vma;

	symbol_information_t *sdata = (symbol_information_t*) data;

	if (sdata->found)
		return;

	if ((bfd_section_flags (section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_section_vma (section);

	if (sdata->pc < vma)
		return;

#if HAVE_BFD_GET_SECTION_SIZE
	size = bfd_section_size (section);
	if (sdata->pc >= vma + size)
		return;
#elif HAVE_BFD_GET_SECTION_SIZE_BEFORE_RELOC
	size = bfd_get_section_size_before_reloc (section);
	if (sdata->pc >= vma + size)
		return;
#endif

    sdata->found = bfd_find_nearest_line (abfd, section, sdata->symbols,
      sdata->pc - vma, (const char **) &sdata->filename, &sdata->function,
      &sdata->line);
}

bool BFDManager::translate_address (
	const void *address, const char **function, char **file, unsigned *line)
{
// #define HAVE_BFD_DEMANGLE
// #warning "Need to work on BFD_demangle - collision w/ realloc ?"

	if (BFDImage && BFDSymbols)
	{
		symbol_information_t symbol_info;
		symbol_info.found = 0;
		symbol_info.pc = (bfd_vma) address;
		symbol_info.symbols = BFDSymbols;

		/* Iterate through sections of bfd Image */
		bfd_map_over_sections (BFDImage, ::find_address_in_section, &symbol_info);
		//bfdmanager_find_address_in_section (BFDImage, &symbol_info);
		//
		DBG ("symbol_info.found = %d\n", symbol_info.found);

		/* Found the symbol? */
		if (symbol_info.found)
		{
			*file = (char*) symbol_info.filename;
			*line = symbol_info.line;

#if defined(HAVE_BFD_DEMANGLE)
			char *demangled = nullptr;
			if (symbol_info.function)
				demangled = bfd_demangle (BFDImage, symbol_info.function, 0);

			if (demangled)
				*function = demangled;
			else
				*function = (char*) symbol_info.function;
#else
			*function = (char*) symbol_info.function;
#endif

			DBG ("function = %s file = %s line = %d\n", *function, *file, *line);

			/* Sometimes BFD seems to mess a bit, indicates that translation was
			   successful but data is not available */
			// return *function != nullptr && *file != nullptr && *line > 0;
			return *function != nullptr && *file != nullptr;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}
