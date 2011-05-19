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
dnl Check for GLib
dnl ******************************

GLIB_REQUIRED=2.12.3
AC_SUBST(GLIB_REQUIRED)
AM_PATH_GLIB_2_0([],
                 [],
                 [AC_MSG_ERROR([GLib >= $GLIB_REQUIRED required.])],
                 [gmodule gobject gthread gio])

AC_OUTPUT