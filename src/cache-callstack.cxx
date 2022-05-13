
#include "common.hxx"
#include "cache-callstack.hxx"

CacheCallstacks::CacheCallstacks ()
	: _num_entries (0), _first_entry(0)
{
	_stats.n_hits =
	  _stats.n_miss =
	  _stats.n_miss_too_long = 0;
}

CacheCallstacks::~CacheCallstacks ()
{
}

bool CacheCallstacks::match (unsigned nframes, void *frames[], Allocator *&a, unsigned &id) const
{
	if (nframes <= CALLSTACKS_PER_ENTRY)
	{
		for (unsigned e = 0; e < _num_entries; ++e)
			if (_entries[e].n_frames == nframes)
			{
				bool match = true;
				for (unsigned f = 0; f < nframes && match; ++f)
					match = _entries[e].frames[f] == frames[f];
				if (match)
				{
					a = _entries[e].allocator;
					id = _entries[e].id;
					_stats.n_hits++;
					return true;
				}
			}
		_stats.n_miss++;
	}
	else
	{
		_stats.n_miss_too_long++;
	}

	return false;
}

void CacheCallstacks::add_match (unsigned nframes, void *frames[], Allocator *a, unsigned id)
{
	if (nframes <= CALLSTACKS_PER_ENTRY)
	{
		if (_num_entries < NUM_ENTRIES)
		{
			// We have space in cache -- no need to evict an entry
			_entries[_num_entries].allocator = a;
			_entries[_num_entries].n_frames = nframes;
			_entries[_num_entries].id = id;
			for (unsigned f = 0; f < nframes; ++f)
				_entries[_num_entries].frames[f] = frames[f];
			_num_entries++;
		}
		else
		{
			// We don't have enough space -- need to evict an entry
			// Use circular buffer approach
			_entries[_first_entry].allocator = a;
			_entries[_first_entry].n_frames = nframes;
			_entries[_first_entry].id = id;
			for (unsigned f = 0; f < nframes; ++f)
				_entries[_first_entry].frames[f] = frames[f];
			_first_entry = (_first_entry+1) & MASK_ENTRIES; // Set next entry
		}
	}
}

void CacheCallstacks::show_statistics (void) const
{
	VERBOSE_MSG(1, "- Cache size = %u with %u call-stack segments per entry\n", NUM_ENTRIES, CALLSTACKS_PER_ENTRY);
	VERBOSE_MSG(1, "- Cache hits = %u, misses = %u, misses for being too long = %u\n",
	  _stats.n_hits, _stats.n_miss, _stats.n_miss_too_long);

	float cache_hits = _stats.n_hits;
	float cache_miss = _stats.n_miss;
	float cache_miss_too_long = _stats.n_miss_too_long;
	float cache_hit_ratio = 0., n_cache_hit_ratio = 0.;
	if (cache_hits + cache_miss > 0.)
		cache_hit_ratio = 100. * cache_hits / ( cache_hits + cache_miss );
	if (cache_hits + cache_miss + cache_miss_too_long > 0.)
		n_cache_hit_ratio = 100. * cache_hits / ( cache_hits + cache_miss + cache_miss_too_long );
	VERBOSE_MSG(1, "- Cache hit ratio = %.1f %%\n", cache_hit_ratio);
	VERBOSE_MSG(1, "- Normalized cache hit ratio = %.1f %%\n", n_cache_hit_ratio);
}

