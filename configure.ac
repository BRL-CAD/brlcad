dnl                   C O N F I G U R E . A C
dnl BRL-CAD
dnl
dnl Copyright (C) 2004-2005 United States Government as represented by
dnl U.S. Army Research Laboratory.
dnl
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the GNU General Public License as
dnl published by the Free Software Foundation; either version 2 of the
dnl License, or (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this file; see the file named COPYING for more
dnl information.
dnl
dnl $Id$
dnl
dnl ******************************************************************
dnl ***                  BRL-CAD's configure.ac                    ***
dnl ******************************************************************
dnl
dnl Herein lies the venerable GNU Autotools configure template for
dnl BRL-CAD.  As best is reasonably possible, proper ordering and
dnl separation of tests and settings should be maintained per the
dnl recommended standard layout.  The tests should be added to the
dnl rather clearly labeled sections below so that they are as follows:
dnl
dnl     0) information on the package
dnl     1) check command-line arguments
dnl     2) check programs
dnl     3) check libraries
dnl     4) check headers
dnl     5) check types/structures
dnl     6) check compiler characteristics
dnl     7) check functions
dnl     8) check system services
dnl     9) output a summary
dnl
dnl Any useful build settings should be added to the output summary
dnl at the end.  Although it may be possible to check for certain 
dnl features near the top in the command-line arguments section, any
dnl related tests should be delayed and placed into the appropriate
dnl check section.
dnl
dnl You should use enable/disable arguments for build settings and
dnl optional compilation components that are part of this package.
dnl You specify with/without arguments for components that are not a
dnl part of this package.
dnl
dnl Features of the GNU Autotools that would require an increase in
dnl the minimum version requirement are highly discouraged.  Likewise
dnl discouraged is rolling back support for versions released prior
dnl to the AC_PREREQ version shown below.
dnl
dnl Strive to tame the chaos that is so easily achieved.
dnl

AC_PREREQ(2.52)

AC_INIT(BRL-CAD, 7.2.1, [http://brlcad.org], brlcad)
AC_REVISION($Revision$)
AC_CONFIG_AUX_DIR(misc)

dnl See HACKING for details on how to properly update the version
MAJOR_VERSION=7
MINOR_VERSION=2
PATCH_VERSION=1
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(PATCH_VERSION)

dnl just in case
PACKAGE_NAME="BRL-CAD"
AC_SUBST(PACKAGE_NAME)

# force locale setting to C so things like date output as expected
LC_ALL=C

CONFIG_DATE=`date +%Y%m%d`
CONFIG_TIME="`date '+%H %M %S'`"
BRLCAD_VERSION="$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION"

# print out the title with a pretty box computed to wrap around
title="Configuring BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE"
length="`echo x${title}x | wc -c`"
separator=""
while test $length -gt 1 ; do
      separator="${separator}*"
      length="`expr $length - 1`"
done
echo "***${separator}***"
echo "*** ${title} ***"
echo "***${separator}***"

AC_SUBST(CONFIG_DATE)
AC_SUBST(CONFIG_TIME)
AC_SUBST(BRLCAD_VERSION)

# necessary for proper file creation on nfs volumes
umask 022

# classify this machine
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# am_init_automake performs a ac_prog_install check so need to handle
# problematic /usr/brl/bin/install on irix
if test "x$build_vendor" = "xsgi" ; then
   PATH="`echo $PATH | sed 's/\/brl\/bin/bin/g'`"
fi

# where are we coming from and where are we going to.
# prefix is set to NONE until AC_OUTPUT unless --prefix is used
AC_CONFIG_SRCDIR([src/librt/bool.c])
AC_PREFIX_DEFAULT([/usr/brlcad])
AC_MSG_CHECKING([where BRL-CAD is to be installed])
if test "x$prefix" = "xNONE" ; then
	prefix="$ac_default_prefix"
fi
AC_DEFINE_UNQUOTED([BRLCAD_ROOT], "${prefix}", "Location where BRL-CAD is installed")
AC_MSG_RESULT($prefix)
if test ! "x$BRLCAD_ROOT" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([BRLCAD_ROOT should only be used to override an install directory at runtime])
	AC_MSG_WARN([BRLCAD_ROOT is presently set to "${BRLCAD_ROOT}"])
	AC_MSG_NOTICE([It is highly recommended that BRLCAD_ROOT be unset and not used])
	if test "x$BRLCAD_ROOT" = "x$prefix" ; then
		AC_MSG_WARN([BRLCAD_ROOT is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable BRLCAD_ROOT conflicts with --prefix ***])
	fi
fi

AC_MSG_CHECKING([whether dependency tracking should be enabled])
if test -d $srcdir/CVS ; then
	AC_MSG_RESULT(it should)
	AM_INIT_AUTOMAKE
else
	AC_MSG_RESULT(it should not)
	AM_INIT_AUTOMAKE([no-dependencies])
fi
AM_CONFIG_HEADER([include/config.h])

# a secondary automated automake may blow away COPYING & INSTALL due
# to an automake bug in older version, so restore those files if
# needed.
BC_RESTORE_CLOBBERED([COPYING INSTALL], [misc])

# automatically enable and load our configure cache file if available
BC_CONFIG_CACHE([config.cache.${host_os}.${ac_hostname}])

dnl be sure to search these directories for components
BC_SEARCH_DIRECTORY([/usr/X11R6], [X11])

dnl search several additional locations if configure happens to fail
dnl to find a required component
BC_RETRY_DIRECTORY([/usr/freeware], [freeware])
BC_RETRY_DIRECTORY([/usr/local])
BC_RETRY_DIRECTORY([/opt/X11R6], [X11])
BC_RETRY_DIRECTORY([/opt/X11], [X11])
BC_RETRY_DIRECTORY([/usr/X11], [X11])
BC_RETRY_DIRECTORY([/opt])
BC_RETRY_DIRECTORY([/sw], [fink])

AC_DEFINE(DM_X,1,[Display for X])
AC_DEFINE(DM_OGL,1,[yeah, GL])


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

AC_MSG_CHECKING([... a r g u m e n t s ])
AC_MSG_RESULT([])

dnl Arguments that are auto should check both yes and no values,
dnl otherwise there only needs to be a check for not no to imply a yes
dnl value.  Arguments should only set their flag as to whether they
dnl are enabled -- usage of that flag should be handled by the 
dnl appropriate section further below.

# Allow user to specify flags without relying on environment
AC_ARG_WITH(cflags, AC_HELP_STRING(--with-cflags,
		[Specify additional flags to pass to the C compiler]),
	[
		if test "x$withval" != "xno" ; then
			CFLAGS="$CFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(cppflags, AC_HELP_STRING(--with-cppflags,
		[Specify additional flags to pass to C preprocessor]),
	[
		if test "x$withval" != "xno"; then
			CPPFLAGS="$CPPFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(ldflags, AC_HELP_STRING(--with-ldflags,
		[Specify additional flags to pass to linker]),
	[
		if test "x$withval" != "xno" ; then
			LDFLAGS="$LDFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(libs, AC_HELP_STRING(--with-libs,
		[Specify additional libraries to link against]),
	[
		if test "x$withval" != "xno" ; then
			LIBS="$LIBS $withval"
		fi
	]	
)

# use the jdk
JAVA_CFLAGS=
JAVA_LDFLAGS=
with_java=no
AC_ARG_WITH(jdk, AC_HELP_STRING(--with-jdk=path,
		[Specify location of Java Development Kit]),
	[
		JAVA_CFLAGS="-I$withval/include -I$withval/include/`uname | tr [A-Z] [a-z]`"
		with_java=yes
        ]
)

case "$target" in
	*-*-darwin*)
		JAVA_CFLAGS="-framework JavaVM"
		JAVA_LDFLAGS="-shrext .jnilib -framework JavaVM"
		with_java=yes
		;;
esac

AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)

# only build benchmark
AC_MSG_CHECKING(whether to only build benchmark)
only_benchmark=no
AC_ARG_ENABLE(only-benchmark, AC_HELP_STRING(--enable-only-benchmark,
		[Only build the components necessary for the benchmark suite (default=no)]),
	[if test "x$enableval" != "xno" ; then
		only_benchmark=yes
	fi]
)
AC_MSG_RESULT($only_benchmark)

# only build librtserver
AC_MSG_CHECKING(whether to only build librtserver)
only_rts=no
warn_only_both=no
AC_ARG_ENABLE(only-rtserver, AC_HELP_STRING(--enable-only-rtserver,
		[Only build the components necessary for the raytrace server (default=no)]),
	[if test "x$enableval" != "xno" ; then
		only_rts=yes
		if test "x$only_benchmark" != "xno" ; then
			warn_only_both=yes
		fi
	fi]
)
AC_MSG_RESULT($only_rts)
if test "x$warn_only_both" != "xno" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([Requesting BOTH benchmark and librtserver "only"?])
	AC_MSG_WARN([Building both benchmark and librtserver])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
fi


# run-time debugging 
dnl has aliases
build_runtime_debug=yes
AC_ARG_ENABLE(runtime-debug, AC_HELP_STRING(--disable-runtime-debug,
		[disable runtime debug for performance (default=no)]),
	[if test "x$enableval" = "xno" ; then
		build_runtime_debug=no
	elif test "x$enableval" = "xyes" ; then
		build_runtime_debug=yes
	fi]
)

