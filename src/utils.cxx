// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include "utils.hxx"

// Process /proc/self/maps entries like
//   2aff88d78000-2aff88d7a000 rw-p 001c7000 fd:00 33555446                   /usr/lib64/libc-2.17.so
//   ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]

bool parse_proc_self_maps_entry (const char *entry,
	size_t *start, size_t *end, size_t lenpermissions, char *permissions,
	size_t *offset, size_t lenmodule, char *module)
{
	const char blankchars[] = "\n\r\t\f\v";
	constexpr size_t LENPERMISSIONS = 5;
	bool res = false;
	char *beginptr = nullptr, *endptr = nullptr;
	char _module[lenmodule];
	size_t _start, _end, _offset;
	char _perms[LENPERMISSIONS];

	if (lenpermissions < LENPERMISSIONS)
		return false;

	// Parse first hex number, start address (e.g. 2aff88d78000)
	_start = strtoull (entry, &endptr, 16);
	if (endptr != entry)
	{
		if (*endptr == '-')
		{
			// Skip hyphen
			beginptr = endptr+1;
			// Parse second hex number, end address (e.g. 2aff88d7a000)
			_end = strtoull (beginptr, &endptr, 16);
			if (endptr != beginptr)
			{
				// Now get the permissions (e.g. rw-p)
				// if there are at least 4 chars to be processed
				if ((*endptr == ' ') && (strlen(endptr) >= 5))
				{
					_perms[0] = endptr[1];
					_perms[1] = endptr[2];
					_perms[2] = endptr[3];
					_perms[3] = endptr[4];
					_perms[4] = '\0';
					endptr += 4;

					// Get offset after a white space (e.g. 001c7000)
					if (strlen(endptr) > 1)
					{
						beginptr = endptr+1;
						_offset = strtoull (beginptr, &endptr, 16);

						// If we have parsed an offset, now ignore the fd:00 and size,
						// and grab the module name.
						// Need to skip 2 fields, plus an indefinite number of spaces
						if (endptr != beginptr && endptr != nullptr)
						{
							lenmodule -= endptr-entry; // Update remaining length of the string to parse
							enum skipped_fields_e { MAJOR_MINOR, INODE, FILEPATH };
							size_t skip_count = 0;
							for (int sf = MAJOR_MINOR; sf < FILEPATH; sf++)
							{
								// Skip over some blankspace + everything that is not a blankspace (in regex: "\s+\S+")
								skip_count += std::min (strspn (endptr+skip_count, " "), lenmodule);
								lenmodule -= skip_count;
								skip_count += std::min (strcspn (endptr+skip_count, " "), lenmodule);
								lenmodule -= skip_count;
							}
							// Skip over indefinite number of spaces
							skip_count += std::min (strspn (endptr+skip_count, " "), lenmodule);
							lenmodule -= skip_count;
							endptr += skip_count;
							// We found the module name
							if (*endptr != '\0')
							{
								size_t count = std::min (strcspn (endptr, blankchars), lenmodule);
								strncpy (_module, endptr, count);
								_module[count] = '\0';
								// If module is non-empty, then we have all
								// the data needed and we can return
								res = strlen(_module) > 0;
							}
						}
					}
				}
			}
		}
	}

	// If everything worked, let's copy the data back
	if (res)
	{
		*start = _start;
		*end = _end;
		memcpy (permissions, _perms, LENPERMISSIONS);
		*offset = _offset;
		memcpy (module, _module, lenmodule);
	}

	return res;
}
