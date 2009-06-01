dnl                    C O N F I G U R E . A C
dnl SCL
dnl
dnl Copyright (c) 2004-2009 United States Government as represented by
dnl the U.S. Army Research Laboratory.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl
dnl 1. Redistributions of source code must retain the above copyright
dnl notice, this list of conditions and the following disclaimer.
dnl
dnl 2. Redistributions in binary form must reproduce the above
dnl copyright notice, this list of conditions and the following
dnl disclaimer in the documentation and/or other materials provided
dnl with the distribution.
dnl
dnl 3. The name of the author may not be used to endorse or promote
dnl products derived from this software without specific prior written
dnl permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
dnl OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
dnl WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
dnl ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
dnl DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
dnl GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
dnl INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
dnl WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
dnl NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
dnl SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
dnl
dnl $Id: configure.ac 33887 2009-02-24 06:02:51Z scl $
dnl
dnl ******************************************************************
dnl ***                    SCL's configure.ac                      ***
dnl ******************************************************************
dnl
dnl Herein lies the venerable GNU Build System configure template for
dnl SCL.  As best is reasonably possible, proper ordering and
dnl separation of tests and settings should be maintained per the
dnl recommended standard layout.  The tests should be added to the
dnl rather clearly labeled sections below so that they are as
dnl follows:
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
dnl to the AC_PREREQ version shown below unless extensive testing has
dnl been performed.
dnl
dnl Strive to tame the chaos that is so easily achieved.
dnl

dnl minimum version of autoconf required.  should coincide with
dnl setting in autogen.sh script.
AC_PREREQ(2.52)

dnl See HACKING for details on how to properly update the version
define([MAJOR_VERSION], [patsubst(esyscmd([cat include/conf/MAJOR]), [
])])
define([MINOR_VERSION], [patsubst(esyscmd([cat include/conf/MINOR]), [
])])
define([PATCH_VERSION], [patsubst(esyscmd([cat include/conf/PATCH]), [
])])
define([SCL_VERSION], [patsubst([MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION], [
])])