# enable 64-bit builds
dnl has aliases
build_64bit=auto
AC_ARG_ENABLE(64bit-build, AC_HELP_STRING(--enable-64bit-build,
		[enable 64-bit compilation mode (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_64bit=no
	elif test "x$enableval" = "xyes" ; then
		build_64bit=yes
	fi]
)

# force enable/disable building of libregex
dnl build_regex has alias(es)
build_regex=auto
AC_ARG_ENABLE(regex-build, AC_HELP_STRING(--enable-regex-build,
		[Build and use the included regular expression library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_regex=no
	elif test "x$enableval" = "xyes" ; then
		build_regex=yes
	fi]
)

# force enable/disable building of libpng
dnl build_png has alias(es)
build_png=auto
AC_ARG_ENABLE(png-build, AC_HELP_STRING(--enable-png-build,
		[Build and use the included Portable Network Graphics library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_png=no
	elif test "x$enableval" = "xyes" ; then
		build_png=yes
	fi]
)

# force enable/disable building of zlib
dnl build_zlib has alias(es)
build_zlib=auto
AC_ARG_ENABLE(zlib-build, AC_HELP_STRING(--enable-zlib-build,
		[Build and use the included zlib compression library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_zlib=no
	elif test "x$enableval" = "xyes" ; then
		build_zlib=yes
	fi]
)

# doxygen
AC_MSG_CHECKING(whether to generate documentation with doxygen)
use_doxygen=no
AC_ARG_ENABLE(doxygen, AC_HELP_STRING(--enable-doxygen,
		[enable documentation generation with doxygen (default=no)]),
	[if test "xenable_doxygen" != "xno" ; then
		use_doxygen=yes
	fi]
)
AC_MSG_RESULT($use_doxygen)

# jove
AC_MSG_CHECKING(whether to compile jove or not)
build_jove=no
AC_ARG_ENABLE(jove, AC_HELP_STRING(--enable-jove,
		[enable compilation of jove (default=no)]),
	[if test "x$enable_jove" != "xno" ; then
		build_jove=yes
	fi]
)
AC_MSG_RESULT($build_jove)

# optimized
dnl optimized has aliases
use_optimized=no
AC_ARG_ENABLE(optimized, AC_HELP_STRING(--enable-optimized,
		[enable optimized compilation (default=no)]),
	[if test "x$enable_optimized" != "xno" ; then
		use_optimized=yes
	fi]
)

# warnings
dnl warnings has aliases
use_warnings=no
AC_ARG_ENABLE(warnings, AC_HELP_STRING(--enable-warnings,
		[enable verbose compilation warnings (default=no)]),
	[if test "x$enable_warnings" != "xno" ; then
		use_warnings=yes
	fi]
)

# debug
dnl debug has aliases
use_debug=yes
AC_ARG_ENABLE(debug, AC_HELP_STRING(--disable-debug,
		[disable debug symbols (default=no)]),
	[if test "x$disable_debug" != "xno" ; then
		use_debug=no
	fi]
)

# profiling
dnl profiling has aliases
use_profiling=no
AC_ARG_ENABLE(profiling, AC_HELP_STRING(--enable-profiling,
		[enable profiling (default=no)]),
	[if test "x$enable_profiling" != "xno" ; then
		use_profiling=yes
	fi]
)


###
# argument aliases
# they need to go below here in order for --help to consolidate the
# blank line to exactly one line in the right place
###

# run-time debugging
AC_ARG_ENABLE(run-time-debug,,
	[if test "x$enable_run_time_debug" = "xno" ; then
		build_runtime_debug=no
	elif test "x$enable_run_time_debug" = "xyes" ; then
		build_runtime_debug=yes
	fi]
)
AC_ARG_ENABLE(runtime-debugging,,
	[if test "x$enable_run_time_debug" = "xno" ; then
		build_runtime_debug=no
	elif test "x$enable_run_time_debug" = "xyes" ; then
		build_runtime_debug=yes
	fi]
)
AC_ARG_ENABLE(run-time-debugging,,
	[if test "x$enable_run_time_debug" = "xno" ; then
		build_runtime_debug=no
	elif test "x$enable_run_time_debug" = "xyes" ; then
		build_runtime_debug=yes
	fi]
)

# 64-bit compilation
AC_ARG_ENABLE(64bit,,
	[if test "x$enable_64bit" = "xno" ; then
		build_64bit=no
	elif test "x$enable_64bit" = "xyes" ; then
		build_64bit=yes
	fi]
)
AC_ARG_ENABLE(64-bit,,
	[if test "x$enable_64bit" = "xno" ; then
		build_64bit=no
	elif test "x$enable_64bit" = "xyes" ; then
		build_64bit=yes
	fi]
)
AC_ARG_ENABLE(64-bit-build,,
	[if test "x$enable_64bit" = "xno" ; then
		build_64bit=no
	elif test "x$enable_64bit" = "xyes" ; then
		build_64bit=yes
	fi]
)
AC_MSG_CHECKING(whether to compile in 64-bit mode)
AC_MSG_RESULT($build_64bit)

# alias to force building libregex
AC_ARG_ENABLE(regex,,
	[if test "x$enableval" = "xno" ; then
		build_regex=no
	elif test "x$enableval" = "xyes" ; then
		build_regex=yes
	fi]
)
AC_MSG_CHECKING(whether to build the regular expression library)
AC_MSG_RESULT($build_regex)

# alias to force building libpng
AC_ARG_ENABLE(png,,
	[if test "x$enableval" = "xno" ; then
		build_png=no
	elif test "x$enableval" = "xyes" ; then
		build_png=yes
	fi]
)
AC_MSG_CHECKING(whether to build the PNG library)
AC_MSG_RESULT($build_png)

# alias to force building zlib
AC_ARG_ENABLE(zlib,,
	[if test "x$enableval" = "xno" ; then
		build_zlib=no
	elif test "x$enableval" = "xyes" ; then
		build_zlib=yes
	fi]
)
AC_ARG_ENABLE(libz,,
	[if test "x$enableval" = "xno" ; then
		build_zlib=no
	elif test "x$enableval" = "xyes" ; then
		build_zlib=yes
	fi]
)
AC_ARG_ENABLE(libz-build,,
	[if test "x$enableval" = "xno" ; then
		build_zlib=no
	elif test "x$enableval" = "xyes" ; then
		build_zlib=yes
	fi]
)
AC_MSG_CHECKING(whether to build the data compression library)
AC_MSG_RESULT($build_zlib)

# optimized alias
AC_ARG_ENABLE(optimization,,
	[if test "x$enable_optimization" != "xno" ; then
		use_optimized=yes
	fi]
)
AC_ARG_ENABLE(optimizations,,
	[if test "x$enable_optimizations" != "xno" ; then
		use_optimized=yes
	fi]
)
AC_MSG_CHECKING(whether to enable optimized compilation)
AC_MSG_RESULT($use_optimized)

# debug alias
AC_ARG_ENABLE(debugging,,
	[if test "x$disable_debug" != "xno" ; then
		use_debug=no
	fi]
)
AC_MSG_CHECKING(whether to disable debug mode compilation)
AC_MSG_RESULT($use_debug)

# profiling alias
AC_ARG_ENABLE(profile,,
	[if test "x$enable_profile" != "xno" ; then
		use_profiling=yes
	fi]
)
AC_ARG_ENABLE(profiled,,
	[if test "x$enable_profiled" != "xno" ; then
		use_profiling=yes
	fi]
)
AC_MSG_CHECKING(whether to enable profile mode compilation)
AC_MSG_RESULT($use_profiling)

# verbose warnings alias
AC_ARG_ENABLE(verbose-warnings,,
	[if test "x$enable_verbose_warnings" != "xno" ; then
		use_warnings=yes
	fi]
)
AC_ARG_ENABLE(warning,,
	[if test "x$enable_warning" != "xno" ; then
		use_warnings=yes
	fi]
)
AC_MSG_CHECKING(whether to enable verbose compilation warnings)
AC_MSG_RESULT($use_warnings)


# override the default autoconf cflags if user has not modified them
if test "x$CFLAGS" = "x" ; then
	# an unset CFLAGS var is set to "-O2 -g" during AC_PROG_CC, so
	# set it to something benign
	CFLAGS=" "
fi

# cannot override LD directly, so warn about that (configure sets it)
if test "x$LD" != "x" ; then
	AC_MSG_WARN([LD cannot be set directly yet it seems to be set ($LD)])
fi


dnl **************************
dnl *** Check for programs ***
dnl **************************

AC_MSG_CHECKING([... p r o g r a m s ])
AC_MSG_RESULT([])

AC_LANG(C)

AC_AIX
AC_GNU_SOURCE
AC_MINIX

AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CPP
AC_REQUIRE_CPP
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_LEX
AC_PROG_YACC
AM_PROG_MKDIR_P

# XXX libtool has a bug that causes a /lib/cpp sanity check failure if
# a C++ compiler is not installed.  This makes the sanity test pass
# regardless of whether there is a c++ compiler
if test "x$CXXCPP" = "x" ; then
	if test "x$CPP" = "x" ; then
		CXXCPP="cpp"
	else
		CXXCPP="$CPP"
	fi
fi

# XXX libtool shouldn't be generated until after LD is set
# dnl went poof in libtool 1.9 -- AC_PROG_LIBTOOL
# LT_INIT
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

# AN_MAKEVAR([AR], [BC_PROG_AR])
# AN_PROGRAM([ar], [BC_PROG_AR])
# AC_DEFUN([BC_PROG_AR], [AC_CHECK_TOOL(AR, ar, :)])
# BC_PROG_AR

# XXX tests for ranlib may be required.  "ar ts" was used in cake for
# sgi and aix so more compensation may be required still.

dnl May need AR so try to find it
AC_PATH_PROG(AR, ar, [], $PATH:/usr/bin:/usr/local/bin:/usr/ccs/bin)
AC_SUBST(AR)

dnl check for basic shell facilities
AC_PATH_PROG(SH, sh)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(RM, rm)

dnl figure out which compiler is seems like we are using
dnl detected values include: gcc, mipspro, sunpro
dnl XXX -- none of the tests should be compiler-specific, but it's good to know in the log
compiler=unknown

dnl Determine whether we are using GCC
if test "x$GCC" = "xyes" ; then
	compiler=gcc
fi

dnl Determine whether we are using the MIPSpro compiler on SGI systems
if test "x$build_vendor" = "xsgi" ; then
	AC_MSG_CHECKING([whether we are using the MIPSpro compiler])
	using_mipspro=no
	AC_LINK_IFELSE([#if defined(sgi) && defined(_COMPILER_VERSION) && !defined(__GNUC__)
If we get here, we are using the MIPSPro compiler (and the link should fail).
#endif
int main() { return 0; }
	], [using_mipspro=no], [using_mipspro=yes])
	AC_MSG_RESULT($using_mipspro)
	if test "x$using_mipspro" = "xyes" ; then
		compiler=mipspro
	fi
fi

dnl Determine whether we are using the SUNWspro Sun Workshop compiler
if test "x$build_vendor" = "xsun" ; then
	AC_MSG_CHECKING([whether we are using the SUNWspro compiler])
	using_sunpro=no
	AC_LINK_IFELSE([#if defined(sun) && defined(__SUNPRO_C) && !defined(__GNUC__)
If we get here, we are using the SUNWspro compiler (and the link should fail).
#endif
int main() { return 0; }
	], [using_sunpro=no], [using_sunpro=yes])
	AC_MSG_RESULT($using_sunpro)
	if test "x$using_sunpro" = "xyes" ; then
		AR="CC -xar"
		LD="CC -G"
		export AR LD
		compiler=sunpro
	fi
fi

AC_DEFINE(EXTERN_STDOUT,,[Stdout is extern])

if test "x$use_doxygen" = "xyes" ; then
	AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
	if test "x$DOXYGEN" = "x" ; then
		BC_RETRY_CONFIGURE([$0])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_ERROR([*** Unable to find doxygen ***])
	fi
fi
AC_MSG_CHECKING(whether to enable doxygen)
AC_MSG_RESULT($use_doxygen)


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

AC_MSG_CHECKING([... l i b r a r i e s ])
AC_MSG_RESULT([])

dnl check if there is a threading library that links
thread_link_works=no
LIBTHREAD=""
dnl try sgi sproc first
AC_CHECK_LIB(c, sproc, thread_link_works=yes ; LIBTHREAD="-lc",
    dnl check for pthreading as next best option
    dnl XXX need to test HPUX -- may require -lcma with pthreads
    [AC_CHECK_LIB(pthread, pthread_create, thread_link_works=yes ; LIBTHREAD="-lpthread",
    	dnl check if the BSD c_r threading library link works
	[AC_CHECK_LIB(c_r, pthread_create, thread_link_works=yes ; LIBTHREAD="-pthread",
	    dnl try solaris threading
	    [AC_CHECK_LIB(thread, thr_create, thread_link_works=yes ; LIBTHREAD="-lthread")]
	)]
    )]
)

dnl need curses, would prefer ncurses
curses_lib_works=no
CURSES=""
AC_CHECK_LIB(ncurses, tputs, curses_lib_works=yes ; CURSES="-lncurses",
    [AC_CHECK_LIB(curses, tputs, curses_lib_works=yes ; CURSES="-lcurses",
	[AC_CHECK_LIB(termlib, tputs, curses_lib_works=yes ; CURSES="-ltermlib",
	    [AC_CHECK_LIB(termcap, tputs, curses_lib_works=yes ; CURSES="-ltermcap")]
	)]
    )]
)

dnl check if X11 library link works
x11_link_works=no
AC_CHECK_LIB(X11, XCreateWindow, x11_link_works=yes)

xext_link_works=no
AC_CHECK_LIB(Xext, XShapeCombineMask, xext_link_works=yes)

xi_link_works=no
AC_CHECK_LIB(Xi, XGetExtensionVersion, xi_link_works=yes)

ice_link_works=no
AC_CHECK_LIB(ICE, IceOpenConnection, ice_link_works=yes)

sm_link_works=no
AC_CHECK_LIB(SM, SmcOpenConnection, sm_link_works=yes)

xt_link_works=no
AC_CHECK_LIB(Xt, XtMalloc, xt_link_works=yes)

xmu_link_works=no
AC_CHECK_LIB(Xmu, XmuLookupStandardColormap, xmu_link_works=yes)

dnl detect a useable system regular expression library
regex_link_works=no
REGEX=""
AC_CHECK_LIB(c, regcomp, regex_link_works=yes ; REGEX="-lc",
    [AC_CHECK_LIB(regex, regcomp, regex_link_works=yes ; REGEX="-lregex",
	[AC_CHECK_LIB(compat, regcomp, regex_link_works=yes ; REGEX="-lcompat")]
    )]
)

dnl check if zlib library links
z_link_works=no
AC_CHECK_LIB(z, deflate, z_link_works=yes)

dnl check if a png library links
png_link_works=no
AC_CHECK_LIB(png, png_access_version_number, png_link_works=yes)

dnl check if a math library links
m_link_works=no
LIBM=""
AC_CHECK_LIB(m, cos, m_link_works=yes ; LIBM="-lm")

dnl check if a socket library links
socket_link_works=no
LIBSOCKET=""
AC_CHECK_LIB(socket, socket, socket_link_works=yes ; LIBSOCKET="-lsocket")

dnl check if a network socket library links
nsl_link_works=no
LIBNSL=""
AC_CHECK_LIB(nsl, gethostbyaddr, nsl_link_works=yes)

dnl check if a malloc library links
malloc_link_works=no
LIBMALLOC=""
AC_CHECK_LIB(c, mallopt, malloc_link_works=yes ; LIBMALLOC="-lc",
    [AC_CHECK_LIB(malloc, mallopt, malloc_link_works=yes ; LIBMALLOC="-lmalloc")]
)

dnl check if a mx library links
mx_link_works=no
LIBMX=""
AC_CHECK_LIB(mx, matherr, mx_link_works=yes ; LIBMX="-lmx")

dnl check if a dynamic link library links
dl_link_works=no
LIBDL=""
AC_CHECK_LIB(dl, dlopen, dl_link_works=yes ; LIBDL="-ldl")

dnl check if a graphics library links
gl_link_works=no
LIBGL=""
AC_CHECK_LIB(GL, glEnable, gl_link_works=yes ; LIBGL="-lGL",
    [AC_CHECK_LIB(GL2, glEnable, gl_link_works=yes ; LIBGL="-lGL2")]
)

dnl check if the sgigl library links
sgigl_link_works=no
LIBSGIGL=""
AC_CHECK_LIB(gl, getvideo, sgigl_link_works=yes ; LIBSGIGL="-lgl")


dnl *************************
dnl *** Check for headers ***
dnl *************************

AC_MSG_CHECKING([... h e a d e r s ])
AC_MSG_RESULT([])

AC_HEADER_STDC
AC_PATH_X
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS( \
	GL/gl.h \
	GL/glx.h \
	X11/Xlib.h \
	arpa/inet.h \
	curses.h \
	errno.h \
	fcntl.h \
	float.h \
	getopt.h \
	gl/device.h \
	langinfo.h \
	limits.h \
	locale.h \
	malloc.h \
	math.h \
	memory.h \
	ncurses.h \
	netdb.h \
	netinet/in.h \
	pthread.h \
	pwd.h \
	sgtty.h \
	socket.h \
	stdarg.h \
	stddef.h \
	stdlib.h \
	string.h \
	strings.h \
	sys/errno.h \
	sys/file.h \
	sys/_ioctl.h \
	sys/ioctl.h \
	sys/ioctl_compat.h \
	sys/machd.h \
	sys/mman.h \
	sys/param.h \
	sys/select.h \
	sys/sched.h \
	sys/socket.h \
	sys/stat.h \
	sys/statfs.h \
	sys/sysmp.h \
	sys/time.h \
	sys/timeb.h \
	sys/types.h \
	sys/unistd.h \
	sys/wait.h \
	syslog.h \
	termcap.h \
	termio.h \
	termios.h \
	time.h \
	ulocks.h \
	unistd.h \
	utime.h \
	values.h \
	varargs.h \
	wchar.h \
	xosdefs.h \
)
AC_CHECK_HEADER(sys/stat.h, AC_DEFINE(HAVE_UNIX_IO,, Fix this, sys/types.h and sys/stat.h ...))
AC_CHECK_HEADER(sys/time.h, AC_DEFINE(TIME_WITH_SYS_TIME,1,sys/time.h))

AC_CHECK_HEADER(sys/mount.h,
		AC_DEFINE(HAVE_SYS_MOUNT_H,1, Define to 1 if you have the <sys/mount.h> header file.),,
		[
#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#include <sys/mount.h>
])

AC_CHECK_HEADER(term.h,
		AC_DEFINE(HAVE_TERM_H, 1, Define to 1 if you have the <term.h> header file.),,
		[
#ifdef HAVE_CURSES_H
#  include <curses.h>
#endif
#include <term.h>
])

found_regex_h=no
AC_CHECK_HEADER(regex.h, 
		AC_DEFINE(HAVE_REGEX_H,1,[Define to 1 if you have the '<regex.h>' header file])
		found_regex_h=yes )

found_jni_h=no
AC_CHECK_HEADER(JavaVM/jni.h, 
		AC_DEFINE(HAVE_JAVAVM_JNI_H,1,[Define to 1 if you have the '<JavaVM/jni.h>' header file])
		found_jni_h=yes	)
AC_CHECK_HEADER(jni.h,
		AC_DEFINE(HAVE_JNI_H,1,[Define to 1 if you have the '<jni.h>' header file])
		found_jni_h=yes )
search_for_jni_h_more=no
if test "x$found_jni_h" = "xno" ; then
	if test "x$with_java" != "xno" ; then
		with_java=no
		search_for_jni_h_more=yes
		if test "x$only_benchmark" != "xno" ; then
			if test "x$only_rts" = "xno" ; then
				search_for_jni_h_more=no
			fi
		fi
	fi
else
	# even though --with-jdk was not given, header was found and is needed
	if test "x$only_rts" != "xno" ; then
		with_java=yes
	fi
fi
if test "x$search_for_jni_h_more" != "xno" ; then
	# darwin systems may have a prior jni.h stashed away elsewhere
	pre_cppflags="$CPPFLAGS"
	macjvmdir=/System/Library/Frameworks/JavaVM.framework/Versions
	if test -d "$macjvmdir"  ; then
		for dir in `ls $macjvmdir` ; do
			if test -d "$macjvmdir/$dir" ; then
				CPPFLAGS="-I$macjvmdir/$dir/Headers $CPPFLAGS"
			fi
		done
	fi

	# try one last time to find jni.h (must disable cached result)
	unset ac_cv_header_jni_h
	AC_CHECK_HEADER(jni.h,
			AC_DEFINE(HAVE_JNI_H,
				  1,
				  [Define to 1 if you have the '<jni.h>' header file])
			found_jni_h=yes )

	if test "x$found_jni_h" != "xno" ; then
		with_java=yes
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([jni.h was found by hunting through the JavaVM framework])
		AC_MSG_WARN([This means you probably need to install the latest Java SDK])
		AC_MSG_WARN([Build will try to use what it found, but you should update])
		AC_MSG_NOTICE([See http://developer.apple.com/java/ for updates])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		CPPFLAGS="$pre_cppflags"
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Code dependant upon java and jni.h will not be built])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	fi
fi


dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

AC_MSG_CHECKING([... t y p e s ])
AC_MSG_RESULT([])

AC_C_CHAR_UNSIGNED
AC_HEADER_STAT
AC_HEADER_STDBOOL
AC_HEADER_TIME
AC_STRUCT_ST_BLOCKS
dnl Tcl still uses HAVE_ST_BLKSIZE
AC_STRUCT_ST_BLKSIZE
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_STRUCT_TM
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

dnl XXX - union wait testing from tcl8.4's configure.in, should be revamped.
AC_MSG_CHECKING([union wait])
AC_CACHE_VAL(tcl_cv_union_wait,
    AC_TRY_LINK([#include <sys/types.h>
#include <sys/wait.h>], [
union wait x;
WIFEXITED(x);           /* Generates an error if WIFEXITED
                         * uses an int. */
    ], tcl_cv_union_wait=yes, tcl_cv_union_wait=no))
AC_MSG_RESULT($tcl_cv_union_wait)
if test $tcl_cv_union_wait = no; then
    AC_DEFINE(NO_UNION_WAIT,, From Tcl -- Lacking a "union wait")
fi

dnl XXX -- need to really check for time zone
AC_DEFINE(HAVE_TM_ZONE,,string in time zone)

dnl XXX -- necessary checks from tcl8.4 tcl.m4
builtin(include,src/other/libtcl/unix/tcl.m4)
SC_SERIAL_PORT
SC_TCL_64BIT_FLAGS


dnl ******************************************
dnl *** Check for compiler characteristics ***
dnl ******************************************

AC_MSG_CHECKING([... c o m p i l e r ])
AC_MSG_RESULT([])

dnl include the BRL-CAD include directory
CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/include"

dnl try to use -pipe to speed up the compiles
pipe_flag="-pipe"
AC_MSG_CHECKING([if the compiler understands $pipe_flag])
pipe_flag_works=yes
PRECFLAGS="$CFLAGS"
PRELDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $pipe_flag"
LDFLAGS="$LDFLAGS $pipe_flag"
AC_TRY_COMPILE( [], [], [], [pipe_flag_works=no])
AC_MSG_RESULT($pipe_flag_works)
if test "x$pipe_flag_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
	LDFLAGS="$PRELDFLAGS"
fi

dnl check for -fno-strict-aliasing
fno_strict_aliasing_flag="-fno-strict-aliasing"
AC_MSG_CHECKING([if the compiler understands $fno_strict_aliasing_flag])
fno_strict_aliasing_flag_works=yes
PRECFLAGS="$CFLAGS"
PRELDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $fno_strict_aliasing_flag"
LDFLAGS="$LDFLAGS $fno_strict_aliasing_flag"
AC_TRY_COMPILE( [], [], [], [fno_strict_aliasing_flag_works=no])
AC_MSG_RESULT($fno_strict_aliasing_flag_works)
if test "x$fno_strict_aliasing_flag_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
	LDFLAGS="$PRELDFLAGS"
fi

dnl check for -fno-common (libtcl needs it on darwin)
fno_common_flag="-fno-common"
AC_MSG_CHECKING([if the compiler understands $fno_common_flag])
fno_common_flag_works=yes
PRECFLAGS="$CFLAGS"
PRELDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $fno_common_flag"
LDFLAGS="$LDFLAGS $fno_common_flag"
AC_TRY_COMPILE( [], [], [], [fno_common_flag_works=no])
AC_MSG_RESULT($fno_common_flag_works)
if test "x$fno_common_flag_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
	LDFLAGS="$PRELDFLAGS"
fi

dnl check for -fPIC (libtcl needs it on altix)
fPIC_flag="-fPIC"
AC_MSG_CHECKING([if the compiler understands $fPIC_flag])
fPIC_flag_works=yes
PRECFLAGS="$CFLAGS"
PRELDFLAGS="$LDFLAGS"
CFLAGS="$CFLAGS $fPIC_flag"
LDFLAGS="$LDFLAGS $fPIC_flag"
AC_TRY_COMPILE( [], [], [], [fPIC_flag_works=no])
AC_MSG_RESULT($fPIC_flag_works)
if test "x$fPIC_flag_works" = "xno" ; then
	CFLAGS="$PRECFLAGS"
	LDFLAGS="$PRELDFLAGS"
fi

dnl check for -search_paths_first linker flag when making dylibs
search_paths_first_flag="-Wl,-search_paths_first -mdynamic-no-pic"
AC_MSG_CHECKING([if the compiler understands $search_paths_first_flag])
search_paths_first_flag_works=yes
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $search_paths_first_flag"
AC_TRY_LINK( [], [], [], [search_paths_first_flag_works=no])
AC_MSG_RESULT($search_paths_first_flag_works)
if test "x$search_paths_first_flag_works" = "xno" ; then
	LDFLAGS="$PRELDFLAGS"
fi

dnl 64bit compilation flags
if test "x$build_64bit" = "xyes" ; then
	found_64bit_flag=no

	AC_MSG_CHECKING([if configure snuck on a 32bit flag to ld])
	PRELD="$LD"
	LD="`echo $LD | sed 's/32/64/'`"
	if test "x$LD" = "x$PRELD" ; then
		AC_MSG_RESULT(no)
	else
		AC_MSG_RESULT(yes)
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		flag_64="-64"
		AC_MSG_CHECKING([if the compiler understands $flag_64])
		flag_64_works=yes
		PRECFLAGS="$CFLAGS"
		PRELDFLAGS="$LDFLAGS"
		CFLAGS="$CFLAGS $flag_64"
		LDFLAGS="$LDFLAGS $flag_64"
		m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
		AC_TRY_COMPILE( [], [], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then flag_64_works=no ; fi], [flag_64_works=no])
		m4_popdef([AC_TRY_EVAL]) 
		rm -f conftest.err
		AC_MSG_RESULT($flag_64_works)
		if test "x$flag_64_works" = "xno" ; then
			CFLAGS="$PRECFLAGS"
			LDFLAGS="$PRELDFLAGS"
		else
			found_64bit_flag=yes
		fi
	fi
	
	if test "x$found_64bit_flag" = "xno" ; then
		mabi_64_flag="-mabi=64"
		AC_MSG_CHECKING([if the compiler understands $mabi_64_flag])
		mabi_64_flag_works=yes
		PRECFLAGS="$CFLAGS"
		PRELDFLAGS="$LDFLAGS"
		CFLAGS="$CFLAGS $mabi_64_flag"
		LDFLAGS="$LDFLAGS $mabi_64_flag"
		m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
		AC_TRY_COMPILE( [], [], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then mabit_64_flag_works=no ; fi], [mabit_64_flag_works=no])
		m4_popdef([AC_TRY_EVAL]) 
		AC_TRY_COMPILE( [], [], [], [mabi_64_flag_works=no])
		AC_MSG_RESULT($mabi_64_flag_works)
		if test "x$mabi_64_flag_works" = "xno" ; then
			CFLAGS="$PRECFLAGS"
			LDFLAGS="$PRELDFLAGS"
		else
			found_64bit_flag=yes
		fi
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		m64_flag="-m64"
		AC_MSG_CHECKING([if the compiler understands $m64_flag])
		m64_flag_works=yes
		PRECFLAGS="$CFLAGS"
		PRELDFLAGS="$LDFLAGS"
		CFLAGS="$CFLAGS $m64_flag"
		LDFLAGS="$LDFLAGS $m64_flag"
		m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
		AC_TRY_COMPILE( [], [], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then m64_flag_works=no ; fi], [m64_flag_works=no])
		m4_popdef([AC_TRY_EVAL]) 
		AC_TRY_COMPILE( [], [], [], [m64_flag_works=no])
		AC_MSG_RESULT($m64_flag_works)
		if test "x$m64_flag_works" = "xno" ; then
			CFLAGS="$PRECFLAGS"
			LDFLAGS="$PRELDFLAGS"
		else
			found_64bit_flag=yes
		fi
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		q64_flag="-q64"
		AC_MSG_CHECKING([if the compiler understands $q64_flag])
		q64_flag_works=yes
		PRECFLAGS="$CFLAGS"
		PRELDFLAGS="$LDFLAGS"
		CFLAGS="$CFLAGS $q64_flag"
		LDFLAGS="$LDFLAGS $q64_flag"
		m4_pushdef([AC_TRY_EVAL], [_AC_EVAL_STDERR]($$[1]))
		AC_TRY_COMPILE( [], [], [if AC_TRY_COMMAND([grep "nrecognize" conftest.err >/dev/null 2>&1]) ; then q64_flag_works=no ; fi], [q64_flag_works=no])
		m4_popdef([AC_TRY_EVAL]) 
		AC_MSG_RESULT($q64_flag_works)
		if test "x$q64_flag_works" = "xno" ; then
			CFLAGS="$PRECFLAGS"
			LDFLAGS="$PRELDFLAGS"
		else
			found_64bit_flag=yes
		fi
	fi
fi

dnl optimization flags
if test "x$use_optimized" != "xno" ; then
	O3_flag="-O3"
	AC_MSG_CHECKING([if the compiler understands $O3_flag])
	O3_flag_works=yes
	PRECFLAGS="$CFLAGS"
	PRELDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS $O3_flag"
	LDFLAGS="$LDFLAGS $O3_flag"
	AC_TRY_COMPILE( [], [], [], [O3_flag_works=no])
	AC_MSG_RESULT($O3_flag_works)
	if test "x$O3_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to compile optimized with this compiler])
		CFLAGS="$PRECFLAGS"
		LDFLAGS="$PRELDFLAGS"
	fi

	dnl check for a slew of gcc optimizations that may or may not be available with -O3
	extra_optimization_flag="-ffast-math -fstrength-reduce -fexpensive-optimizations -fomit-frame-pointer -finline-functions"
	AC_MSG_CHECKING([if the compiler understands extra optimization flags])
	extra_optimization_flag_works=yes
	PRECFLAGS="$CFLAGS"
	PRELDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS $extra_optimization_flag"
	LDFLAGS="$LDFLAGS $extra_optimization_flag"
	AC_TRY_COMPILE( [], [], [], [extra_optimization_flag_works=no])
	AC_MSG_RESULT($extra_optimization_flag_works)
	if test "x$extra_optimization_flag_works" = "xno" ; then
		CFLAGS="$PRECFLAGS"
		LDFLAGS="$PRELDFLAGS"
	fi
fi

dnl debug flags
if test "x$use_debug" != "xno" ; then
	g_flag="-g"
	AC_MSG_CHECKING([if the compiler understands $g_flag])
	g_flag_works=yes
	PRECFLAGS="$CFLAGS"
	PRELDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS $g_flag"
	LDFLAGS="$LDFLAGS $g_flag"
	AC_TRY_COMPILE( [], [], [], [g_flag_works=no])
	AC_MSG_RESULT($g_flag_works)
	if test "x$g_flag_works" = "xno" ; then
		AC_MSG_WARN([Do not know how to debug with this compiler])
		CFLAGS="$PRECFLAGS"
		LDFLAGS="$PRELDFLAGS"
	fi
fi

dnl profile flags
if test "x$use_profiling" != "xno" ; then
	pg_flag="-pg"
	AC_MSG_CHECKING([if the compiler understands $pg_flag])
	pg_flag_works=yes
	PRECFLAGS="$CFLAGS"
	PRELDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS $pg_flag"
	LDFLAGS="$LDFLAGS $pg_flag"
	AC_TRY_COMPILE( [], [], [], [pg_flag_works=no])
	AC_MSG_RESULT($pg_flag_works)
	if test "x$pg_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to profile with this compiler])
		CFLAGS="$PRECFLAGS"
		LDFLAGS="$PRELDFLAGS"
	fi
fi

dnl verbose warning flags
if test "x$use_warnings" != "xno" ; then
	warning_flag="-W -Wall -Wundef -Wfloat-equal -Wshadow -Wunreachable-code -Winline -Wconversion"
		# XXX also of interest
		# -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -pedantic -ansi -Werror
	AC_MSG_CHECKING([if the compiler understands warning flags])
	warning_flag_works=yes
	PRECFLAGS="$CFLAGS"
	PRELDFLAGS="$LDFLAGS"
	CFLAGS="$CFLAGS $warning_flag"
	LDFLAGS="$LDFLAGS $warning_flag"
	AC_TRY_COMPILE( [], [], [], [warning_flag_works=no])
	AC_MSG_RESULT($warning_flag_works)
	if test "x$warning_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to output verbose warnings with this compiler])
		CFLAGS="$PRECFLAGS"
		LDFLAGS="$PRELDFLAGS"
	fi
fi

AC_C_CONST
AC_C_VOLATILE
AC_C_INLINE
AC_C_BIGENDIAN(AC_DEFINE(NATURAL_IEEE,,64b floating point storage technique), 
	       AC_DEFINE(REVERSE_IEEE,,64b floating point storage technique),
	       AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	       AC_MSG_ERROR([*** Don't know the endian of this machine! ***]) )

dnl AC_C_PROTOTYPES
AM_C_PROTOTYPES

AC_DEFINE(USE_PROTOTYPES,1,heh)

# figure out what size pointers the compiler is actually generating
AC_CHECK_SIZEOF(void *, 4)
pointer_size="$ac_cv_sizeof_void_p"

dnl Last step is to make sure that we can actually compile
AC_MSG_CHECKING(compiler and flags for sanity)
AC_TRY_RUN([
#include <stdio.h>
int main(){exit(0);}
	],
	[	AC_MSG_RESULT(yes) ],
	[
		AC_MSG_RESULT(no)
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_ERROR([*** compiler cannot create working executables, check config.log ***])
	]
)


dnl ***************************
dnl *** Check for functions ***
dnl ***************************

AC_MSG_CHECKING([... f u n c t i o n s ])
AC_MSG_RESULT([])

AC_TYPE_SIGNAL
AC_CHECK_FUNCS(\
	XQueryExtension \
	access \
	alarm \
	atexit \
	basename \
	bzero \
	calloc \
	dlopen \
	drand48 \
	dup2 \
	endgrent \
	endpwent \
	fabs  \
	floor \
	ftime \
	ftruncate \
	getcwd \
	gethostbyaddr \
	gethostbyname \
	gethostname \
	getopt \
	getpagesize \
	getpass \
	gettimeofday \
	inet_ntoa \
	isascii \
	localtime_r \
	memchr \
	memmove \
	memset \
	mkdir \
	mkfifo \
	modf \
	munmap \
	nl_langinfo \
	panic \
	pow \
	prtcl \
	putenv \
	re_comp \
	realpath \
	regcomp \
	rmdir \
	sbrk \
	select \
	setlocale \
	socket \
	sqrt \
	srand48 \
	strcasecmp \
	strchr \
	strcpy \
	strcspn \
	strdup \
	strerror \
	strerror_r \
	strncasecmp \
	strpbrk \
	strrchr \
	strsep \
	strspn \
	strstr \
	strtol \
	strtoul \
	strtoull \
	sysmp \
	tzset \
	uname \
	utime\
	vfork \
	vprintf \
	writev \
)

dnl XXX for tcl (some duplicate)
AC_CHECK_FUNC(fstatfs, , [AC_DEFINE(NO_FSTATFS,, Lacking fstatfs)])
AC_CHECK_FUNC(strerror, , [AC_DEFINE(NO_STRERROR,, From Tcl -- No strerror)])
AC_CHECK_FUNC(getwd, , [AC_DEFINE(NO_GETWD,, From Tcl -- No getwd)])
AC_CHECK_FUNC(wait3, , [AC_DEFINE(NO_WAIT3,, From Tcl -- No wait3)])
AC_CHECK_FUNC(uname, , [AC_DEFINE(NO_UNAME,, From Tcl -- No uname)])
AC_CHECK_FUNC(realpath, , [AC_DEFINE(NO_REALPATH,, From Tcl -- No realpath)])
AC_CHECK_FUNC(memmove, , [AC_DEFINE(NO_MEMMOVE,, From Tcl -- No memmove) 
		          AC_DEFINE(NO_STRING_H,, From Tcl -- No string)])
AC_CHECK_FUNC(opendir, , [AC_DEFINE(USE_DIRENT2_H,, From Tcl -- use dirent2)])

AC_CHECK_FUNC(popen, AC_DEFINE(HAVE_POPEN_DECL,1,popen))

# XXX duplicate for decl (needs to go away)
AC_CHECK_FUNC(getopt, AC_DEFINE(HAVE_GETOPT_DECL,1,getopt))
AC_CHECK_FUNC(strdup, AC_DEFINE(HAVE_STRDUP_DECL,1,strdup))


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

AC_MSG_CHECKING([... s e r v i c e s ])
AC_MSG_RESULT([])

dnl *** java ***
if test "x$only_rts" != "xno" ; then
	AC_MSG_CHECKING(if java seems to be available)
	if test "x$with_java" = "xno" ; then
		AC_MSG_RESULT(no)
		BC_RETRY_CONFIGURE([$0])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
		AC_MSG_ERROR([*** Cannot locate jni.h for building librtserver ***])
	else 
		AC_MSG_NOTICE([with-jdk: $with_java])
	fi
	AC_MSG_RESULT(yes)
fi

dnl *** libpng ***
png_works=no
AC_MSG_CHECKING(for portable network graphics functionality)
if test "x$png_link_works" = "xyes" ; then
  PRELDFLAGS="$LDFLAGS"
  LDFLAGS="$LDFLAGS -lpng"
fi
AC_TRY_RUN([
#include <png.h>
main() {
	int i = png_access_version_number();
	return 0;
}
	],
	[png_works=yes]
)
if test "x$png_link_works" = "xyes" ; then
  LDFLAGS="$PRELDFLAGS"
fi
AC_MSG_RESULT($png_works)

dnl figure out whether we need to build libpng
build_brlcad_png="no (using system)"
if test "x$build_png" = "xyes" ; then
	build_brlcad_png=yes
else
	if test "x$build_png" = "xno" ; then
		if test "x$png_works" != "xyes" ; then
			BC_RETRY_CONFIGURE([$0])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-png-build])
			AC_MSG_ERROR([*** Building libpng was disabled, yet no system png library was found ***])
		fi
	else
		dnl automatic detection, we need to build if it won't run or won't link
		if test "x$png_works" = "xno" ; then
			build_brlcad_png=yes
		elif test "x$png_link_works" = "xno" ; then
			build_brlcad_png=yes
		fi
	fi
fi
AC_MSG_CHECKING(whether to build the included portable network graphics library)
if test "x$build_brlcad_png" = "xyes" ; then
	LIBPNG='${top_builddir}/src/other/libpng/libpng.la'
	AC_MSG_RESULT(yes)
else
	LIBPNG="-lpng"
	AC_MSG_RESULT(no)
fi
AC_SUBST(LIBPNG)

dnl *** libregex ***
dnl See if regular expression support works
regex_works=no
AC_MSG_CHECKING(for regular expression functionality)
AC_TRY_RUN([
#include <sys/types.h>
#include <regex.h>
int main() {
  regex_t re;
  regcomp(&re, "abc", REG_EXTENDED);
  regexec(&re, "", 0, 0, 0);
  return 0;
}
        ],
	[regex_works=yes]
)
AC_MSG_RESULT($regex_works)

dnl figure out whether we need to build libregex
build_brlcad_regex="no (using system)"
if test "x$build_regex" = "xyes" ; then
	build_brlcad_regex=yes
else
	if test "x$build_regex" = "xno" ; then
		if test "x$regex_works" != "xyes" ; then
			BC_RETRY_CONFIGURE([$0])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-regex-build])
			AC_MSG_ERROR([*** Building libregex was disabled, yet no system regex was found ***])
		fi
	else
		dnl automatic detection
		if test "x$regex_works" = "xno" ; then
			build_brlcad_regex=yes
		fi
	fi
fi
AC_MSG_CHECKING(whether to build the included regular expression library)
if test "x$build_brlcad_regex" = "xyes" ; then
	REGEX_LIBS='${top_builddir}/src/other/libregex/libregex.la'
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(REGEX_LIBS)

dnl *** libz ***
dnl See if zlib library support works
zlib_works=no
AC_MSG_CHECKING(for zlib compression functionality)
AC_TRY_RUN([
#include <zlib.h>
int main () {
  char *zlibver = ZLIB_VERSION;
  return 0;
}
	],
	[zlib_works=yes]
)
AC_MSG_RESULT($zlib_works)

dnl figure out whether we need to build libz
build_brlcad_zlib="no (using system)"
if test "x$build_zlib" = "xyes" ; then
	build_brlcad_zlib=yes
else
	if test "x$build_zlib" = "xno" ; then
		if test "x$zlib_works" != "xyes" ; then
			BC_RETRY_CONFIGURE([$0])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-zlib-build])
			AC_MSG_ERROR([*** Building libz was disabled, yet no system zlib compression was found ***])
		fi
	else
		dnl automatic detection
		if test "x$zlib_works" = "xno" ; then
			build_brlcad_zlib=yes
		fi
	fi
