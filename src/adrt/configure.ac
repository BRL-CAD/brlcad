#                    C O N F I G U R E . A C
# BRL-CAD / ADRT
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
# $Id$

AC_INIT
AC_CANONICAL_SYSTEM
AC_CONFIG_SRCDIR([rise/master/main.c])
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(rise, 0.1)

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_LONG_64_BITS

dnl autosearch /usr/local
AC_MSG_CHECKING([for /usr/local])
if test -d /usr/local/include ; then
	AC_MSG_RESULT([found, adding /usr/local to search paths])
	CPPFLAGS="$CPPFLAGS -I/usr/local/include"
	if test -d /usr/local/lib ; then
		LDFLAGS="$LDFLAGS -L/usr/local/lib"
	fi
else
	AC_MSG_RESULT([not found])
fi

dnl autosearch fink paths
AC_MSG_CHECKING([for fink in /sw])
if test -d /sw/include ; then
	AC_MSG_RESULT([found, adding /sw to search paths])
	CPPFLAGS="$CPPFLAGS -I/sw/include"
	if test -d /sw/lib ; then
		LDFLAGS="$LDFLAGS -L/sw/lib"
	fi
else
	AC_MSG_RESULT([not found])
fi

# mandatory headers
AC_CHECK_HEADERS([\
	arpa/inet.h\
	math.h\
	netdb.h\
	netinet/in.h\
	pthread.h\
	signal.h\
	stdio.h\
	stdlib.h\
	string.h\
	sys/select.h\
	sys/socket.h\
	time.h\
	unistd.h\
	],,
	AC_MSG_ERROR([Missing a necessary header.]))

# desired headers
AC_CHECK_HEADERS([\
		  getopt.h\
		  sys/sysctl.h\
		  sys/sysinfo.h\
		  ])


AC_MSG_CHECKING(whether to generate documentation with doxygen)
use_doxygen=no
AC_ARG_ENABLE(doxygen, AC_HELP_STRING(--enable-doxygen,
		[enable documentation generation with doxygen (default=no)]),
	[if test "xenable_doxygen" != "xno" ; then
		use_doxygen=yes
	fi]
)
AC_MSG_RESULT($use_doxygen)
found_doxygen=no
if test "x$use_doxygen" = "xyes" ; then
   AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
   echo "------------------- $DOXYGEN"
   if test "x$DOXYGEN" = "x" ; then
      found_doxygen=no
   else
      found_doxygen=yes
   fi
fi
echo "Found doxygen: $found_doxygen"
AM_CONDITIONAL(HAVE_DOXYGEN, [test "x$found_doxygen" != "xno"])


AC_MSG_CHECKING(xeon linux tuned optimization)
want_optim_xeon_linux=no
AC_ARG_ENABLE(optim-xeon-linux,
	AC_HELP_STRING(--enable-optim-xeon-linux, [Build with Intel P4-Xeon linux optimizations [default=no]]),
	[
		if test "x$enableval" = "xyes"
		then
			want_optim_xeon_linux=yes
		fi
	]
)
AC_MSG_RESULT($want_optim_xeon_linux)
if test "x$want_optim_xeon_linux" = "xyes"
then
	export CFLAGS="-O3 -Wall -Wmissing-prototypes -pedantic -std=c99 -msse -mfpmath=sse -ffast-math -fomit-frame-pointer -pipe"
	AC_DEFINE(TIE_SSE, 1, intel SSE simd shtuff)
fi


AC_MSG_CHECKING(xeon freebsd tuned optimization)
want_optim_xeon_freebsd=no
AC_ARG_ENABLE(optim-xeon-freebsd,
	AC_HELP_STRING(--enable-optim-xeon-freebsd, [Build with Intel P4-Xeon freebsd optimizations [default=no]]),
	[
		if test "x$enableval" = "xyes"
		then
			want_optim_xeon_freebsd=yes
		fi
	]
)
AC_MSG_RESULT($want_optim_xeon_freebsd)
if test "x$want_optim_xeon_freebsd" = "xyes"
then
	export CFLAGS="-O3 -Wall -Wmissing-prototypes -pedantic -std=c99 -ffast-math -fomit-frame-pointer -pipe"
fi


AC_MSG_CHECKING(ppc tuned optimization)
want_optim_ppc=no
AC_ARG_ENABLE(optim-ppc,
	AC_HELP_STRING(--enable-optim-ppc, [Build with PPC optimizations [default=no]]),
	[
		if test "x$enableval" = "xyes"
		then
			want_optim_ppc=yes
		fi
	]
)
AC_MSG_RESULT($want_optim_ppc)
if test "x$want_optim_ppc" = "xyes"
then
	export CFLAGS="-O3 -Wall -Wmissing-prototypes -pedantic -std=c99 -ffast-math -fomit-frame-pointer -pipe -faltivec"
fi


AC_MSG_CHECKING(prescott)
want_optim_prescott=no
AC_ARG_ENABLE(optim-prescott,
	AC_HELP_STRING(--enable-optim-prescott, [Build with P4 Prescott optimizations [default=no]]),
	[
		if test "x$enableval" = "xyes"
		then
			want_optim_prescott=yes
		fi
	]
)
AC_MSG_RESULT($want_optim_prescott)
if test "x$want_optim_prescott" = "xyes"
then
	export CFLAGS="-O3 -Wall -Wmissing-prototypes -pedantic -std=c99 -ffast-math -fomit-frame-pointer -pipe -mtune=prescott"
fi


AC_PATH_PROG(SDL_CONFIG_PROG, sdl-config)
if test "x$SDL_CONFIG_PROG" != "x" ; then
	AM_PATH_SDL(1.2.0,,AC_MSG_ERROR([Cannot find SDL.]))
fi
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)

AC_CHECK_FUNCS([drand48 getopt_long])

#AC_CHECK_LIB(m, sqrt, , AC_ERROR(no sqrt???))
AC_CHECK_LIB(z, compress, , AC_ERROR(libz is required.))


# Compile in BRL-CAD support if specified by user
with_brlcadpfx=no
AC_ARG_WITH(brlcad, AC_HELP_STRING(--with-brlcad=path,
		[Specify location of BRL-CAD Installation]),
	[
		BRLCAD_CFLAGS="-I$withval/include/brlcad"
		BRLCAD_LDFLAGS="-L$withval/lib -lbu -ltcl8.4 -lrt"
		AC_DEFINE(HAVE_BRLCAD,1,Checking for BRL-CAD Installation)
		with_brlcadpfx=yes
	]
)
AC_SUBST(BRLCAD_CFLAGS)
AC_SUBST(BRLCAD_LDFLAGS)
AM_CONDITIONAL(HAVE_BRLCAD, [test "x$with_brlcadpfx" != "xno"])

AC_CONFIG_FILES([Makefile libtie/Makefile libtienet/Makefile libcommon/Makefile librender/Makefile libtexture/Makefile libutil/Makefile rise/Makefile rise/master/Makefile rise/slave/Makefile rise/observer/Makefile])
AC_OUTPUT

echo
echo "CFLAGS: $CFLAGS"

# Local Variables:
# tab-width: 8
# mode: autoconf
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
