dnl BRL-CAD
dnl Copyright (c) 2004 BRL-CAD Developers
dnl $Id$
dnl
dnl This program is free software; you can redistribute it and/or
dnl modify it under the terms of the appropriate license described in
dnl the file named COPYING that should have accompanied this file.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
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
dnl      0) information on the package
dnl	 1) check command-line arguments
dnl	 2) check programs
dnl	 3) check libraries
dnl	 4) check headers
dnl	 5) check types/structures
dnl      6) check compiler characteristics
dnl	 7) check functions
dnl	 8) check system services
dnl	 9) output a summary
dnl
dnl Any useful build arguments should be added to the output summary
dnl at the end.  Although it may be possible to check for certain
dnl features of command line arguments near the top, related tests
dnl should be delayed and placed into the appropriate test section.
dnl
dnl Features of the GNU Autotools that would require an increase in
dnl the minimum version requirement are highly discouraged.  Likewise
dnl discouraged is rolling back support for versions released prior
dnl to autoconf 2.50.
dnl
dnl Strive to tame the chaos that is so easily achieved.
dnl

AC_INIT(src/librt/bool.c)

dnl put configure stuff here
dnl AC_CONFIG_AUX_DIR(config)

AC_PREREQ(2.50)

MAJOR_VERSION=6
MINOR_VERSION=1
PATCH_VERSION=0
CONFIG_DATE=`date +%Y%m%d`
BRLCAD_VERSION="$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION"
echo "Configuring BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE"

# necessary for proper file creation on nfs volumes
umask 022

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# am_init_automake performs a ac_prog_install check so need to handle
# problematic /usr/brl/bin/install on irix
if test "x$build_vendor" = "xsgi" ; then
   PATH="`echo $PATH | sed 's/\/brl\/bin/bin/g'`"
fi
AM_INIT_AUTOMAKE(brlcad, $BRLCAD_VERSION.$CONFIG_DATE)
AM_CONFIG_HEADER(include/config.h)

AC_PREFIX_DEFAULT(/usr/brlcad)


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

# Allow user to specify flags without relying on environment
AC_ARG_WITH(cflags, AC_HELP_STRING(--with-cflags,
		    [Specify additional flags to pass to the C compiler]) ,
	[
		if test "x$withval" != "xno" ; then
			CFLAGS="$CFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(cppflags, AC_HELP_STRING(--with-cppflags,
		      [Specify additional flags to pass to C preprocessor]) ,
	[
		if test "x$withval" != "xno"; then
			CPPFLAGS="$CPPFLAGS $withval"
		fi
	]
)
AC_ARG_WITH(ldflags, AC_HELP_STRING(--with-ldflags,
		     [Specify additional flags to pass to linker]) ,
	[
		if test "x$withval" != "xno" ; then
			LDFLAGS="$LDFLAGS $withval"
		fi
	]	
)
AC_ARG_WITH(libs, AC_HELP_STRING(--with-libs,
		  [Specify additional libraries to link against]) ,
	[
		if test "x$withval" != "xno" ; then
			LIBS="$LIBS $withval"
		fi
	]	
)

# use the jdk
JAVA_CFLAGS=
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
AC_ARG_ENABLE(only-benchmark,
	AC_HELP_STRING(--enable-only-benchmark,
		[Only build the components necessary for the benchmark suite (default=no)]),
	[if ! test "x$enableval" = "xno" ; then
		only_benchmark=yes
	fi]
)
AC_MSG_RESULT($only_benchmark)

AM_CONDITIONAL(ONLY_BENCHMARK, [! test "x$only_benchmark" = "xno"])

# only build librtserver
AC_MSG_CHECKING(whether to only build librtserver)
only_rts=no
warn_only_both=no
AC_ARG_ENABLE(only-rtserver,
	AC_HELP_STRING(--enable-only-rtserver,
		[Only build the components necessary for the raytrace server (default=no)]),
	[if ! test "x$enableval" = "xno" ; then
		only_rts=yes
		if ! test "x$only_benchmark" = "xno" ; then
			warn_only_both=yes
		fi
	fi]
)
AC_MSG_RESULT($only_rts)
AM_CONDITIONAL(ONLY_RTS, [! test "x$only_rts" = "xno"])
if ! test "x$warn_only_both" = "xno" ; then
	AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
	AC_MSG_WARN(Requesting BOTH benchmark and librtserver "only"?)
	AC_MSG_WARN(Building both benchmark and librtserver)
	AC_MSG_NOTICE([<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<])
