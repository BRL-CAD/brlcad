dnl **************************
dnl ** BRL-CAD configure.ac **
dnl **************************
dnl $Id$
dnl
dnl Herein lies the venerable GNU Autotools configure script for BRL-CAD.
dnl As best is reasonably possible, proper ordering and separation of tests
dnl and settings should be maintained per the recommended standard layout.
dnl The tests should be added to the rather clearly labeled sections below so
dnl that they are as follows:
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
dnl Any useful build arguments should be added to the output summary at the
dnl end.  Although it may be possible to check for certain features of command
dnl line arguments near the top, related tests should be delayed and placed
dnl into the appropriate test section.
dnl
dnl Features of the GNU Autotools that would require an increase in the minimum
dnl version requirement are highly discouraged.  Likewise discouraged is
dnl rolling back support for versions released prior to autoconf 2.50.
dnl
dnl Strive to tame the chaos that is so easily achieved.
dnl

AC_INIT(librt/bool.c)
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

AM_INIT_AUTOMAKE(brlcad, $BRLCAD_VERSION.$CONFIG_DATE)
AM_CONFIG_HEADER(h/config.h)

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

# XXX these flags are GCC-specific -- perhaps should set after the compiler check
CFLAGS="$CFLAGS "'-I$(top_srcdir)/h -fno-strict-aliasing'

# XXX pthread lib stuff should go away (and be made proper lib check)
case "$target" in
	*-*-linux*)
		PTHREAD_LIBS="-lpthread"
		;;
	*-*-*BSD*)
		PTHREAD_LIBS="-pthread"
		;;
esac
AC_SUBST(PTHREAD_LIBS)

dnl autosearch fink paths
AC_MSG_CHECKING([for fink])
if test -d /sw ; then
	AC_MSG_RESULT([found, adding /sw to search paths])
	CFLAGS="$CFLAGS -I/sw/include"
	LDFLAGS="$LDFLAGS -L/sw/lib"
else
	AC_MSG_RESULT([not found])
fi

JAVA_CFLAGS=
with_java=no
AC_ARG_WITH(jdk,
        [  --with-jdk=path         Specify location of Java Development Kit],[
        JAVA_CFLAGS="-I$withval/include -I$withval/include/`uname | tr [A-Z] [a-z]`"
        with_java=yes
        ])
case "$target" in
	*-*-darwin*)
		JAVA_CFLAGS="-framework JavaVM"
		JAVA_LDFLAGS="-shrext .jnilib -framework JavaVM"
		with_java=yes
		;;
esac
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)

AC_DEFINE(DM_X,1,[Display for X])
AC_DEFINE(USE_MESA_GL,1,[Assume mesagl is always around (fixme)])
AC_DEFINE(DM_OGL,1,[yeah, GL])
GL_CFLAGS="-I/usr/X11R6/include"
GL_LIBS="-L/usr/X11R6/lib -lGL"
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)

LEX_CFLAGS="-Dyylex=cmd_nop"
AC_SUBST(LEX_CFLAGS)

AC_C_BIGENDIAN(AC_DEFINE(NATURAL_IEEE,,64b floating point storage technique), AC_DEFINE(REVERSED_IEEE,,64b floating point storage technique), AC_ERROR("Don't know the endian of this machine!"))

AC_DEFINE(USE_PROTOTYPES,1,heh)
AC_DEFINE(USE_REGCOMP,1,new regex stuff)

# debugging
AC_ARG_ENABLE(debug, AC_HELP_STRING(--enable-debug,
		     [enable debug mode compilation [default=no]]))

# profile
AC_ARG_ENABLE(profile, AC_HELP_STRING(--enable-profile,
		       [enable profiling [default=no]]))

# warnings
AC_ARG_ENABLE(warnings, AC_HELP_STRING(--enable-warnings,
			[enable verbose compilation warnings [default=automatic]]))