AC_INIT(STEP Class Libraries, [SCL_VERSION], [http://brlcad.org], scl)
AC_REVISION($Revision: 33887 $)

AC_CONFIG_AUX_DIR(misc)

# keep track of cmd-line options for later
BC_ARG0="$0"
AC_SUBST(BC_ARG0)
BC_ARGS="$*"
AC_SUBST(BC_ARGS)

dnl just in case
PACKAGE_NAME="SCL"
AC_SUBST(PACKAGE_NAME)

SCLLIB_VERSION=SCL_VERSION
AC_SUBST(SCLLIB_VERSION)

# force locale setting to C so things like date output as expected
LC_ALL=C
LANG=C

CONFIG_DAY=`date +%d`
CONFIG_MONTH=`date +%m`
CONFIG_YEAR=`date +%Y`
CONFIG_DATE="${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}"
CONFIG_TS="`date -R 2>/dev/null || date +'%a, %d %b %Y %H:%M:%S %z' 2>/dev/null || date`" # try RFC 2822
AC_SUBST(CONFIG_DAY)
AC_SUBST(CONFIG_MONTH)
AC_SUBST(CONFIG_YEAR)
AC_SUBST(CONFIG_DATE)
AC_SUBST(CONFIG_TS)


# print out the title with a pretty box computed to wrap around
title="Configuring STEP Class Libraries Release SCL_VERSION, Build $CONFIG_DATE"
length="`echo x${title}x | wc -c`"
separator=""
while test $length -gt 1 ; do
    separator="${separator}*"
    length="`expr $length - 1`"
done

BC_BOLD
AC_MSG_RESULT([***${separator}***])
AC_MSG_RESULT([*** ${title} ***])
AC_MSG_RESULT([***${separator}***])
BC_UNBOLD

# necessary for proper file creation on nfs volumes
umask 022


# override the default autoconf cflags if user has not modified them
if test "x$CFLAGS" = "x" ; then
	# an unset CFLAGS var is set to "-O2 -g" during AC_PROG_CC, so
	# set it to something benign instead like whitespace
	CFLAGS=" "
fi
if test "x$CXXFLAGS" = "x" ; then
	# an unset CXXFLAGS var is set to "-O2 -g" during AC_PROG_CXX, so
	# set it to something benign instead like whitespace
	CXXFLAGS=" "
fi

# override the default (empty) yflags (yacc) if user has not modified them
if test "x$YFLAGS" = "x" ; then
	YFLAGS="-d"
fi
AC_SUBST(YFLAGS)

# cannot override LD directly, so warn about that (configure sets it)
if test "x$LD" != "x" ; then
	AC_MSG_WARN([LD cannot be set directly yet it seems to be set ($LD)])
	sleep 1
fi

# classify this machine
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# sanity check, make sure we have sources where we expect them
AC_CONFIG_SRCDIR([src/express/express.c])

# where are we coming from and where are we going to.
# prefix is set to NONE until AC_OUTPUT unless --prefix is used
AC_PREFIX_DEFAULT([/usr/scl])

# set up the SCL_ROOT installation path, sets bc_prefix
BC_SCL_ROOT

# set up the SCL_DATA install directory, sets bc_data_dir
#   e.g. /usr/local/share/scl/3.2.0
BC_SCL_DATA

# init the venerable automake only _once_ or incur the wrath of
# several automake bugs (like "aclocal-" and install detection)
AM_INIT_AUTOMAKE([1.6 dist-zip dist-bzip2])

# disables the build system dependency tracking by default for the
# automake and autoconf template files.
#AM_MAINTAINER_MODE

# write out all of our definitions to this header
AM_CONFIG_HEADER([include/scl_config.h])

# automatically enable and load our configure cache file if available
BC_CONFIG_CACHE([config.cache.${host_os}.${ac_hostname}])


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

#
# DOCUMENT NEW ARGUMENTS AND ALIASES IN THE "INSTALL" FILE.
#

BC_CONFIGURE_STAGE([arguments], [1 of 9])

dnl *** enable options ***

# only build express
BC_ARG_ENABLE([only_express], [only-express], [Only build the core express library], [no])

# run-time debugging
BC_ARG_ENABLE([build_runtime_debug], [runtime-debug], [Enable run-time debug checking], [yes])

# enable 64-bit builds
BC_ARG_ENABLE([build_64bit], [64bit-build], [Enable 64-bit compilation mode], [auto])

# optimized
BC_ARG_ENABLE([use_optimized], [optimized], [Enable optimized compilation], [no])

# debug
BC_ARG_ENABLE([use_debug], [debug], [Enable debug symbols], [auto])

# profiling
BC_ARG_ENABLE([use_profiling], [profiling], [Enable profiling], [no])

# verbosity
BC_ARG_ENABLE([build_verbose], [verbose], [Enable verbose compilation], [auto])

# warnings
BC_ARG_ENABLE([build_warnings], [warnings], [Enable verbose compilation warnings], [no])

# build progress status
BC_ARG_ENABLE([build_progress], [progress], [Enable verbose compilation progress], [no])


###
# argument aliases
# they need to go below here in order for --help to consolidate the
# blank line that it inserts to exactly one line in the right place.
#
# DOCUMENT NEW ARGUMENTS AND ALIASES IN THE "INSTALL" FILE.
###

# aliases for building only the express library
BC_ARG_ALIAS([only_express], [only-exp])
BC_ARG_ALIAS([only_express], [only-express])
BC_ARG_ALIAS([only_express], [exp-only])
BC_ARG_ALIAS([only_express], [express-only])
BC_ARG_ALIAS([only_express], [libexpress-only])

# run-time debugging (help uses runtime-debug)
BC_ARG_ALIAS([build_runtime_debug], [run-time-debug])
BC_ARG_ALIAS([build_runtime_debug], [runtime-debugging])
BC_ARG_ALIAS([build_runtime_debug], [run-time-debugging])

# 64-bit compilation (help uses 64bit-build)
BC_ARG_ALIAS([build_64bit], [64bit])
BC_ARG_ALIAS([build_64bit], [64])
BC_ARG_ALIAS([build_64bit], [64-build])
BC_ARG_ALIAS([build_64bit], [64-bit])
BC_ARG_ALIAS([build_64bit], [64-bit-build])

# optimized aliases (help uses optimized)
BC_ARG_ALIAS([use_optimized], [opt])
BC_ARG_ALIAS([use_optimized], [optimize])
BC_ARG_ALIAS([use_optimized], [optimization])
BC_ARG_ALIAS([use_optimized], [optimizations])

# debug aliases (help uses debug)
BC_ARG_ALIAS([use_debug], [debugging])

# profiling aliases (help uses profiling)
BC_ARG_ALIAS([use_profiling], [profile])
BC_ARG_ALIAS([use_profiling], [profiled])

# verbose output (help uses verbose)
BC_ARG_ALIAS([build_verbose], [verbosity])
BC_ARG_ALIAS([build_verbose], [output-verbose])
BC_ARG_ALIAS([build_verbose], [verbose-output])
BC_ARG_ALIAS([build_verbose], [build-verbose])
BC_ARG_ALIAS([build_verbose], [verbose-build])

# verbose warning aliases (help uses warnings)
BC_ARG_ALIAS([build_warnings], [warning])
BC_ARG_ALIAS([build_warnings], [verbose-warnings])
BC_ARG_ALIAS([build_warnings], [warnings-verbose])
BC_ARG_ALIAS([build_warnings], [build-warnings])
BC_ARG_ALIAS([build_warnings], [warnings-build])

# verbose compilation progress status (help uses progress)
BC_ARG_ALIAS([build_progress], [verbose-progress])
BC_ARG_ALIAS([build_progress], [progress-verbose])
BC_ARG_ALIAS([build_progress], [build-progress])
BC_ARG_ALIAS([build_progress], [progress-build])

###
# set up path searching
###

dnl automatically scan /usr/local (e.g. BSD uses /usr/local for ports)
BC_SEARCH_DIRECTORY([/usr/local])


###
# argument sanity checks and meta-argument settings
# here go checks to warn or abort when conflicting options specified
###

# turning on verbose means turning on progress and warning verbosity
if test "x$bc_build_verbose" = "xyes" ; then
	if test "x$bc_build_warnings_set" = "xno" ; then
		bc_build_warnings=yes
	fi
	if test "x$bc_build_progress_set" = "xno" ; then
		bc_build_progress=yes
	fi
fi

###
# argument summary printing
###

AC_MSG_CHECKING(whether to only build the express library)
AC_MSG_RESULT($bc_only_express)

AC_MSG_CHECKING(whether to use run-time debug checks)
AC_MSG_RESULT($bc_build_runtime_debug)

AC_MSG_CHECKING(whether to compile in 64-bit mode)
AC_MSG_RESULT($bc_build_64bit)

AC_MSG_CHECKING(whether to enable optimized compilation)
AC_MSG_RESULT($bc_use_optimized)

AC_MSG_CHECKING(whether to disable debug mode compilation)
AC_MSG_RESULT($bc_use_debug)

AC_MSG_CHECKING(whether to enable profile mode compilation)
AC_MSG_RESULT($bc_use_profiling)

AC_MSG_CHECKING(whether to enable verbose output)
AC_MSG_RESULT($bc_build_verbose)

AC_MSG_CHECKING(whether to enable verbose compilation warnings)
AC_MSG_RESULT($bc_build_warnings)

AC_MSG_CHECKING(whether to enable verbose compilation progress)
AC_MSG_RESULT($bc_build_progress)


dnl **************************
dnl *** Check for programs ***
dnl **************************

BC_CONFIGURE_STAGE([programs], [2 of 9])

AC_LANG(C)

dnl added in autoconf 2.54 to define _GNU_SOURCE
dnl AC_GNU_SOURCE

AC_AIX
AC_MINIX

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_CXX

AC_PROG_CPP
AC_REQUIRE_CPP

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_MKDIR_P

# automake 'requires' AM_PROG_LEX, not AC_PROG_LEX
AM_PROG_LEX
AC_PROG_YACC

# libtool's configuration check has a bug that causes a /lib/cpp
# sanity check failure if a C++ compiler is not installed.  This makes
# the sanity test pass regardless of whether there is a c++ compiler.
if test "x$CXXCPP" = "x" ; then
    if test "x$CPP" = "x" ; then
	CXXCPP="cpp"
    else
	CXXCPP="$CPP"
    fi
fi

dnl disable unnecessary libtool test for fortran
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl

# libtool shouldn't be generated until after LD is set
# XXX went poof in libtool 1.9 -- AC_PROG_LIBTOOL
# LT_INIT
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

dnl verbose compilation progress
if test "x$bc_build_progress" != "xyes" ; then
    LIBTOOLFLAGS=--silent
    AC_SUBST(LIBTOOLFLAGS)
    # LIBTOOLFLAGS isn't often not enough;
    # tack --silent onto the LIBTOOL command.
    if test "x$LIBTOOL" != "x" ; then
	LIBTOOL="$LIBTOOL --silent"
    fi
fi

# XXX tests for ranlib may be required.  "ar ts" was used in cake for
# sgi and aix so more compensation may be required still.

dnl Libtool may need AR so try to find it
AC_PATH_PROG(AR, ar, [], $PATH:/usr/bin:/usr/local/bin:/usr/ccs/bin)
AC_SUBST(AR)

dnl check for basic shell facilities
AC_PATH_PROG(SH, sh)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(RM, rm)

dnl figure out which compiler is seems like we are using
dnl detected values include: gcc, mipspro, sunpro
dnl none of the tests should be compiler-specific, but it's good to
dnl know in the log which it seems we are using
compiler=unknown

dnl Determine whether we are using GCC
if test "x$GCC" = "xyes" ; then
    compiler=gnu
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
	compiler=sgi
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
	compiler=sun
    fi
fi

AC_FUNC_ALLOCA


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

BC_CONFIGURE_STAGE([libraries], [3 of 9])

dnl check whether we ended up with functioning posix threading.
AC_MSG_CHECKING([whether we are using POSIX threading])
posix_threading=no
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTHREAD"
AC_TRY_LINK([#include <pthread.h>], [
    pthread_t th;
    pthread_join(th, 0);
    pthread_attr_init(0);
    pthread_cleanup_push(0,0);
    pthread_create(0,0,0,0);
    pthread_cleanup_pop(0);
], [posix_threading=yes])
LIBS="$PRELIBS"
AC_MSG_RESULT($posix_threading)

dnl find a curses or other terminal library
LIBTERMLIB=""
AC_CHECK_LIB(termlib, tputs, LIBTERMLIB="-ltermlib",
    [AC_CHECK_LIB(ncurses, tputs, LIBTERMLIB="-lncurses",
	[AC_CHECK_LIB(curses, tputs, LIBTERMLIB="-lcurses",
	    [AC_CHECK_LIB(termcap, tputs, LIBTERMLIB="-ltermcap",
		[AC_CHECK_LIB(terminfo, tputs, LIBTERMLIB="-lterminfo",
		    [AC_CHECK_LIB(tinfo, tputs, LIBTERMLIB="-ltinfo",
			[AC_CHECK_LIB(ocurses, tputs, LIBTERMLIB="-locurses")]
		    )]
		)]
	    )]
	)]
    )]
)

dnl check if a math library links
m_link_works=no
LIBM=""
AC_CHECK_LIB(m, cos, m_link_works=yes ; LIBM="-lm")

dnl detect a useable system regular expression library
regex_link_works=no
LIBREGEX=""
AC_CHECK_LIB(c, regcomp, regex_link_works=yes ; LIBREGEX="-lc",
    [AC_CHECK_LIB(regex, regcomp, regex_link_works=yes ; LIBREGEX="-lregex",
	[AC_CHECK_LIB(compat, regcomp, regex_link_works=yes ; LIBREGEX="-lcompat")]
    )]
)

dnl check if a socket library links
socket_link_works=no
LIBSOCKET=""
AC_CHECK_LIB(socket, socket, socket_link_works=yes ; LIBSOCKET="-lsocket")

dnl check if a network socket library links
nsl_link_works=no
LIBNSL=""
AC_CHECK_LIB(nsl, gethostbyaddr, nsl_link_works=yes ; LIBNSL="-lnsl")

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

dnl check if the SGI generic SCSI library links
ds_link_works=no
LIBDS=""
AC_CHECK_LIB(ds, dsopen, ds_link_works=yes ; LIBDS="-lds")

dnl check if the Solaris lexer library links
l_link_works=no
LIBL=""
AC_CHECK_LIB(l, yyless, l_link_works=yes ; LIBL="-ll")

dnl check for the IRIX libgen library (for basename/dirname)
gen_link_works=no
LIGEN=""
AC_CHECK_LIB(gen, basename, gen_link_works=yes ; LIBGEN="-lgen")

dnl check if the standard c++ library links without checking for any particular symbol
dnl library is needed when linking c++ code against c code (e.g. openNURBS)
stdcxx_link_works=no
LIBSTDCXX=""
AC_CHECK_LIB(stdc++, main, stdcxx_link_works=yes ; LIBSTDCXX="-lstdc++",
	[PRELIBS="$LIBS"
	 LIBS="$LIBS $LIBM"
	 AC_CHECK_LIB(stdc++, main, stdcxx_link_works=yes ; LIBSTDCXX="-lstdc++ $LIBM"
	 LIBS="$PRELIBS"
	 )]
)


dnl *************************
dnl *** Check for headers ***
dnl *************************

BC_CONFIGURE_STAGE([headers], [4 of 9])

# C89 headers: assert.h, ctype.h, errno.h, float.h, limits.h, locale.h,
# math.h, setjmp.h, signal.h, stdarg.h, stddef.h, stdio.h, stdlib.h,
# string.h, time.h

# C95 headers: wchar.h, wctype.h, iso646.h

# C99 headers: complex.h, fenv.h, inttypes.h, stdbool.h, stdint.h,
# tgmath.h

# POSIX.1 headers includes C89, C95, and C99 plus the following:
# aio.h, arpa/inet.h, cpio.h, dirent.h, dlfcn.h, fcntl.h, fmtmsg.h,
# fnmatch.h, ftw.h, glob.h, grp.h, iconv.h, langinfo.h, libgen.h,
# monetary.h, mqueue.h, ndbm.h, net/if.h, netdb.h, netinet/in.h,
# netinet/tcp.h, nl_types.h, poll.h, pthread.h, pwd.h, regex.h,
# sched.h, search.h, semaphore.h, spawn.h, strings.h, stropts.h,
# sys/ipc.h, sys/mman.h, sys/msg.h, sys/resource.h, sys/select.h,
# sys/stat.h, sys/statvfs.h, sys/time.h, sys/timeb.h, sys/times.h,
# sys/types.h, sys/uio.h, sys/un.h, sys/utsname.h, sys/wait.h,
# syslog.h, tar.h, termios.h, trace.h, ucontext.h, ulimit.h, unistd.h,
# utime.h, utmpx.h, wordexp.h

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS( \
	inttypes.h \
	io.h \
	memory.h \
	poll.h \
	pthread.h \
	pwd.h \
	rle.h \
	sched.h \
	sgtty.h \
	stdint.h \
	sys/file.h \
	sys/_ioctl.h \
	sys/ioctl.h \
	sys/ioctl_compat.h \
	sys/machd.h \
	sys/mman.h \
	sys/param.h \
	sys/prctl.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/sysctl.h \
	sys/sysinfo.h \
	sys/sysmp.h \
	sys/time.h \
	sys/times.h \
	sys/types.h \
	sys/un.h \
	sys/wait.h \
	unistd.h \
)

AC_CHECK_HEADER(sys/sched.h,
		AC_DEFINE(HAVE_SYS_SCHED_H, 1, Define to 1 if you have the <sys/sched.h> header file.),,
		[
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#include <sys/sched.h>
])

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
#if HAVE_NCURSES_H
#  include <ncurses.h>
#else
#  ifdef HAVE_CURSES_H
#    include <curses.h>
#  else
#    ifdef HAVE_TERMCAP_H
#      include <termcap.h>
#    else
#      ifdef HAVE_TERMINFO_H
#        include <terminfo.h>
#      else
#        ifdef HAVE_TINFO_H
#          include <tinfo.h>
#        endif
#      endif
#    endif
#  endif
#endif
#include <term.h>
])

dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

BC_CONFIGURE_STAGE([types], [5 of 9])

dnl added in autoconf 2.55
dnl AC_HEADER_STDBOOL

AC_C_CHAR_UNSIGNED
AC_HEADER_STAT
AC_HEADER_TIME
AC_STRUCT_ST_BLOCKS
AC_STRUCT_ST_BLKSIZE
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_STRUCT_TM
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_TYPE(socklen_t, ,
    AC_DEFINE([socklen_t], [int], [Define to int if <sys/socket.h> nor <sys/types.h define socklen_t]),
    [
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
])


dnl ******************************************
dnl *** Check for compiler characteristics ***
dnl ******************************************

BC_CONFIGURE_STAGE([compiler], [6 of 9])

if test "xyes" = "xyes" ; then

    dnl try to use -pipe to speed up the compiles
    BC_COMPILER_AND_LINKER_RECOGNIZES([-pipe])


    dnl check for -fno-strict-aliasing
    dnl XXX - THIS FLAG IS REQUIRED if any level of optimization is
    dnl enabled with GCC as we do use aliasing and type-punning.
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fno-strict-aliasing])

    dnl check for -fno-common (libtcl needs it on darwin)
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fno-common])

    dnl check for -fexceptions
    dnl this is needed to resolve __Unwind_Resume when compiling and
    dnl linking against openNURBS in librt for some binaries, for
    dnl example rttherm (i.e. any -static binaries)
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fexceptions], [fexceptions])
    if test "x$bc_fexceptions_works" = "xyes" ; then
       FEXCEPTIONS="-fexceptions"
       AC_SUBST(FEXCEPTIONS)
    fi

    dnl dynamic SSE optimizations for NURBS processing