fi


# force enable/disable building of libregex
AC_MSG_CHECKING(whether to build the regular expression library)
build_regex=auto
AC_ARG_ENABLE(regex-build,
	AC_HELP_STRING(--enable-regex-build,
		[Build and use the included regular expression library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_regex=no
	elif test "x$enableval" = "xyes" ; then
		build_regex=yes
	fi]
)
AC_MSG_RESULT($build_regex)

# force enable/disable building of libpng
AC_MSG_CHECKING(whether to build the PNG library)
build_png=auto
AC_ARG_ENABLE(png-build,
	AC_HELP_STRING(--enable-png-build,
		[Build and use the included Portable Network Graphics library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_png=no
	elif test "x$enableval" = "xyes" ; then
		build_png=yes
	fi]
)
AC_MSG_RESULT($build_png)

# force enable/disable building of zlib
AC_MSG_CHECKING(whether to build the data compression library)
build_zlib=auto
AC_ARG_ENABLE(zlib-build,
	AC_HELP_STRING(--enable-zlib-build,
		[Build and use the included zlib compression library (default=auto)]),
	[if test "x$enableval" = "xno" ; then
		build_zlib=no
	elif test "x$enableval" = "xyes" ; then
		build_zlib=yes
	fi]
)
AC_MSG_RESULT($build_zlib)

# doxygen
AC_MSG_CHECKING(whether to generate documentation with doxygen)
use_doxygen=no
AC_ARG_ENABLE(doxygen,
	AC_HELP_STRING(--enable-doxygen,
		[enable documentation generation with doxygen (default=no)]),
	[if ! test "xenable_doxygen" = "xno" ; then
		use_doxygen=yes
	fi]
)
AC_MSG_RESULT($use_doxygen)

# optimized
AC_MSG_CHECKING(whether to enable optimized compilation)
use_optimized=no
AC_ARG_ENABLE(optimized,
	AC_HELP_STRING(--enable-optimized,
		[enable optimized compilation (default=no)]),
	[if ! test "x$enable_optimized" = "xno" ; then
		use_optimized=yes
	fi]
)
AC_MSG_RESULT($use_optimized)

# warnings
AC_MSG_CHECKING(whether to enable verbose compilation warnings)
use_warnings=no
AC_ARG_ENABLE(warnings,
	AC_HELP_STRING(--enable-warnings,
		[enable verbose compilation warnings (default=no)]),
	[if ! test "x$enable_warnings" = "xno" ; then
		use_warnings=yes
	fi]
)
AC_MSG_RESULT($use_warnings)

# debugging
AC_MSG_CHECKING(whether to enable debug mode compilation)
use_debug=no
AC_ARG_ENABLE(debug,
	AC_HELP_STRING(--enable-debug,
		[enable debug symbols (default=no)]),
	[if ! test "x$enable_debug" = "xno" ; then
		use_debug=yes
	fi]
)
AC_MSG_RESULT($use_debug)

# profile
AC_MSG_CHECKING(whether to enable profile mode compilation)
use_profiling=no
AC_ARG_ENABLE(profiling,
	AC_HELP_STRING(--enable-profiling,
		[enable profiling (default=no)]),
	[if ! test "x$enable_profiling" = "xno" ; then
		use_profiling=yes
	fi]
)
AC_ARG_ENABLE(profile,,
	[if ! test "x$enable_profile" = "xno" ; then
		use_profiling=yes
	fi]
)
AC_MSG_RESULT($use_profiling)

AC_MSG_CHECKING(whether to compile jove or not)
use_jove=no
AC_ARG_ENABLE(jove,
	AC_HELP_STRING(--enable-jove,
		[enable compilation of jove (default=no)]),
	[if ! test "x$enable_jove" = "xno" ; then
		use_jove=yes
	fi]
)
AC_MSG_RESULT($use_jove)
AM_CONDITIONAL(BUILD_JOVE, [test "x$use_jove" = "xyes"])

# XXX pthread lib stuff should go away (and be made proper lib check)
case "$target" in
	*-*-linux*)
		PTHREAD_LIBS="-lpthread"
		;;
	*-*-*BSD*)
		PTHREAD_LIBS="-pthread"
		;;
	*-*-*irix*)
		is_irix=yes
		;;
