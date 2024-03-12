AC_INIT([flexmalloc],[0.2.2],[harald.servat@intel.com])

AC_CONFIG_AUX_DIR(./config)
# Note that this directory must *exactly* match what was specified via
# -I in ACLOCAL_AMFLAGS in the top-level Makefile.am.
AC_CONFIG_MACRO_DIR(./config)

AM_INIT_AUTOMAKE([tar-ustar no-dependencies])

AM_SILENT_RULES([yes])

AX_CONFIG_SCRIPT([scripts/flexmalloc.sh])
AX_CONFIG_SCRIPT([scripts/count.sh])
AX_CONFIG_SCRIPT([tests/test-wrapper.sh])

AC_CONFIG_HEADERS([flexmalloc-config.h])

LT_INIT([dlopen,shared,pic,disable-static])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AS

AX_SET_ARCH_RELATED_VARS

AC_ARG_WITH([binutils],
	[AS_HELP_STRING([--with-binutils=DIR],
		[Support for binutils installed in DIR - search by default in /usr])],
	[],
	[with_binutils="/usr"])
AS_IF([test "${with_binutils}" = "no"],
	[AC_MSG_FAILURE([--with-binutils=<DIR> is needed!])],
	[AS_IF([test -r "${with_binutils}/include/bfd.h"],
		[AS_IF([test -r "${with_binutils}/lib/libbfd.so" -o -r "${with_binutils}/lib64/libbfd.so" -o -r "${with_binutils}/lib/${multiarch_triplet}/libbfd.so"],
			[AS_IF([test -r "${with_binutils}/lib/libiberty.a" -o -r "${with_binutils}/lib64/libiberty.a" -o -r "${with_binutils}/lib/${multiarch_triplet}/libiberty.a"],
				[BINUTILS_HOME=${with_binutils}
				 AC_SUBST(BINUTILS_HOME)
				],
				[AC_MSG_FAILURE([Cannot find libiberty.a within given binutils DIR])])
			],
			[AC_MSG_FAILURE([Cannot find libbfd.so within given binutils DIR])])
		],
		[AC_MSG_FAILURE([Cannot find bfd.h within given binutils DIR])])
	])

AC_ARG_WITH([memkind],
	[AS_HELP_STRING([--with-memkind=DIR],
		[Support for memkind installed in DIR])],
	[],
	[with_memkind="no"])
AS_IF([test "${with_memkind}" != "no"],
	[AS_IF([test -x "${with_memkind}/bin/memkind-hbw-nodes"],
		[AS_IF([test -r "${with_memkind}/lib/libmemkind.so" -o -r "${with_memkind}/lib64/libmemkind.so" -o -r "${with_memkind}/lib/${multiarch_triplet}/libmemkind.so"],
			[AS_IF([test -r "${with_memkind}/include/memkind.h" -a -r "${with_memkind}/include/hbwmalloc.h"],
				[MEMKIND_SUPPORT="yes"
				 AC_DEFINE([MEMKIND_SUPPORTED], [1], [Defined if Memkind is available])
				 MEMKIND_HOME=${with_memkind}
				 AC_SUBST(MEMKIND_HOME)
				],
				[AC_MSG_FAILURE([Cannot find memkind.h - hbwmalloc.h within given memkind DIR])])
			],
			[AC_MSG_FAILURE([Cannot find libmemkind.so within given memkind DIR])])
		],
		[AC_MSG_FAILURE([Cannot find memkind-hbw-nodes within given memkind DIR])])
	])
AM_CONDITIONAL([HAVE_MEMKIND], [test "${MEMKIND_SUPPORT}" = "yes"])

AS_IF([test x"${MEMKIND_SUPPORT}" = x"yes"],
	[AC_MSG_CHECKING(for NUMA libraries)
	 AS_IF([test -r "/usr/lib/libnuma.so" -o -r "/usr/lib64/libnuma.so" -o -r "/usr/lib/${multiarch_triplet}/libnuma.so"],
		[AC_MSG_RESULT([found])],
		[AC_MSG_FAILURE([Cannot find libnuma.so])])
	 AC_MSG_CHECKING(for NUMA header files)
	 AS_IF([test -r "/usr/include/numa.h"],
		[AC_MSG_RESULT([found])],
		[AC_MSG_FAILURE([Cannot find /usr/include/numa.h])])
	])

