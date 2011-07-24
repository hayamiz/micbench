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
dnl Check for standard headers
dnl ******************************

AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_CHECK_HEADERS(inttypes.h)

dnl ******************************
dnl Check for standard programs
dnl ******************************

AC_PROG_GREP

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

dnl ******************************
dnl Check for numactl
dnl ******************************

AC_PATH_PROG(NUMACTL, numactl, none)
if test "$NUMACTL" = "none"; then
 AC_MSG_ERROR([numactl is required.])
fi

dnl ******************************
dnl Check for OProfile
dnl ******************************

AC_PATH_PROG(OPCONTROL, opcontrol, none)

dnl find kernel image with debug info
VMLINUX=none
if lsb_release --id|grep "\(Ubuntu\|Debian\)"; then
  AC_CHECK_FILE(/usr/lib/debug/boot/vmlinux-$(uname -r),
                [have_vmlinux=yes],
                [have_vmlinux=no])
  if test "$have_vmlinux" = "yes"; then
    VMLINUX="/usr/lib/debug/boot/vmlinux-\$(uname -r)"
    AC_DEFINE([HAVE_VMLINUX], [$VMLINUX], [Define if vmlinux exists.])
  fi
fi

AC_OUTPUT

echo
echo "Configure Result:"
echo
echo "== required headers and libraries =="
echo "  numa.h  : $have_numa_h"
echo "  libnuma : $have_libnuma"
echo
echo "== required commands =="
echo "  Ruby    : $RUBY"
echo "  numactl : $NUMACTL"
echo
echo "== optional =="
echo "  OProfile  : $OPCONTROL"
echo "    vmlinux : $VMLINUX"
echo