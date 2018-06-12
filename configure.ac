AC_INIT([circe], [0.1], [lh_mouse@126.com])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX
AC_LANG([C++])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT([disable-static dlopen])

AC_CONFIG_FILES([
	Makefile
	circe-common/Makefile
	circe-gate/Makefile
	circe-foyer/Makefile
	circe-auth/Makefile
	circe-box/Makefile
	circe-pilot/Makefile])
##_INSERT_NEW_MAKEFILE_HERE
AC_OUTPUT
