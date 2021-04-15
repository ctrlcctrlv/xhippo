dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([xhippo.c])
AC_CONFIG_HEADERS([config.h])

PACKAGE=xhippo
VERSION=4.0
ALL_LINGUAS="de"
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

AM_INIT_AUTOMAKE

dnl Checks for programs.
AC_PROG_CC

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.14.5])

AC_ARG_WITH(prefs-dialog,
[  --with-prefs-dialog     compile in support for experimental prefs dialog [default=yes]], AC_DEFINE(USEPREFS, 1, [Enable preferences dialog]))

dnl Checks for header files.
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T

dnl Checks for library functions.
AC_CHECK_FUNCS(strdup, , AC_MSG_ERROR(No strdup found. Unusual.))
AC_CHECK_FUNCS(fork, , AC_MSG_ERROR(No fork found.))
AC_CHECK_FUNCS(getopt_long, , AC_MSG_WARN(getopt_long not found. Long options will not work.) ; AC_DEFINE(NOGETOPTLONG, 1, [No getopt_long]) )
AC_CHECK_FUNCS(strcasecmp, , AC_DEFINE(NOSTRCASECMP, 1, [No strcasecmp]))
AC_CHECK_FUNCS(getopt, , AC_MSG_ERROR(Ack! You don't even have getopt!))
AC_CHECK_FUNCS(snprintf, , AC_MSG_WARN(snprintf not found. Range checks will be disabled.))
AC_CHECK_FUNCS(nftw, , AC_MSG_WARN(nftw not found. Add Directory will be disabled.))

PKG_CHECK_MODULES(GLIB2, glib-2.0 >= 2.0.0)
PKG_CHECK_MODULES(GTK2, gtk+-2.0 >= 2.0.0)

ID3LIBS=""
AC_CHECK_LIB(id3tag, id3_file_tag, ID3LIBS="$ID3LIBS -lid3tag -lz"; AC_DEFINE(HAVE_LIBID3TAG, 1, [Have libid3tag]), , -lz)
AC_SUBST(ID3LIBS)

I18N_CFLAGS='-I$(top_srcdir)/intl'
I18N_LIBS=$INTLLIBS
AC_SUBST(I18N_CFLAGS)
AC_SUBST(I18N_LIBS)

dnl This is *not* the right way to do this, but GNOME's native
dnl autoconf support is extremely baroque.

AC_CONFIG_FILES([Makefile po/Makefile.in doc/Makefile])
AC_OUTPUT