# doxygen
AC_ARG_ENABLE(doxygen, AC_HELP_STRING(--enable-doxygen,
		       [enable documentation generation with doxygen [default=automatic]]))

# only build benchmark
only_benchmark="no"
AC_ARG_ENABLE(only-benchmark, AC_HELP_STRING(--enable-only-benchmark,
		  [Only build the components necessary for benchmark]) ,
	[
		if ! test "x$enableval" = "xno" ; then
			only_benchmark="yes"
		fi
	]
)
AC_MSG_CHECKING(whether to only build benchmark)
AC_MSG_RESULT($only_benchmark)
AM_CONDITIONAL(ONLY_BENCHMARK, test "x$only_benchmark" = "xyes")

# only build librtserver
only_rts="no"
AC_ARG_ENABLE(only-rts, AC_HELP_STRING(--enable-only-rts,
		  [Only build the components necessary for librtserver]) ,
	[
		if ! test "x$enableval" = "xno" ; then
			only_rts="yes"
			if test "x$only_benchmark" = "xyes" ; then
				AC_MSG_WARN(Requesting BOTH benchmark and librtserver "only"?)
				AC_MSG_WARN(Building both benchmark and librtserver)
			fi
		fi
	]
)
AC_MSG_CHECKING(whether to only build librtserver)
AC_MSG_RESULT($only_rts)
AM_CONDITIONAL(ONLY_RTS, test "x$only_rts" = "xyes")


dnl **************************
dnl *** Check for programs ***
dnl **************************

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LEX

AM_C_PROTOTYPES

AC_DEFINE(HAS_POSIX_THREADS,1,[Posix Threading API])

AC_DEFINE(EXTERN_STDOUT,,[Stdout is extern])

if ! test "x$enable_doxygen" = "xno" ; then
	AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
	if test "x$DOXYGEN" = "x" ; then
		if test "x$enable_doxygen" = "xyes" ; then
			AC_MSG_ERROR([cound not find doxygen])
		fi
		enable_doxygen=no
	fi
fi
AC_MSG_CHECKING(whether to enable doxygen)
AC_MSG_RESULT($enable_doxygen)

AC_MSG_CHECKING(compiler and flags for sanity)
AC_TRY_RUN([
#include <stdio.h>
int main(){exit(0);}
	],
	[	AC_MSG_RESULT(yes) ],
	[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([*** compiler cannot create working executables, check config.log ***])
	]
)


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

SAVELIBS="${LIBS}"

dnl need curses, would prefer ncurses
LIBS=
AC_SEARCH_LIBS(initscr, ncurses curses,, AC_MSG_ERROR([Need curses!]))
CURSES="${LIBS}"

LIBS=
AC_SEARCH_LIBS(getvideo, gl, AC_DEFINE(HAS_SGIGL,1, Irix GL is present.))
SGIGL="${LIBS}"
AC_SUBST(SGIGL)

LIBS="${SAVELIBS}"

AC_SEARCH_LIBS(mallopt, malloc)
AC_SEARCH_LIBS(matherr, mx)
AC_SEARCH_LIBS(dlopen, [dl ldl], AC_DEFINE(HAVE_DLOPEN,1,dlopen is available))
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
		TCL_LIBS='-L${top_builddir}/libtcl${TCLVER}/build -ltcl${TCLVER} ${DL} ${SOCKET} -lm'
dnl -compatibility_version 20 -current_version 20.1
		;;
	*)
		TCL_LIBS='-L${top_builddir}/libtcl${TCLVER}/build -ltcl${TCLVER} -Wl,-rpath,${libdir} -rdynamic ${DL} ${SOCKET} -lm'
		;;
esac
TCL_LIBRARY="$prefix/tcl${TCLVER}"
if test "$FRAMEWORK_BUILD" = "1" ; then
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
TK_LIBS='-L${top_builddir}/libtk${TKVER}/build -ltk${TKVER} ${TCL_LIBS} ${X_LIBS}'
TK_LIBRARY="$prefix/tk${TKVER}"
AC_SUBST(TKVER)
AC_SUBST(TK_LIBRARY)
AC_SUBST(TK_LIBS)