dnl
dnl XXX disable the SSE flags for now as they can cause illegal instructions.
dnl     the test needs to also be tied to run-time functionality since gcc
dnl     may still output SSE instructions (e.g., for cross-compiling).
dnl
dnl     BC_COMPILER_AND_LINKER_RECOGNIZES([-msse], [msse], [nokeep])
dnl     MSSE=""
dnl     if test "x$bc_msse_works" = "xyes" ; then
dnl        MSSE="-msse"
dnl     fi
dnl     MSSE2=""
dnl     BC_COMPILER_AND_LINKER_RECOGNIZES([-msse2], [msse2], [nokeep])
dnl     if test "x$bc_msse2_works" = "xyes" ; then
dnl        MSSE2="-msse2"
dnl     fi
dnl     SSE="$MSSE $MSSE2"
dnl     AC_SUBST(SSE)

    dnl check for -search_paths_first linker flag.
    dnl prevent a false-positive where it can be treated as a -s and-e
    dnl linker option by adding a benign flag that should succeed.
    dnl this flag allows dylibs and archives to be found based on a
    dnl library path search order, not biasing a preference for dylibs.
    BC_LINKER_RECOGNIZES([-Wl,-search_paths_first -Wnewline-eof])

    dnl 64bit compilation flags
    if test "x$bc_build_64bit" = "xyes" ; then
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
		BC_COMPILER_AND_LINKER_RECOGNIZES([-64], [64_flag])
		if test "x$bc_64_flag_works" = "xyes" ; then
			found_64bit_flag=yes
		fi
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-mabi=64], [64_flag])
		if test "x$bc_64_flag_works" = "xyes" ; then
			found_64bit_flag=yes
		fi
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-m64], [64_flag])
		if test "x$bc_64_flag_works" = "xyes" ; then
			found_64bit_flag=yes
		fi
	fi

	if test "x$found_64bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-q64], [64_flag])
		if test "x$bc_64_flag_works" = "xyes" ; then
			found_64bit_flag=yes
		fi
	fi
    fi