fi
AC_MSG_CHECKING(whether to build the included zlib compression library)
LIBZ=""
if test "x$build_brlcad_zlib" = "xyes" ; then
	LIBZ='${top_builddir}/src/other/libz/libz.la'
	AC_MSG_RESULT(yes)
else
	if test "x$z_link_works" = "xyes" ; then
		LIBZ='-lz'
	fi
	AC_MSG_RESULT(no)
fi
AC_SUBST(LIBZ)

dnl *** libthread ***
AC_MSG_CHECKING(for threading library availability)
AC_MSG_RESULT($thread_link_works)
AC_SUBST(LIBTHREAD)

AC_MSG_CHECKING(if pthreading works)
thread_works=no
AC_TRY_RUN([
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
void *thread_hook(void *arg) {
     return NULL;
}
main () {
     pthread_t thread;
     pthread_create(&thread, (void *)0, thread_hook, (void *)0);
     pthread_join(thread, NULL);
     return 0;
}
	],
	[thread_works=yes]
)
AC_MSG_RESULT($thread_works)

dnl *** X11 ***
AC_PATH_X
AC_PATH_XTRA

# XXX ugly hack that needs to die
X_PREFIX=/usr/X11R6
AC_SUBST(X_CFLAGS)
X_CFLAGS="-I${X_PREFIX}/include"
AC_SUBST(X_LIBS)
X_LIBS="-L${X_PREFIX}/lib -lX11 -lXi"
AC_SUBST(X_PREFIX)

