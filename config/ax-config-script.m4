# AX_CONFIG_SCRIPT
# ---------------------
# Configure the script and ensure it has the execution rights
AC_DEFUN([AX_CONFIG_SCRIPT],
[
	AC_CONFIG_FILES([$1], [AS_IF([test $# -lt 2], [chmod +x $1], [chmod +x $2])])
])