# end check for automatic flags
fi

dnl profile flags
if test "x$bc_use_profiling" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-pg], [profile_flag])
	if test "x$bc_profile_flag_works" = "xno" ; then
		# intel gprof compiler flag
		BC_COMPILER_AND_LINKER_RECOGNIZES([-p], [profile_flag])
	fi

	if test "x$bc_profile_flag_works" = "xno" ; then
		# no profiling
		if test "x$bc_use_profiling" = "xyes" ; then
			dnl profiling was requested, so abort
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Profiling was enabled but could not find a profile flag])
			AC_MSG_ERROR([*** Don't know how to profile with this compiler ***])
		fi
		AC_MSG_WARN([Don't know how to profile with this compiler])
		sleep 1
	else
		# profiling works
		if test "x$bc_use_profiling" = "xauto" ; then
			# convert 'auto' to 'no' even though it works
			bc_use_profiling=no
		fi
	fi
fi

dnl debug flags
if test "x$bc_use_debug" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-g], [debug_flag])
	if test "x$bc_debug_flag_works" = "xno" ; then
		# no debug
		if test "x$bc_use_debug" = "xyes" ; then
			dnl debug was requested, so abort
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Debug was enabled but -g does not work])
			AC_MSG_ERROR([*** Don't know how to debug with this compiler ***])
		fi
		AC_MSG_WARN([Do not know how to debug with this compiler])
		sleep 1
	else
		# debug works
		if test "x$bc_use_debug" = "xauto" ; then
			# convert 'auto' to 'yes'
			bc_use_debug="yes"
		fi
	fi
fi

dnl optimization flags
if test "x$bc_use_optimized" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-O3], [o3_flag])
	if test "x$bc_o3_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to compile optimized with this compiler])
		sleep 1
	fi


# -fast provokes a stack corruption in the shadow computations because
# of strict aliasing getting enabled.  we _require_
# -fno-strict-aliasing until someone changes how lists are managed.
#
#	BC_COMPILER_AND_LINKER_RECOGNIZES([-fast], [mac_opt_flag])
#	if test "x$bc_mac_opt_flag_works" = "xno" ; then
#		BC_COMPILER_AND_LINKER_RECOGNIZES([-fast -mcpu=7450], [mac_opt_flag])
#	fi
	if test "xdisabled" = "xyes" ; then
		extra_optimization_flag="-ffast-math -fstrength-reduce -fexpensive-optimizations -finline-functions"
		if test "x$bc_use_debug" = "xno" ; then
			dnl non-debug can omit the frame pointer, debug cannot
			extra_optimization_flag="$extra_optimization_flag -fomit-frame-pointer"
		else
			extra_optimization_flag="$extra_optimization_flag -fno-omit-frame-pointer"
		fi
		BC_COMPILER_AND_LINKER_RECOGNIZES([$extra_optimization_flag])
	fi
fi

dnl verbose warning flags (if yes or auto)
if test "x$bc_build_warnings" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-W -Wall -Wundef -Wfloat-equal -Wshadow -Wunreachable-code -Winline], [warning])
	# XXX also of interest
	# -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -pedantic -ansi -Werror

	if test "x$bc_warning_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to output verbose warnings with this compiler])
		sleep 1
	fi
fi

# determine the minimum single-precision floating point tolerance
# value at compile time such that: 1.0 + value != 1.0
# ANSI defines this as FLT_EPSILON but float.h may not provide it.
BC_FLOAT_EPSILON

# determine the minimum double-precision floating point tolerance
# value at compile time such that: 1.0 + value != 1.0
# ANSI defines this as DBL_EPSILON but float.h may not provide it.
BC_DOUBLE_EPSILON

# determine whether the single-precision floating point implementation
# seems IEEE 754 compliant.
BC_COMPLIANT_FLOAT

# determine whether the double-precision floating point implementation
# seems IEEE 754 compliant.
BC_COMPLIANT_DOUBLE

# warn if either floating point compliance failed
if test "x$bc_compliant_float" = "xno" || test "x$bc_compliant_double" = "xno" ; then
    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
    AC_MSG_WARN([The floating point implementation does not seem to be IEEE 754])
    AC_MSG_WARN([compliant.  The behavior of htond and htonf may be incorrect.])
    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
fi