dnl *** libgl ***
AC_MSG_CHECKING(whether to link with the OpenGL library)
AC_MSG_RESULT($gl_link_works)
AC_SUBST(LIBGL)

dnl *** sgigl ***
AC_MSG_CHECKING(whether to link with the SGI graphics library)
AC_MSG_RESULT($sgigl_link_works)
AC_SUBST(LIBSGIGL)
dnl XXX the HAS_SGIGL define should go away eventually
if test "x$sgigl_link_works" = "xyes" ; then
	AC_DEFINE(HAS_SGIGL, 1, Irix GL is present.)
fi

dnl *** curses ***
AC_MSG_CHECKING(whether to link with the curses library)
AC_MSG_RESULT($curses_lib_works)
AC_SUBST(CURSES)

dnl *** libm ***
AC_MSG_CHECKING(whether to link with the math library)
AC_MSG_RESULT($m_link_works)
AC_SUBST(LIBM)

dnl *** libsocket ***
AC_MSG_CHECKING(for socket library availability)
AC_MSG_RESULT([$socket_link_works])
AC_SUBST(LIBSOCKET)

dnl *** libnsl ***
AC_MSG_CHECKING(for network socket library availablity)
AC_MSG_RESULT([$nsl_link_works])
AC_SUBST(LIBNSL)

