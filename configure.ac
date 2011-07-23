AC_PREREQ(2.59)

AC_INIT(micbench, 0.0.1, haya@tkl.iis.u-tokyo.ac.jp)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE($PACKAGE_NAME, $PACKAGE_VERSION)

AC_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile
	src/Makefile])

dnl ******************************
dnl Check for headers
dnl ******************************

AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_TIME

dnl ******************************
dnl Check for ruby
dnl ******************************

AC_ARG_WITH([ruby],
            AS_HELP_STRING([--with-ruby=PATH],
                           [Ruby interpreter path (default: auto-detect)]),
            [RUBY="$withval"])
AC_PATH_PROG(RUBY, ruby, none)
if test "$RUBY" = "none"; then
  AC_MSG_ERROR([Ruby is required.])
fi

dnl ******************************
dnl Check for libnuma
dnl ******************************

AC_CHECK_HEADERS(numa.h, [have_numa_h=yes], [have_numa_h=no])
AC_CHECK_LIB(numa, numa_available, [have_libnuma=yes], [have_libnuma=no])

if test "$have_numa_h" = "yes"; then
  AC_DEFINE([HAVE_NUMA_H], [1], [Define if numa.h exists.])
fi
AC_DEFINE([HAVE_NUMA_H], [1], [Define if numa.h exists.])

if test "$have_libnuma" = "yes"; then
  LIBS="$LIBS -lnuma"
fi

AC_OUTPUT

echo
echo "Configure Result:"
echo
echo "  Ruby    : $RUBY"
echo "  numa.h  : $have_numa_h"
echo "  libnuma : $have_libnuma"
echo