esac
AC_SUBST(PTHREAD_LIBS)
AM_CONDITIONAL(IRIX, [test "x$is_irix" == yes])

dnl XXX should only add to build paths when something fails that is necessary
dnl additionally, a compiler might complain if /usr/local or /sw are already
dnl in the system search path -- so need to try a compile test..

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


AC_DEFINE(DM_X,1,[Display for X])
AC_DEFINE(USE_MESA_GL,1,[Assume mesagl is always around (fixme)])
AC_DEFINE(DM_OGL,1,[yeah, GL])
GL_CFLAGS="-I/usr/X11R6/include"
GL_LIBS="-L/usr/X11R6/lib -lGL"
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

LEX_CFLAGS="-Dyylex=cmd_nop"
AC_SUBST(LEX_CFLAGS)


dnl **************************
dnl *** Check for programs ***
dnl **************************

AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CPP

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LEX

dnl check for basic shell facilities
AC_PATH_PROG(SH, sh)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(RM, rm)

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
fi

AC_DEFINE(EXTERN_STDOUT,,[Stdout is extern])

if test "x$use_doxygen" = "xyes" ; then
	AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
	if test "x$DOXYGEN" = "x" ; then
		AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
		AC_MSG_ERROR([*** Unable to find doxygen ***])
	fi
fi
AC_MSG_CHECKING(whether to enable doxygen)
AC_MSG_RESULT($use_doxygen)

dnl now that the compiler is known, we can enable a compiler-specific cache file
dnl unless we are on sgi, where bash may choke on sed syntax in the cache
AC_MSG_CHECKING(what cache file is set to)
if test "x$cache_file" = "x/dev/null" ; then
	if ! test "x$host_os" = "xirix6.5" ; then
		compiler="`echo $CC | awk '{print $1}'`"
		cache_file="config.cache.${compiler}.${host_os}"
	fi
fi
AC_MSG_RESULT($cache_file)


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

SAVELIBS="${LIBS}"

dnl need curses, would prefer ncurses
LIBS=
AC_SEARCH_LIBS(initscr, ncurses curses,, 
	       AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
	       AC_MSG_ERROR([*** Need curses! ***]) )
CURSES="${LIBS}"

LIBS=
AC_SEARCH_LIBS(getvideo, gl, AC_DEFINE(HAS_SGIGL, 1, Irix GL is present.))
SGIGL="${LIBS}"
AC_SUBST(SGIGL)

LIBS="${SAVELIBS}"

AC_SEARCH_LIBS(mallopt, malloc)
AC_SEARCH_LIBS(matherr, mx)
AC_SEARCH_LIBS(dlopen, [dl ldl], AC_DEFINE(HAVE_DLOPEN, 1, dlopen is available))
AC_SUBST(DL)

dnl X
X_PREFIX=/usr/X11R6
X_CFLAGS="-I${X_PREFIX}/include"
X_LIBS="-L${X_PREFIX}/lib -lX11 -lXi"
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
AC_SUBST(X_PREFIX)

dnl tcl
TCLVER=8.4
TCL=libtcl${TCLVER}
case "$target" in
	powerpc-apple-darwin*)
		TCL_LIBS='-L${top_builddir}/src/other/libtcl/unix -ltcl${TCLVER} ${DL} ${SOCKET} -lm'
dnl -compatibility_version 20 -current_version 20.1
		;;
	*)
		TCL_LIBS='-L${top_builddir}/src/other/libtcl/unix -ltcl${TCLVER} -Wl,-rpath,${libdir} -rdynamic ${DL} ${SOCKET} -lm'
esac
TCL_LIBRARY="$prefix/tcl${TCLVER}"
if test "x$FRAMEWORK_BUILD" = "x1" ; then
	dnl this may be wrong
	TCL_BUILD_CFLAGS="-DTCL_PACKAGE_PATH=\\\"$libdir/Resources/Scripts\\\" -DTCL_LIBRARY=\\\"$TCL_LIBRARY\\\""