dnl *** libmalloc ***
AC_MSG_CHECKING(for malloc library availability)
AC_MSG_RESULT([$malloc_link_works])
AC_SUBST(LIBMALLOC)

dnl *** libmx ***
AC_MSG_CHECKING(for mx library availability)
AC_MSG_RESULT([$mx_link_works])
AC_SUBST(LIBMX)

dnl *** libdl ***
AC_MSG_CHECKING(for dynamic linker library availability)
AC_MSG_RESULT([$dl_link_works])
AC_SUBST(LIBDL)


dnl *** 64-bit compilation ***
dnl figure out whether we are building 64-bit
build_brlcad_64bit="no"
if test "x$build_64bit" = "xyes" ; then
	if test "x$pointer_size" != "x8" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_NOTICE([Try removing --enable-64bit-build or modifying the CFLAGS/LDFLAGS])
		AC_MSG_ERROR([*** Building 64-bit was requested, yet the build seems to be non-64-bit ***])
	fi
	build_brlcad_64bit=yes
else
	if test "x$build_64bit" = "xno" ; then
		if test "x$pointer_size" = "x8" ; then
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try removing --disable-64bit-build or modifying the CFLAGS/LDFLAGS])
			AC_MSG_ERROR([*** Building non-64-bit was requested, yet the build seems to be 64-bit ***])
		fi
	fi

	dnl automatic detection
	if test "x$pointer_size" = "x8" ; then
		build_brlcad_64bit="yes"
	elif test "x$pointer_size" = "x4" ; then
		build_brlcad_64bit="no (32-bit)"
	elif test "x$pointer_size" = "x2" ; then
		build_brlcad_64bit="no (16-bit)"
	elif test "x$pointer_size" = "x1" ; then
		build_brlcad_64bit="no (8-bit)"
	else
		AC_MSG_WARN([Unknown pointer size: $pointer_size])
	fi
