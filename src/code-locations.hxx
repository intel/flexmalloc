// Authors: Harald Servat <harald.servat@intel.com>
// Date: 02/02/2017
// License: 

#pragma once

#include "allocators.hxx"

typedef struct {
	bool translated;
	char *file;
	unsigned line;
} translated_frame_t;

class CodeLocations
{
	private:

	typedef struct
	{
	    char file[PATH_MAX];
	    unsigned line;
	    bool valid;
	} source_frame_t;

	typedef struct
	{
		long frame;
	} raw_frame_t;

	typedef struct {
		size_t   current_used_memory;
		size_t   HWM;
		size_t   current_used_memory_fb;
		size_t   HWM_fb;
		unsigned n_living_objects;
		unsigned n_max_living_objects;
		unsigned n_allocations_in_cache;
		unsigned n_allocations_not_in_cache;
		unsigned n_allocations_not_fit;
		unsigned n_allocations;
	} location_stats_t;

	typedef struct st_pending_module
	{
		char path[PATH_MAX+1];
		unsigned nframes;
		struct st_pending_module* next;
	} pending_module_t;

	typedef struct st_pending_raw_frame
	{
		pending_module_t* module;
		size_t offset;
		size_t index;
		struct st_pending_raw_frame* next;
	} pending_raw_frame_t;

	typedef struct
	{
		union
		{
			source_frame_t *source;
			raw_frame_t    *raw;
		} frames;
		Allocator * allocator;
		location_stats_t stats;
		unsigned nframes;
		unsigned id;
		pending_raw_frame_t* pending_frames;
	} location_t;

	#define LINE_SIZE 2048
	typedef struct
	{
		size_t start;
		size_t end;
		size_t offset;
		char module[LINE_SIZE+1];
		bool perm_read;
		bool perm_write;
		bool perm_exec;
	} memory_maps_entry_t;

	typedef struct
	{
		unsigned num_entries;
		unsigned capacity;
		memory_maps_entry_t* entries;
	} memory_maps_t;

	static bool comparator_by_ID (const location_t &lhs, const location_t &rhs);
	static bool comparator_by_NumberOfFrames (const location_t &lhs, const location_t &rhs);

	unsigned * _fast_indexes_frames;// This is an array (up to _max_frames)
					// pointing to the first element with N frames
					// when locations are sorted by comparator_by_NumberOfFrames
	const allocation_functions_t _af;
	Allocators * const _allocators;
	location_t * _locations;
	unsigned _nlocations;
	unsigned _min_nframes;
	unsigned _max_nframes;

	memory_maps_t _maps_info;

	pending_module_t* _pending_modules;

	unsigned get_min_index_for_number_of_frames (unsigned nframes) const;
	unsigned get_max_index_for_number_of_frames (unsigned nframes) const;

	char * find_and_set_allocator (char *location_txt, location_t * location, const char * fallback_allocator_name);
	size_t count_frames (char *location_txt, location_t * location, const char * const allocator_marker, char marker);
	bool process_source_location (char *location_txt, location_t * location, const char * fallback_allocator_name);
	bool process_raw_location (char *location_txt, location_t * location, const char * fallback_allocator_name);
	void clean_source_location (location_t * location);
	void show_frames (void);
	size_t file_offset_to_address (const char *lib, size_t address, bool& found);
	void create_fast_indexes_for_frames (void);
	bool load_memory_mappings_info (memory_maps_t& maps);
	pending_module_t* get_pending_module(const char* path);
	pending_module_t* add_or_get_pending_module(const char* path);
	void delete_unused_pending_modules(void);

	public:
	CodeLocations (allocation_functions_t &, Allocators *);
	~CodeLocations();
	bool readfile (const char *f, const char *fallback_allocator_name);
	void show_stats (void);
	void show_hmem_visualizer_stats (const char *fallback_allocator_name);
	Allocator * match (unsigned nframes, const translated_frame_t *tf,
	  unsigned &location_id);
	Allocator * match (unsigned nframes, void **frames, unsigned & location_id);
	Allocators * allocators () const
	  { return _allocators; };
	void record_location (unsigned location_id, bool fits, bool in_cache);
	void record_location (unsigned location_id, bool fits);
	void record_location_add_memory (unsigned location_id, size_t sz, bool fallback_allocator);
	void record_location_sub_memory (unsigned location_id, size_t sz, bool fallback_allocator);
	unsigned num_locations (void) const { return _nlocations; };
	unsigned min_nframes (void) const { return _min_nframes; };
	unsigned max_nframes (void) const { return _max_nframes; };
	unsigned has_locations (void) const { return _nlocations > 0; };
	Allocator * allocator (unsigned cl) const { return cl <= _nlocations ? _locations[cl].allocator : nullptr; };
	void translate_pending_frames(const char* module);
};

