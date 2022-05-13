AC_INIT(flexmalloc, 0.2.2, harald.servat@intel.com)
AM_INIT_AUTOMAKE([tar-ustar no-dependencies])
AM_SILENT_RULES([yes])

AC_CONFIG_HEADERS([flexmalloc-config.h])

LT_INIT([dlopen,shared,pic,disable-static])

AC_PROG_CC
AC_PROG_CXX
AM_PROG_AS

which dpkg-architecture &> /dev/null                                                                                   
if test "$?" -eq "0"; then 
	if test "${Selected_Binary_Type}" = "default" ; then 
		AC_MSG_CHECKING([the multiarch triplet through dpkg-architecture])
		multiarch_triplet=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
		AC_MSG_RESULT([$multiarch_triplet])
	fi
else
	AC_MSG_NOTICE([cannot locate multiarch triplet])
fi


AC_ARG_WITH([binutils],
	[AS_HELP_STRING([--with-binutils=DIR],
		[Support for binutils installed in DIR - search by default in /usr])],
	[],
	[with_binutils="/usr"])
if test "${with_binutils}" = "no"; then
	AC_MSG_FAILURE([--with-binutils=<DIR> is needed!])
else
	if test -r "${with_binutils}/include/bfd.h"; then
		if test -r "${with_binutils}/lib/libbfd.so" -o "${with_binutils}/lib/${multiarch_triplet}/libbfd.so"; then
			if test -r "${with_binutils}/lib/libiberty." -o "${with_binutils}/lib/${multiarch_triplet}/libiberty.a"; then
				BINUTILS_HOME=${with_binutils}
				AC_SUBST(BINUTILS_HOME)
			else
				AC_MSG_FAILURE([Cannot find libiberty.a within given binutils DIR])
			fi
		else
			AC_MSG_FAILURE([Cannot find libbfd.so within given binutils DIR])
		fi
	else
		AC_MSG_FAILURE([Cannot find bfd.h within given binutils DIR])
	fi
fi

AC_ARG_WITH([memkind],
	[AS_HELP_STRING([--with-memkind=DIR],
		[Support for memkind installed in DIR])],
	[],
	[with_memkind="no"])
if test "${with_memkind}" != "no"; then
	if test -x "${with_memkind}/bin/memkind-hbw-nodes" ; then
		if test -r "${with_memkind}/lib/libmemkind.so" -o -r "${with_memkind}/lib/${multiarch_triplet}/libmemkind.so" ; then
			if test -r "${with_memkind}/include/memkind.h" -a -r "${with_memkind}/include/hbwmalloc.h"; then
				MEMKIND_SUPPORT="yes"
				AC_DEFINE([MEMKIND_SUPPORTED], [1], [Defined if Memkind is available])
				MEMKIND_HOME=${with_memkind}
				AC_SUBST(MEMKIND_HOME)
			else
				AC_MSG_FAILURE([Cannot find memkind.h - hbwmalloc.h within given memkind DIR])
			fi
		else
			AC_MSG_FAILURE([Cannot find libmemkind.so within given memkind DIR])
		fi
	else
		AC_MSG_FAILURE([Cannot find memkind-hbw-nodes within given memkind DIR])
	fi
fi
AM_CONDITIONAL(HAVE_MEMKIND, test "${MEMKIND_SUPPORT}" = "yes")

if test x"{MEMKIND_SUPPORT}" = x"yes" ; then
	AC_MSG_CHECKING(for NUMA libraries)
	if test -r /usr/lib/x86_64-linux-gnu/libnuma.so -o -r /usr/lib/libnuma.so -o -r /usr/lib64/libnuma.so ; then
		AC_MSG_RESULT([found])
	else
		AC_MSG_FAILURE([Cannot find libnuma.so])
	fi
	AC_MSG_CHECKING(for NUMA header files)
	if test -r /usr/include/numa.h ; then
		AC_MSG_RESULT([found])
	else
		AC_MSG_FAILURE([Cannot find /usr/include/numa.h])
	fi
fi

AC_ARG_WITH([pmdk],
	[AS_HELP_STRING([--with-pmdk=DIR],
		[Support for PMDK installed in DIR])],
	[],
	[with_pmdk="no"])
if test "${with_pmdk}" != "no"; then
	if test -r "${with_pmdk}/lib/libpmem.so" -o -r "${with_pmdk}/lib64/libpmem.so" -o -r "${with_pmdk}/lib/${multiarch_triplet}/libpmem.so" ; then
		if test -r "${with_pmdk}/include/libpmem.h" ; then
			PMDK_SUPPORT="yes"
			AC_DEFINE([PMDK_SUPPORTED], [1], [Defined if PMDK is available])
			PMDK_HOME=${with_pmdk}
			AC_SUBST(PMDK_HOME)
		else
			AC_MSG_FAILURE([Cannot find libpmem.h within given PMDK DIR])
		fi
	else
		AC_MSG_FAILURE([Cannot find libpmem.so within given PMDK DIR])
	fi
fi
AM_CONDITIONAL(HAVE_PMDK, test "${PMDK_SUPPORT}" = "yes")

AC_ARG_WITH([papi],
	[AS_HELP_STRING([--with-papi=DIR],
		[Support for PAPI installed in DIR])],
	[],
	[with_papi="no"])
if test "${with_papi}" != "no"; then
	if test -r "${with_papi}/include/papi.h" ; then
		if test -r "${with_papi}/lib/libpapi.so" -o -r "${with_papi}/lib/${multiarch_triplet}/libpapi.so" ; then
			PAPI_SUPPORT="yes"
			AC_DEFINE([PAPI_SUPPORTED], [1], [Defined if PAPI is available])
			PAPI_HOME=${with_papi}
			AC_SUBST(PAPI_HOME)
		else
			AC_MSG_FAILURE([Cannot find libpapi.so within given PAPI DIR])
		fi
	else
		AC_MSG_FAILURE([Cannot find PAPI header fileswithin given PAPI DIR])
	fi
fi
AM_CONDITIONAL(HAVE_PAPI, test "${PAPI_SUPPORT}" = "yes")

CFLAGS_saved=$CFLAGS
LIBS_saved=$LIBS
CFLAGS="-I${MEMKIND_HOME}/include"
LIBS="-L${MEMKIND_HOME}/lib -lmemkind"
AC_MSG_CHECKING([for hbw_malloc_usable_size])
AC_TRY_LINK([
  #include <hbwmalloc.h>
],[
  size_t s = hbw_malloc_usable_size ((void*) 0);
],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_HBW_MALLOC_USABLE_SIZE], [1], [Defined if hbw_malloc_usable_size is available])
],[
  AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([for memkind_malloc_usable_size])
AC_TRY_LINK([
  #include <memkind.h>
],[
  size_t s = memkind_malloc_usable_size (0, (void*) 0);
],[
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_MEMKIND_MALLOC_USABLE_SIZE], [1], [Defined if memkind_malloc_usable_size is available])
],[
  AC_MSG_RESULT([no])
])
CFLAGS=$CFLAGS_saved
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

AC_OUTPUT([Makefile src/Makefile tests/Makefile doc/Makefile scripts/Makefile])

if test "${PMDK_SUPPORT}" != "yes" -a "${MEMKIND_SUPPORT}" = "yes" ; then
	AC_MSG_WARN([You configured FlexMalloc with Memkind support but without PMDK.])
elif test "${PMDK_SUPPORT}" = "yes" -a "${MEMKIND_SUPPORT}" != "yes" ; then
	AC_MSG_WARN([You configured FlexMalloc with PMDK support but without Memkind.])
fi