fi

# XXX nasty temp hack for backwards cake compatibility.
if test "x$host_os" = "xirix6.5" ; then
	# XXX the code that relies on this will be going away, but until then..
	if test "x$build_brlcad_64bit" = "xyes" ; then
		CFLAGS="$CFLAGS -DIRIX64"
	else
		CFLAGS="$CFLAGS -DIRIX"
	fi
fi

# make sure ECHO and ECHO_N got defined and substituted
if test "x$ECHO" = "x" ; then
	ECHO=echo
	AC_MSG_NOTICE([ECHO was not defined by configure so defining manually])
fi
AC_SUBST(ECHO)
if test "x$ECHO_N" = "x" ; then
	ECHO_N=
fi
AC_SUBST(ECHO_N)


dnl ***************************
dnl *** Configure Makefiles ***
dnl ***************************

AC_MSG_CHECKING([... o u t p u t ])
AC_MSG_RESULT([])

AM_CONDITIONAL(BUILD_JOVE, [test "x$build_jove" = "xyes"])
# XXX only irix can build libtkglx right now?
AM_CONDITIONAL(BUILD_LIBTKGLX, [test "x$host_os" = "xirix6.5"])
AM_CONDITIONAL(BUILD_PNG, [test "x$build_brlcad_png" = "xyes"])
AM_CONDITIONAL(BUILD_REGEX, [test "x$build_brlcad_regex" = "xyes"])
AM_CONDITIONAL(BUILD_ZLIB, [test "x$build_brlcad_zlib" = "xyes"])
AM_CONDITIONAL(JAVA, [test "x$with_java" != "xno"])
AM_CONDITIONAL(ONLY_BENCHMARK, [test "x$only_benchmark" != "xno"])
AM_CONDITIONAL(ONLY_RTS, [test "x$only_rts" != "xno"])