dnl itcl
ITCL_VERSION="3.3"
ITCL_LIBS='${top_builddir}/libitcl/itcl/libitcl.la'
ITK_LIBS='${top_builddir}/libitcl/itk/libitk.la'
AC_SUBST(ITCL_LIBS)
AC_SUBST(ITCL_VERSION)
AC_SUBST(ITK_LIBS)

PNG_LIBS='${top_builddir}/libpng/libpng.la ${top_builddir}/libz/libz.la'
AC_SUBST(PNG_LIBS)


dnl *************************
dnl *** Check for headers ***
dnl *************************

AC_HEADER_STDC
AC_CHECK_HEADERS( \
	float.h \
	getopt.h \
	limits.h \
	limits.h \
	memory.h \
	stdarg.h \
	stdlib.h \
	string.h \
	sys/ioctl_compat.h \
	sys/machd.h \
	sys/mman.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/time.h \
	sys/types.h \
	termcap.h \
	termios.h \
	time.h \
	unistd.h \
	varargs.h \
	xosdefs.h \
)
AC_CHECK_HEADER(sys/stat.h, AC_DEFINE(HAVE_UNIX_IO,, Fix this, sys/types.h and sys/stat.h ...))
AC_CHECK_HEADER(sys/time.h, AC_DEFINE(TIME_WITH_SYS_TIME,1,sys/time.h))

