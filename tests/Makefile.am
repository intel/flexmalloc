EXTRA_DIST = malloc+free-libtester-locations \
	malloc+free-locations \
    base-memory-configuration

CFLAGS=

lib_LTLIBRARIES = libtester.la

bin_PROGRAMS = malloc+free \
	malloc+free-libtester \
	multiple-tests \
	realloc \
	posix_memalign+realloc \
	malloc+realloc

libtester_la_SOURCES = libtester.c libtester.h
libtester_la_CFLAGS = -g -O0

malloc_free_SOURCES = malloc+free.c
malloc_free_CFLAGS = -g -O0

malloc_free_libtester_SOURCES = malloc+free-libtester.c
malloc_free_libtester_CFLAGS = -g -O0
malloc_free_libtester_LDFLAGS = libtester.la
malloc_free_libtester_DEPENDENCIES = libtester.la

multiple_tests_SOURCES = multiple-tests.c
multiple_tests_CFLAGS = -g -O0

realloc_SOURCES = realloc.c
realloc_CFLAGS = -g -O0

posix_memalign_realloc_SOURCES = posix_memalign+realloc.c
posix_memalign_realloc_CFLAGS = -g -O0

malloc_realloc_SOURCES = malloc+realloc.c
malloc_realloc_CFLAGS = -g -O0

install-data-hook:
	$(mkdir_p) $(datadir)
	cp $(srcdir)/*-locations $(srcdir)/base-memory-configuration $(datadir)