dnl Any combination of the different optimizations listed below may individually be 
dnl optionally turned off by commenting out the unwanted defines in the config file.
dnl
dnl NO_BOMBING_MACROS
dnl      turns off many macros in h/bu.h and h/bn.h that check a condition (such as a
dnl      magic number) and bu_bomb on failure.  note that turning this flag on will
dnl      remove the debug capability to catch fatal runtime erros (bugs) and exit
dnl      gracefully.
dnl NO_MAGIC_CHECKING
dnl      turns off allocation of (some) variables and calls to check magic numbers.
dnl      note that this will hinder memory corrupting bug detection.
dnl NO_BADRAY_CHECKING
dnl      is a risky optimization to turn off checking in librt/shoot.c for bad rays 
dnl      passed in to the expensive shootray() routine.
dnl NO_DEBUG_CHECKING
dnl      is an optimization that replaces instances of rt_g.debug in if statements 
dnl      where checking for a debug level occurs (in order to output a debug message)
dnl      with the integer constant 0.  note that turning this flag on will remove the
dnl      capability to output debug messages and commands that take a debug-level args
dnl      will not output as expected.  see h/raytrace.h for the implementation details.

if test "x$build_runtime_debug" = "xno" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([While disabling run-time debugging should increase])
	AC_MSG_WARN([performance, it will likewise remove several])
	AC_MSG_WARN([data-protection safeguards that are in place to])
	AC_MSG_WARN([minimize the possibility of corrupted data files])
	AC_MSG_WARN([in the inevitable event of a user encountering a bug.])
	AC_MSG_WARN([You have been warned.  Proceed at your own risk.])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	AC_DEFINE(NO_BOMBING_MACROS, 1, [Define to not do anything for macros that only bomb on a fatal error])
	AC_DEFINE(NO_MAGIC_CHECKING, 1, [Define to not perform magic number checking])
	AC_DEFINE(NO_BADRAY_CHECKING, 1, [Define to not check for divide by zero during ray shots])
	AC_DEFINE(NO_DEBUG_CHECKING, 1, [Define to not provide run-time debug facilities via rt_g.debug])
fi


BN='${top_builddir}/src/libbn/libbn.la'
AC_SUBST(BN)

BU='${top_builddir}/src/libbu/libbu.la'
AC_SUBST(BU)

CURSOR='${top_builddir}/src/libcursor/libcursor.la'
AC_SUBST(CURSOR)

DM='${top_builddir}/src/libdm/libdm.la'
AC_SUBST(DM)

FB='${top_builddir}/src/libfb/libfb.la'
AC_SUBST(FB)

FFT='${top_builddir}/src/libfft/libfft.la'
AC_SUBST(FFT)

MULTISPECTRAL='${top_builddir}/src/libmultispectral/libmultispectral.la'
AC_SUBST(MULTISPECTRAL)

OPTICAL='${top_builddir}/src/liboptical/liboptical.la'
AC_SUBST(OPTICAL)

ORLE='${top_builddir}/src/liborle/liborle.la'
AC_SUBST(ORLE)

PKG='${top_builddir}/src/libpkg/libpkg.la'
AC_SUBST(PKG)

RLE='${top_builddir}/src/other/libutahrle/libutahrle.la'
AC_SUBST(RLE)

LIBRT='${top_builddir}/src/librt/librt.la'
AC_SUBST(LIBRT)

SYSV='${top_builddir}/src/libsysv/libsysv.la'
AC_SUBST(SYSV)

TCLCAD='${top_builddir}/src/libtclcad/libtclcad.la'
AC_SUBST(TCLCAD)

TERMIO='${top_builddir}/src/libtermio/libtermio.la'
AC_SUBST(TERMIO)

WDB='${top_builddir}/src/libwdb/libwdb.la'
AC_SUBST(WDB)

dnl tcl
dnl Need to update version number(s) in src/other/libtcl/Makefile.am
TCL_VERSION="8.4"
AC_SUBST(TCL_VERSION)
TCL_MAJOR_VERSION=8
AC_SUBST(TCL_MAJOR_VERSION)
TCL_MINOR_VERSION=4
AC_SUBST(TCL_MINOR_VERSION)
TCL_PATCH_LEVEL=".6"
AC_SUBST(TCL_PATCH_LEVEL)
TCL_LIBRARY='${prefix}/lib/tcl${TCL_VERSION}'
AC_SUBST(TCL_LIBRARY)
TCL_PACKAGE_PATH="'${libdir}'"
AC_SUBST(TCL_PACKAGE_PATH)
TCL='${top_builddir}/src/other/libtcl/libtcl${TCL_VERSION}.la '"${SOCKET}"
AC_SUBST(TCL)
TCLSTUB='${top_builddir}/src/other/libtcl/generic/libtclstub.la'
AC_SUBST(TCLSTUB)

dnl XXX -- hack to make unfixed libtk behave when reading tclConfig.sh
dnl EXTRA_CFLAGS='-DMAC_OSX_TCL -DHAVE_CFBUNDLE -DUSE_VFORK -DTCL_DEFAULT_ENCODING=\"utf-8\"'
AC_SUBST(CC)
AC_SUBST(DEFS)
AC_SUBST(TCL_DBGX)
CFLAGS_DEBUG="-g"
AC_SUBST(CFLAGS_DEBUG)
CFLAGS_OPTIMIZE="-Os"
AC_SUBST(CFLAGS_OPTIMIZE)
AC_SUBST(LDFLAGS_DEBUG)
AC_SUBST(LDFLAGS_OPTIMIZE)
TCL_SHARED_BUILD=1
AC_SUBST(TCL_SHARED_BUILD)
AC_SUBST(CFG_TCL_EXPORT_FILE_SUFFIX)
AC_SUBST(DL_LIBS)
AC_SUBST(LIBS)
AC_SUBST(MATH_LIBS)
TCL_LIB_FILE=libtcl8.4.so
AC_SUBST(TCL_LIB_FILE)
TCL_NEEDS_EXP_FILE=0
AC_SUBST(TCL_NEEDS_EXP_FILE)
AC_SUBST(SHLIB_CFLAGS)
AC_SUBST(CFLAGS_WARNING)
AC_SUBST(EXTRA_CFLAGS)
AC_SUBST(SHLIB_LD)
AC_SUBST(SHLIB_SUFFIX)
AC_SUBST(STLIB_LD)
AC_SUBST(SHLIB_LD_LIBS)
TCL_LIB_FLAG='-ltcl8.4'
AC_SUBST(TCL_LIB_FLAG)
TCL_BUILD_LIB_SPEC='-L../../libtcl/.libs -ltcl8.4 -Wl,--rpath -Wl,${libdir}'
AC_SUBST(TCL_BUILD_LIB_SPEC)
TCL_INCLUDE_SPEC='-I${prefix}/include -I../../libtcl/generic'
AC_SUBST(TCL_INCLUDE_SPEC)
TCL_LIB_VERSIONS_OK='ok'
AC_SUBST(TCL_LIB_VERSIONS_OK)
AC_SUBST(TCL_LIB_SPEC)
AC_SUBST(CFG_TCL_SHARED_LIB_SUFFIX)
AC_SUBST(CFG_TCL_UNSHARED_LIB_SUFFIX)
TCL_SRC_DIR='../../libtcl'
AC_SUBST(TCL_SRC_DIR)
TCL_STUB_LIB_FILE='libtclstub8.4.a'
AC_SUBST(TCL_STUB_LIB_FILE)
TCL_STUB_LIB_FLAG='-ltclstub8.4'
AC_SUBST(TCL_STUB_LIB_FLAG)
AC_SUBST(TCL_BUILD_STUB_LIB_SPEC)
AC_SUBST(TCL_STUB_LIB_SPEC)
TCL_BUILD_STUB_LIB_PATH="../../libtcl/.libs/libtclstub8.4.a"
AC_SUBST(TCL_BUILD_STUB_LIB_PATH)
TCL_STUB_LIB_PATH='${prefix}/lib/libtclstub8.4.a'
AC_SUBST(TCL_STUB_LIB_PATH)
TCL_THREADS=0
AC_SUBST(TCL_THREADS)

dnl tk
dnl Need to update version number(s) in src/other/libtk/Makefile.am
TK_VERSION="8.4"
AC_SUBST(TK_VERSION)
TK_LIBRARY='${prefix}/lib/tk${TK_VERSION}'
AC_SUBST(TK_LIBRARY)
TK='${top_builddir}/src/other/libtk/libtk${TK_VERSION}.la'
AC_SUBST(TK)
TKSTUB='${top_builddir}/src/other/libtk/generic/libtkstub.la'
AC_SUBST(TKSTUB)