found_jni_h=no
AC_CHECK_HEADER(JavaVM/jni.h, 
		AC_DEFINE(HAVE_JAVAVM_JNI_H,1,[Define to 1 if you have the `<JavaVM/jni.h>' header file])
		found_jni_h=yes	)
AC_CHECK_HEADER(jni.h,
		AC_DEFINE(HAVE_JNI_H,1,[Define to 1 if you have the `<jni.h>' header file])
		found_jni_h=yes )
if ! test "x$found_jni_h" = "xyes" ; then
	if test "x$with_java" = "xyes" ; then
		AC_MSG_WARN([Code dependant upon java and jni.h will not be built])
	fi
	with_java=no
fi


dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

AC_C_CONST
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

AC_CHECK_DECL(sys_errlist, AC_DEFINE(HAVE_SYS_ERRLIST_DECL,1,sys_errlist[]), AC_DEFINE(HAVE_SYS_ERRLIST_DECL,0,don't have sys_errlist[]),[# include <stdio.h>])
AC_CHECK_DECL(re_comp, AC_DEFINE(HAVE_REGEX_DECL,, BSD regular expressions))


dnl ******************************************
dnl *** Check for compiler characteristics ***
dnl ******************************************

# nothing to do compiler-wise yet


dnl ***************************
dnl *** Check for functions ***
dnl ***************************

AC_TYPE_SIGNAL
AC_CHECK_FUNCS( \
	bzero \
	dlopen \
	drand \
	gethostname \
	getopt \
	regex \
	sbrk \
	strcasecmp \
	strchr \
	strdup \
	strdup\
	vfork \
	vprintf \
	writev \
)

AC_CHECK_FUNC(fstatfs, , [AC_DEFINE(NO_FSTATFS,, Lacking fstatfs)])
AC_CHECK_FUNC(popen, AC_DEFINE(HAVE_POPEN_DECL,1,popen))

# XXX duplicate for decl (needs to go away)
AC_CHECK_FUNC(getopt, AC_DEFINE(HAVE_GETOPT_DECL,1,getopt))
AC_CHECK_FUNC(strdup, AC_DEFINE(HAVE_STRDUP_DECL,1,strdup))

AC_CHECK_FUNCS(socket, SOCKET="" ,[AC_CHECK_LIB(socket, socket, SOCKET="-lsocket", AC_MSG_ERROR([No internet socket found]))])
AC_SUBST(SOCKET)

dnl AC_CHECK_FUNCS(re_comp, REGEX_LIB="", [AC_CHECK_LIB(compat, re_comp, REGEX_LIB="-lcompat",)])
AC_SUBST(REGEX_LIB)


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

if test "x$only_rts" = "xyes" ; then
	if ! test "x$with_java" = "xyes" ; then
		AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
		AC_MSG_ERROR([Cannot locate jni.h for building librtserver])
	fi
fi
AM_CONDITIONAL(JAVA, test "x$with_java" = "xyes")


dnl ***************************
dnl *** Configure Makefiles ***
dnl ***************************

dnl XXX - These defines are available but not presently necessary
dnl POSIX_REGEXEC
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
dnl REGEX_DECL
dnl XOPEN
dnl BZERO
dnl STRCHR
dnl MATHERR
dnl UNIX_DOMAIN_SOCKETS
dnl FLOAT
dnl GETOPT
dnl OFF_T
dnl WRITEV
dnl REGEX
dnl BSDGETTIMEOFDAY

BN='${top_builddir}/libbn/libbn.la ${BU}'
AC_SUBST(BN)

BU='${top_builddir}/libbu/libbu.la ${TCL_LIBS} ${PTHREAD_LIBS}'
AC_SUBST(BU)

CURSOR='${top_builddir}/libcursor/libcursor.la ${CURSES}'
AC_SUBST(CURSES)
AC_SUBST(CURSOR)

DM='${top_builddir}/libdm/libdm.la ${TK_LIBS} ${RT}'
AC_SUBST(DM)

FB='${top_builddir}/libfb/libfb.la ${X_LIBS} ${PKG} ${BU} ${GL_LIBS}'
AC_SUBST(FB)

FFT='${top_builddir}/libfft/libfft.la -lm'
AC_SUBST(FFT)

OPTICAL='${top_builddir}/liboptical/liboptical.la'
AC_SUBST(OPTICAL)

ORLE='${top_builddir}/liborle/liborle.la'
AC_SUBST(ORLE)

PKG='${top_builddir}/libpkg/libpkg.la'
AC_SUBST(PKG)

RLE='${top_builddir}/libutahrle/libutahrle.la -lm'
AC_SUBST(RLE)

RT='${top_builddir}/librt/librt.la ${BN} ${REGEX_LIB}'
AC_SUBST(RT)

SYSV='${top_builddir}/libsysv/libsysv.la'
AC_SUBST(SYSV)

TCLCAD='${top_builddir}/libtclcad/libtclcad.la'
AC_SUBST(TCLCAD)

TERMIO='${top_builddir}/libtermio/libtermio.la'
AC_SUBST(TERMIO)

WDB='${top_builddir}/libwdb/libwdb.la'
AC_SUBST(WDB)

AC_CONFIG_FILES([
	Makefile
	anim/Makefile
	awf/Makefile
	bench/Makefile
	brlman/Makefile
	burst/Makefile
	bwish/Makefile
	comgeom-g/Makefile
	conv/Makefile
	db/Makefile
	doc/Makefile
	fb/Makefile
	fbed/Makefile
	fbserv/Makefile
	gtools/Makefile
	h/Makefile
	halftone/Makefile
	html/Makefile
	html/ReleaseNotes/Makefile
	html/ReleaseNotes/Rel5.0/Makefile
	html/ReleaseNotes/Rel5.0/Summary/Makefile
	html/ReleaseNotes/Rel6.0/Makefile
	html/manuals/Anim_Tutorial/Makefile
	html/manuals/Makefile
	html/manuals/ReleaseNotes/Makefile
	html/manuals/cadwidgets/Makefile
	html/manuals/libbu/Makefile
	html/manuals/libdm/Makefile
	html/manuals/librt/Makefile
	html/manuals/mged/Makefile
	html/manuals/mged/animmate/Makefile
	html/manuals/shaders/Makefile
	iges/Makefile
	irprep/Makefile
	iwidgets4.0.1/Makefile
	iwidgets4.0.1/doc/Makefile
	iwidgets4.0.1/generic/Makefile
	iwidgets4.0.1/iwidgets.tcl
	iwidgets4.0.1/pkgIndex.tcl
	jack/Makefile
	lgt/Makefile
	libbn/Makefile
	libbu/Makefile
	libcursor/Makefile
	libdm/Makefile
	libfb/Makefile
	libfft/Makefile
	libitcl/Makefile
	libitcl/itcl/Makefile
	libitcl/itcl/doc/Makefile
	libitcl/itcl/generic/Makefile
	libitcl/itcl/library/Makefile
	libitcl/itk/Makefile
	libitcl/itk/doc/Makefile
	libitcl/itk/generic/Makefile
	libitcl/itk/library/Makefile
	liboptical/Makefile
	liborle/Makefile
	libpkg/Makefile
	libpng/Makefile
	librt/Makefile
	librtserver/Makefile
	libsysv/Makefile
	libtcl8.4/Makefile
	libtclcad/Makefile
	libtermio/Makefile
	libtk8.4/Makefile
	libutahrle/Makefile
	libwdb/Makefile
	libz/Makefile
	mged/Makefile
	mk/Makefile
	nirt/Makefile
	off/Makefile
	patch/Makefile
	pro-engineer/Makefile
	pro-engineer/resource/Makefile
	pro-engineer/sgi_mips4/Makefile
	pro-engineer/sgi_mips4/mk
	pro-engineer/text/Makefile
	remrt/Makefile
	rt/Makefile
	sample_applications/Makefile
	sample_applications/mk
	sh/Makefile
	sig/Makefile
	tab/Makefile
	tclscripts/Makefile
	tclscripts/geometree/Makefile
	tclscripts/lib/Makefile
	tclscripts/mged/Makefile
	tclscripts/nirt/Makefile
	tclscripts/pl-dm/Makefile
	tclscripts/rtwizard/Makefile
	tclscripts/rtwizard/examples/Makefile
	tclscripts/rtwizard/examples/PictureTypeA/Makefile
	tclscripts/rtwizard/examples/PictureTypeB/Makefile
	tclscripts/rtwizard/examples/PictureTypeC/Makefile
	tclscripts/rtwizard/examples/PictureTypeD/Makefile
	tclscripts/rtwizard/examples/PictureTypeE/Makefile
	tclscripts/rtwizard/examples/PictureTypeF/Makefile
	tclscripts/rtwizard/lib/Makefile
	tclscripts/util/Makefile
	tools/Makefile
	unigraphics/Makefile
	util/Makefile
	vas4/Makefile
	vdeck/Makefile
	vfont/Makefile
 	brlcad.spec
])
#AC_CONFIG_SUBDIRS([
#	libtcl8.4/unix
#	libtk8.4/unix
#])
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
echo Build Tcl ............................: yes
echo Build Tk .............................: yes
echo Build Itcl/Itk .......................: yes
echo Build IWidgets .......................: yes
echo Build libregex .......................: yes
echo Build libpng .........................: yes
echo Build libz ...........................: yes
echo 
echo Java Developer Kit support ...........: $with_java
echo X11 support ..........................: yes
echo Open GL support ......................: yes
echo Enable run-time debugging ............: yes
echo
echo Build static libraries ...............: $enable_static
echo Build shared/dynamic libraries .......: $enable_shared
echo Build debug release ..................: $enable_debug
echo Build profile release ................: $enable_profile
echo Print verbose compilation warnings ...: $enable_warnings
echo
echo Only build benchmark suite ...........: $only_benchmark
echo Only build librtserver ...............: $only_rts
echo Generate doxygen documentation .......: $enable_doxygen
echo
echo ---
echo $0 complete, type \'make\' to begin building
echo
