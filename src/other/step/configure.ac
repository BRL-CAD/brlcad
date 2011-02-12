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

define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
define([MAJOR_VERSION], [3])
define([MINOR_VERSION], [2])
define([PATCH_VERSION], [0])
define([SCL_VERSION], [patsubst([MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION], [
])])

AC_INIT(STEP Class Libraries, [SCL_VERSION], [http://brlcad.org], scl)
AC_REVISION($Revision: 33887 $)

AC_CONFIG_AUX_DIR(misc)
AC_CONFIG_MACRO_DIR([m4])

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

# set up the SCL_ROOT installation path, sets bc_prefix
BC_SCL_ROOT

# set up the SCL_DATA install directory, sets bc_data_dir
#   e.g. /usr/local/share/scl/3.2.0
BC_SCL_DATA

# init the venerable automake only _once_ or incur the wrath of
# several automake bugs (like "aclocal-" and install detection)
AM_INIT_AUTOMAKE([1.6 dist-zip dist-bzip2])

# write out all of our definitions to this header
AM_CONFIG_HEADER([include/scl_cf.h])

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


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

BC_CONFIGURE_STAGE([libraries], [3 of 9])

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

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS( \
	ndir.h \
	stdarg.h \
	sys/stat.h \
	sysent.h \
	unistd.h \
)


dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

BC_CONFIGURE_STAGE([types], [5 of 9])

AC_C_CHAR_UNSIGNED
AC_TYPE_SIZE_T


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
		BC_COMPILER_AND_LINKER_RECOGNIZES([-arch x86_64], [64_flag])
		if test "x$bc_64_flag_works" = "xyes" ; then
			found_64bit_flag=yes
		fi
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
BC_COMPILER_AND_LINKER_RECOGNIZES([-w], [nowarn], [nokeep])
if test "x$bc_nowarn_works" = "xyes" ; then
	NOWARN="-w"
else
	NOWARN=""
fi
AC_SUBST(NOWARN)
if test "x$bc_build_warnings" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-W -Wall -Wundef -Wfloat-equal -Wshadow -Wunreachable-code -Winline], [warning])
	# XXX also of interest
	# -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -pedantic -ansi -Werror

	if test "x$bc_warning_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to output verbose warnings with this compiler])
		sleep 1
	fi
else
	BC_COMPILER_AND_LINKER_RECOGNIZES([-w], [nowarn])
fi

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

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
	abs \
	memcpy \
	memmove \
)


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

BC_CONFIGURE_STAGE([services], [8 of 9])

dnl *** libstdc++ C++ library ***
AC_MSG_CHECKING(whether the Standard C++ library is available)
AC_MSG_RESULT($stdcxx_link_works)
AC_SUBST(LIBSTDCXX)

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