dnl itcl/itk
ITCL_VERSION="3.3"
AC_SUBST(ITCL_VERSION)
ITCL_LIBS='${top_builddir}/src/other/incrTcl/libitcl${ITCL_VERSION}.la'
AC_SUBST(ITCL_LIBS)
ITK_LIBS='${top_builddir}/src/other/incrTcl/libitk${ITCL_VERSION}.la'
AC_SUBST(ITK_LIBS)

dnl sample applications
SAMPLE_APPLICATIONS_DIR='${prefix}/sample_applications'
AC_SUBST(SAMPLE_APPLICATIONS_DIR)


AC_CONFIG_FILES([
	Makefile
	bench/Makefile
	regress/Makefile
	db/Makefile
	db/include/Makefile
	doc/Makefile
	doc/html/Makefile
	doc/html/ReleaseNotes/Makefile
	doc/html/ReleaseNotes/Rel5.0/Makefile
	doc/html/ReleaseNotes/Rel5.0/Summary/Makefile
	doc/html/ReleaseNotes/Rel6.0/Makefile
	doc/html/manuals/Anim_Tutorial/Makefile
	doc/html/manuals/Makefile
	doc/html/manuals/ReleaseNotes/Makefile
	doc/html/manuals/cadwidgets/Makefile
	doc/html/manuals/libbu/Makefile
	doc/html/manuals/libdm/Makefile
	doc/html/manuals/librt/Makefile
	doc/html/manuals/mged/Makefile
	doc/html/manuals/mged/animmate/Makefile
	doc/html/manuals/shaders/Makefile
	doc/legal/Makefile
	include/Makefile
	m4/Makefile
	misc/Makefile
	misc/macosx/Makefile
	misc/macosx/Resources/Makefile
	misc/macosx/Resources/ReadMe.rtfd/Makefile
	misc/macosx/Resources/Welcome.rtfd/Makefile
	misc/pro-engineer/Makefile
	misc/pro-engineer/mk
	misc/pro-engineer/protk.dat
	misc/vfont/Makefile
	pix/Makefile
	sh/Makefile
	src/Makefile
	src/anim/Makefile
	src/brlman/Makefile
	src/burst/Makefile
	src/bwish/Makefile
	src/canon/Makefile
	src/comgeom-g/Makefile
	src/conv/Makefile
	src/fb/Makefile
	src/fbed/Makefile
	src/fbserv/Makefile
	src/gtools/Makefile
	src/halftone/Makefile
	src/iges/Makefile
	src/irprep/Makefile
	src/jack/Makefile
	src/java/Makefile
	src/java/mil/Makefile
	src/java/mil/army/Makefile
	src/java/mil/army/arl/Makefile
	src/java/mil/army/arl/brlcad/Makefile
	src/lgt/Makefile
	src/libbn/Makefile
	src/libbu/Makefile
	src/libcursor/Makefile
	src/libdm/Makefile
	src/libfb/Makefile
	src/libfft/Makefile
	src/libmultispectral/Makefile
	src/liboptical/Makefile
	src/liborle/Makefile
	src/libpkg/Makefile
	src/librt/Makefile
	src/librtserver/Makefile
	src/libsysv/Makefile
	src/libtclcad/Makefile
	src/libtermio/Makefile
	src/libwdb/Makefile
	src/mged/Makefile
	src/mged/dm_old/Makefile
	src/mk/Makefile
	src/nirt/Makefile
	src/off/Makefile
	src/other/Makefile
	src/other/URToolkit/Makefile
	src/other/URToolkit/cnv/Makefile
	src/other/URToolkit/cnv/rletoabA62/Makefile
	src/other/URToolkit/cnv/rletogif/Makefile
	src/other/URToolkit/man/Makefile
	src/other/URToolkit/tools/Makefile
	src/other/URToolkit/tools/clock/Makefile
	src/other/awf/Makefile
	src/other/incrTcl/Makefile
	src/other/incrTcl/doc/Makefile
	src/other/incrTcl/itcl/Makefile
	src/other/incrTcl/itcl/doc/Makefile
	src/other/incrTcl/itcl/generic/Makefile
	src/other/incrTcl/itcl/library/Makefile
	src/other/incrTcl/itcl/pkgIndex.tcl
	src/other/incrTcl/itk/Makefile
	src/other/incrTcl/itk/doc/Makefile
	src/other/incrTcl/itk/generic/Makefile
	src/other/incrTcl/itk/library/Makefile
	src/other/incrTcl/itk/pkgIndex.tcl
	src/other/incrTcl/tools/Makefile
	src/other/iwidgets/Makefile
	src/other/iwidgets/doc/Makefile
	src/other/iwidgets/generic/Makefile
	src/other/iwidgets/iwidgets.tcl
	src/other/iwidgets/pkgIndex.tcl
	src/other/jove/Makefile
	src/other/libpng/Makefile
	src/other/libregex/Makefile
	src/other/libtcl/Makefile
	src/other/libtcl/doc/Makefile
	src/other/libtcl/generic/Makefile
	src/other/libtcl/library/Makefile
	src/other/libtcl/library/dde/Makefile
	src/other/libtcl/library/encoding/Makefile
	src/other/libtcl/library/http/Makefile
	src/other/libtcl/library/http1.0/Makefile
	src/other/libtcl/library/msgcat/Makefile
	src/other/libtcl/library/opt/Makefile
	src/other/libtcl/library/reg/Makefile
	src/other/libtcl/library/tcltest/Makefile
	src/other/libtcl/unix/Makefile
	src/other/libtcl/unix/tclConfig.sh
	src/other/libtk/Makefile
	src/other/libtk/doc/Makefile
	src/other/libtk/generic/Makefile
	src/other/libtk/library/Makefile
	src/other/libtk/library/demos/Makefile
	src/other/libtk/library/demos/images/Makefile
	src/other/libtk/library/images/Makefile
	src/other/libtk/library/msgs/Makefile
	src/other/libtk/library/pkgIndex.tcl
	src/other/libtk/unix/Makefile
	src/other/libtk/unix/tkConfig.sh
	src/other/libtkGLX/Makefile
	src/other/libutahrle/Makefile
	src/other/libz/Makefile
	src/patch/Makefile
	src/proc-db/Makefile
	src/remrt/Makefile
	src/rt/Makefile
	src/rttherm/Makefile
	src/sig/Makefile
	src/tab/Makefile
	src/tclscripts/Makefile
	src/tclscripts/geometree/Makefile
	src/tclscripts/lib/Makefile
	src/tclscripts/mged/Makefile
	src/tclscripts/nirt/Makefile
	src/tclscripts/pl-dm/Makefile
	src/tclscripts/rtwizard/Makefile
	src/tclscripts/rtwizard/examples/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeA/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeB/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeC/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeD/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeE/Makefile
	src/tclscripts/rtwizard/examples/PictureTypeF/Makefile
	src/tclscripts/rtwizard/lib/Makefile
	src/tclscripts/util/Makefile
	src/unigraphics/Makefile
	src/util/Makefile
	src/vas4/Makefile
	src/vdeck/Makefile
 	brlcad.spec
])

AC_OUTPUT


dnl
dnl Expand the variables for summary reporting
dnl
prefix=`eval "echo $prefix"`
prefix=`eval "echo $prefix"`
bindir=`eval "echo $bindir"`
bindir=`eval "echo $bindir"`
sysconfdir=`eval "echo $sysconfdir"`
sysconfdir=`eval "echo $sysconfdir"`
mandir=`eval "echo $mandir"`
mandir=`eval "echo $mandir"`

dnl
dnl Compute configuration time elapsed
dnl
if test -x "${srcdir}/sh/elapsed.sh" ; then
	time_elapsed="`${srcdir}/sh/elapsed.sh $CONFIG_TIME`"
else
	time_elapsed="unknown"
fi


dnl **********************
dnl *** Report Summary ***
dnl **********************

echo "Done."
echo
echo "BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE"
echo
echo "             Prefix: ${prefix}"
echo "           Binaries: ${bindir}"
echo "Configuration files: ${sysconfdir}"
echo "       Manual pages: ${mandir}"
echo
echo "CC       = ${CC}"
echo "CFLAGS   = ${CFLAGS}"
echo "CPPFLAGS = ${CPPFLAGS}"
echo "LDFLAGS  = ${LDFLAGS}"
echo "LIBS     = ${LIBS}"
echo
echo Build IWidgets .......................: yes
echo Build Itcl/Itk .......................: yes
echo Build Tcl ............................: yes
# we have to build Tk as it's been customized with tkCanvBezier
# echo Build Tk .............................: yes
echo Build libpng .........................: $build_brlcad_png
echo Build libregex .......................: $build_brlcad_regex
echo Build libz ...........................: $build_brlcad_zlib
echo Build jove ...........................: $build_jove
echo 
echo Java Developer Kit support ...........: $with_java
echo X11 support ..........................: yes
echo Open GL support ......................: yes
echo Enable run-time debugging ............: $build_runtime_debug
echo
echo Build 64-bit release .................: $build_brlcad_64bit
echo Build optimized release ..............: $use_optimized
echo Build debug release ..................: $use_debug
echo Build profile release ................: $use_profiling
echo Build static libraries ...............: $enable_static
echo Build shared/dynamic libraries .......: $enable_shared
echo Print verbose compilation warnings ...: $use_warnings
echo
echo Only build benchmark suite ...........: $only_benchmark
echo Only build librtserver ...............: $only_rts
echo Generate doxygen documentation .......: $use_doxygen
echo
if test "x$time_elapsed" != "xunknown" ; then
echo Elapsed configuration time ...........: $time_elapsed
fi
echo ---
echo "$0 complete, type 'make' to begin building"
echo
