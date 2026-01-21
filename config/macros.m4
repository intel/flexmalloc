dnl Picked-up from https://github.com/bsc-performance-tools/tracking/blob/master/config/macros.m4

AC_DEFUN([AX_SET_ARCH_RELATED_VARS],
[
	AC_REQUIRE([AX_SELECT_BINARY_TYPE])
])

# AX_CHECK_POINTER_SIZE
# ---------------------
AC_DEFUN([AX_CHECK_POINTER_SIZE],
[
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
			int main () {
				return sizeof(void *)*8;
			}
		]])],
		[ POINTER_SIZE="0"  ],
		[ POINTER_SIZE="$?" ],
		[])
])

# AX_SELECT_BINARY_TYPE
# ---------------------
# Check the binary type the user wants to build and verify whether it can be successfully built
AC_DEFUN([AX_SELECT_BINARY_TYPE],
[
	AC_ARG_WITH([binary-type],
		[AS_HELP_STRING([--with-binary-type@<:@=ARG@:>@],[choose the binary type between: 32, 64, default @<:@default=default@:>@])],
		[Selected_Binary_Type="$withval"],
		[Selected_Binary_Type="default"])

	AS_IF([test "$Selected_Binary_Type" != "default" -a "$Selected_Binary_Type" != "32" -a "$Selected_Binary_Type" != "64"],
		[AC_MSG_ERROR([--with-binary-type: Invalid argument '$Selected_Binary_Type'. Valid options are: 32, 64, default.])])

	C_compiler="$CC"
	CXX_compiler="$CXX"

	AC_LANG_SAVE([])
	m4_foreach([language], [[C], [C++]], [
		AC_LANG_PUSH(language)

		AC_CACHE_CHECK([for $_AC_LANG_PREFIX[]_compiler compiler default binary type],
			[[]_AC_LANG_PREFIX[]_ac_cv_compiler_default_binary_type],
			[AX_CHECK_POINTER_SIZE
			 Default_Binary_Type="$POINTER_SIZE"
			 []_AC_LANG_PREFIX[]_ac_cv_compiler_default_binary_type="$Default_Binary_Type""-bit"
			])

		AS_IF([test "$Default_Binary_Type" != "32" -a "$Default_Binary_Type" != 64],
			[AC_MSG_ERROR([Unknown default binary type (pointer size is $POINTER_SIZE!?)])])

		AS_IF([test "$Selected_Binary_Type" = "default"],
			[which dpkg-architecture &> /dev/null
			 AS_IF([test "$?" -eq "0"],
				[AC_MSG_CHECKING([the multiarch triplet through dpkg-architecture])
				 multiarch_triplet=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
				 AC_MSG_RESULT([$multiarch_triplet])],
				[AC_MSG_NOTICE([cannot locate multiarch triplet])])
			 Selected_Binary_Type="$Default_Binary_Type"])

		AS_IF([test "$Selected_Binary_Type" != "$Default_Binary_Type"],
			[force_bit_flags="-m32 -q32 -32 -maix32 -m64 -q64 -64 -maix64 none"

			 AC_MSG_CHECKING([for $_AC_LANG_PREFIX[]_compiler compiler flags to build a $Selected_Binary_Type-bit binary])
			 for flag in $force_bit_flags
			 do
				old_[]_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS"
				 []_AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $flag"

				 AX_CHECK_POINTER_SIZE
				 AS_IF([test "$POINTER_SIZE" = "$Selected_Binary_Type"],
					[AC_MSG_RESULT([$flag])
					 break],
					[[]_AC_LANG_PREFIX[]FLAGS="$old_[]_AC_LANG_PREFIX[]FLAGS"
					 AS_IF([test "$flag" = "none"],
						[AC_MSG_RESULT([unknown])
						 AC_MSG_NOTICE([${Selected_Binary_Type}-bit binaries not supported])
						 AC_MSG_ERROR([Please use '--with-binary-type' to select an appropriate binary type.])
						])
					])
			 done
			])
		AC_LANG_POP(language)
	])
	AC_LANG_RESTORE([])
])

