dnl $Id$

AC_INIT(librt/bool.c)
AC_PREREQ(2.14)

MAJOR_VERSION=6
MINOR_VERSION=1
PATCH_VERSION=0
CONFIG_DATE=`date +%Y%m%d`
BRLCAD_VERSION="$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION"
echo "Configuring BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE"

AC_CANONICAL_TARGET([])
AM_INIT_AUTOMAKE(brlcad, $BRLCAD_VERSION.$CONFIG_DATE)
AM_CONFIG_HEADER(h/config.h)


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

CFLAGS="$CFLAGS "'-I$(top_srcdir)/h -fno-strict-aliasing'

case "$target" in
	*-*-linux*)
		PTHREAD_LIBS="-lpthread"
		;;
	*-*-*BSD*)
		PTHREAD_LIBS="-pthread"
		;;
esac

JAVA_CFLAGS=
have_java=no
AC_ARG_WITH(jdk,
        [  --with-jdk=path         Specify location of Java Development Kit],[
        JAVA_CFLAGS="-I$withval/include -I$withval/include/`uname | tr [A-Z] [a-z]`"
        have_java=yes
        ])
AM_CONDITIONAL(JAVA, test x$have_java = xyes)

AC_DEFINE(DM_X,1,[Display for X])
AC_DEFINE(USE_MESA_GL,1,[Assume mesagl is always around (fixme)])
AC_DEFINE(DM_OGL,1,[yeah, GL])
GL_CFLAGS="-I/usr/X11R6/include"
GL_LIBS="-L/usr/X11R6/lib -lGL"

LEX_CFLAGS="-Dyylex=cmd_nop"

AC_C_BIGENDIAN(AC_DEFINE(NATURAL_IEEE,,64b floating point storage technique), AC_DEFINE(REVERSED_IEEE,,64b floating point storage technique), AC_ERROR("Don't know the endian of this machine!"))

AC_DEFINE(USE_PROTOTYPES,1,heh)
AC_DEFINE(USE_REGCOMP,1,new regex stuff)


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

LIBS="${SAVELIBS}"

AC_SEARCH_LIBS(mallopt, malloc)
AC_SEARCH_LIBS(matherr, mx)
AC_SEARCH_LIBS(dlopen, [dl ldl], AC_DEFINE(HAVE_DLOPEN,1,dlopen is available))

dnl X
X_PREFIX=/usr/X11R6
X_CFLAGS="-I${X_PREFIX}/include"
X_LIBS="-L${X_PREFIX}/lib -lX11 -lXi"

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

dnl tk
TKVER="8.4"
TK_LIBS='-L${top_builddir}/libtk${TKVER}/build -ltk${TKVER} ${TCL_LIBS} ${X_LIBS}'
TK_LIBRARY="$prefix/tk${TKVER}"

dnl itcl
ITCL_VERSION="3.3"
ITCL_LIBS='${top_builddir}/libitcl/itcl/libitcl.la'
ITK_LIBS='${top_builddir}/libitcl/itk/libitk.la'

PNG_LIBS='${top_builddir}/libpng/libpng.la ${top_builddir}/libz/libz.la'


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
	xosdefs.h
)
AC_CHECK_HEADER(sys/stat.h, AC_DEFINE(HAVE_UNIX_IO,, Fix this, sys/types.h and sys/stat.h ...))
AC_CHECK_HEADER(sys/time.h, AC_DEFINE(TIME_WITH_SYS_TIME,1,sys/time.h))


dnl ***********************
dnl *** Check for types ***
dnl ***********************

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


dnl ***************************
dnl *** Check for functions ***
dnl ***************************

AC_CHECK_FUNC(fstatfs, , [AC_DEFINE(NO_FSTATFS,, Lacking fstatfs)])
AC_CHECK_FUNC(getopt, AC_DEFINE(HAVE_GETOPT_DECL,1,getopt))
AC_CHECK_FUNC(popen, AC_DEFINE(HAVE_POPEN_DECL,1,popen))
AC_CHECK_FUNC(strdup, AC_DEFINE(HAVE_STRDUP_DECL,1,strdup))
AC_CHECK_FUNCS(drand gethostname vprintf sbrk vfork dlopen bzero strdup strcasecmp strchr getopt writev regex strdup)
AC_CHECK_FUNCS(socket, SOCKET="" ,[AC_CHECK_LIB(socket, socket, SOCKET="-lsocket", AC_MSG_ERROR([No internet socket found]))])
dnl AC_CHECK_FUNCS(re_comp, REGEX_LIB="", [AC_CHECK_LIB(compat, re_comp, REGEX_LIB="-lcompat",)])


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
BU='${top_builddir}/libbu/libbu.la ${TCL_LIBS} ${PTHREAD_LIBS}'
CURSOR='${top_builddir}/libcursor/libcursor.la ${CURSES}'
DM='${top_builddir}/libdm/libdm.la ${TK_LIBS} ${RT}'
FB='${top_builddir}/libfb/libfb.la ${X_LIBS} ${PKG} ${BU} ${GL_LIBS}'
FFT='${top_builddir}/libfft/libfft.la -lm'
OPTICAL='${top_builddir}/liboptical/liboptical.la'
ORLE='${top_builddir}/liborle/liborle.la'
PKG='${top_builddir}/libpkg/libpkg.la'
RLE='${top_builddir}/libutahrle/libutahrle.la -lm'
RT='${top_builddir}/librt/librt.la ${BN} ${REGEX_LIB}'
SYSV='${top_builddir}/libsysv/libsysv.la'
TCLCAD='${top_builddir}/libtclcad/libtclcad.la'
TERMIO='${top_builddir}/libtermio/libtermio.la'
WDB='${top_builddir}/libwdb/libwdb.la'

AC_SUBST(BN)
AC_SUBST(BU)
AC_SUBST(CURSES)
AC_SUBST(CURSOR)
AC_SUBST(DL)
AC_SUBST(DM)
AC_SUBST(FB)
AC_SUBST(FFT)
AC_SUBST(GL_CFLAGS)
AC_SUBST(GL_LIBS)
AC_SUBST(ITCL_LIBS)
AC_SUBST(ITCL_VERSION)
AC_SUBST(ITK_LIBS)
AC_SUBST(JAVARTSERVER)
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(LEX_CFLAGS)
AC_SUBST(OPTICAL)
AC_SUBST(ORLE)
AC_SUBST(PKG)
AC_SUBST(PNG_LIBS)
AC_SUBST(PTHREAD_LIBS)
AC_SUBST(REGEX_LIB)
AC_SUBST(RLE)
AC_SUBST(RT)
AC_SUBST(SGIGL)
AC_SUBST(SOCKET)
AC_SUBST(SYSV)
AC_SUBST(TCL)
AC_SUBST(TCLCAD)
AC_SUBST(TCLVER)
AC_SUBST(TCL_BUILD_CFLAGS)
AC_SUBST(TCL_LIBRARY)
AC_SUBST(TCL_LIBS)
AC_SUBST(TERMIO)
AC_SUBST(TKVER)
AC_SUBST(TK_LIBRARY)
AC_SUBST(TK_LIBS)
AC_SUBST(WDB)
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
AC_SUBST(X_PREFIX)

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
AC_OUTPUT


dnl **********************
dnl *** Report Summary ***
dnl **********************

dnl XXX - should report significant summary configuration details (what options
dnl and args are enabled, disabled, build settings, etc).