dnl AC_C_PROTOTYPES
AM_C_PROTOTYPES

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_C_BIGENDIAN

# figure out what size pointers the compiler is actually generating
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(void *, 4)
pointer_size="$ac_cv_sizeof_void_p"

dnl Make sure that we can actually compile C code
BC_SANITY_CHECK([$CC compiler and flags for sanity])

dnl Make sure that we can actually compile C++ code
AC_LANG_PUSH([C++])
BC_SANITY_CHECK([$CXX compiler and flags for sanity])
AC_LANG_POP


dnl ***************************
dnl *** Check for functions ***
dnl ***************************

BC_CONFIGURE_STAGE([functions], [7 of 9])

AC_TYPE_SIGNAL

# specifically not checked for and not used:
#   access - security concerns

# specifically not checked for but used including any well-behaved
# C89/posix functions.  this includes:
#   stat

# specifically checked for even though C89 (provided by libsysv):
#   memset, strchr, strtok

# some C89 functions: assert, isalnum, isalpha, iscntrl, isdigit,
# isgraph, islower, isprint, ispunct, isspace, isupper, isxdigit,
# tolower, toupper, localeconv, setlocale, acos, asin, atan, atan2,
# ceil, cos, cosh, exp, fabs, floor, fmod, frexp, ldexp, log, log10,
# modf, pow, sin, sinh, sqrt, tan, longjmp, setjmp, raise, signal,
# va_arg, va_end, va_start, offsetof, clearerr, fclose, feof, ferror,
# fflush, fgetc, fgetpos, fgets, fopen, fprintf, fputc, fputs, fread,
# freopen, fscanf, fseek, fsetpos, fwrite, getc, getchar, gets,
# perror, printf, putc, putchar, puts, remove, rename, rewind, scanf,
# setbuf, setvbuf, sprintf, sscanf, tmpfile, tmpnam, ungetc, vprintf,
# vsprintf, abort, abs, atexit, atof, atoi, atol, bsearch, calloc,
# div, exit, free, ftell, getenv, labs, ldiv, malloc, mblen, mbtowcs,
# mbtowc, qsort, rand, realloc, sizeof, srand, strtod, strtol,
# strtoul, system, wctomb, wcstombs, memchr, memcmp, memcpy, memmove,
# memset, strcat, strchr, strcmp, strcoll, strcpy, strcspn, strerror,
# strlen, strncat, strncmp, strncpy, strpbrk, strrchr, strspn, strstr,
# strtok, strxfrm, asctime, clock, ctime, difftime, gmtime, localtime,
# mktime, strftime, time

AC_CHECK_FUNCS(\
	alarm \
	dlopen \
	drand48 \
	fchmod \
	fsync \
	geteuid \
	getegid \
	gethostname \
	getopt_long \
	getprogname \
	kill \
	lrand48 \
	memcpy \
	memmove \
	memset \
	mkstemp \
	pipe \
	random \
	sbrk \
	setenv \
	setpgid \
	setpriority \
	setprogname \
	srand48 \
	strchr \
	strdup \
	strlcat \
	strlcpy \
	strsep \
	strtok \
	sync \
	sysctl \
	sysmp \
	vfork \
	writev \
)
# strcasecmp checked for BLT

dnl *** check if basename() exists ***
AC_MSG_CHECKING([for basename])
PRELIBS="$LIBS"
LIBS="$LIBS $LIBGEN"
AC_TRY_RUN([
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif
int main(int argc, char *argv[]) {
  (void)basename(argv[0]);
  return 0;
}],
[ AC_MSG_RESULT(yes)
  AC_DEFINE([HAVE_BASENAME], [1], [Define to 1 if you have the `basename' function.])
],
[ AC_MSG_RESULT(no) ])
LIBS="$PRELIBS"

dnl *** check if dirname() exists ***
AC_MSG_CHECKING([for dirname])
PRELIBS="$LIBS"
LIBS="$LIBS $LIBGEN"
AC_TRY_RUN([
#ifdef HAVE_LIBGEN_H
#  include <libgen.h>
#endif
int main(int argc, char *argv[]) {
  (void)dirname(argv[0]);
  return 0;
}],
[ AC_MSG_RESULT(yes)
  AC_DEFINE([HAVE_DIRNAME], [1], [Define to 1 if you have the `dirname' function.])
],
[ AC_MSG_RESULT(no) ])
LIBS="$PRELIBS"


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

BC_CONFIGURE_STAGE([services], [8 of 9])

dnl *** check for bison/yacc or flex/lex ***
# scanner and parser are optionally needed for mged and other tools
# that utilize a parser.  if both are found, provide a make var so we
# can conditionally compile the parsers.
build_parsers=no
if test "x$ac_cv_prog_LEX" != "x" ; then
    if test "x$ac_cv_prog_YACC" != "x" ; then
	# we have both
	build_parsers=yes
    else
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([A YACC-style parser was not found so dependant portions])
	AC_MSG_WARN([of the package (various MGED commands) will be disabled.])
	AC_MSG_NOTICE([Installation of GNU Bison or Yacc before continuing is recommended.])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	sleep 5
    fi
else
    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
    if test "x$ac_cv_prog_YACC" != "x" ; then
	AC_MSG_WARN([A LEX-style scanner was not found so dependant portions])
	AC_MSG_WARN([of the package (various MGED commands) will be disabled.])
	AC_MSG_NOTICE([Installation of GNU Flex or Lex before continuing is recommended.])
    else
	AC_MSG_WARN([Neither a YACC-style parser or a LEX-style scanner was found.])
	AC_MSG_WARN([Dependant portions of the package will be disabled.])
	AC_MSG_NOTICE([Installation of Bison/Yacc and Flex/Lex before continuing is recommended.])
    fi
    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
    sleep 5
fi
AC_MSG_CHECKING([whether we have the essentials to make parsers])
if test "x$build_parsers" = "xyes" ; then
    AC_DEFINE(BC_WITH_PARSERS, 1, [Define to enable compilation of parsers])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


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
], [thread_works=yes])
AC_MSG_RESULT($thread_works)