AC_ARG_WITH([pmdk],
	[AS_HELP_STRING([--with-pmdk=DIR],
		[Support for PMDK installed in DIR])],
	[],
	[with_pmdk="no"])
AS_IF([test "${with_pmdk}" != "no"],
	[AS_IF([test -r "${with_pmdk}/lib/libpmem.so" -o -r "${with_pmdk}/lib64/libpmem.so" -o -r "${with_pmdk}/lib/${multiarch_triplet}/libpmem.so"],
		[AS_IF([test -r "${with_pmdk}/include/libpmem.h"],
			[PMDK_SUPPORT="yes"
			 AC_DEFINE([PMDK_SUPPORTED], [1], [Defined if PMDK is available])
			 PMDK_HOME=${with_pmdk}
			 AC_SUBST(PMDK_HOME)
			],
			[AC_MSG_FAILURE([Cannot find libpmem.h within given PMDK DIR])])
		],
		[AC_MSG_FAILURE([Cannot find libpmem.so within given PMDK DIR])])
	])
AM_CONDITIONAL([HAVE_PMDK], [test "${PMDK_SUPPORT}" = "yes"])

AC_ARG_WITH([papi],
	[AS_HELP_STRING([--with-papi=DIR],
		[Support for PAPI installed in DIR])],
	[],
	[with_papi="no"])
AS_IF([test "${with_papi}" != "no"],
	[AS_IF([test -r "${with_papi}/include/papi.h"],
		[AS_IF([test -r "${with_papi}/lib/libpapi.so" -o -r "${with_papi}/lib64/libpapi.so" -o -r "${with_papi}/lib/${multiarch_triplet}/libpapi.so"],
			[PAPI_SUPPORT="yes"
			 AC_DEFINE([PAPI_SUPPORTED], [1], [Defined if PAPI is available])
			 PAPI_HOME=${with_papi}
			 AC_SUBST(PAPI_HOME)
			],
			[AC_MSG_FAILURE([Cannot find libpapi.so within given PAPI DIR])])
		],
		[AC_MSG_FAILURE([Cannot find PAPI header files within given PAPI DIR])])
	])
AM_CONDITIONAL([HAVE_PAPI], [test "${PAPI_SUPPORT}" = "yes"])

CPPFLAGS_saved=$CPPFLAGS
LIBS_saved=$LIBS
CPPFLAGS="-I${MEMKIND_HOME}/include ${CPPFLAGS}"
LIBS="-L${MEMKIND_HOME}/lib -lmemkind ${LIBS}"
AC_MSG_CHECKING([for hbw_malloc_usable_size])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <hbwmalloc.h>
]], [[
  size_t s = hbw_malloc_usable_size ((void*) 0);
]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_HBW_MALLOC_USABLE_SIZE], [1], [Defined if hbw_malloc_usable_size is available])
],[
  AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([for memkind_malloc_usable_size])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <memkind.h>
]], [[
  size_t s = memkind_malloc_usable_size (0, (void*) 0);
]])],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_MEMKIND_MALLOC_USABLE_SIZE], [1], [Defined if memkind_malloc_usable_size is available])
],[
  AC_MSG_RESULT([no])
])
CPPFLAGS=$CPPFLAGS_saved
LIBS=$LIBS_saved

AC_CHECK_HEADERS([ \
 assert.h \
 bfd.h \
 ctype.h \
 dlfcn.h \
 dirent.h \
 execinfo.h \
 errno.h \
 fcntl.h \
 float.h \
 libgen.h \
 limits.h \
 math.h \
 pthread.h \
 sched.h \
 stdint.h \
 stdio.h \
 stdlib.h \
 string.h \
 strings.h \
 time.h \
 unistd.h \
 linux/limits.h \
 sys/mman.h \
 sys/stat.h \
 sys/time.h \
 sys/resource.h \
 sys/param.h
])

AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile doc/Makefile scripts/Makefile])
AC_OUTPUT

AS_IF([test "${PMDK_SUPPORT}" != "yes" -a "${MEMKIND_SUPPORT}" = "yes"],
	[AC_MSG_WARN([You configured FlexMalloc with Memkind support but without PMDK.])],
	[test "${PMDK_SUPPORT}" = "yes" -a "${MEMKIND_SUPPORT}" != "yes"],
	[AC_MSG_WARN([You configured FlexMalloc with PMDK support but without Memkind.])],
	[])

