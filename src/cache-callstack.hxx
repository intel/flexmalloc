
#include "allocator.hxx"

#pragma once

#define CALLSTACKS_PER_ENTRY	32	// Deepest callpath that we can store
#define NUM_ENTRIES             (1 << 6) // 64 entries, needs to be power of 2
#define MASK_ENTRIES            (NUM_ENTRIES-1) // to simplify calculations

class CacheCallstacks
{
	private:
	typedef struct cache_entry_st 
	{
		void *frames[CALLSTACKS_PER_ENTRY];
		Allocator *allocator;
		unsigned n_frames;
		unsigned id;
	} cache_entry_t;

	cache_entry_t _entries[NUM_ENTRIES];
	unsigned _num_entries;
	unsigned _first_entry;

	typedef struct cache_stats_st
	{
		unsigned n_hits;
		unsigned n_miss;
		unsigned n_miss_too_long;
	} cache_stats_t;

	mutable cache_stats_t _stats;

	public:
	CacheCallstacks ();
	~CacheCallstacks ();

	bool match (unsigned nframes, void *frames[], Allocator *&a, unsigned &id) const;
	void add_match (unsigned nframes, void *frames[], Allocator *a, unsigned);
	void show_statistics (void) const;
};