dnl *** libsocket ***
AC_MSG_CHECKING(for system socket library availability)
AC_MSG_RESULT([$socket_link_works])
AC_SUBST(LIBSOCKET)

dnl *** libnsl ***
AC_MSG_CHECKING(for system network socket library availablity)
AC_MSG_RESULT([$nsl_link_works])
AC_SUBST(LIBNSL)

dnl *** libmalloc ***
AC_MSG_CHECKING(for system malloc library availability)
AC_MSG_RESULT([$malloc_link_works])
AC_SUBST(LIBMALLOC)

dnl *** libmx ***
AC_MSG_CHECKING(for system mx library availability)
AC_MSG_RESULT([$mx_link_works])
AC_SUBST(LIBMX)

dnl *** libdl ***
AC_MSG_CHECKING(for system dynamic linker library availability)
AC_MSG_RESULT([$dl_link_works])
AC_SUBST(LIBDL)

dnl *** libds ***
AC_MSG_CHECKING(for SGI /dev/scsi library availability)
AC_MSG_RESULT([$ds_link_works])
AC_SUBST(LIBDS)

dnl *** libl Solaris lexer library ***
AC_MSG_CHECKING(for Solaris lexer library availability)
AC_MSG_RESULT($l_link_works)
AC_SUBST(LIBL)

dnl *** libgen IRIX dirname/basename library ***
AC_MSG_CHECKING(for libgen library availability)
AC_MSG_RESULT($gen_link_works)
AC_SUBST(LIBGEN)

dnl *** libstdc++ C++ library ***
AC_MSG_CHECKING(whether the Standard C++ library is available)
AC_MSG_RESULT($stdcxx_link_works)
AC_SUBST(LIBSTDCXX)

dnl *** 64-bit compilation ***
dnl figure out whether we are building 64-bit
build_scl_64bit="no"
if test "x$bc_build_64bit" = "xyes" ; then
    if test "x$pointer_size" != "x8" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_NOTICE([Try removing --enable-64bit-build or modifying the CFLAGS/LDFLAGS])
	AC_MSG_ERROR([*** Building 64-bit was requested, yet the build seems to be non-64-bit ***])
    fi
    build_scl_64bit=yes
else
    if test "x$bc_build_64bit" = "xno" ; then
	if test "x$pointer_size" = "x8" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try removing --disable-64bit-build or modifying the CFLAGS/LDFLAGS])
	    AC_MSG_ERROR([*** Building non-64-bit was requested, yet the build seems to be 64-bit ***])
	fi
    fi

    dnl automatic detection
    if test "x$pointer_size" = "x8" ; then
	build_scl_64bit="yes"
    elif test "x$pointer_size" = "x4" ; then
	build_scl_64bit="no (32-bit)"
    elif test "x$pointer_size" = "x2" ; then
	build_scl_64bit="no (16-bit)"
    elif test "x$pointer_size" = "x1" ; then
	build_scl_64bit="no (8-bit)"
    else
	AC_MSG_WARN([Unknown pointer size: $pointer_size])
	sleep 5
    fi
fi

dnl search the SCL include directory (required for non-srcdir builds).
dnl should come after the system services checks otherwise or our headers
dnl may conflict.
CPPFLAGS="$CPPFLAGS -DSCLBUILD=1 -I\${top_srcdir}/include"

#-I\${top_srcdir}/include/stepcore -I\${top_srcdir}/include/steputils -I\${top_srcdir}/include/stepdai"

# make sure ECHO and ECHO_N got defined and substituted
if test "x$ECHO" = "x" ; then
    ECHO=echo
    AC_MSG_NOTICE([ECHO was not defined by configure so defining manually])
fi
AC_SUBST(ECHO)
AC_SUBST(ECHO_N)


dnl **************************************
dnl *** Configure Makefiles and output ***
dnl **************************************

BC_CONFIGURE_STAGE([output], [9 of 9])

# remove surrounding whitspace
CFLAGS="`echo $CFLAGS`"
CXXFLAGS="`echo $CXXFLAGS`"
CPPFLAGS="`echo $CPPFLAGS`"
LDFLAGS="`echo $LDFLAGS`"
LIBS="`echo $LIBS`"

AM_CONDITIONAL(ONLY_EXPRESS, [test "x$bc_only_express" != "xno"])


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

if test "x$bc_build_runtime_debug" = "xno" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([While disabling run-time debugging should increase])
	AC_MSG_WARN([performance, it will likewise remove several])
	AC_MSG_WARN([data-protection safeguards that are in place to])
	AC_MSG_WARN([minimize the possibility of corrupted data files])
	AC_MSG_WARN([in the inevitable event of a user encountering a bug.])
	AC_MSG_WARN([You have been warned.  Proceed at your own risk.])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	sleep 5
	AC_DEFINE(NO_BOMBING_MACROS, 1, [Define to not do anything for macros that only bomb on a fatal error])
	AC_DEFINE(NO_MAGIC_CHECKING, 1, [Define to not perform magic number checking])
	AC_DEFINE(NO_BADRAY_CHECKING, 1, [Define to not check for divide by zero during ray shots])
	AC_DEFINE(NO_DEBUG_CHECKING, 1, [Define to not provide run-time debug facilities via rt_g.debug])
fi

dnl compile-time debug
if test "x$bc_use_debug" != "xno" ; then
	AC_DEFINE(DEBUG, 1, [Define to enable compile-time debug code])
