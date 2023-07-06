lib_LTLIBRARIES  = libflexmalloc.la libflexmalloc_dbg.la libcounter.la

libflexmalloc_la_SOURCES = \
 common.cxx common.hxx \
 utils.cxx utils.hxx \
 bfd-manager.cxx bfd-manager.hxx \
 code-locations.cxx code-locations.hxx \
 allocators.cxx allocators.hxx \
 allocator.cxx allocator.hxx \
 allocator-posix.cxx allocator-posix.hxx \
 allocator-statistics.cxx allocator-statistics.hxx \
 statistics-recorder-allocator.hxx \
 cache-callstack.cxx cache-callstack.hxx \
 flex-malloc.cxx flex-malloc.hxx \
 malloc-interposer.cxx
libflexmalloc_dbg_la_SOURCES = $(libflexmalloc_la_SOURCES)

libflexmalloc_la_CXXFLAGS      = -O3 -DNDEBUG -Wall -Wextra -std=c++11 -I.. -I$(BINUTILS_HOME)/include -pthread
libflexmalloc_la_LDFLAGS       = -DNDEBUG -ldl -L$(BINUTILS_HOME)/lib -lbfd -liberty -lpthread
libflexmalloc_dbg_la_CXXFLAGS  = -O3 -Wall -Wextra -DDEBUG -std=c++11 -I.. -I$(BINUTILS_HOME)/include -pthread
libflexmalloc_dbg_la_LDFLAGS   = -ldl -L$(BINUTILS_HOME)/lib -lbfd -liberty -lpthread

if HAVE_MEMKIND
libflexmalloc_la_SOURCES       += \
 allocator-memkind-hbwmalloc.cxx allocator-memkind-hbwmalloc.hxx \
 allocator-memkind-pmem.cxx allocator-memkind-pmem.hxx
libflexmalloc_la_CXXFLAGS      += -I$(MEMKIND_HOME)/include
libflexmalloc_la_LDFLAGS       += -L$(MEMKIND_HOME)/lib -lmemkind -R $(MEMKIND_HOME)/lib -lnuma
libflexmalloc_dbg_la_CXXFLAGS  += -I$(MEMKIND_HOME)/include
libflexmalloc_dbg_la_LDFLAGS   += -L$(MEMKIND_HOME)/lib -lmemkind -R $(MEMKIND_HOME)/lib -lnuma
endif

if HAVE_PAPI
libflexmalloc_la_CXXFLAGS      += -DHWC -I$(PAPI_HOME)/include
libflexmalloc_la_LDFLAGS       += -L$(PAPI_HOME)/lib -lpapi -R $(PAPI_HOME)/lib
libflexmalloc_dbg_la_CXXFLAGS  += -DHWC -I$(PAPI_HOME)/include
libflexmalloc_dbg_la_LDFLAGS   += -L$(PAPI_HOME)/lib -lpapi -R $(PAPI_HOME)/lib
endif

if HAVE_PMDK
libflexmalloc_la_CXXFLAGS      += -I$(PMDK_HOME)/include
libflexmalloc_la_LDFLAGS       += -L$(PMDK_HOME)/lib -lpmem -R $(PMDK_HOME)/lib
libflexmalloc_dbg_la_CXXFLAGS  += -I$(PMDK_HOME)/include
libflexmalloc_dbg_la_LDFLAGS   += -L$(PMDK_HOME)/lib -lpmem -R $(PMDK_HOME)/lib
endif

libcounter_la_SOURCES          = counter.cxx
libcounter_la_CXXFLAGS         = -O3 -DNDEBUG -Wall -Wextra -std=c++11
libcounter_la_LDFLAGS          = -DNDEBUG -ldl
