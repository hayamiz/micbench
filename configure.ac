AC_PREREQ(2.59)

AC_INIT(micbench, 0.1.1, y.hayamizu@gmail.com)
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE($PACKAGE_NAME, $PACKAGE_VERSION)

AC_PROG_LIBTOOL

AC_CONFIG_FILES([Makefile
	src/Makefile
	test/Makefile
	test/lib/Makefile
	test/fixtures/Makefile])

dnl ******************************
dnl Check for CPU arch.
dnl ******************************

AC_PATH_PROG(UNAME, uname, none)
if test "$UNAME" = "none"; then
  AC_MSG_ERROR([uname(1) is required to detect CPU arch.])
fi
case $(uname -m) in
x86_64)
    ac_arch=x86_64
    ;;
amd64)
    ac_arch=x86_64
    ;;
i?86)
    ac_arch=x86
    ;;
*)
    ac_arch=none
esac

if test "$ac_arch" = "none"; then
  AC_MSG_ERROR([Arch. not supported: $ac_arch])
fi

AC_CHECK_FILE([/sys/devices/system/node], [numa_arch=yes], [numa_arch=no])

if test "$numa_arch" = "yes"; then
  AC_DEFINE([NUMA_ARCH], [1], [Define to 1 if CPUs are numa architecture.])
fi

AM_CONDITIONAL([WITH_X86_64], [test "$ac_arch" = "x86_64"])
AM_CONDITIONAL([WITH_X86], [test "$ac_arch" = "x86"])

dnl ******************************
dnl Check for standard headers
dnl ******************************

AC_HEADER_STDC
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_CHECK_HEADER(inttypes.h, [], [AC_MSG_ERROR([inttypes.h required.])])
AC_CHECK_HEADER(getopt.h, [], [AC_MSG_ERROR([getopt.h required.])])

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

if test "$numa_arch" = "yes"; then
  AC_CHECK_HEADERS(numa.h, [have_numa_h=yes], [have_numa_h=no])
  AC_CHECK_HEADERS(numaif.h, [have_numaif_h=yes], [have_numaif_h=no])
  AC_CHECK_LIB(numa, numa_available, [have_libnuma=yes], [have_libnuma=no])
  
  if ! test "$have_numaif_h" = "yes"; then
    AC_MSG_ERROR([numaif.h is required.])
  fi
  if ! test "$have_numa_h" = "yes"; then
    AC_MSG_ERROR([numa.h is required.])
  fi
  
  if test "$have_libnuma" = "yes"; then
    LIBS="$LIBS -lnuma"
  fi
fi

dnl ******************************
dnl Check for numactl
dnl ******************************

if test "$numa_arch" = "yes"; then
  AC_PATH_PROG(NUMACTL, numactl, none)
  if test "$NUMACTL" = "none"; then
   AC_MSG_ERROR([numactl is required.])
  fi
fi

dnl ******************************
dnl Check for libaio
dnl ******************************

AC_CHECK_HEADERS(libaio.h, [have_libaio_h=yes], [have_libaio_h=no])
AC_CHECK_LIB(aio, io_submit, [have_libaio=yes], [have_libaio=no])

if ! test "$have_libaio" = "yes"; then
  AC_MSG_ERROR([libaio is required.])
fi
if ! test "$have_libaio_h" = "yes"; then
  AC_MSG_ERROR([libaio.h is required.])
fi

if test "$have_libaio" = "yes"; then
  LIBS="$LIBS -laio"
fi

dnl ******************************
dnl Check for numactl
dnl ******************************

AC_PATH_PROG(BLKTRACE, blktrace, none)
AC_PATH_PROG(BLKPARSE, blkparse, none)
if test "$BLKTRACE" != "none" && test "$BLKPARSE" != "none"; then
  AC_DEFINE([HAVE_BLKTRACE], [1], [Define to 1 if blktrace command is available])
fi
AM_CONDITIONAL([WITH_BLKTRACE], [test "$BLKTRACE" != "none"])

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
    VMLINUX="/usr/lib/debug/boot/vmlinux-$(uname -r)"
  fi
fi

dnl **************************************************************
dnl Check for Cutter
dnl **************************************************************

m4_ifdef([AC_CHECK_CUTTER],
         [AC_CHECK_CUTTER],
         [ac_cv_use_cutter="no"])

m4_ifdef([AC_CHECK_GCUTTER],
         [AC_CHECK_GCUTTER],
         [ac_cv_use_gcutter="no"])

if test "$ac_cv_use_cutter" != "no" && test "$ac_cv_use_gcutter" != "no"; then
  have_cutter="yes"
else
  have_cutter="no"
fi

AM_CONDITIONAL([WITH_CUTTER], [test "$have_cutter" = "yes"])
if test "$have_cutter" = "yes"; then
  AC_DEFINE(WITH_CUTTER, 1, [Define to 1 if you use Cutter])
fi

dnl **************************************************************
dnl Check for GLib
dnl **************************************************************

PKG_CHECK_MODULES([GLIB],
                  [glib-2.0 gthread-2.0],
                  [ac_glib_2_0_available="yes"],
                  [ac_glib_2_0_available="no"])

if test "$ac_cv_use_cutter" != "no"; then
  if test "$ac_glib_2_0_available" = "no"; then
    AC_MSG_ERROR([GLib is required for testing with Cutter.])
  fi
fi
AM_CONDITIONAL([WITH_GLIB], [test "$ac_glib_2_0_available" = "yes"])

AC_OUTPUT

echo
echo "Configure Result:"
echo
echo "  CPU arch: $ac_arch"
echo
echo "== required headers and libraries =="
if test "$numa_arch" = "yes"; then
echo "  numa.h  : $have_numa_h"
echo "  libnuma : $have_libnuma"
fi
echo " libaio.h : $have_libaio_h"
echo "   libaio : $have_libaio"
echo
echo "== required commands =="
echo "  Ruby    : $RUBY"
if test "$numa_arch" = "yes"; then
echo "  numactl : $NUMACTL"
fi
echo
echo "== optional =="
echo "   blktrace : $BLKTRACE"
echo "   blkparse : $BLKPARSE"
echo "  OProfile  : $OPCONTROL"
echo "    vmlinux : $VMLINUX"
echo "     Cutter : $CUTTER"
echo "       GLib : $ac_glib_2_0_available"
echo