else
	AC_DEFINE(NDEBUG, 1, [Define to indicate non-debug code (assert utilizes)])
fi

#
# set up the SCL libraries
#

EXPRESS='${top_builddir}/src/express/libexpress.la'
EXPPP='${top_builddir}/src/exppp/libexppp.la'
CLUTILS='${top_builddir}/src/clutils/libsteputils.la'
CLDAI='${top_builddir}/src/cldai/libstepdai.la'
CLEDITOR='${top_builddir}/src/cleditor/libstepeditor.la'
CLSTEPCORE='${top_builddir}/src/clstepcore/libstepcore.la'

AC_SUBST(EXPRESS)
AC_SUBST(EXPPP)
AC_SUBST(CLUTILS)
AC_SUBST(CLDAI)
AC_SUBST(CLEDITOR)
AC_SUBST(CLSTEPCORE)

# if compiling with the MIPSpro 7.3 compiler, there is a bug when
# trying to link using an rpath longer than 256 that causes the linker
# to core dump.
AM_CONDITIONAL(LINK_STATIC_REQUIRED, [test "x$build_vendor" = "xsgi" -a "x$compiler" = "xsgi" ])

dnl sample applications
SAMPLE_APPLICATIONS_DIR='${SCL_DATA}/sample_applications'
AC_SUBST(SAMPLE_APPLICATIONS_DIR)

# stash the top builddir before configuring subdirectories
bc_top_builddir="$ac_top_builddir"

# stash the current configuration args before adding additional ones
# for subconfigure
bc_configure_args="$ac_configure_args"

AC_CONFIG_FILES([
	Makefile
	m4/Makefile
	misc/Makefile
	include/Makefile
	include/conf/Makefile
	include/express/Makefile
	include/exppp/Makefile
	doc/Makefile
	doc/man/Makefile
	doc/man/man1/Makefile
	data/Makefile
	data/ap203/Makefile
	data/ap227/Makefile
	data/example/Makefile
	src/Makefile
	src/express/Makefile
	src/exppp/Makefile
	src/fedex_plus/Makefile
	src/clstepcore/Makefile
	src/cleditor/Makefile
	src/cldai/Makefile
	src/clutils/Makefile
])

AC_OUTPUT

# ac_top_builddir and other variables are modified after AC_OUTPUT so
# plan accordingly and save them beforehand.
ac_configure_args="$bc_configure_args"

# patch libtool if it has one of several common bugs and/or busted
# default configurations (e.g. Debian)
BC_PATCH_LIBTOOL


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
datadir=`eval "echo $datadir"`
datadir=`eval "echo $datadir"`

dnl
dnl Compute configuration time elapsed
dnl
if test -x "${srcdir}/sh/elapsed.sh" ; then
	time_elapsed="`${srcdir}/sh/elapsed.sh $CONFIG_TS`"
else
	time_elapsed="unknown"
fi


dnl **********************
dnl *** Report Summary ***
dnl **********************

AC_MSG_RESULT([Done.])
AC_MSG_RESULT([])
BC_BOLD
AC_MSG_RESULT([STEP Class Libraries Release SCL_VERSION, Build $CONFIG_DATE])
BC_UNBOLD
AC_MSG_RESULT([])
AC_MSG_RESULT([             Prefix: ${bc_prefix}])
AC_MSG_RESULT([           Binaries: ${bindir}])
AC_MSG_RESULT([       Manual pages: ${mandir}])
AC_MSG_RESULT([Configuration files: ${sysconfdir}])
AC_MSG_RESULT([Data resource files: ${bc_data_dir}])
if test ! "x$BC_ARGS" = "x" ; then
AC_MSG_RESULT([Options & variables: $BC_ARGS])
fi
AC_MSG_RESULT([])
AC_MSG_RESULT([CC       = ${CC}])
AC_MSG_RESULT([CXX      = ${CXX}])
if test ! "x$CFLAGS" = "x" ; then
AC_MSG_RESULT([CFLAGS   = ${CFLAGS}])
fi
if test ! "x$CXXFLAGS" = "x" ; then
AC_MSG_RESULT([CXXFLAGS = ${CXXFLAGS}])
fi
if test ! "x$CPPFLAGS" = "x" ; then
AC_MSG_RESULT([CPPFLAGS = ${CPPFLAGS}])
fi
if test ! "x$LDFLAGS" = "x" ; then
AC_MSG_RESULT([LDFLAGS  = ${LDFLAGS}])
fi
if test ! "x$LIBS" = "x" ; then
AC_MSG_RESULT([LIBS     = ${LIBS}])
fi
AC_MSG_RESULT([])
AC_MSG_RESULT([Enable run-time debugging (optional)..: $bc_build_runtime_debug])
AC_MSG_RESULT([])
AC_MSG_RESULT([Build 64-bit release .................: $build_scl_64bit])
AC_MSG_RESULT([Build optimized release ..............: $bc_use_optimized])
AC_MSG_RESULT([Build debug release ..................: $bc_use_debug])
AC_MSG_RESULT([Build profile release ................: $bc_use_profiling])
AC_MSG_RESULT([Print verbose compilation warnings ...: $bc_build_warnings])
AC_MSG_RESULT([Print verbose compilation progress ...: $bc_build_progress])
AC_MSG_RESULT([])
AC_MSG_RESULT([Only build libexpress.................: $bc_only_express])
AC_MSG_RESULT([])
if test "x$time_elapsed" != "xunknown" ; then
AC_MSG_RESULT([Elapsed configuration time ...........: $time_elapsed])
fi
AC_MSG_RESULT([---])
AC_MSG_RESULT([$0 complete, type 'make' to begin building])
AC_MSG_RESULT([])

# Local Variables:
# tab-width: 8
# mode: autoconf
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
