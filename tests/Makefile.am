EXTRA_DIST = malloc+free-libtester-locations \
	malloc+free-locations \
	base-memory-configuration

AM_CPPFLAGS = -DDEBUG
AM_CFLAGS = -g -O0

noinst_LTLIBRARIES = libtester.la

TESTS = malloc+free \
	malloc+free-libtester \
	multiple-tests \
	realloc \
	posix_memalign+realloc \
	malloc+realloc

bin_PROGRAMS = $(TESTS)

libtester_la_SOURCES = libtester.c libtester.h

malloc_free_libtester_LDADD = libtester.la

testlocationsdir = $(datadir)
dist_testlocations_DATA = malloc+free-libtester-locations  malloc+free-locations

basememoryconfigurationdir = $(datadir)
dist_basememoryconfiguration_DATA = base-memory-configuration