else
	TCL_BUILD_CFLAGS="-DTCL_PACKAGE_PATH=\\\"$libdir\\\" -DTCL_LIBRARY=\\\"$TCL_LIBRARY\\\""
fi
AC_SUBST(TCL)
AC_SUBST(TCLVER)
AC_SUBST(TCL_BUILD_CFLAGS)
AC_SUBST(TCL_LIBRARY)
AC_SUBST(TCL_LIBS)

dnl tk
TKVER="8.4"
TK_LIBS='-L${top_builddir}/src/other/libtk/unix -ltk${TKVER} ${TCL_LIBS} ${X_LIBS}'
TK_LIBRARY="$prefix/tk${TKVER}"
AC_SUBST(TKVER)
AC_SUBST(TK_LIBRARY)
AC_SUBST(TK_LIBS)

dnl itcl
ITCL_VERSION="3.3"
ITCL_LIBS='-L${top_builddir}/src/other/incrTcl/itcl -litcl${ITCL_VERSION} -Wl,-rpath,${libdir}/itcl${ITCL_VERSION}'
ITK_LIBS='-L${top_builddir}/src/other/incrTcl/itk -litk${ITCL_VERSION} -Wl,-rpath,${libdir}/itk${ITCL_VERSION}'
AC_SUBST(ITCL_LIBS)
AC_SUBST(ITCL_VERSION)
AC_SUBST(ITK_LIBS)

dnl detect a useable system regular expression library
regex_link_works=no
AC_CHECK_LIB(c, regcomp, regex_link_works=yes)
if test "x$regex_link_works" = "xno" ; then
	AC_CHECK_LIB(regex, regcomp, regex_link_works=yes)
fi
if test "x$regex_link_works" = "xno" ; then
	AC_CHECK_LIB(compat, regcomp, regex_link_works=yes)
fi

dnl detect a useable system zlib
zlib_link_works=no
AC_CHECK_LIB(z, deflate, zlib_link_works=yes)

dnl detect a useable system png library
png_link_works=no
AC_CHECK_LIB(png, png_check_sig, png_link_works=yes)


dnl *************************
dnl *** Check for headers ***
dnl *************************

AC_HEADER_STDC
AC_CHECK_HEADERS( \
	errno.h \
	fcntl.h \
	float.h \
	getopt.h \
	limits.h \
	limits.h \
	math.h \
	memory.h \
	pthread.h \
	stdarg.h \
	stdlib.h \
	string.h \
	sys/errno.h \
	sys/ioctl_compat.h \
	sys/machd.h \
	sys/mman.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/time.h \
	sys/types.h \
	sys/unistd.h \
	termcap.h \
	termios.h \
	time.h \
	unistd.h \
	varargs.h \
	xosdefs.h \
)
AC_CHECK_HEADER(sys/stat.h, AC_DEFINE(HAVE_UNIX_IO,, Fix this, sys/types.h and sys/stat.h ...))
AC_CHECK_HEADER(sys/time.h, AC_DEFINE(TIME_WITH_SYS_TIME,1,sys/time.h))

found_regex_h=no
AC_CHECK_HEADER(regex.h, 
		AC_DEFINE(HAVE_REGEX_H,1,[Define to 1 if you have the '<regex.h>' header file])
		found_regex_h=yes )

