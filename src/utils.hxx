// Author: Harald Servat <harald.servat@intel.com>
// Date: Feb 10, 2017
// License: To determine

#pragma once

bool parse_proc_self_maps_entry (const char *entry,
	size_t *start, size_t *end, size_t lenpermissions, char *permissions,
	size_t *offset, size_t lenmodule, char *module);
