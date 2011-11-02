AC_PREREQ([2.60])
AC_INIT([atomic-install], [0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.6 foreign dist-bzip2 -Wall])

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])

AC_PROG_CC
AM_PROG_CC_C_O
AC_USE_SYSTEM_EXTENSIONS
LT_INIT([disable-static])
GTK_DOC_CHECK([1.15])

AC_CHECK_FUNCS([fchmodat lockf posix_fallocate posix_fadvise utimensat])

AC_TYPE_OFF_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_ARG_ENABLE([libattr],
	[AS_HELP_STRING([--disable-libattr],
		[Disable use of libattr (default: autodetect)])])
AS_IF([test x"$enable_libattr" != x"no"], [
	AC_CHECK_LIB([attr], [attr_copy_file])
])

AC_ARG_ENABLE([debug],
	[AS_HELP_STRING([--disable-debug],
		[Disable debugging asserts])])
AS_IF([test x"$enable_debug" = x"no"], [
	AC_DEFINE([NDEBUG], [1], [Set this to disable debugging asserts])
])

AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile doc/Makefile])
AC_OUTPUT