found_jni_h=no
AC_CHECK_HEADER(JavaVM/jni.h, 
		AC_DEFINE(HAVE_JAVAVM_JNI_H,1,[Define to 1 if you have the `<JavaVM/jni.h>' header file])
		found_jni_h=yes	)
AC_CHECK_HEADER(jni.h,
		AC_DEFINE(HAVE_JNI_H,1,[Define to 1 if you have the `<jni.h>' header file])
		found_jni_h=yes )
search_for_jni_h_more=no
if test "x$found_jni_h" = "xno" ; then
	if ! test "x$with_java" = "xno" ; then
		with_java=no
		serach_for_jni_h_more=yes
		if ! test "x$only_benchmark" = "xno" ; then
			if test "x$only_rts" = "xno" ; then
				search_for_jni_h_more=no
			fi
		fi
	fi
else
	# even though --with-jdk was not given, header was found and is needed
	if ! test "x$only_rts" = "xno" ; then
		with_java=yes
	fi
fi
if ! test "x$search_for_jni_h_more" = "xno" ; then
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
				  [Define to 1 if you have the `<jni.h>' header file])
			found_jni_h=yes )

	if ! test "x$found_jni_h" = "xno" ; then
		with_java=yes
		AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
		AC_MSG_WARN([jni.h was found by hunting through the JavaVM framework])
		AC_MSG_WARN([This means you probably need to install the latest Java SDK])
		AC_MSG_WARN([Build will try to use what it found, but you should update])
		AC_MSG_NOTICE([See http://developer.apple.com/java/ for updates])
		AC_MSG_NOTICE([<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<])
	else
		CPPFLAGS="$pre_cppflags"
		AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
		AC_MSG_WARN([Code dependant upon java and jni.h will not be built])
		AC_MSG_NOTICE([<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<])
	fi
fi


dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

AC_STRUCT_TM

dnl XXX - union wait testing from tcl8.4's configure.in, should be revamped?
dnl ============================================================
AC_MSG_CHECKING([union wait])
AC_CACHE_VAL(tcl_cv_union_wait,
    AC_TRY_LINK([#include <sys/types.h>
#include <sys/wait.h>], [
union wait x;
WIFEXITED(x);           /* Generates compiler error if WIFEXITED
                         * uses an int. */
    ], tcl_cv_union_wait=yes, tcl_cv_union_wait=no))
AC_MSG_RESULT($tcl_cv_union_wait)
if test $tcl_cv_union_wait = no; then
    AC_DEFINE(NO_UNION_WAIT,, Lacking a "union wait")
fi
dnl ============================================================

dnl XXX -- need to really check for time zone
AC_DEFINE(HAVE_TM_ZONE,,string in time zone)

AC_CHECK_MEMBERS([struct stat.st_blksize],[AC_DEFINE(HAVE_ST_BLKSIZE, 1, [the old blksize define])])

AC_CHECK_DECL(sys_errlist, 
	      AC_DEFINE(HAVE_SYS_ERRLIST_DECL,1,sys_errlist[]), 
	      AC_DEFINE(HAVE_SYS_ERRLIST_DECL,0,don't have sys_errlist[]),
	      [# include <stdio.h>])


dnl ******************************************
dnl *** Check for compiler characteristics ***
dnl ******************************************

# XXX some of these flags are presently GCC-specific
CPPFLAGS="$CPPFLAGS -I\$(top_srcdir)/include"
CFLAGS="$CFLAGS -fno-strict-aliasing"

if ! test "x$use_optimized" = "xno" ; then
	CFLAGS="$CFLAGS -O3 -fno-common -ffast-math -fstrength-reduce -fexpensive-optimizations"
	LDFLAGS="$LDFLAGS -O3 -fno-common -ffast-math -fstrength-reduce -fexpensive-optimizations"
fi

if ! test "x$use_debug" = "xno" ; then
	CFLAGS="$CFLAGS -g"
	LDFLAGS="$LDFLAGS -g"
fi

if ! test "x$use_profiling" = "xno" ; then
	CFLAGS="$CFLAGS -pg"
	LDFLAGS="$LDFLAGS -pg"
fi

if ! test "x$use_warnings" = "xno" ; then
	CFLAGS="$CFLAGS -W -Wall -Wundef -Wfloat-equal -Wshadow -Wunreachable-code -Winline -Wconversion"
	# XXX also of interest
	# -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -pedantic -ansi -Werror
fi

AC_C_CONST
AC_C_VOLATILE
AC_C_INLINE
AC_C_BIGENDIAN(AC_DEFINE(NATURAL_IEEE,,64b floating point storage technique), 
	       AC_DEFINE(REVERSED_IEEE,,64b floating point storage technique),
	       AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
	       AC_MSG_ERROR([*** Don't know the endian of this machine! ***]) )

dnl AC_C_PROTOTYPES
AM_C_PROTOTYPES

AC_DEFINE(USE_PROTOTYPES,1,heh)

dnl Last step is to make sure that we can actually compile
AC_MSG_CHECKING(compiler and flags for sanity)
AC_TRY_RUN([
#include <stdio.h>
int main(){exit(0);}
	],
	[	AC_MSG_RESULT(yes) ],
	[
		AC_MSG_RESULT(no)
		AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
		AC_MSG_ERROR([*** compiler cannot create working executables, check config.log ***])
	]
)


dnl ***************************
dnl *** Check for functions ***
dnl ***************************

AC_TYPE_SIGNAL
AC_CHECK_FUNCS(\
	access \
	bzero \
	dlopen \
	drand \
	fabs  \
	gethostname \
	getopt \
	sbrk \
	strcasecmp \
	strchr \
	strcpy \
	strdup \
	vfork \
	vprintf \
	writev \
)

AC_CHECK_FUNC(fstatfs, , [AC_DEFINE(NO_FSTATFS,, Lacking fstatfs)])
AC_CHECK_FUNC(popen, AC_DEFINE(HAVE_POPEN_DECL,1,popen))

# XXX duplicate for decl (needs to go away)
AC_CHECK_FUNC(getopt, AC_DEFINE(HAVE_GETOPT_DECL,1,getopt))
AC_CHECK_FUNC(strdup, AC_DEFINE(HAVE_STRDUP_DECL,1,strdup))

AC_CHECK_FUNCS(socket, SOCKET="" ,
	       [AC_CHECK_LIB(socket, socket, SOCKET="-lsocket", 
			     AC_MSG_NOTICE([<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<])
			     AC_MSG_ERROR([*** No internet socket found ***]))
	       ])
AC_SUBST(SOCKET)


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

AC_PATH_X
AC_PATH_XTRA

if ! test "x$only_rts" = "xno" ; then
	if test "x$with_java" = "xno" ; then
		AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
		AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
		AC_MSG_ERROR([*** Cannot locate jni.h for building librtserver ***])
	fi
fi
AM_CONDITIONAL(JAVA, [! test "x$with_java" = "xno"])

dnl *** libpng ***

dnl See if png library support works
png_works=no
AC_MSG_CHECKING(for portable network graphics functionality)
AC_TRY_RUN([
#include <png.h>
int main () {
  char *pngver = PNG_LIBPNG_VER_STRING;
  return 0;
}
	],
	[png_works=yes]
)
AC_MSG_RESULT($png_works)

dnl figure out whether we need to build libz
build_brlcad_png="no (using system)"
if test "x$build_png" = "xyes" ; then
	build_brlcad_png=yes
else
	if test "x$build_png" = "xno" ; then
		if ! test "x$png_works" = "xyes" ; then
			AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
			AC_MSG_NOTICE([Try adding --enable-png-build])
			AC_MSG_ERROR([*** Building libpng was disabled, yet no system png library was found ***])
		fi
	else
		dnl automatic detection
		if test "x$png_works" = "xno" ; then
			build_brlcad_png=yes
		else
			PNG_LIBS="-lpng -lz"
		fi
	fi
fi
AC_MSG_CHECKING(whether to build the included portable network graphics library)
if test "x$build_brlcad_png" = "xyes" ; then
	PNG_LIBS='${abs_top_builddir}/src/other/libpng/libpng.la -lz'
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(PNG_LIBS)
AM_CONDITIONAL(BUILD_PNG, [test "x$build_brlcad_png" = "xyes"])

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
	build_brlcad_bregex=yes
else
	if test "x$build_regex" = "xno" ; then
		if ! test "x$regex_works" = "xyes" ; then
			AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
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
	REGEX_LIBS='${abs_top_builddir}/src/other/libregex/libregex.la'
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(REGEX_LIBS)
AM_CONDITIONAL(BUILD_REGEX, [test "x$build_brlcad_regex" = "xyes"])

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
		if ! test "x$zlib_works" = "xyes" ; then
			AC_MSG_NOTICE([>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>])
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
if test "x$build_brlcad_zlib" = "xyes" ; then
	ZLIB_LIBS='${abs_top_builddir}/src/other/libz/libz.la'
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(ZLIB_LIBS)
AM_CONDITIONAL(BUILD_ZLIB, [test "x$build_brlcad_zlib" = "xyes"])


dnl ***************************
dnl *** Configure Makefiles ***
dnl ***************************

dnl XXX - These defines are available but not presently necessary
dnl CALTECH_MALLOC
dnl DRAND
dnl DLOPEN
dnl GETHOSTNAME
dnl SHELL_ESCAPE
dnl VPRINTF
dnl SBRK
dnl VFORK
dnl SBRK_DECL
dnl POPEN_DECL
dnl GETOPT_DECL
dnl XOPEN
dnl BZERO
dnl STRCHR
dnl MATHERR
dnl UNIX_DOMAIN_SOCKETS
dnl FLOAT
dnl GETOPT
dnl OFF_T
dnl WRITEV
dnl BSDGETTIMEOFDAY

BN='${top_builddir}/src/libbn/libbn.la ${BU}'
AC_SUBST(BN)

BU='${top_builddir}/src/libbu/libbu.la ${TCL_LIBS} ${PTHREAD_LIBS}'
AC_SUBST(BU)

CURSOR='${top_builddir}/src/libcursor/libcursor.la ${CURSES}'
AC_SUBST(CURSES)
AC_SUBST(CURSOR)

DM='${top_builddir}/src/libdm/libdm.la ${TK_LIBS} ${RT}'
AC_SUBST(DM)

FB='${top_builddir}/src/libfb/libfb.la ${X_LIBS} ${PKG} ${BU} ${GL_LIBS}'
AC_SUBST(FB)

FFT='${top_builddir}/src/libfft/libfft.la -lm'
AC_SUBST(FFT)

OPTICAL='${top_builddir}/src/liboptical/liboptical.la'
AC_SUBST(OPTICAL)

ORLE='${top_builddir}/src/liborle/liborle.la'
AC_SUBST(ORLE)

PKG='${top_builddir}/src/libpkg/libpkg.la'
AC_SUBST(PKG)

RLE='${top_builddir}/src/other/libutahrle/libutahrle.la -lm'
AC_SUBST(RLE)

RT='${top_builddir}/src/librt/librt.la ${BN} ${REGEX_LIBS}'
AC_SUBST(RT)

SYSV='${top_builddir}/src/libsysv/libsysv.la'
AC_SUBST(SYSV)

TCLCAD='${top_builddir}/src/libtclcad/libtclcad.la'
AC_SUBST(TCLCAD)

TERMIO='${top_builddir}/src/libtermio/libtermio.la'
AC_SUBST(TERMIO)

WDB='${top_builddir}/src/libwdb/libwdb.la'
AC_SUBST(WDB)


AC_CONFIG_SUBDIRS([ \
	src/other/libtcl/unix \
	src/other/libtk/unix \
	src/other/incrTcl \
])

AC_CONFIG_FILES([
	Makefile
	bench/Makefile
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
	misc/Makefile
	misc/macosx/Makefile
	misc/macosx/Resources/Makefile
	misc/macosx/Resources/ReadMe.rtfd/Makefile
	misc/macosx/Resources/Welcome.rtfd/Makefile
	misc/pro-engineer/Makefile
	misc/pro-engineer/resource/Makefile
	misc/pro-engineer/sgi_mips4/Makefile
	misc/pro-engineer/sgi_mips4/mk
	misc/pro-engineer/text/Makefile
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
	src/other/awf/Makefile
	src/other/iwidgets/Makefile
	src/other/iwidgets/doc/Makefile
	src/other/iwidgets/generic/Makefile
	src/other/iwidgets/iwidgets.tcl
	src/other/iwidgets/pkgIndex.tcl
	src/other/jove/Makefile
	src/other/libpng/Makefile
	src/other/libregex/Makefile
	src/other/libtcl/Makefile
	src/other/libtk/Makefile
	src/other/libtkGLX/Makefile
	src/other/libutahrle/Makefile
	src/other/libz/Makefile
	src/patch/Makefile
	src/proc-db/Makefile
	src/remrt/Makefile
	src/rt/Makefile
	src/rttherm/Makefile
	src/sample_applications/Makefile
	src/sample_applications/mk
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
	src/tools/Makefile
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


dnl **********************
dnl *** Report Summary ***
dnl **********************

echo
echo BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE
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
echo Build Tk .............................: yes
echo Build libpng .........................: ${build_brlcad_png}
echo Build libregex .......................: ${build_brlcad_regex}
echo Build libz ...........................: ${build_brlcad_zlib}
echo Build jove ...........................: ${use_jove}
echo 
echo Java Developer Kit support ...........: $with_java
echo X11 support ..........................: yes
echo Open GL support ......................: yes
echo Enable run-time debugging ............: yes
echo
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
echo ---
echo "$0 complete, type 'make' to begin building"
echo

