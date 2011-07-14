dnl                    C O N F I G U R E . A C
dnl BRL-CAD
dnl
dnl Copyright (c) 2004-2011 United States Government as represented by
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
dnl NOTE: BRL-CAD as a collective work is distributed under the LGPL.
dnl       BRL-CAD's build system is under the BSD license.
dnl       See the COPYING file for more details.
dnl
dnl $Id$
dnl
dnl ******************************************************************
dnl ***                  BRL-CAD's configure.ac                    ***
dnl ******************************************************************
dnl
dnl Herein lies the venerable GNU Build System configure template for
dnl BRL-CAD.  As best is reasonably possible, proper ordering and
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

# minimum version of autoconf required.  should coincide with setting
# in autogen.sh script.
AC_PREREQ(2.52)

# See HACKING for details on how to properly update the version
define([MAJOR_VERSION], [patsubst(esyscmd([cat include/conf/MAJOR]), [
])])
define([MINOR_VERSION], [patsubst(esyscmd([cat include/conf/MINOR]), [
])])
define([PATCH_VERSION], [patsubst(esyscmd([cat include/conf/PATCH]), [
])])
define([CAD_VERSION], [patsubst([MAJOR_VERSION.MINOR_VERSION.PATCH_VERSION], [
])])

AC_INIT(BRL-CAD, [CAD_VERSION], [http://brlcad.org], brlcad)
AC_REVISION($Revision$)

AC_CONFIG_AUX_DIR([misc])
AC_CONFIG_MACRO_DIR([m4])

# keep track of cmd-line options for later
BC_ARG0="$0"
AC_SUBST(BC_ARG0)
if test "x$ac_configure_args" = "x" ; then
    BC_ARGS="$*"
else
    BC_ARGS="$ac_configure_args"
fi
AC_SUBST(BC_ARGS)

# just in case
PACKAGE_NAME="BRL-CAD"
AC_SUBST(PACKAGE_NAME)

BRLCAD_VERSION=CAD_VERSION
AC_SUBST(BRLCAD_VERSION)

# force lang/locale setting to C so things like date output as expected
LANG=C
LC_ALL=C

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
title="Configuring BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE"
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
AC_CONFIG_SRCDIR([src/librt/bool.c])

# where are we coming from and where are we going to.
# prefix is set to NONE until AC_OUTPUT unless --prefix is used
AC_PREFIX_DEFAULT([/usr/brlcad])

# set up the BRLCAD_ROOT installation path, sets bc_prefix
BC_BRLCAD_ROOT

# set up the BRLCAD_DATA install directory, sets bc_data_dir
#   e.g. /usr/local/share/brlcad/7.4.0
# Note that BRLCAD_DATA is expected to be a full path - relative
# paths will not work
BC_BRLCAD_DATA

# if this is not a checkout, disable dependency tracking for a faster
# build. it's not likely that the user is doing development from a
# source release. at least they really should not given the extra
# steps that are necessary for making a proper useful diff.
AC_MSG_CHECKING([whether dependency tracking should be enabled])
if test -d $srcdir/CVS -o -d $srcdir/.svn ; then
	if test "x$enable_maintainer_mode" = "x" ; then
		enable_maintainer_mode="yes"
	fi
	if test "x$enable_dependency_tracking" = "x" ; then
		enable_dependency_tracking="yes"
	fi
	AC_MSG_RESULT([yes])
else
	if test "x$enable_maintainer_mode" = "x" ; then
		enable_maintainer_mode="no"
	fi
	if test "x$enable_dependency_tracking" = "x" ; then
		enable_dependency_tracking="no"
	fi
	AC_MSG_RESULT([no])
fi

# init the venerable automake only _once_ or incur the wrath of
# several automake bugs (like "aclocal-" and install detection)
AM_INIT_AUTOMAKE([1.6 dist-zip dist-bzip2])

# disables the build system dependency tracking by default for the
# automake and autoconf template files.
AM_MAINTAINER_MODE

# write out all of our definitions to this header
AM_CONFIG_HEADER([include/brlcad_config.h])

# load the tcl SC_* macros
builtin(include,src/other/tcl/unix/tcl.m4)

# automatically enable and load our configure cache file if available
BC_CONFIG_CACHE([config.cache.${host_os}.${ac_hostname}], [yes])


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

#
# DOCUMENT NEW ARGUMENTS AND ALIASES IN THE "INSTALL" FILE.
#

BC_CONFIGURE_STAGE([arguments], [1 of 9])

dnl *** enable options ***

# enable everything
BC_ARG_ENABLE([everything], [almost-everything], [Turn on compilation of almost everything], [auto])

# only build benchmark
BC_ARG_ENABLE([only_benchmark], [only-benchmark], [Only build the components necessary for the benchmark], [no])

# only build librtserver
BC_ARG_ENABLE([only_rts], [only-rtserver], [Only build the components necessary for the raytrace server], [no])

# run-time debugging
BC_ARG_ENABLE([build_runtime_debug], [runtime-debug], [Enable run-time debug checking], [yes])

# strict compilation flags
BC_ARG_ENABLE([build_strict], [strict-build], [Enable strict compiler settings], [yes])

# enable 64-bit builds
BC_ARG_ENABLE([build_64bit], [64bit-build], [Enable 64-bit compilation mode], [auto])

# force enable/disable building of libregex
BC_ARG_ENABLE([build_regex], [regex-build], [Build the included regular expression library], [auto])

# force enable/disable building of libpng
BC_ARG_ENABLE([build_png], [png-build], [Build the included Portable Network Graphics library], [auto])

# force enable/disable building of zlib
BC_ARG_ENABLE([build_zlib], [zlib-build], [Build the included zlib compression library], [auto])

# force enable/disable building of utah raster toolkit
BC_ARG_ENABLE([build_urt], [urt-build], [Build the included Utah Raster Toolkit], [auto])

# force enable/disable building of openNURBS
BC_ARG_ENABLE([build_opennurbs], [opennurbs-build], [Build the included openNURBS library], [auto])

# force enable/disable building of NIST STEP Class Libraries
BC_ARG_ENABLE([build_step], [step-build], [Build the included STEP Class Libraries], [auto])

# force enable/disable building of termlib
BC_ARG_ENABLE([build_termlib], [termlib-build], [Build the included termlib library], [auto])

# force enable/disable building of Tcl
BC_ARG_ENABLE([build_tcl], [tcl-build], [Build the included Tcl], [auto])

# force enable/disable building of Tk
BC_ARG_ENABLE([build_tk], [tk-build], [Build the included Tk], [auto])

# force enable/disable building Tk using AquaTk (Mac OS X specific)
BC_ARG_ENABLE([build_aquatk], [aquatk-build], [Use Aqua windowing system on Mac OS X], [no])

# force enable/disable building of incrTcl
BC_ARG_ENABLE([build_itcl], [itcl-build], [Build the included incrTcl], [auto])

# force enable/disable building of iwidgets
BC_ARG_ENABLE([build_iwidgets], [iwidgets-install], [Install the included iwidgets], [auto])

# tkhtml
BC_ARG_ENABLE([build_tkhtml], [tkhtml-build], [Build the included tkhtml], [yes])

# tkpng
BC_ARG_ENABLE([build_tkpng], [tkpng-build], [Build the included tkpng], [auto])

# tktable
BC_ARG_ENABLE([build_tktable], [tktable-build], [Build the included tktable], [yes])

# force enable/disable building of tnt
BC_ARG_ENABLE([build_tnt], [tnt-install], [Install the included Template Numerical Toolkit], [auto])

# jove
BC_ARG_ENABLE([build_jove], [jove-build], [Enable compilation of jove (optional)], [no])

# endgame framework simulation module
BC_ARG_ENABLE([build_ef], [ef-build], [Enable compilation of Endgame Framework module (optional)], [no])

# unigraphics plugin
BC_ARG_ENABLE([build_ug], [unigraphics-build], [Enable compilation of Unigraphics importer (optional)], [no])

# example geometry
BC_ARG_ENABLE([install_geometry], [models-install], [Enable installation of example geometry models], [yes])

# enable building Docbook documentation
BC_ARG_ENABLE([build_docs], [documentation], [Enable compilation and installation of extra documentation], [auto])

# optimized
BC_ARG_ENABLE([use_optimized], [optimized], [Enable optimized compilation], [no])

# debug
BC_ARG_ENABLE([use_debug], [debug], [Enable debug symbols], [auto])

# profiling
BC_ARG_ENABLE([use_profiling], [profiling], [Enable profiling], [no])

# parallel
BC_ARG_ENABLE([build_parallel], [parallel], [Compile for SMP architectures], [yes])

# Sun dtrace
BC_ARG_ENABLE([use_dtrace], [dtrace], [Enable dtrace support], [no])

# verbosity
BC_ARG_ENABLE([build_verbose], [verbose], [Enable verbose compilation], [auto])

# warnings
BC_ARG_ENABLE([build_warnings], [warnings], [Enable verbose compilation warnings], [yes])

# build progress status
BC_ARG_ENABLE([build_progress], [progress], [Enable verbose compilation progress], [no])

# build libdm/mged's rtgl interface.  this is presently the only dm
# interface directly toggled instead of passively via --with options.
BC_ARG_ENABLE([rtgl], [rtgl], [Enable compilation of experimental Ray Trace OpenGL display manager], [no])


dnl *** with options ***

# provides convenience argument handlers for specifying CFLAGS,
# LDFLAGS, CPPFLAGS, and LIBS using --with argument handlers.
BC_WITH_FLAG_ARGS

# use the jdk
BC_ARG_WITH([with_java], [jdk], [Specify use of or path to the JDK for librtserver (optional)], [auto])

# use x11
BC_ARG_WITH([with_x11], [x11], [Specify use of or path to X11 (optional)], [auto])

# use opengl
BC_ARG_WITH([with_opengl], [ogl], [Specify use of or path to OpenGL (optional)], [no])

# use Apple opengl
BC_ARG_WITH([with_agl], [agl], [Specify use of or path to Apple OpenGL (optional)], [no])

# use wgl
BC_ARG_WITH([with_wgl], [wgl], [Specify use of or path to WindowsGL], [auto])

# use pro/engineer
BC_ARG_WITH([with_proe], [proe], [Specify build of Pro/ENGINEER plugin or path to Pro/ENGINEER installation (optional)], [no])

###
# argument aliases
# they need to go below here in order for --help to consolidate the
# blank line that it inserts to exactly one line in the right place.
#
# DOCUMENT NEW ARGUMENTS AND ALIASES IN THE "INSTALL" FILE.
###

# aliases for enabling configuration of everything (help uses almost-everything)
BC_ARG_ALIAS([everything], [all])
BC_ARG_ALIAS([everything], [all-build])
BC_ARG_ALIAS([everything], [build-all])
BC_ARG_ALIAS([everything], [all-builds])
BC_ARG_ALIAS([everything], [everything])
BC_ARG_ALIAS([everything], [everything-build])
BC_ARG_ALIAS([everything], [build-everything])

# aliases for building only the benchmark (help uses only-benchmark)
BC_ARG_ALIAS([only_benchmark], [only-bench])
BC_ARG_ALIAS([only_benchmark], [only-benchmarks])
BC_ARG_ALIAS([only_benchmark], [bench-only])
BC_ARG_ALIAS([only_benchmark], [benchmark-only])
BC_ARG_ALIAS([only_benchmark], [benchmarks-only])

# aliases for building only the raytrace server (help uses only-rtserver)
BC_ARG_ALIAS([only_rts], [only-rts])
BC_ARG_ALIAS([only_rts], [only-librtserver])
BC_ARG_ALIAS([only_rts], [rts-only])
BC_ARG_ALIAS([only_rts], [rtserver-only])
BC_ARG_ALIAS([only_rts], [librtserver-only])

# run-time debugging (help uses runtime-debug)
BC_ARG_ALIAS([build_runtime_debug], [run-time-debug])
BC_ARG_ALIAS([build_runtime_debug], [runtime-debugging])
BC_ARG_ALIAS([build_runtime_debug], [run-time-debugging])

# run-time debugging (help uses runtime-debug)
BC_ARG_ALIAS([build_strict], [strict])
BC_ARG_ALIAS([build_strict], [strict-compile])
BC_ARG_ALIAS([build_strict], [strict-compile-flags])

# Docbook documentation (help uses documentation)
BC_ARG_ALIAS([build_docs], [documentation-install])
BC_ARG_ALIAS([build_docs], [docs])
BC_ARG_ALIAS([build_docs], [docs-install])

# 64-bit compilation (help uses 64bit-build)
BC_ARG_ALIAS([build_64bit], [64bit])
BC_ARG_ALIAS([build_64bit], [64])
BC_ARG_ALIAS([build_64bit], [64-build])
BC_ARG_ALIAS([build_64bit], [64-bit])
BC_ARG_ALIAS([build_64bit], [64-bit-build])

# force building libregex (help uses regex-build)
BC_ARG_ALIAS([build_regex], [regex])
BC_ARG_ALIAS([build_regex], [libregex])
BC_ARG_ALIAS([build_regex], [libregex-build])

# aliases to force building libpng (help uses png-build)
BC_ARG_ALIAS([build_png], [png])
BC_ARG_ALIAS([build_png], [libpng])
BC_ARG_ALIAS([build_png], [libpng-build])

# aliases to force building zlib (help uses zlib-build)
BC_ARG_ALIAS([build_zlib], [zlib])
BC_ARG_ALIAS([build_zlib], [libz])
BC_ARG_ALIAS([build_zlib], [libz-build])

# aliases to force building urt (help uses urt-build)
BC_ARG_ALIAS([build_urt], [urt])
BC_ARG_ALIAS([build_urt], [urtoolkit])
BC_ARG_ALIAS([build_urt], [urtoolkit-build])
BC_ARG_ALIAS([build_urt], [utahrle])
BC_ARG_ALIAS([build_urt], [utahrle-build])
BC_ARG_ALIAS([build_urt], [libutahrle])
BC_ARG_ALIAS([build_urt], [libutahrle-build])
BC_ARG_ALIAS([build_urt], [utah-raster-toolkit])
BC_ARG_ALIAS([build_urt], [utah-raster-toolkit-build])

# aliases to force building openNURBS (help uses opennurbs-build)
BC_ARG_ALIAS([build_opennurbs], [opennurbs])
BC_ARG_ALIAS([build_opennurbs], [libopennurbs])
BC_ARG_ALIAS([build_opennurbs], [libopennurbs-build])
BC_ARG_ALIAS([build_opennurbs], [open-nurbs])
BC_ARG_ALIAS([build_opennurbs], [open-nurbs-build])

# aliases to force building NIST STEP Class Libraries (help uses step-build)
BC_ARG_ALIAS([build_step], [step])
BC_ARG_ALIAS([build_step], [steplibs])
BC_ARG_ALIAS([build_step], [libstep-build])
BC_ARG_ALIAS([build_step], [libstep])

# aliases to force building tcl (help uses tcl-build)
BC_ARG_ALIAS([build_tcl], [tcl])
BC_ARG_ALIAS([build_tcl], [libtcl])
BC_ARG_ALIAS([build_tcl], [libtcl-build])

# aliases to force building tk (help uses tk-build)
BC_ARG_ALIAS([build_tk], [tk])
BC_ARG_ALIAS([build_tk], [libtk])
BC_ARG_ALIAS([build_tk], [libtk-build])

# aliases to force building tkaqua (help uses aquatk-build)
BC_ARG_ALIAS([build_aquatk], [aquatk])
BC_ARG_ALIAS([build_aquatk], [aqua-tk])
BC_ARG_ALIAS([build_aquatk], [aqua-tk-build])
BC_ARG_ALIAS([build_aquatk], [tkaqua])
BC_ARG_ALIAS([build_aquatk], [tkaqua-build])
BC_ARG_ALIAS([build_aquatk], [tk-aqua-build])

# aliases to force building itcl (help uses itcl-build)
BC_ARG_ALIAS([build_itcl], [itcl])
BC_ARG_ALIAS([build_itcl], [itk])
BC_ARG_ALIAS([build_itcl], [itk-build])
BC_ARG_ALIAS([build_itcl], [libitcl])
BC_ARG_ALIAS([build_itcl], [libitcl-build])
BC_ARG_ALIAS([build_itcl], [libitk])
BC_ARG_ALIAS([build_itcl], [libitk-build])
BC_ARG_ALIAS([build_itcl], [incrtcl])
BC_ARG_ALIAS([build_itcl], [incrtcl-build])

# aliases to force building iwidgets (help uses iwidgets-install)
BC_ARG_ALIAS([build_iwidgets], [iwidgets])
BC_ARG_ALIAS([build_iwidgets], [iwidgets-build])

# aliases to force building tkhtml (help uses tkhtml-build)
BC_ARG_ALIAS([build_tkhtml], [tkhtml])

# aliases to force building tktable (help uses tktable-build)
BC_ARG_ALIAS([build_tktable], [tktable])

# aliases to force building tkpng(help uses tkpng-build)
BC_ARG_ALIAS([build_tkpng], [tkpng])
BC_ARG_ALIAS([build_tkpng], [libtkpng])
BC_ARG_ALIAS([build_tkpng], [libtkpng-build])

# aliases to force building termlib (help uses termlib-build)
BC_ARG_ALIAS([build_termlib], [termlib])
BC_ARG_ALIAS([build_termlib], [termcap])
BC_ARG_ALIAS([build_termlib], [termcap-build])
BC_ARG_ALIAS([build_termlib], [libtermlib])
BC_ARG_ALIAS([build_termlib], [libtermlib-build])
BC_ARG_ALIAS([build_termlib], [libtermcap])
BC_ARG_ALIAS([build_termlib], [libtermcap-build])

# aliases to force building tnt (help uses tnt-install)
BC_ARG_ALIAS([build_tnt], [tnt])
BC_ARG_ALIAS([build_tnt], [tnt-build])
BC_ARG_ALIAS([build_tnt], [template-numerical-toolkit])
BC_ARG_ALIAS([build_tnt], [template-numerical-toolkit-build])
BC_ARG_ALIAS([build_tnt], [template-numerical-toolkit-install])

# jove aliases (help uses jove-build)
BC_ARG_ALIAS([build_jove], [jove])

# endgame framework module aliases (help uses ef-build)
BC_ARG_ALIAS([build_ef], [ef])
BC_ARG_ALIAS([build_ef], [endgameframework])
BC_ARG_ALIAS([build_ef], [endgameframework-build])
BC_ARG_ALIAS([build_ef], [endgame-framework])
BC_ARG_ALIAS([build_ef], [endgame-framework-build])

# unigraphics (NX) importer aliases (help uses unigraphics-build)
BC_ARG_ALIAS([build_ug], [unigraphics])
BC_ARG_ALIAS([build_ug], [ug])
BC_ARG_ALIAS([build_ug], [ug-build])
BC_ARG_ALIAS([build_ug], [nx])
BC_ARG_ALIAS([build_ug], [nx-build])

# install geometry aliases (help uses models-install)
BC_ARG_ALIAS([install_geometry], [models])
BC_ARG_ALIAS([install_geometry], [models-build])
BC_ARG_ALIAS([install_geometry], [geometry])
BC_ARG_ALIAS([install_geometry], [geometry-build])
BC_ARG_ALIAS([install_geometry], [geometry-install])

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

# parallel aliases (help uses parallel)
BC_ARG_ALIAS([build_parallel], [parallel-build])
BC_ARG_ALIAS([build_parallel], [smp])
BC_ARG_ALIAS([build_parallel], [smp-build])

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

# with java aliases (help uses jdk)
BC_ARG_WITH_ALIAS([with_java], [java])

# with proe aliases
BC_ARG_WITH_ALIAS([with_proe], [proe])

# with x11 aliases (help uses x11)
BC_ARG_WITH_ALIAS([with_x11], [x])

# with opengl aliases (help uses ogl)
BC_ARG_WITH_ALIAS([with_opengl], [opengl])

# with agl aliases (help uses agl)
BC_ARG_WITH_ALIAS([with_agl], [agl])

# with wgl aliases (help uses wgl)
BC_ARG_WITH_ALIAS([with_wgl], [windowsgl])


###
# set up path searching
###

dnl automatically scan /usr/local (e.g. BSD uses /usr/local for ports)
BC_SEARCH_DIRECTORY([/usr/local])


###
# argument sanity checks and meta-argument settings
# here go checks to warn or abort when conflicting options specified
###

# handle enabling/disabling all the packages and codes that ship with
# the package.  if something is changed from a default, leave it alone
# so that the user may specify everything and then disable specific
# ones.  specifically only test yes/no as default 'auto' is to leave
# the options alone.
set_everything=""
if test "x$bc_everything" = "xyes" ; then
	set_everything=yes
elif test "x$bc_everything" = "xno" ; then
	set_everything=no
fi
if test "x$set_everything" != "x" ; then
	if test "x$bc_build_runtime_debug_set" = "xno" ; then
		bc_build_runtime_debug="$set_everything"
	fi
	if test "x$bc_build_strict_compile" = "xno" ; then
		bc_build_strict="$set_everything"
	fi
	if test "x$bc_build_regex_set" = "xno" ; then
		bc_build_regex="$set_everything"
	fi
	if test "x$bc_build_png_set" = "xno" ; then
		bc_build_png="$set_everything"
	fi
	if test "x$bc_build_zlib_set" = "xno" ; then
		bc_build_zlib="$set_everything"
	fi
	if test "x$bc_build_urt_set" = "xno" ; then
		bc_build_urt="$set_everything"
	fi
	if test "x$bc_build_opennurbs_set" = "xno" ; then
		bc_build_opennurbs="$set_everything"
	fi
	if test "x$bc_build_step" = "xno" ; then
		bc_build_step="$set_everything"
	fi
	if test "x$bc_build_termlib_set" = "xno" ; then
		bc_build_termlib="$set_everything"
	fi
	if test "x$bc_build_tcl_set" = "xno" ; then
		bc_build_tcl="$set_everything"
	fi
	if test "x$bc_build_tk_set" = "xno" ; then
		bc_build_tk="$set_everything"
	fi
	if test "x$bc_build_itcl_set" = "xno" ; then
		bc_build_itcl="$set_everything"
	fi
	if test "x$bc_build_iwidgets_set" = "xno" ; then
		bc_build_iwidgets="$set_everything"
	fi
	if test "x$bc_build_tkhtml" = "xno" ; then
		bc_build_tkhtml="$set_everything"
	fi
	if test "x$bc_build_tkpng_set" = "xno" ; then
		bc_build_tkpng="$set_everything"
	fi
	if test "x$bc_build_tktable" = "xno" ; then
		bc_build_tktable="$set_everything"
	fi
	if test "x$bc_build_tnt_set" = "xno" ; then
		bc_build_tnt="$set_everything"
	fi
	if test "x$bc_build_jove_set" = "xno" ; then
		bc_build_jove="$set_everything"
	fi
	# Singe the Endgame Framework "simulation module" will only
	# compile if you have it installed, don't enable it even if
	# requesting everything.  fine to disable, though.
	if test "x$bc_build_ef_set" = "xno" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_ef=no
		fi
	fi
	# Since the Unigraphics importer is primarily tested on only one
	# platform and requires UGOPEN, don't enable it even if
	# requesting everything.  fine to disable, though.
	if test "x$bc_build_ug_set" = "xno" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_ug=no
		fi
	fi
fi


# make sure multiple "only" options were not given
if test "x$bc_only_benchmark" != "xno" ; then
	if test "x$bc_only_rts" != "xno" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Requesting BOTH benchmark and librtserver "only"?])
		AC_MSG_WARN([Building both benchmark and librtserver])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		sleep 1
	fi
	if test "x$bc_everything" = "xyes" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Requesting ONLY benchmark and EVERYTHING?])
		AC_MSG_WARN([Building everything])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		bc_only_benchmark=no
		sleep 1
	fi
fi

# the opengl interface is presently tied to the x11 interface
if test "x$bc_with_opengl" = "xyes" ; then
	if test "x$bc_with_x11" = "xno" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([OpenGL interface presently requires X11 support])
		AC_MSG_WARN([Presuming X11 availability for the OpenGL interface])
		AC_MSG_WARN([The X11 interface will not be built])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		sleep 1
	fi
fi

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

AC_MSG_CHECKING(whether to compile everything was requested)
AC_MSG_RESULT($bc_everything)

AC_MSG_CHECKING(whether to only build benchmark)
AC_MSG_RESULT($bc_only_benchmark)

AC_MSG_CHECKING(whether to only build the raytrace server)
AC_MSG_RESULT($bc_only_rts)

AC_MSG_CHECKING(whether to use run-time debug checks)
AC_MSG_RESULT($bc_build_runtime_debug)

AC_MSG_CHECKING(whether to use strict compiler options)
AC_MSG_RESULT($bc_build_strict)

AC_MSG_CHECKING(whether to compile and install Docbook documentation)
AC_MSG_RESULT($bc_build_docs)

AC_MSG_CHECKING(whether to compile in 64-bit mode)
AC_MSG_RESULT($bc_build_64bit)

AC_MSG_CHECKING(whether to build the regular expression library)
AC_MSG_RESULT($bc_build_regex)

AC_MSG_CHECKING(whether to build the PNG library)
AC_MSG_RESULT($bc_build_png)

AC_MSG_CHECKING(whether to build the zlib compression library)
AC_MSG_RESULT($bc_build_zlib)

AC_MSG_CHECKING(whether to build the Utah Raster Toolkit)
AC_MSG_RESULT($bc_build_urt)

AC_MSG_CHECKING(whether to build the openNURBS library)
AC_MSG_RESULT($bc_build_opennurbs)

AC_MSG_CHECKING(whether to build the NIST Step Class libraries)
AC_MSG_RESULT($bc_build_step)

AC_MSG_CHECKING(whether to build the Tcl library)
AC_MSG_RESULT($bc_build_tcl)

AC_MSG_CHECKING(whether to build the Tk library)
AC_MSG_RESULT($bc_build_tk)

AC_MSG_CHECKING(whether to use AquaTk)
AC_MSG_RESULT($bc_build_aquatk)

AC_MSG_CHECKING(whether to build the incrTcl library)
AC_MSG_RESULT($bc_build_itcl)

AC_MSG_CHECKING(whether to install the iwidgets library)
AC_MSG_RESULT($bc_build_iwidgets)

AC_MSG_CHECKING(whether to build the tkhtml library)
AC_MSG_RESULT($bc_build_tkhtml)

AC_MSG_CHECKING(whether to build the tktable library)
AC_MSG_RESULT($bc_build_tktable)

AC_MSG_CHECKING(whether to build the tkpng library)
AC_MSG_RESULT($bc_build_tkpng)

AC_MSG_CHECKING(whether to build the termlib library)
AC_MSG_RESULT($bc_build_termlib)

AC_MSG_CHECKING(whether to build the Template Numerical Toolkit)
AC_MSG_RESULT($bc_build_tnt)

AC_MSG_CHECKING(whether to compile jove or not)
AC_MSG_RESULT($bc_build_jove)

AC_MSG_CHECKING(whether to compile the Endgame Framework module)
AC_MSG_RESULT($bc_build_ef)

AC_MSG_CHECKING(whether to compile the Unigraphics importer)
AC_MSG_RESULT($bc_build_ug)

AC_MSG_CHECKING(whether to install example geometry databases)
AC_MSG_RESULT($bc_install_geometry)

AC_MSG_CHECKING(whether to enable optimized compilation)
AC_MSG_RESULT($bc_use_optimized)

AC_MSG_CHECKING(whether to disable debug mode compilation)
AC_MSG_RESULT($bc_use_debug)

AC_MSG_CHECKING(whether to enable profile mode compilation)
AC_MSG_RESULT($bc_use_profiling)

AC_MSG_CHECKING(whether to enable support for SMP architectures)
AC_MSG_RESULT($bc_build_parallel)

AC_MSG_CHECKING(whether to enable verbose output)
AC_MSG_RESULT($bc_build_verbose)

AC_MSG_CHECKING(whether to enable verbose compilation warnings)
AC_MSG_RESULT($bc_build_warnings)

AC_MSG_CHECKING(whether to enable verbose compilation progress)
AC_MSG_RESULT($bc_build_progress)

AC_MSG_CHECKING(whether to compile with the Java Development Kit)
AC_MSG_RESULT($bc_with_java)

AC_MSG_CHECKING(whether to compile the X11 interfaces)
AC_MSG_RESULT($bc_with_x11)

AC_MSG_CHECKING(whether to compile the OpenGL interfaces)
AC_MSG_RESULT($bc_with_opengl)

AC_MSG_CHECKING(whether to compile the AGL interfaces)
AC_MSG_RESULT($bc_with_agl)

AC_MSG_CHECKING(whether to compile the WindowsGL interfaces)
AC_MSG_RESULT($bc_with_wgl)

AC_MSG_CHECKING(whether to compile the Pro/ENGINEER plugin)
AC_MSG_RESULT($bc_with_proe)

AC_MSG_CHECKING(whether to compile experimental RTGL display manager)
AC_MSG_RESULT($bc_rtgl)


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

# xsltproc is needed for docbook processing, fop is needed for pdf generation
xsltproc_works=no
fop_works=no
if test "x$bc_build_docs" != "xno" ; then
    AC_PATH_PROG(XSLTPROC, xsltproc)
    AC_PATH_PROG(APACHEFOP, fop)
fi

dnl Libtool may need AR so try to find it
AC_PATH_PROG(AR, ar, [], $PATH:/usr/bin:/usr/local/bin:/usr/ccs/bin)
AC_SUBST(AR)

dnl check for basic shell facilities
AC_PATH_PROG(SH, sh)
AC_PATH_PROG(MV, mv)
AC_PATH_PROG(CP, cp)
AC_PATH_PROG(RM, rm)

if test "x$bc_with_dtrace" = "xyes" ; then
    AC_PATH_PROG(DTRACE, dtrace, bc_with_dtrace=no, [$PATH:/usr/sbin])
fi

dnl figure out which compiler is seems like we are using
dnl detected values include: gcc, mipspro, sunpro
dnl none of the tests should be compiler-specific, but it's good to
dnl know in the log which it seems we are using
compiler=unknown

dnl Determine whether we are using GCC
if test "x$GCC" = "xyes" ; then
    compiler=gnu
fi

dnl Determine whether we are using the SUNWspro Sun Workshop compiler
if test "x$build_vendor" = "xsun" ; then
    AC_MSG_CHECKING([whether we are using the SUNWspro compiler])
    using_sunpro=no
    AC_LINK_IFELSE([AC_LANG_SOURCE([#if defined(sun) && defined(__SUNPRO_C) && !defined(__GNUC__)
If we get here, we are using the SUNWspro compiler (and the link should fail).
#endif
int main() { return 0; }
    ], [using_sunpro=no], [using_sunpro=yes])])
    AC_MSG_RESULT($using_sunpro)
    if test "x$using_sunpro" = "xyes" ; then
	AR="CC -xar"
	LD="CC -G"
	export AR LD
	compiler=sun
    fi
fi

AC_FUNC_ALLOCA

dnl if building jove wasn't specifically requested and wasn't
dnl disabled, see if there is either emacs or jove available.
dnl i.e. auto for jove is dependant on what is installed
BCEDITOR=""
if test "x$bc_build_jove" = "xauto" ; then
    AC_PATH_PROG(BCEDITOR, emacs, , $PATH)
    if test "x$BCEDITOR" = "x" ; then
	AC_PATH_PROG(BCEDITOR, jove, , $PATH)
    fi
fi


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

BC_CONFIGURE_STAGE([libraries], [3 of 9])

dnl check if there is a threading library that links, the order of
dnl these checks is consistent with libbu's bu_parallel()
dnl implementation preference.  this is usually in order of highest
dnl performing threading interface.
thread_link_works=no
LIBTHREAD=""
dnl try sgi sproc first
AC_CHECK_LIB(c, sproc, thread_link_works=yes ; LIBTHREAD="-lc",
    dnl try solaris threading
    [AC_CHECK_LIB(thread, thr_create, thread_link_works=yes ; LIBTHREAD="-lthread",
	dnl pthreading is a good default if none of the others are available
	dnl XXX need to test HPUX -- may require -lcma with pthreads
	[AC_CHECK_LIB(pthread, pthread_create, thread_link_works=yes ; LIBTHREAD="-lpthread",
	    dnl check if the BSD c_r threading library link works
	    [AC_CHECK_LIB(c_r, pthread_create, thread_link_works=yes ; LIBTHREAD="-pthread")]
	)]
    )]
)

dnl check for the daemon function in -lbsd and/or -lc for adrt
LIBBSD=""
AC_CHECK_LIB(bsd, daemon, LIBBSD="-lbsd",
    [AC_CHECK_LIB(c, daemon, LIBBSD="-lc")]
)
AC_SUBST(LIBBSD)

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

dnl check whether the CoreFoundation framework is available (used when
dnl linking against static tcl)
AC_MSG_CHECKING([whether -framework CoreFoundation is available])
framework_cf=no
FRAMEWORK_COREFOUNDATION=""
PRELIBS="$LIBS"
LIBS="$LIBS -Xlinker -framework -Xlinker CoreFoundation"
AC_TRY_LINK([#include <CoreFoundation/CoreFoundation.h>], [
CFBundleRef bref = CFBundleGetMainBundle();
], [framework_cf=yes ; FRAMEWORK_COREFOUNDATION="-Xlinker -framework -Xlinker CoreFoundation"])
LIBS="$PRELIBS"
AC_MSG_RESULT($framework_cf)
AC_SUBST(FRAMEWORK_COREFOUNDATION)

dnl check whether the Carbon framework is available (used for X11 focus)
AC_MSG_CHECKING([whether -framework Carbon is available])
framework_carbon=no
FRAMEWORK_CARBON=""
PRELIBS="$LIBS"
LIBS="$LIBS -Xlinker -framework -Xlinker Carbon"
AC_TRY_LINK([#include <Carbon/Carbon.h>], [
OSStatus status;
ProcessSerialNumber psn = {kNoProcess, kNoProcess};
CFStringRef processName = NULL;
GetCurrentProcess(&psn);
CopyProcessName(&psn, &processName);
if (processName != NULL) {
   CFShow(processName);
}
], [framework_carbon=yes ; FRAMEWORK_CARBON="-Xlinker -framework -Xlinker Carbon"])
LIBS="$PRELIBS"
AC_MSG_RESULT($framework_carbon)
AC_SUBST(FRAMEWORK_CARBON)

dnl check whether the JavaVM framework is available (used for librtserver)
AC_MSG_CHECKING([whether -framework JavaVM is available])
framework_javavm=no
FRAMEWORK_JAVAVM=""
PRELIBS="$LIBS"
LIBS="$LIBS -framework JavaVM"
AC_TRY_LINK([#include <JavaVM/jni.h>], [
JavaVM *jvm;
JNIEnv *env;
JavaVMInitArgs args;
jint i = JNI_GetDefaultJavaVMInitArgs(&args);
jboolean j = JNI_CreateJavaVM(&jvm, (void**)&env, &args);
if (j && jvm) {
  (*jvm)->DestroyJavaVM(jvm);
}
], [framework_javavm=yes ; FRAMEWORK_JAVAVM="-framework JavaVM"])
LIBS="$PRELIBS"
AC_MSG_RESULT($framework_javavm)
AC_SUBST(FRAMEWORK_JAVAVM)


dnl check for java
JAVA_CFLAGS="$FRAMEWORK_JAVAVM"
JAVA_LDFLAGS="-shrext .jnilib $FRAMEWORK_JAVAVM"
if test "x$bc_with_java" = "xyes" ; then
    if test "x$bc_with_java_val" = "xyes" ; then
	bc_with_java_val="/usr"
    fi
    if test "x$bc_with_java_val" != "x" ; then
	JAVA_CFLAGS="-I$bc_with_java_val/include -I$bc_with_java_val/include/`uname | tr [A-Z] [a-z]`"
	JAVA_LDFLAGS="-shrext .jnilib -L$bc_with_java_val/lib64 -L$bc_with_java_val/lib"
    fi
fi
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)


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

dnl *** X11 ***
if test "x$bc_with_x11" != "xno" ; then

    if test "x$bc_with_x11_val" != "x" ; then
        if test "x$bc_with_x11_val" != "xyes" ; then
	    if test "x$x_includes" = "xNONE"; then
		x_includes="$bc_with_x11_val/include"
		ac_x_includes="$x_includes"
	    fi
	    if test "x$x_libraries" = "xNONE"; then
		x_libraries="$bc_with_x11_val/lib"
		ac_x_libraries="$x_libraries"
	    fi
	fi
    fi

    dnl find X11 libraries.  sets X_CFLAGS, X_LIBS, and X_EXTRA_LIBS
    AC_PATH_XTRA
    AC_CHECK_HEADERS([X11/Xlib.h])

    dnl add X11 libraries that link to X_LIBS
    LIBX11=""
    LIBXEXT=""
    LIBXI=""

    PRELIBS="$LIBS"
    PRECPPFLAGS="$CPPFLAGS"

    CPPFLAGS="$CPPFLAGS $X_CFLAGS"

    LIBS="$LIBS $X_LIBS"
    AC_CHECK_LIB(X11, XCreateWindow, LIBX11="-lX11")

    LIBS="$LIBS $LIBX11"
    AC_CHECK_LIB(Xext, XShapeCombineMask, LIBXEXT="-lXext")

    LIBS="$LIBS $LIBXEXT"
    AC_CHECK_LIB(Xi, XGetExtensionVersion, LIBXI="-lXi")
    X_LIBS="$LIBS $LIBXI"

    # common for systems to not have Xi X11 input library .. warn
    # verbosely about that being a problem since the build will
    # probably fail in src/bwish with undefined references to
    # XFreeDeviceList, XOpenDevice, XSelectExtensionEvent, and
    # XListInputDevices.
    if test "x$LIBXI" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([X11 support is enabled but the Xi library was not found.])
	AC_MSG_WARN([This will likely result in a build failure.])
	AC_MSG_WARN([See config.log for details why (look for this comment)])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	sleep 5
    fi

    CPPFLAGS="$PRECPPFLAGS"
    LIBS="$PRELIBS"
fi

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

dnl check if zlib library links
z_link_works=no
LIBZLIB=""
if test "x$bc_build_zlib" != "xyes" ; then
    AC_CHECK_LIB(z, deflate, z_link_works=yes ; LIBZLIB="-lz")
fi

dnl check if a png library links
png_link_works=no
LIBPNG=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBZLIB"
AC_CHECK_LIB(png, png_access_version_number, png_link_works=yes ; LIBPNG="-lpng $LIBZLIB")
LIBS="$PRELIBS"

dnl check if utahrle library links
utahrle_link_works=no
UTAHRLE=""
if test "x$bc_build_urt" != "xyes" ; then
    # checking for rle_hdr_init is NOT sufficient to get urt 3.x
    PRELIBS="$LIBS"
    LIBS="$LIBS $LIBM"
    AC_CHECK_LIB(utahrle, rle_getraw, utahrle_link_works=yes ; UTAHRLE="-lutahrle $LIBM",
	[AC_CHECK_LIB(rle, rle_getraw, utahrle_link_works=yes ; UTAHRLE="-lrle $LIBM")]
    )
    LIBS="$PRELIBS"
fi

dnl check if openNURBS library links
opennurbs_link_works=no
LIBOPENNURBS=""
if test "x$bc_build_opennurbs" != "xyes" ; then
    AC_CHECK_LIB(openNURBS, onmalloc, opennurbs_link_works=yes ; LIBOPENNURBS="-lopenNURBS")
fi

dnl check if libtcl library links
tcl_link_works=no
LIBTCL=""
AC_CHECK_LIB(tcl8.6, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.6",
    [AC_CHECK_LIB(tcl86, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl86",
	[AC_CHECK_LIB(tcl8.5, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.5",
	    [AC_CHECK_LIB(tcl85, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl85",
		[AC_CHECK_LIB(tcl, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl")]
	    )]
	)]
    )]
)

dnl check if libtclstub library links
tclstub_link_works=no
LIBTCLSTUB=""
AC_CHECK_LIB(tclstub8.6, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub8.6",
    [AC_CHECK_LIB(tclstub86, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub86",
	[AC_CHECK_LIB(tclstub8.5, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub8.5",
	    [AC_CHECK_LIB(tclstub85, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub85",
		[AC_CHECK_LIB(tclstub, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub")]
	    )]
	)]
    )]
)

dnl check if libtk library links
tk_link_works=no
LIBTK=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTCLSTUB"
AC_CHECK_LIB(tk8.6, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk8.6",
    [AC_CHECK_LIB(tk86, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk86",
	[AC_CHECK_LIB(tk8.5, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk8.5",
	    [AC_CHECK_LIB(tk85, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk85",
		[AC_CHECK_LIB(tk, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk")]
	    )]
	)]
    )]
)
LIBS="$PRELIBS"

dnl check if libtkstub library links
tkstub_link_works=no
LIBTKSTUB=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTCLSTUB"
AC_CHECK_LIB(tkstub8.6, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub8.6",
    [AC_CHECK_LIB(tkstub86, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub86",
	[AC_CHECK_LIB(tkstub8.5, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub8.5",
	    [AC_CHECK_LIB(tkstub85, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub85",
		[AC_CHECK_LIB(tkstub, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub")]
	    )]
	)]
    )]
)
LIBS="$PRELIBS"

dnl check if libitcl library links
itcl_link_works=no
LIBITCL=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTCLSTUB"
AC_CHECK_LIB(itcl3.4, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.4",
    [AC_CHECK_LIB(itcl34, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl34",
	[AC_CHECK_LIB(itcl3.3, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.3",
	    [AC_CHECK_LIB(itcl33, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl33",
		[AC_CHECK_LIB(itcl3.2, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.2",
		    [AC_CHECK_LIB(itcl32, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl32",
			[AC_CHECK_LIB(itcl, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl")]
		    )]
		)]
	    )]
	)]
    )]
)
LIBS="$PRELIBS"

dnl check if libitk library links
itk_link_works=no
LIBITK=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTKSTUB $LIBTCLSTUB"
AC_CHECK_LIB(itk3.4, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.4",
    [AC_CHECK_LIB(itk34, Itk_Init, itk_link_works=yes ; LIBITK="-litk34",
	[AC_CHECK_LIB(itk3.3, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.3",
	    [AC_CHECK_LIB(itk33, Itk_Init, itk_link_works=yes ; LIBITK="-litk33",
		[AC_CHECK_LIB(itk3.2, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.2",
		    [AC_CHECK_LIB(itk32, Itk_Init, itk_link_works=yes ; LIBITK="-litk32",
			[AC_CHECK_LIB(itk, Itk_Init, itk_link_works=yes ; LIBITK="-litk")]
		    )]
		)]
	    )]
	)]
    )]
)
LIBS="$PRELIBS"

dnl check if a socket library links
socket_link_works=no
LIBSOCKET=""
AC_CHECK_LIB(socket, socket, socket_link_works=yes ; LIBSOCKET="-lsocket")

dnl check if a network socket library links
nsl_link_works=no
LIBNSL=""
AC_CHECK_LIB(nsl, gethostbyaddr, nsl_link_works=yes ; LIBNSL="-lnsl")

dnl check if a network library links
newtork_link_works=no
LIBNETWORK=""
AC_CHECK_LIB(network, socket, network_link_works=yes ; LIBNETWORK="-lnetwork")

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

dnl check if an OpenGL graphics library links
gl_link_works=no
LIBGL=""
if test "x$bc_with_opengl" != "xno" ; then
    AC_CHECK_LIB(GL, glEnable, gl_link_works=yes ; LIBGL="$LIBGL -lGL",
	 [AC_CHECK_LIB(GL2, glEnable, gl_link_works=yes ; LIBGL="$LIBGL -lGL2")]
    )
    if test "x$gl_link_works" = "xno" ; then
	PRELDFLAGS="$LDFLAGS"
	PRELIBS="$LIBS"
	for bc_dir in "$ac_x_libraries" `echo "$ac_x_includes $ac_x_header_dirs" |
		sed s/include/lib/g` ; do

	    GL_LDFLAGS="-L$bc_dir"
	    LDFLAGS="$PRELDFLAGS $GL_LDFLAGS"
	    LIBS="$PRELIBS -lGL"
	    AC_LINK_IFELSE([AC_LANG_CALL([],[glEnable])],
		gl_link_works=yes ; LIBGL="-L$bc_dir -lGL" ; break)
	    LIBS="$PRELIBS -lGL2"
	    AC_LINK_IFELSE([AC_LANG_CALL([],[glEnable])],
		gl_link_works=yes ; LIBGL="-L$bc_dir -lGL2" ; break)
	done
	LDFLAGS="$PRELDFLAGS"
	LIBS="$PRELIBS"
    fi

    # Mac OS X 10.5 has a linker bug resulting in the following error:
    #   ld: cycle in dylib re-exports with /usr/X11/lib/libGL.dylib
    #
    # This fixes that error by forcing the GL exports specified in the
    # /usr/X11/lib dylib to use the framework dylib instead of being a
    # cyclic dependency due to search paths.  Fugly workaround.

    if test "x$gl_link_works" = "xno" ; then
	unset ac_cv_lib_GL_glEnable # disable cache result
	PRELDFLAGS="$LDFLAGS"
	GL_LDFLAGS="-Wl,-dylib_file -Wl,/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib:/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/libGL.dylib"
	LDFLAGS="$PRELDFLAGS $GL_LDFLAGS"
	LIBGL=-lGL
	AC_CHECK_LIB(GL, glEnable, gl_link_works=yes)
	# leave LDFLAGS set with this gl workaround
	# LDFLAGS="$PRELDFLAGS"
	if test "x$gl_link_works" = "xno" ; then
	    GL_LDFLAGS=""
	    LIBGL=""
	fi
    fi
fi

dnl check if the Apple OpenGL framework is available
FRAMEWORK_OPENGL=""
if test "xbc_with_agl" != "xno" ; then
    AC_MSG_CHECKING([whether -framework OpenGL is available])
    framework_opengl=no
    PRELIBS="$LIBS"
    LIBS="$LIBS -framework OpenGL"
    AC_TRY_LINK([#include <OpenGL/OpenGL.h>], [
    GLboolean values;
    void (*func)() = glBegin;
    if (GL_FALSE) {
        func(GL_POINTS);
        glGetBooleanv(GL_DEPTH_TEST, &values);
        glEnd();
    }
    ], [framework_opengl=yes ; FRAMEWORK_OPENGL="-framework OpenGL"])
    LIBS="$PRELIBS"
    AC_MSG_RESULT($framework_opengl)
fi
AC_SUBST(FRAMEWORK_OPENGL)

dnl check if the Windows WGL graphics library links
wgl_link_works=no
LIBWGL=""
if test "xbc_with_wgl" != "xno" ; then
    AC_CHECK_LIB(opengl32, glEnable, wgl_link_works=yes ; LIBWGL="-lopengl32")
fi

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

# make our minimum compliance goal explicit (needs to be tested here
# so header files and types are tested properly)
BC_COMPILER_AND_LINKER_RECOGNIZES([-std=gnu99], [cstd], [no])
STD_C99_FLAGS=""
if test "x$bc_cstd_works" = "xyes" ; then
   STD_C99_FLAGS="-std=gnu99"
fi
AC_SUBST(STD_C99_FLAGS)

# C89 headers: assert.h, ctype.h, errno.h, float.h, limits.h, locale.h,
# math.h, setjmp.h, signal.h, stdarg.h, stddef.h, stdio.h, stdlib.h,
# string.h, time.h

# non-C89 headers that seem to be cross-platform safe: fcntl.h

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
	Carbon/Carbon.h \
	arpa/inet.h \
	curses.h \
	dslib.h \
	getopt.h \
	gl/device.h \
	grp.h \
	inttypes.h \
	io.h \
	libgen.h \
	memory.h \
	ncurses.h \
	netdb.h \
	netinet/in.h \
	poll.h \
	pthread.h \
	pwd.h \
	rle.h \
	sched.h \
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
	syslog.h \
	tcl.h \
	termcap.h \
	terminfo.h \
	termio.h \
	termios.h \
	termlib.h \
	tinfo.h \
	tk.h \
	ulocks.h \
	unistd.h \
	winsock.h \
)

# specifically for openNURBS
AC_CHECK_HEADERS( \
	ieeefp.h \
)

# these are needed to test for boost (used by BUILD_BOOST test)
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS( \
	limits \
	istream \
)
AC_LANG_POP

if test "x$bc_with_dtrace" != "xno" ; then
    AC_CHECK_HEADER(sys/sdt.h,, [bc_with_dtrace=no])
fi

GL_H_CPPFLAGS=""
GLX_H_CPPFLAGS=""
PRECPPFLAGS="$CPPFLAGS"
# optionally check for OpenGL headers
if test "x$bc_with_opengl" != "xno" -o "x$bc_with_wgl" != "xno" ; then
    gl_include_works=no
    unset ac_cv_header_GL_gl_h
    AC_CHECK_HEADER([GL/gl.h], [
	AC_DEFINE(HAVE_GL_GL_H, 1, [Define to 1 if you have the <GL/gl.h> header file.])
	gl_include_works="yes"
    ], [unset ac_cv_header_GL_gl_h])
    # didn't find header, try harder
    if test "x$gl_include_works" = "xno" ; then
	# this won't work for paths with spaces
	for bc_dir in "$ac_x_includes" `echo $ac_x_header_dirs` `echo $ac_x_libraries | sed 's/\/lib/\/include/g'`; do
	    GL_H_CPPFLAGS="-I$bc_dir"
	    CPPFLAGS="$PRECPPFLAGS $GL_H_CPPFLAGS"
	    AC_MSG_NOTICE([ .. looking in $bc_dir for gl.h])
	    AC_CHECK_HEADER([GL/gl.h], [
		AC_DEFINE(HAVE_GL_GL_H, 1, [Define to 1 if you have the <GL/gl.h> header file.])
		gl_include_works="yes"
		break
	    ], [unset ac_cv_header_GL_gl_h])
	done
    fi
    CPPFLAGS="$PRECPPFLAGS"

    glx_include_works=no
    unset ac_cv_header_GL_glx_h
    AC_CHECK_HEADER([GL/glx.h], [
	AC_DEFINE(HAVE_GL_GLX_H, 1, [Define to 1 if you have the <GL/glx.h> header file.])
	glx_include_works="yes"
    ], [unset ac_cv_header_GL_glx_h])
    # didn't find header, try harder
    if test "x$glx_include_works" = "xno" ; then
	# this won't work for paths with spaces
	for bc_dir in "$ac_x_includes" `echo $ac_x_header_dirs` `echo $ac_x_libraries | sed 's/\/lib/\/include/g'`; do
	    GLX_H_CPPFLAGS="-I$bc_dir"
	    CPPFLAGS="$PRECPPFLAGS $GLX_H_CPPFLAGS"
	    AC_MSG_NOTICE([ .. looking in $bc_dir for glx.h])
	    AC_CHECK_HEADER([GL/glx.h], [
	    	AC_DEFINE(HAVE_GL_GLX_H, 1, [Define to 1 if you have the <GL/glx.h> header file.])
		glx_include_works="yes"
		break
	    ], [unset ac_cv_header_GL_glx_h])
	done
    fi
    CPPFLAGS="$PRECPPFLAGS"
fi # bc_with_opengl

AGL_CPPFLAGS=""
if test "x$bc_with_agl" != "xno"; then
    # Apple OpenGL is a bit different - the GL_CPPFLAGS need to contain
    # the call to the OpenGL framework.
    GL_H_CPPFLAGS=
    GLX_H_CPPFLAGS=
    AGL_CPPFLAGS="-framework OpenGL"
fi

GL_CPPFLAGS="$GL_H_CPPFLAGS $GLX_H_CPPFLAGS $AGL_CPPFLAGS"
AC_SUBST(GL_CPPFLAGS)


# optionally (yes or auto) check for vectorization headers for opennurbs
if test "x$bc_build_opennurbs" != "xno" ; then
    AC_CHECK_HEADERS( \
	emmintrin.h \
    )
fi # bc_build_opennurbs

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

found_regex_h=no
AC_CHECK_HEADER(regex.h,
		AC_DEFINE(HAVE_REGEX_H,1,[Define to 1 if you have the '<regex.h>' header file])
		found_regex_h=yes )


# check for java headers
if test "x$bc_with_java " != "xno" ; then
    PRECPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $JAVA_CFLAGS"
    found_jni_h=no
    AC_CHECK_HEADER(JavaVM/jni.h,
		    AC_DEFINE(HAVE_JAVAVM_JNI_H,1,[Define to 1 if you have the '<JavaVM/jni.h>' header file])
		    found_jni_h=yes)
    AC_CHECK_HEADER(jni.h,
		    AC_DEFINE(HAVE_JNI_H,1,[Define to 1 if you have the '<jni.h>' header file])
		    found_jni_h=yes )

    search_for_jni_h_more=no
    if test "x$found_jni_h" = "xno" ; then
	if test "x$bc_only_rts" = "xyes" ; then
	    search_for_jni_h_more=yes
	fi
    fi

    if test "x$search_for_jni_h_more" = "xyes" ; then
	# darwin systems may have a prior jni.h stashed away elsewhere
	macjvmdir=/System/Library/Frameworks/JavaVM.framework/Versions
	if test -d "$macjvmdir"  ; then
	    for dir in `ls $macjvmdir` ; do
		if test -d "$macjvmdir/$dir/Headers" ; then
		    JAVA_CFLAGS="$JAVA_CFLAGS -I$macjvmdir/$dir/Headers"
		    CPPFLAGS="$CPPFLAGS -I$macjvmdir/$dir/Headers"
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
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([jni.h was found by hunting through the JavaVM framework])
	    AC_MSG_WARN([This means you probably need to install the latest Java SDK])
	    AC_MSG_WARN([Build will try to use what it found, but you should update])
	    AC_MSG_NOTICE([See http://developer.apple.com/java/ for updates])
	    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	    sleep 5
	fi
    fi # search_for_jni_h_more
    CPPFLAGS="$PRECPPFLAGS"
fi # bc_with_java

# check for proe headers
PROE_CFLAGS=""
if test "x$bc_with_proe" != "xno" ; then
   PRE_CFLAGS="$CFLAGS"
   found_protk_h=no
   if test "x$bc_with_proe" = "xyes" ; then
      if test "x$bc_with_proe_val" = "x" ; then
            found_proe_base=no
	    proe_base=/usr/local/ptc/proeWildfire3.0
	    if test -e "$proe_base" ; then
	       found_proe_base=yes
	       PROE_CFLAGS="-I$proe_base/protoolkit/includes -I$proe_base/prodevelop/includes"
	       CFLAGS="$CFLAGS $PROE_CFLAGS"
	       AC_CHECK_HEADER(ProToolkit.h,
			 AC_DEFINE(HAVE_PROTOOLKIT,1,[Define to 1 if you have Pro/TOOLKIT header files])
			 found_protk_h=yes)
	       if test "x$found_protk_h" = "xyes" ; then
		  bc_with_proe_val=$proe_base
		  PRO_ENGINEER_DIR=$bc_with_proe_val
		  AC_SUBST(PRO_ENGINEER_DIR)
	       fi
	    fi
      else
        PROE_CFLAGS="-I$bc_with_proe_val/protoolkit/includes -I$bc_with_proe_val/prodevelop/includes"
        CFLAGS="$CFLAGS $PROE_CFLAGS"
        AC_CHECK_HEADER(ProToolkit.h,
		        AC_DEFINE(HAVE_PROTOOLKIT,1,[Define to 1 if you have Pro/TOOLKIT header files])
		        found_protk_h=yes)
      fi
   fi
   CFLAGS="$PRE_CFLAGS"
fi


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
AC_CHECK_TYPE([socklen_t], [],
    AC_DEFINE([socklen_t], [int], [Define to int if <sys/socket.h> nor <sys/types.h> define socklen_t]),
    [
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
])
AC_CHECK_TYPES([sig_t], [], [], [#include <signal.h>])
# AC_TYPE_INT32_T
# AC_TYPE_INT64_T
AC_CHECK_TYPES([uint64_t])
AC_CHECK_TYPES([ssize_t])

# AC_TYPE_UINTPTR_T
AC_CHECK_TYPE([uintptr_t],
    [AC_DEFINE([HAVE_UINTPTR_T], 1, [Define to 1 if the system has the type `uintptr_t'.])],
    [for ac_type in 'unsigned int' 'unsigned long int' 'unsigned long long int'; do
	AC_COMPILE_IFELSE(
	    [AC_LANG_BOOL_COMPILE_TRY(
		[AC_INCLUDES_DEFAULT],
		[[sizeof (void *) <= sizeof ($ac_type)]])],
	    [AC_DEFINE_UNQUOTED([uintptr_t], [$ac_type],
		[Define to the type of an unsigned integer type wide enough to
		 hold a pointer, if such a type exists, and if the system
		 does not define it.])
	    ac_type=])
	test -z "$ac_type" && break
    done])

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

    dnl check for -ftemplate-depth-NN this is needed in libpc and
    dnl other code using boost where the template instantiation depth
    dnl needs to be increased from the default ANSI minimum of 17.
    BC_COMPILER_RECOGNIZES([-ftemplate-depth-50], [template_depth], [nokeep])
    if test "x$bc_template_depth_works" = "xyes" ; then
	TEMPLATE_DEPTH="-ftemplate-depth-50"
	AC_SUBST(TEMPLATE_DEPTH)
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

    dnl 32-bit/64-bit compilation flags
    if test "x$bc_build_64bit" = "xyes" ; then

	# 64-bit enabled, try to force 64-bit

	found_64bit_flag=no

	AC_MSG_CHECKING([if configure snuck on a 32bit flag to ld])
	PRELD="$LD"
	LD="`echo $LD | sed 's/32/64/'`"
	if test "x$LD" = "x$PRELD" ; then
		AC_MSG_RESULT(no)
	else
		AC_MSG_RESULT(yes)
	fi

	dnl -arch x86_64 must come before -64 or osX.5+ gets ... confused.
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

    elif test "x$bc_build_64bit" = "xno" ; then

	# 64-bit disabled, try to force 32-bit

	found_32bit_flag=no

	AC_MSG_CHECKING([if configure snuck on a 64bit flag to ld])
	PRELD="$LD"
	LD="`echo $LD | sed 's/64/32/'`"
	if test "x$LD" = "x$PRELD" ; then
		AC_MSG_RESULT(no)
	else
		AC_MSG_RESULT(yes)
	fi

	if test "x$found_32bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-arch i386], [32_flag])
		if test "x$bc_32_flag_works" = "xyes" ; then
			found_32bit_flag=yes
		fi
	fi

	if test "x$found_32bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-32], [32_flag])
		if test "x$bc_32_flag_works" = "xyes" ; then
			found_32bit_flag=yes
		fi
	fi

	if test "x$found_32bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-mabi=32], [32_flag])
		if test "x$bc_32_flag_works" = "xyes" ; then
			found_32bit_flag=yes
		fi
	fi

	if test "x$found_32bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-m32], [32_flag])
		if test "x$bc_32_flag_works" = "xyes" ; then
			found_32bit_flag=yes
		fi
	fi

	if test "x$found_32bit_flag" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-q32], [32_flag])
		if test "x$bc_32_flag_works" = "xyes" ; then
			found_32bit_flag=yes
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
		# intel profiler compiler flag
		BC_COMPILER_AND_LINKER_RECOGNIZES([-prof_gen], [profile_flag])
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

	dnl FIXME: fugly workaround as there is no known
	dnl feature-specific way to detect -ggdb3 or -gstabs+ is
	dnl preferred and libtool+Mac 10.4 will work with -ggdb3.

	AC_CHECK_PROGS(SW_VERS, sw_vers)
	if test "x$SW_VERS" != "x"; then
		AC_MSG_CHECKING(Mac OS X version for debugging)
		MACOSX_VERSION=`$SW_VERS | sed -n "s/^ProductVersion:[[[:space:]]]//p" | cut -f 1-3 -d.`
		AC_MSG_RESULT([$MACOSX_VERSION])
	fi
	case "x$MACOSX_VERSION" in
	    x10.[456])
		BC_COMPILER_AND_LINKER_RECOGNIZES([-gstabs+], [debug_flag])
		;;
	    *)
		BC_COMPILER_AND_LINKER_RECOGNIZES([-ggdb3], [debug_flag])
		;;
	esac
	if test "x$bc_debug_flag_works" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-g], [debug_flag])
	fi
	if test "x$bc_debug_flag_works" = "xno" ; then
		BC_COMPILER_AND_LINKER_RECOGNIZES([-debug], [debug_flag])
	fi

	if test "x$bc_debug_flag_works" = "xyes" ; then
		# debug works
	   	if test "x$bc_use_debug " = "xauto" ; then
		   	# convert 'auto' to 'yes'
			bc_use_debug="yes"
		fi
	else
		# no debug
		if test "x$bc_use_debug" = "xyes" ; then
			dnl debug was requested, so abort
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Debug was enabled but cannot find debug flags that work])
			AC_MSG_ERROR([*** Don't know how to debug with this compiler ***])
		fi
		AC_MSG_WARN([Do not know how to debug with this compiler])
		sleep 1
	fi

	dnl add -D_FORTIFY_SOURCE=2 to flags. provides compile-time
	dnl best-practice error checking on certain libc functions
	dnl (e.g., memcpy), and provides run-time checks on buffer
	dnl lengths and memory regions.
	BC_COMPILER_AND_LINKER_RECOGNIZES([-D_FORTIFY_SOURCE=2])
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
	BC_COMPILER_AND_LINKER_RECOGNIZES([-pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long], [warning])
	# XXX also of interest
	# -Wunreachable-code -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -ansi
	# -Wstrict-prototypes -Wold-style-definition -Wshorten-64-to-32 -Wmissing-field-initializers
	# -Wmissing-noreturn -Wredundant-decls
	# -Wformat=2 (after bu_fopen_uniq() is obsolete)
	# -Wparentheses -Wswitch-default -Wswitch-enum

	if test "x$bc_warning_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to output verbose warnings with this compiler])
		sleep 1
	fi
else
	BC_COMPILER_AND_LINKER_RECOGNIZES([-w], [nowarn])
fi

dnl we are more strict on clean dirs by default (no-long-long needed due to tcl.h)
if test "x$bc_build_strict" = "xyes" ; then
   BC_COMPILER_AND_LINKER_RECOGNIZES([-w -pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long -Werror], [strict], [no])
   if test "x$bc_strict_works" = "xyes" ; then
       # do not include -w as that was just to suppress any test warnings
       STRICT_FLAGS="-pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long  -Werror"
       AC_SUBST(STRICT_FLAGS)
       AC_DEFINE([STRICT_FLAGS], [1], [Define to 1 if strict standards compliance compilation is enabled.])
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
    sleep 5
fi

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
	GetFullPathName \
	XQueryExtension \
	alarm \
	dlopen \
	drand48 \
	fchmod \
	fsync \
	getcwd \
	getegid \
	geteuid \
	gethostname \
	getloadavg \
	getopt_long \
	getprogname \
	htonll \
	kill \
	lrand48 \
	memset \
	mkstemp \
	ntohll \
	pipe \
	popen \
	realpath \
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
[ AC_MSG_RESULT(no) ],
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
[ AC_MSG_RESULT(no) ],
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
	AC_MSG_WARN([A YACC-style parser was not found so dependant portions of the package])
	AC_MSG_WARN([(various MGED commands and STEP conversion support) will be disabled.])
	AC_MSG_NOTICE([Installation of GNU Bison or Yacc before continuing is recommended.])
	AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	sleep 5
    fi
else
    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
    if test "x$ac_cv_prog_YACC" != "x" ; then
	AC_MSG_WARN([A LEX-style scanner was not found so dependant portions of the package])
	AC_MSG_WARN([(various MGED commands and STEP conversion support) will be disabled.])
	AC_MSG_NOTICE([Installation of GNU Flex or Lex before continuing is recommended.])
    else
	AC_MSG_WARN([Neither a YACC-style parser or a LEX-style scanner was found.])
	AC_MSG_WARN([Dependant portions of the package will be disabled.])
	AC_MSG_NOTICE([Installation of Bison/Yacc and Flex/Lex before continuing is recommended.])
    fi
    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
    sleep 5
fi
#if test "x$build_parsers" = "xyes" ; then
     # test for modern features in lex
     #echo "%option reentrant bison-bridge" >> lex.l
     #echo "%%" >> lex.l
     #$LEX lex.l > /dev/null 2>&1
     #ret=$?
     #rm -f lex.l
     #if test "x${ret}" != "x0" ; then
	#build_parsers=no
	#AC_MSG_WARN([The LEX-style scanner found does not support features required by])
	#AC_MSG_WARN([BRL-CAD's LEX code.  Various MGED commands and geometry conversion])
	#AC_MSG_WARN([routines will be disabled.])
	#AC_MSG_NOTICE([Installation of a modern GNU Flex before continuing is recommended.])
     #fi
#fi

AC_MSG_CHECKING([whether we have the essentials to make parsers])
if test "x$build_parsers" = "xyes" ; then
    AC_MSG_RESULT(yes)
    AC_DEFINE(BC_WITH_PARSERS, 1, [Define to enable compilation of parsers])
else
    AC_MSG_RESULT(no)
    if test "x$bc_build_step" = "xyes" ; then
	AC_MSG_NOTICE([STEP was enabled, but no system lexer/parser was found])
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_NOTICE([Try adding --disable-step-build])
	AC_MSG_ERROR([*** Building STEP support was enabled, yet no system lexer/parser was found ***])
    fi
    bc_build_step="no"
fi

dnl *** libpng ***
png_works=no
AC_MSG_CHECKING(for portable network graphics functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBPNG"
AC_TRY_RUN([
#include <png.h>
main() {
    int i = png_access_version_number();
    return 0;
}
], [png_works=yes], [png_works=no], [png_works=no])
LIBS="$PRELIBS"
AC_MSG_RESULT($png_works)

dnl figure out whether we need to build libpng
build_brlcad_png="no (using system)"
if test "x$bc_build_png" = "xyes" ; then
    build_brlcad_png=yes
else
    if test "x$bc_build_png" = "xno" ; then
	if test "x$png_works" != "xyes" ; then
	    AC_MSG_NOTICE([libpng was disabled, but no system png library was found])
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
PNG=""
PNG_CPPFLAGS=""
if test "x$build_brlcad_png" = "xyes" ; then
    PNG='${top_builddir}/src/other/libpng/libpng14.la'
    PNG_CPPFLAGS='-I${top_srcdir}/src/other/libpng'
    AC_MSG_RESULT(yes)
else
    PNG="$LIBPNG"
    AC_MSG_RESULT(no)
fi
AC_SUBST(PNG)
AC_SUBST(PNG_CPPFLAGS)

dnl *** libregex ***
dnl See if regular expression support works
regex_works=no
AC_MSG_CHECKING(for regular expression functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBREGEX"
AC_TRY_RUN([
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include <regex.h>
int main() {
    regex_t re;
    regcomp(&re, "abc", REG_EXTENDED);
    regexec(&re, "", 0, 0, 0);
    return 0;
}
], [regex_works=yes], [regex_works=no], [regex_works=no])
LIBS="$PRELIBS"
AC_MSG_RESULT($regex_works)

dnl figure out whether we need to build libregex
build_brlcad_regex="no (using system)"
if test "x$bc_build_regex" = "xyes" ; then
    build_brlcad_regex=yes
else
    if test "x$bc_build_regex" = "xno" ; then
	if test "x$regex_works" != "xyes" ; then
	    AC_MSG_NOTICE([libregex was disabled, but no system regular expression library was found])
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
REGEX=""
REGEX_CPPFLAGS=""
if test "x$build_brlcad_regex" = "xyes" ; then
    REGEX='${top_builddir}/src/other/libregex/libregex.la'
    REGEX_CPPFLAGS='-I${top_srcdir}/src/other/libregex'
    AC_MSG_RESULT(yes)
else
    REGEX="$LIBREGEX"
    AC_MSG_RESULT(no)
fi
AC_SUBST(REGEX)
AC_SUBST(REGEX_CPPFLAGS)


dnl *** libz ***
dnl See if zlib library support works
zlib_works=no
AC_MSG_CHECKING(for zlib compression functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBZLIB"
AC_TRY_RUN([
#include <zlib.h>
int main () {
    char *zlibver = ZLIB_VERSION;
    return 0;
}
], [zlib_works=yes], [zlib_works=no], [zlib_works=no])
LIBS="$PRELIBS"
AC_MSG_RESULT($zlib_works)

dnl figure out whether we need to build libz
build_brlcad_zlib="no (using system)"
if test "x$bc_build_zlib" = "xyes" ; then
    build_brlcad_zlib=yes
else
    if test "x$bc_build_zlib" = "xno" ; then
	if test "x$zlib_works" != "xyes" ; then
	    AC_MSG_NOTICE([libz was disabled, but no system zlib compression library was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-zlib-build])
	    AC_MSG_ERROR([*** Building libz was disabled, yet no system zlib compression library was found ***])
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
LIBZ_CPPFLAGS=""
if test "x$build_brlcad_zlib" = "xyes" ; then
    LIBZ='${top_builddir}/src/other/libz/libz.la'
    LIBZ_CPPFLAGS='-I${top_srcdir}/src/other/libz'
    AC_MSG_RESULT(yes)
else
    LIBZ="$LIBZLIB"
    AC_MSG_RESULT(no)
fi
AC_SUBST(LIBZ)
AC_SUBST(LIBZ_CPPFLAGS)


dnl *** X11 interface compilation ***
build_against_x11="no"
if test "x$bc_with_x11" != "xno" ; then
    x11_works=no
    AC_MSG_CHECKING(for X11 header functionality)
    PRELIBS="$LIBS"
    PRECPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    AC_TRY_RUN([
#include <X11/Xlib.h>
main() {
    XPoint pt;
    pt.x = pt.y = 0;
    return pt.x;
}
    ], [x11_works=yes], [x11_works=no], [x11_works=yes])
    AC_MSG_RESULT($x11_works)
    if test "x$x11_works" = "xyes" ; then
	LIBS="$LIBS $X_LIBS"
	AC_MSG_CHECKING(for X11 link functionality)
	AC_TRY_RUN([
#include <X11/Xlib.h>
main() {
    XModifierKeymap *xmkm;
    xmkm=XNewModifiermap(1);
    return xmkm==NULL;
}
	], [x11_works=yes], [x11_works=no], [x11_works=yes])
	AC_MSG_RESULT($x11_works)
    fi
    LIBS="$PRELIBS"
    CPPFLAGS="$PRECPPFLAGS"

    dnl figure out whether we need to build against X11
    if test "x$bc_with_x11" != "xno" ; then
	if test "x$bc_build_aquatk" = "xyes" ; then
	    X_LIBS=""
	    LIBGL=""
	    X_CFLAGS=""
	    build_against_opengl="no"
	    if test "x$bc_with_x11" = "xyes" ; then
		AC_MSG_ERROR([*** X11 was requested, but tk will be built for AquaTk ***])
	    fi
	else
	    if test "x$x11_works" = "xyes" ; then
		build_against_x11="yes"
	    else
		if test "x$bc_with_x11" = "xyes" ; then
		    AC_MSG_NOTICE([X11 was requested, but no system X11 libraries were found])
		    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		    AC_MSG_NOTICE([Try adding --without-x11])
		    AC_MSG_ERROR([*** Using X11 was requested, yet no system X11 was found ***])
		else
		    AC_MSG_NOTICE([X11 does not appear to be functionally available])
		fi
	    fi
	fi
    fi
fi
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
AC_MSG_CHECKING(whether to build against X11)
AC_MSG_RESULT($build_against_x11)


dnl *** utah raster toolkit ***
dnl See if libutahrle seems to work
urt_works=no
AC_MSG_CHECKING(for Utah Raster Toolkit library functionality)
PRELIBS="$LIBS"
if test "x$utahrle_link_works" = "xyes" ; then
    LIBS="$LIBS $UTAHRLE"
fi
AC_TRY_RUN([
#ifdef HAVE_RLE_H
#  include <rle.h>
#endif
int main () {
    rle_hdr header = *rle_hdr_init( NULL );
    return 0;
}
], [urt_works=yes], [urt_works=no], [urt_works=no])
if test "x$utahrle_link_works" = "xyes" ; then
    LIBS="$PRELIBS"
fi
AC_MSG_RESULT($urt_works)

dnl figure out whether we need to build urt
build_brlcad_urt="no (using system)"
if test "x$bc_build_urt" = "xyes" ; then
    build_brlcad_urt=yes
else
    if test "x$bc_build_urt" = "xno" ; then
	if test "x$urt_works" != "xyes" ; then
	    AC_MSG_NOTICE([Utah Raster Toolkit was disabled, but no system URT/RLE library was found ***])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-urt-build])
	    AC_MSG_ERROR([*** Utah Raster Toolkit was disabled, yet no usable RLE system library was found ***])
	fi
    else
	dnl automatic detection
	if test "x$urt_works" = "xno" ; then
	    build_brlcad_urt=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to build the Utah Raster Toolkit)
RLE=""
RLE_CPPFLAGS=""
if test "x$build_brlcad_urt" = "xyes" ; then
    RLE='${top_builddir}/src/other/libutahrle/libutahrle.la'
    RLE_CPPFLAGS='-I${top_srcdir}/src/other/libutahrle/include'
    AC_MSG_RESULT(yes)
else
    if test "x$utahrle_link_works" = "xyes" ; then
	RLE="$UTAHRLE"
    fi
    AC_MSG_RESULT(no)
fi
AC_SUBST(RLE)
AC_SUBST(RLE_CPPFLAGS)


dnl *** openNURBS ***
dnl See if openNURBS library support works
opennurbs_works=no
AC_MSG_CHECKING(for openNURBS functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBOPENNURBS $LIBZLIB"
AC_LANG_PUSH([C++])
AC_TRY_RUN([
#include <opennurbs.h>
int main () {
    int version = ON::Version();
    return 0;
}
], [opennurbs_works=yes], [opennurbs_works=no], [opennurbs_works=no])
AC_LANG_POP
LIBS="$PRELIBS"
AC_MSG_RESULT($opennurbs_works)

dnl figure out whether we need to build openNURBS
build_brlcad_opennurbs="no (using system)"
if test "x$bc_build_opennurbs" = "xyes" ; then
    build_brlcad_opennurbs=yes
else
    if test "x$bc_build_opennurbs" = "xno" ; then
	if test "x$opennurbs_works" != "xyes" ; then
	    AC_MSG_NOTICE([openNURBS was disabled, but no system openNURBS library was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-opennurbs-build])
	    AC_MSG_ERROR([*** openNURBS was disabled, yet no usable system openNURBS was found ***])
	fi
    else
	dnl automatic detection
	if test "x$opennurbs_works" = "xno" ; then
	    build_brlcad_opennurbs=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to build the included openNURBS library)
OPENNURBS=""
OPENNURBS_CPPFLAGS='-I${top_srcdir}/src/other/openNURBS ${LIBZ_CPPFLAGS}'
if test "x$build_brlcad_opennurbs" = "xyes" ; then
    OPENNURBS='${top_builddir}/src/other/openNURBS/libopenNURBS.la ${LIBSTDCXX}'
    AC_MSG_RESULT(yes)
else
    OPENNURBS="$LIBOPENNURBS $LIBSTDCXX $LIBZ"	# produces "  " if we didn't find OpenNURBS
    AC_MSG_RESULT(no)
fi
AC_SUBST(OPENNURBS)
AC_SUBST(OPENNURBS_CPPFLAGS)

dnl *** NIST STEP Class Libraries ***
dnl figure out whether we need to build NIST STEP Class Libraries
STEP_VERSION="3.2.0"
AC_SUBST(STEP_VERSION)
AC_MSG_CHECKING(whether to build NIST Step Class Libraries)
STEP_CPPFLAGS='-I${top_srcdir}/src/other/step/include -I${top_srcdir}/src/other/step/src/cleditor -I${top_srcdir}/src/other/step/src/clstepcore -I${top_srcdir}/src/other/step/src/clutils -I${top_srcdir}/src/other/step/src/cldai -I${top_srcdir}/src/other/step/src/test'
if test ! "x$bc_build_step" = "xno" ; then
    STEP=""
    build_brlcad_step=yes
    AC_MSG_RESULT(no)
else
    build_brlcad_step=no
    STEP=""
    AC_MSG_RESULT(no)
fi
AC_SUBST(STEP)
AC_SUBST(STEP_CPPFLAGS)


dnl *** Tcl ***
dnl See if libtcl seems to work
tcl_works=no
AC_MSG_CHECKING(for Tcl library functionality)
PRELIBS="$LIBS"
if test "x$tcl_link_works" = "xyes" ; then
    LIBS="$LIBS $LIBTCL"
fi
AC_TRY_RUN([
#ifdef HAVE_TCL_H
#  include <tcl.h>
#endif
#ifndef TCL_MAJOR_VERSION
#  error Unknown major version of Tcl
#endif
#ifndef TCL_MINOR_VERSION
#  error Unknown minor version of Tcl
#endif
#include <stdio.h>
int main() {
    Tcl_Time time;
    Tcl_Interp *interp;
    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) == TCL_ERROR) {
	fprintf(stderr, "Tcl_Init returned error\n");
	return 1;
    }
    /* Tcl_GetTime is an 8.4 symbol */
    Tcl_GetTime(&time);
#if TCL_MAJOR_VERSION < 8
    fprintf(stderr, "major version < 8\n");
    return 1;
#elif TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 5
    fprintf(stderr, "minor version < 5\n");
    return 1;
#endif
    return 0;
}
], [tcl_works=yes], [tcl_works=no], [tcl_works=no])
if test "x$tcl_link_works" = "xyes" ; then
    LIBS="$PRELIBS"
fi
AC_MSG_RESULT($tcl_works)

dnl figure out whether we need to build Tcl
build_brlcad_tcl="no (using system)"
if test "x$bc_build_tcl" = "xyes" ; then
    build_brlcad_tcl=yes
else
    if test "x$bc_build_tcl" = "xno" ; then
	if test "x$tcl_works" = "xno" ; then
	    AC_MSG_NOTICE([Tcl was disabled, but no suitable (8.5+) system Tcl was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-tcl-build])
	    AC_MSG_ERROR([*** Tcl was disabled, yet no usable libtcl system library was found ***])
	fi
    else
	dnl automatic detection
	if test "x$tcl_works" = "xno" ; then
	    build_brlcad_tcl=yes
	fi
    fi
fi

AC_MSG_CHECKING(whether to build Tcl)
TCL=""
TCLSTUB=""
TCL_CPPFLAGS=""
unset ac_cv_c_tclconfig
if test "x$build_brlcad_tcl" = "xyes" ; then
    TCL_VERSION="8.5"
    AC_MSG_RESULT(yes)
    SC_ENABLE_THREADS
    case $host_os in
	 darwin*)
	     TCL='-L${top_builddir}/src/other/tcl/unix'" -ltcl${TCL_VERSION} $FRAMEWORK_COREFOUNDATION $THREADS_LIBS"
	     ;;
    	 *)
	     TCL='-L${top_builddir}/src/other/tcl/unix'" -ltcl${TCL_VERSION} ${LIBDL} ${LIBM} $THREADS_LIBS"
	     ;;
    esac
    TCLSTUB='-L${top_builddir}/src/other/tcl/unix'" -ltclstub${TCL_VERSION}"
    TCL_CPPFLAGS='-I${top_srcdir}/src/other/tcl/generic -I${top_srcdir}/src/other/tcl/unix'
else
    AC_MSG_RESULT(no)

    dnl find tclConfig.sh, add --with-tcl option, don't cache result for AC_CONFIG_SUBDIRS
    SC_PATH_TCLCONFIG
    # keep TCL_BIN_DIR, don't keep ac_cv_c_tclconfig
    unset ac_cv_c_tclconfig

    dnl If we're using the system Tcl, then we need to load
    dnl tclConfig.sh to get the TCL_VERSION and other vars.
    SC_LOAD_TCLCONFIG

    if test "x$tcl_link_works" = "xyes" ; then
	TCL="$LIBTCL"
    else
	AC_MSG_WARN([Unable to link against a system Tcl library])
	sleep 1
    fi
    if test "x$tclstub_link_works" = "xyes" ; then
	TCLSTUB="$LIBTCLSTUB"
    else
	AC_MSG_WARN([Unable to link against a system Tclstub library])
	sleep 1
    fi
fi
# needed by incrTcl 3.3
AM_CONDITIONAL(TCL_8_4_HEADERS, [test "x$TCL_VERSION" = "x8.4"])
AC_SUBST(TCL)
AC_SUBST(TCLSTUB)
AC_SUBST(TCL_CPPFLAGS)
AC_SUBST(TCL_VERSION)
AC_SUBST(LIBTCL)


dnl *** Tk ***
dnl See if libtk seems to work
tk_works=no
AC_MSG_CHECKING(for Tk library functionality)
PRELIBS="$LIBS"
if test "x$tk_link_works" = "xyes" ; then
    LIBS="$LIBS $LIBTK $LIBTCL"
fi
AC_TRY_RUN([
#ifdef HAVE_TCL_H
#  include <tcl.h>
#endif
#ifdef HAVE_TK_H
#  include <tk.h>
#endif
#ifndef TK_MAJOR_VERSION
#  error Unknown major version of Tk
#endif
#ifndef TK_MINOR_VERSION
#  error Unknown minor version of Tk
#endif
#include <stdio.h>
int cow = 0;
Tk_ArgvInfo argtbl[] = {
    {"-h", TK_ARGV_CONSTANT, (char *)1, (char *)&cow, "cow."},
    {NULL, TK_ARGV_END, NULL, NULL, NULL}};
int main() {
    Tcl_Interp *interp;
    const char *arg[5] = {".", "xview", "moveto", "1.0", NULL};
    int argl = 1, cow = 0;
    interp = Tcl_CreateInterp();
#define ATTEMPT(try,fail) if(try) { fprintf(stderr, fail": %s\n", \
	Tcl_GetStringResult(interp)); return -1; }
    ATTEMPT(Tcl_Init(interp) == TCL_ERROR,"Tcl_Init returned error");
    ATTEMPT(Tk_ParseArgv(interp, NULL, &argl, arg, argtbl,
 	TK_ARGV_DONT_SKIP_FIRST_ARG) != TCL_OK, "Tk_ParseArgv");
#if TK_MAJOR_VERSION < 8
    fprintf(stderr, "major version < 8  (it's %d)\n", TK_MAJOR_VERSION);
    return 1;
#elif TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 5
    fprintf(stderr, "minor version < 5  (it's %d)\n", TK_MINOR_VERSION);
    return 1;
#endif
    return 0;
}
], [tk_works=yes], [tk_works=no], [tk_works=no])
if test "x$tk_link_works" = "xyes" ; then
    LIBS="$PRELIBS"
fi
AC_MSG_RESULT($tk_works)

dnl figure out whether we need to build Tk
build_brlcad_tk="no (using system)"
build_against_tk=$tk_works
if test "x$bc_build_tk" = "xyes" ; then
    build_brlcad_tk=yes
    build_against_tk=yes
else
    if test "x$bc_build_tk" = "xno" ; then
	if test "x$tk_works" = "xno" ; then
	    AC_MSG_NOTICE([Tk was disabled, but no suitable (8.5+) system Tk was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-tk-build])
	    AC_MSG_ERROR([*** Tk was disabled, yet no usable libtk system library was found ***])
	fi
    else
	dnl automatic detection
	if test "x$tk_works" = "xno" ; then
	    if test "x$build_against_x11" = "xno" && test "x$bc_build_aquatk" = "xno" ; then
		AC_MSG_WARN([Neither X11 nor AquaTk are usable, disabling Tk])
		sleep 1
		build_brlcad_tk="no (incapable)"
	    else
		build_brlcad_tk=yes
		build_against_tk="yes"
	    fi
	fi
    fi
fi

TK=""
TKSTUB=""
TK_CPPFLAGS=""
TK_VERSION="8.5"
unset ac_cv_c_tkconfig

if test "x$build_against_tk" = "xyes" ; then
    AC_MSG_CHECKING(whether to build Tk)
    if test "x$build_brlcad_tk" = "xyes" ; then
	AC_MSG_RESULT(yes)
    	SC_ENABLE_THREADS
	TK='-L${top_builddir}/src/other/tk/unix'" -ltk${TCL_VERSION} $THREADS_LIBS"
	TKSTUB='-L${top_builddir}/src/other/tk/unix'" -ltkstub${TK_VERSION} $THREADS_LIBS"
	TK_CPPFLAGS='-I${top_srcdir}/src/other/tk/generic -I${top_srcdir}/src/other/tk/unix -I${top_srcdir}/src/other/tk/win'
	if test "x$bc_build_aquatk" = "xyes" ; then
	    TK_CPPFLAGS="$TK_CPPFLAGS "'-I${top_srcdir}/src/other/tk/xlib'
	fi
    else
	AC_MSG_RESULT(no)

	dnl find tkConfig.sh, add --with-tk option, don't cache result for AC_CONFIG_SUBDIRS
	SC_PATH_TKCONFIG
	# keep TK_BIN_DIR, don't keep ac_cv_c_tkconfig
	unset ac_cv_c_tkconfig

	dnl If we're using the system Tk, then we need to load
	dnl tkConfig.sh to get the TK_VERSION and other vars.
	SC_LOAD_TKCONFIG

	if test "x$tk_link_works" = "xyes" ; then
	    TK="$LIBTK"
	else
	    AC_MSG_WARN([Unable to link against a system Tk library])
	    sleep 1
	fi
	if test "x$tkstub_link_works" = "xyes" ; then
	    TKSTUB="$LIBTKSTUB"
	else
	    AC_MSG_WARN([Unable to link against a system Tkstub library])
	    sleep 1
        fi
    fi
    AC_DEFINE(HAVE_TK, 1, [Support for a Tk GUI is enabled])
else
    dnl *** NO MGED GUI ***
    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
    AC_MSG_WARN([Support for a BRL-CAD graphical user interface (GUI) will be disabled.])
    AC_MSG_NOTICE([Try installing X11 development headers or system Tcl/Tk libraries.])
    AC_MSG_NOTICE([MGED will still work in classic console mode but without a GUI.])
    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
    sleep 5
fi
AC_SUBST(TK)
AC_SUBST(TKSTUB)
AC_SUBST(TK_CPPFLAGS)
AC_SUBST(TK_VERSION)
AC_SUBST(LIBTK)


dnl *** incrTcl ***
dnl See if libitcl and libitk seem to work, make sure it's not dependant upon libtcl
itcl_works=no
AC_MSG_CHECKING(for incrTcl library functionality)
PRELIBS="$LIBS"
if test "x$itcl_link_works" = "xyes" ; then
    LIBS="$LIBS $LIBITCL $LIBTCL"
fi
AC_TRY_RUN([
#include <itcl.h>
#ifndef ITCL_MAJOR_VERSION
#  error Unknown major version of incrTcl
#endif
#ifndef ITCL_MINOR_VERSION
#  error Unknown minor version of incrTcl
#endif
#include <stdio.h>
int main() {
    Itcl_Stack stack;
    Itcl_InitStack(&stack);
#if ITCL_MAJOR_VERSION < 3
    fprintf(stderr, "major version < 3\n");
    return 1;
#elif ITCL_MAJOR_VERSION == 3 && ITCL_MINOR_VERSION < 2
    fprintf(stderr, "minorversion < 2\n");
    return 1;
#endif
    return 0;
}
], [itcl_works=yes], [itcl_works=no], [itcl_works=no])
if test "x$itcl_link_works" = "xyes" ; then
    LIBS="$PRELIBS"
fi
AC_MSG_RESULT($itcl_works)

dnl figure out whether we need to build incrTcl
build_brlcad_itcl="no (using system)"
if test "x$bc_build_itcl" = "xyes" ; then
    build_brlcad_itcl=yes
else
    if test "x$bc_build_itcl" = "xno" ; then
	if test "x$itcl_works" = "xno" ; then
	    AC_MSG_NOTICE([incrTcl was disabled, but no suitable (3.2+) system incrTcl library was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-itcl-build])
	    AC_MSG_ERROR([*** incrTcl was disabled, yet no usable libitcl system library was found ***])
	fi
    else
	dnl automatic detection
	if test "x$itcl_works" = "xno" ; then
	    build_brlcad_itcl=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to build incrTcl)

dnl itcl/itk
ITCL=""
ITK=""
ITCL_CPPFLAGS=""
ITK_CPPFLAGS=""
ITCL_VERSION=3.4
case $host_os in
     *netbsd-*|*freebsd-[[1-3]]*.*|*openbsd-[[1-3]]*|*sunos4*)
	 # some OS's don't like dots in the lib name
	 ITCL_VERSION="`echo ${ITCL_VERSION} | tr -d .`"
	 ;;
esac
AC_SUBST(ITCL_VERSION)
if test "x$build_brlcad_itcl" = "xyes" ; then
    ITCL='${top_builddir}/src/other/incrTcl/libitcl.la'
    ITK='${top_builddir}/src/other/incrTcl/libitk.la'
    ITCL_CPPFLAGS='-I${top_srcdir}/src/other/incrTcl/itcl/generic'
    ITK_CPPFLAGS='-I${top_srcdir}/src/other/incrTcl/itk/generic'
    AC_MSG_RESULT(yes)
else
    if test "x$itcl_link_works" = "xyes" ; then
	ITCL="$LIBITCL"
	ITK="$LIBITK"
    fi
    AC_MSG_RESULT(no)
fi
AC_SUBST(ITCL)
AC_SUBST(ITK)
AC_SUBST(ITCL_CPPFLAGS)
AC_SUBST(ITK_CPPFLAGS)
AC_SUBST(LIBITCL)
AC_SUBST(LIBITK)

AC_MSG_CHECKING(whether to build incrTk)
build_brlcad_itk="$build_brlcad_itcl"
if test "x$build_against_tk" = "xno" ; then
    build_brlcad_itk=no
else
    if test "$bc_build_aquatk" = "xno" && test "x$build_against_x11" = "xno" ; then
        build_brlcad_itk=no
    fi
fi
AC_MSG_RESULT($build_brlcad_itk)

# XXX hack until we can call incrTcl's configure to get the value
case $host_os in
    darwin*)
	SHLIB_SUFFIX=".dylib"
	;;
    *)
	SHLIB_SUFFIX=".so"
	;;
esac

# XXX hack since we don't call incrTcl's configure for the proper
# library name.
ITCL_LIB_FILE="libitcl${ITCL_VERSION}${SHLIB_SUFFIX}"
ITK_LIB_FILE="libitk${ITCL_VERSION}${SHLIB_SUFFIX}"
AC_SUBST(ITCL_LIB_FILE)
AC_SUBST(ITK_LIB_FILE)

dnl *** iwidgets ***
dnl See if the iwidgets are available
iwidgets_works=no
AC_MSG_CHECKING(for iwidgets functionality)
PRELIBS="$LIBS"
if test "x$tcl_link_works" = "xyes" ; then
    LIBS="$LIBS $LIBTCL $LIBITCL"
fi
dnl this test is probably not sufficient if iwidgets is not locateable
dnl in the tcl auto_path.
AC_TRY_RUN([
#ifdef HAVE_TCL_H
#  include <tcl.h>
#endif
int main() {
    int res = TCL_OK;
    Tcl_Interp *interp;
    const char *cmd = "package require Iwidgets";
    char *b = NULL;
    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) == TCL_ERROR) {
	printf("TCL interpreter failed to init: %s\n", Tcl_GetStringResult(interp));
	return 1;
    }
    if (Itcl_Init(interp) == TCL_ERROR) {
	printf("ITCL interp failed to initaliaze: %s\n", Tcl_GetStringResult(interp));
	return 1;
    }
    res = Tcl_Eval(interp, cmd);
    if ( res != TCL_OK) {
	printf("Failed to load Iwidgets: %s\n", Tcl_GetStringResult(interp));
	if(Tcl_Eval(interp, "package names") == TCL_ERROR)
	    return 1;
	while(b=strtok(b?NULL:Tcl_GetStringResult(interp)," "))
	  if(!strncmp(b,"Iwidgets",8))
	    return 0;
        return 1;
    }
    return 0;
}
], [iwidgets_works=yes], [iwidgets_works=no], [iwidgets_works=no])
if test "x$tcl_link_works" = "xyes" ; then
    LIBS="$PRELIBS"
fi
AC_MSG_RESULT($iwidgets_works)

dnl figure out whether we need to install the iwidgets package
build_brlcad_iwidgets="no (using system)"
if test "x$bc_build_iwidgets" = "xyes" ; then
    build_brlcad_iwidgets=yes
else
    if test "x$bc_build_iwidgets" = "xno" ; then
	if test "x$iwidgets_works" = "xno" ; then
	    AC_MSG_NOTICE([iwidgets was disabled, but no suitable system iwidgets package was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-iwidgets-build])
	    AC_MSG_ERROR([*** iwidgets was disabled, yet no usable system iwidgets package was found ***])
	fi
    else
	dnl automatic detection
	if test "x$iwidgets_works" = "xno" ; then
	    build_brlcad_iwidgets=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to install iwidgets)
AC_MSG_RESULT($build_brlcad_iwidgets)
IWIDGETS_VERSION="4.0.1"
AC_DEFINE_UNQUOTED([IWIDGETS_VERSION], "$IWIDGETS_VERSION", "Version of iWidgets")
AC_SUBST(IWIDGETS_VERSION)


dnl *** tkhtml ***
dnl figure out whether we need to build tkhtml
AC_MSG_CHECKING(whether to build tkhtml)
if test ! "x$bc_build_tkhtml" = "xno" ; then
    build_brlcad_tkhtml=yes
    AC_MSG_RESULT(yes)
else
    build_brlcad_tkhtml=no
    AC_MSG_RESULT(no)
fi

dnl *** tktable ***
dnl figure out whether we need to build tktable
TKTABLE_VERSION="2.10"
AC_SUBST(TKTABLE_VERSION)
AC_MSG_CHECKING(whether to build tktable)
if test ! "x$bc_build_tktable" = "xno" ; then
    build_brlcad_tktable=yes
    AC_MSG_RESULT(yes)
else
    build_brlcad_tktable=no
    TKTABLE=""
    AC_MSG_RESULT(no)
fi
AC_SUBST(TKTABLE)


dnl *** tkpng ***
dnl figure out whether we need to build tkpng
TKPNG_VERSION="0.8"
TKPNG_LIB_FILE="libtkpng${SHLIB_SUFFIX}"
case $host_os in
     *netbsd-*|*freebsd-[[1-3]]*.*|*openbsd-[[1-3]]*|*sunos4*)
	 # some OS's don't like dots in the lib name
	 TKPNG_VERSION="`echo ${TKPNG_VERSION} | tr -d .`"
	 ;;
esac
AC_SUBST(TKPNG_VERSION)
AC_SUBST(TKPNG_LIB_FILE)
AC_MSG_CHECKING(whether to build tkpng)
if test ! "x$bc_build_tkpng" = "xno" ; then
    build_brlcad_tkpng=yes
    TKPNG='${top_builddir}/src/other/tkpng/libtkpng${TKPNG_VERSION}.la'
    AC_MSG_RESULT(yes)
else
    build_brlcad_tkpng=no
    TKPNG=""
    AC_MSG_RESULT(no)
fi
AC_SUBST(TKPNG)


dnl *** TNT ***
dnl See if template numerical toolkit works
tnt_works=no
AC_MSG_CHECKING(for TNT w/JAMA functionality)
AC_LANG_PUSH(C++)
AC_TRY_RUN([
#include <stdio.h>
#include <jama_lu.h>
int main () {
    Array2D<double> matrix = Array2D<double>(2,2);
    matrix[0][0] = matrix[1][0] = 1.0;
    matrix[0][1] = matrix[1][1] = 0.0;
    JAMA::LU<double> decomposition = JAMA::LU<double>(matrix);
    if (decomposition.isNonsingular()) {
	printf("JAMA:LU decomposition returned non-singular on a singular matrix\n");
	return 1;
    }
    return 0;
}
], [tnt_works=yes], [tnt_works=no], [tnt_works=no])
AC_LANG_POP(C++)
AC_MSG_RESULT($tnt_works)

dnl figure out whether we need to build libz
build_brlcad_tnt="no (using system)"
if test "x$bc_build_tnt" = "xyes" ; then
    build_brlcad_tnt=yes
else
    if test "x$bc_build_tnt" = "xno" ; then
	if test "x$tnt_works" != "xyes" ; then
	    AC_MSG_NOTICE([TNT was disabled, but no suitable system TNT was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-tnt-build])
	    AC_MSG_ERROR([*** Use of the included TNT was disabled, yet no system TNT was found ***])
	fi
    else
	dnl automatic detection
	if test "x$tnt_works" = "xno" ; then
	    build_brlcad_tnt=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to use the included Template Numerical Toolkit)
TNT_CPPFLAGS=""
if test "x$build_brlcad_tnt" = "xyes" ; then
    TNT_CPPFLAGS='-I${top_srcdir}/src/other/tnt'
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi
AC_SUBST(TNT_CPPFLAGS)


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
], [thread_works=yes], [thread_works=no], [thread_works=no])
AC_MSG_RESULT($thread_works)


dnl *** libtermlib ***
dnl See if termlib library support works
termlib_works=no
AC_MSG_CHECKING(for termlib functionality)
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTERMLIB"
AC_TRY_RUN([
#ifdef HAVE_TERMLIB_H
#  include <termlib.h>
#else
#  ifdef HAVE_NCURSES_H
#    include <ncurses.h>
#  else
#    ifdef HAVE_CURSES_H
#      include <curses.h>
#    else
#      ifdef HAVE_TERMCAP_H
#        include <termcap.h>
#      else
#        ifdef HAVE_TERMINFO_H
#          include <terminfo.h>
#        else
#          ifdef HAVE_TINFO_H
#            include <tinfo.h>
#          endif
#        endif
#      endif
#    endif
#  endif
#  ifdef HAVE_TERM_H
#    include <term.h>
#  endif
#endif
int main () {
    char buffer[2048] = {0};
    int result = tgetent(buffer, "vt100");
    return 0;
}
], [termlib_works=yes], [termlib_works=no], [termlib_works=no])
LIBS="$PRELIBS"
AC_MSG_RESULT($termlib_works)

dnl figure out whether we need to build libtermlib
build_brlcad_termlib="no (using system)"
if test "x$bc_build_termlib" = "xyes" ; then
    build_brlcad_termlib=yes
else
    if test "x$bc_build_termlib" = "xno" ; then
	if test "x$termlib_works" != "xyes" ; then
	    AC_MSG_NOTICE([termlib was disabled, but no system terminal library was found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --enable-termlib-build])
	    AC_MSG_ERROR([*** Building termlib was disabled, yet no system termcap or curses library was found ***])
	fi
    else
	dnl automatic detection
	if test "x$termlib_works" = "xno" ; then
	    build_brlcad_termlib=yes
	fi
    fi
fi
AC_MSG_CHECKING(whether to build the included termlib library)
TERMLIB=""
TERMLIB_CPPFLAGS=""
if test "x$build_brlcad_termlib" = "xyes" ; then
    TERMLIB='${top_builddir}/src/other/libtermlib/libtermlib.la'
    TERMLIB_CPPFLAGS='-I${top_srcdir}/src/other/libtermlib'
    AC_DEFINE(HAVE_TERMCAP_H, 1, [Define to 1 if you have the <termcap.h> header file.])
    AC_MSG_RESULT(yes)
else
    TERMLIB="$LIBTERMLIB"
    AC_MSG_RESULT(no)
fi
AC_SUBST(TERMLIB)
AC_SUBST(TERMLIB_CPPFLAGS)

dnl *** Boost ***
dnl Header only Library requiring no build
BOOST_CPPFLAGS='-I${top_srcdir}/src/other'
AC_SUBST(BOOST_CPPFLAGS)

dnl *** libm ***
AC_MSG_CHECKING(whether to link with the system math library)
AC_MSG_RESULT($m_link_works)
AC_SUBST(LIBM)

dnl *** libsocket ***
AC_MSG_CHECKING(for system socket library availability)
AC_MSG_RESULT([$socket_link_works])
AC_SUBST(LIBSOCKET)

dnl *** libnsl ***
AC_MSG_CHECKING(for system network socket library availablity)
AC_MSG_RESULT([$nsl_link_works])
AC_SUBST(LIBNSL)

dnl *** libnetwork ***
AC_MSG_CHECKING(for system network library availablity)
AC_MSG_RESULT([$network_link_works])
AC_SUBST(LIBNETWORK)

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

dnl *** libgl ***
AC_MSG_CHECKING(for OpenGL library availability)
AC_MSG_RESULT($gl_link_works)
AC_SUBST(LIBGL)

dnl *** Apple OpenGL ***
AC_MSG_CHECKING(for Apple OpenGL framework availability)
AC_MSG_RESULT($framework_opengl)
AC_SUBST(FRAMEWORK_AGL)

dnl *** libopengl32 ***
AC_MSG_CHECKING(for WindowsGL library availability)
AC_MSG_RESULT($wgl_link_works)
AC_SUBST(LIBWGL)

dnl *** libl Solaris lexer library ***
AC_MSG_CHECKING(for Solaris lexer library availability)
AC_MSG_RESULT($l_link_works)
AC_SUBST(LIBL)

dnl *** libgen IRIX dirname/basename library ***
AC_MSG_CHECKING(for libgen library availability)
AC_MSG_RESULT($gen_link_works)
AC_SUBST(LIBGEN)

dnl *** libregex ***
AC_MSG_CHECKING(for system regular expression library availability)
AC_MSG_RESULT($regex_link_works)
AC_SUBST(LIBREGEX)

dnl *** libpng ***
AC_MSG_CHECKING(for system portable network graphics availability)
AC_MSG_RESULT($png_link_works)
AC_SUBST(LIBPNG)

dnl *** openNURBS library ***
AC_MSG_CHECKING(for system openNURBS library availability)
AC_MSG_RESULT($opennurbs_link_works)
AC_SUBST(LIBOPENNURBS)

dnl *** libstdc++ C++ library ***
AC_MSG_CHECKING(whether the Standard C++ library is available)
AC_MSG_RESULT($stdcxx_link_works)
AC_SUBST(LIBSTDCXX)

if test "x$bc_with_dtrace" = "xyes" ; then
    AC_DEFINE(USE_DTRACE, 1, [Are we building with DTrace support?])
    DTRACE_SRC="\${DTRACE_SRC}"
    DTRACE_HDR="\${DTRACE_HDR}"
    DTRACE_OBJ="\${DTRACE_OBJ}"
    AC_SUBST(DTRACE_SRC)
    AC_SUBST(DTRACE_HDR)
    AC_SUBST(DTRACE_OBJ)
fi

dnl *** 64-bit compilation ***
dnl figure out whether we are building 64-bit
build_brlcad_64bit="no"
if test "x$bc_build_64bit" = "xyes" ; then
    if test "x$pointer_size" != "x8" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_NOTICE([Try removing --enable-64bit-build or modifying the CFLAGS/LDFLAGS])
	AC_MSG_ERROR([*** Building 64-bit was requested, yet the build seems to be non-64-bit ***])
    fi
    build_brlcad_64bit=yes
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
	build_brlcad_64bit="yes"
    elif test "x$pointer_size" = "x4" ; then
	build_brlcad_64bit="no (32-bit)"
    elif test "x$pointer_size" = "x2" ; then
	build_brlcad_64bit="no (16-bit)"
    elif test "x$pointer_size" = "x1" ; then
	build_brlcad_64bit="no (8-bit)"
    else
	AC_MSG_WARN([Unknown pointer size: $pointer_size])
	sleep 5
    fi
fi


dnl *** Jove compilation ***
build_brlcad_jove=no
if test "x$bc_build_jove" = "xyes" ; then
    build_brlcad_jove=yes
elif test "x$bc_build_jove" = "xno" ; then
    build_brlcad_jove=no	# body for the sake of body
else
    # enable jove if no suitable system emacs/jove was found
    if test "x$BCEDITOR" = "x" ; then
	build_brlcad_jove=yes
    fi
fi


dnl *** Java interface compilation ***
build_against_java=no
if test "x$bc_with_java" != "xno" ; then
    AC_MSG_CHECKING(for Java functionality)
    if test "x$found_jni_h" = "xno" ; then
	if test "x$bc_only_rts" != "xno" ; then
	    AC_MSG_RESULT(no)
	    AC_MSG_NOTICE([searching more extensively])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
	    AC_MSG_ERROR([*** Cannot locate jni.h for building librtserver ***])
	fi
	if test "x$bc_with_java" = "xyes" ; then
	    AC_MSG_RESULT(no)
	    AC_MSG_NOTICE([searching more extensively])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_ERROR([Failed to locate a functioning jni.h for Java])
	fi

	if test "x$bc_with_java" != "xauto" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([Unable to locate a functioning Java])
	    AC_MSG_WARN([Compilation of librtserver will be disabled])
	    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	    sleep 1
	fi
	AC_MSG_RESULT(no)
    else # found_jni_h
	if test "x$bc_with_java" = "xauto" ; then
	    build_against_java=yes
	else
	    build_against_java="yes ($bc_with_java_val)"
	fi
	AC_MSG_RESULT(yes)
    fi # found_jni_h
fi # bc_with_java


dnl *** Pro/ENGINEER installation / plugin compilation ***
build_against_proe=no
if test "x$bc_with_proe" != "xno" ; then
    AC_MSG_CHECKING(for Pro/ENGINEER installation)
    if test "x$found_protk_h" = "xno" ; then
	AC_MSG_RESULT(no)
	if test "x$bc_with_proe" = "xyes" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([Unable to locate ProToolkit.h. Make sure your installation of Pro/E also has Pro/TOOLKIT])
	    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	    sleep 5
	fi
	if test "x$bc_with_proe" = "xauto" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([Pro/E was not installed in the standard place (/usr/local/ptc/proeWildfire3.0).])
	    AC_MSG_WARN([Try adding --with-proe=/path/to/proe to find the ProToolkit.h])
	    AC_MSG_WARN([Compilation of Pro/ENGINEER plugin will be disabled])
	    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	    sleep 5
	fi
    else
	if test "x$bc_with_proe" = "xauto" ; then
	    build_against_proe=yes
	else
	    build_against_proe="yes ($bc_with_proe_val)"
	fi
	AC_MSG_RESULT(yes)
    fi
fi


dnl *** OpenGL interface compilation ***
build_against_opengl="no"
if test "x$bc_with_opengl" != "xno" ; then
    AC_MSG_CHECKING(for OpenGL functionality)
    opengl_works=no
    PRELIBS="$LIBS"
    PRECPPFLAGS="$CPPFLAGS"
    PRELDFLAGS="$LDFLAGS"
    LIBS="$LIBS $LIBGL"
    CPPFLAGS="$CPPFLAGS $GL_CPPFLAGS"
    LDFLAGS="$LDFLAGS $GL_LDFLAGS"
    AC_TRY_RUN([
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
main(int argc, char *argv[]) {
    GLboolean values;
    void (*func)() = glBegin;
    if (GL_FALSE || argc > 1) {
	func(GL_POINTS);
	glGetBooleanv(GL_DEPTH_TEST, &values);
	glEnd();
    }
    return 0;
}
    ], [opengl_works=yes], [opengl_works=no], [opengl_works=no])
    LIBS="$PRELIBS"
    CPPFLAGS="$PRECPPFLAGS"
    LDFLAGS="$PRELDFLAGS"
    AC_MSG_RESULT($opengl_works)

    dnl figure out whether we need to build against opengl
    if test "x$opengl_works" = "xyes" ; then
	build_against_opengl="yes"
    else
	if test "x$bc_with_opengl" = "xyes" ; then
	    AC_MSG_NOTICE([OpenGL was requested, but no system OpenGL libraries were found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --without-opengl])
	    AC_MSG_ERROR([*** Using OpenGL was requested, yet no system OpenGL was found ***])
	else
	    AC_MSG_NOTICE([OpenGL does not appear to be functionally available])
	fi
    fi
fi # bc_with_opengl
AC_MSG_CHECKING(whether to build against OpenGL)
AC_MSG_RESULT($build_against_opengl)

dnl *** Apple OpenGL interface compilation ***
build_against_agl="no"
if test "x$bc_with_agl" != "xno" ; then
    AC_MSG_CHECKING(for Apple OpenGL functionality)
    agl_works=no
    PRELIBS="$LIBS"
    PRECPPFLAGS="$CPPFLAGS"
    PRELDFLAGS="$LDFLAGS"
    LIBS="$LIBS $LIBGL"
    CPPFLAGS="$CPPFLAGS $GL_CPPFLAGS"
    LDFLAGS="$LDFLAGS $FRAMEWORK_OPENGL"
    AC_TRY_RUN([
#include <OpenGL/OpenGL.h>
main(int argc, char *argv[]) {
    GLboolean values;
    void (*func)() = glBegin;
    if (GL_FALSE || argc > 1) {
	func(GL_POINTS);
	glGetBooleanv(GL_DEPTH_TEST, &values);
	glEnd();
    }
    return 0;
}
    ], [agl_works=yes], [agl_works=no], [agl_works=no])
    LIBS="$PRELIBS"
    CPPFLAGS="$PRECPPFLAGS"
    LDFLAGS="$PRELDFLAGS"
    AC_MSG_RESULT($agl_works)

    dnl figure out whether we need to build against opengl
    if test "x$agl_works" = "xyes" ; then
	build_against_agl="yes"
    else
	if test "x$bc_with_agl" = "xyes" ; then
	    AC_MSG_NOTICE([Apple OpenGL was requested, but no system OpenGL libraries were found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --without-agl])
	    AC_MSG_ERROR([*** Using Apple OpenGL was requested, yet no system OpenGL was found ***])
	else
	    AC_MSG_NOTICE([OpenGL does not appear to be functionally available])
	fi
    fi
fi # bc_with_agl
AC_MSG_CHECKING(whether to build against Apple OpenGL)
AC_MSG_RESULT($build_against_agl)

dnl *** WindowsGL interface compilation ***
build_against_wgl="no"
if test "x$bc_with_wgl" != "xno" ; then
    AC_MSG_CHECKING(for WindowsGL functionality)
    wgl_works=no
    PRELIBS="$LIBS"
    LIBS="$LIBS $LIBWGL"
    AC_TRY_RUN([
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
main(int argc, char *argv[]) {
    GLboolean values;
    void (*func)() = glBegin;
    wglMakeCurrent(NULL, NULL);
    if (GL_FALSE || argc > 1) {
        func(GL_POINTS);
	glGetBooleanv(GL_DEPTH_TEST, &values);
	glEnd();
    }
    return 0;
}
    ], [wgl_works=yes], [wgl_works=no], [wgl_works=no])
    LIBS="$PRELIBS"
    AC_MSG_RESULT($wgl_works)

    dnl figure out whether we need to build against wgl
    if test "x$wgl_works" = "xyes" ; then
	build_against_wgl="yes"
    else
	if test "x$bc_with_wgl" = "xyes" ; then
	    AC_MSG_NOTICE([WindowsGL was requested, but no system WGL OpenGL libraries were found])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --without-wgl])
	    AC_MSG_ERROR([*** Using WindowsGL was requested, yet no system WGL OpenGL was found ***])
	else
	    AC_MSG_NOTICE([WindowsGL does not appear to be functionally available])
	fi
    fi
fi # bc_with_wgl
AC_MSG_CHECKING(whether to build against WindowsGL)
AC_MSG_RESULT($build_against_wgl)


dnl *** RTGL interface compilation ***
build_rtgl="no"
if test "x$bc_rtgl" = "xyes" ; then
    if test "x$build_against_opengl" = "xno" ; then
	AC_MSG_NOTICE([RTGL was requested, but no system OpenGL libraries were found])
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_NOTICE([Try adding --with-opengl])
	AC_MSG_ERROR([*** Using RTGL was requested, yet no system OpenGL was found ***])
    fi
    build_rtgl="yes"
fi # bc_with_rtgl
AC_MSG_CHECKING(whether to build experimental RTGL display manager)
AC_MSG_RESULT($build_rtgl)


dnl *** Knobs availability ***
dials_and_buttons=no
AC_MSG_CHECKING(for SGI dials and buttons support)
AC_TRY_RUN([
#ifdef HAVE_GL_DEVICE_H
#  include <gl/device.h>
#endif
main () {
    if (DIAL0 | DIAL1 | DIAL2 | DIAL3) {
	return 0;
    }
    return 1;
}
], [dials_and_buttons=yes], [dials_and_buttons=no], [dials_and_buttons=no])
if test "x$dials_and_buttons" = "xyes" ; then
    AC_DEFINE(IR_KNOBS, 8, [Turn on SGI knobs])
    AC_DEFINE(IR_BUTTONS, 32, [Turn on SGI buttons])
fi
AC_MSG_RESULT($dials_and_buttons)


dnl *** Docbook functionality ***
docbook_processing=no
docbook_pdf_processing=no
if test "x$bc_build_docs" != "xno" ; then
    if test "x$bc_build_docs" = "xyes" ; then
	if test "x$XSLTPROC" = "x" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([Extra documentation is enabled but xsltproc was not found])
	    AC_MSG_ERROR([*** Don't know how to generate documentation without xsltproc ***])
	fi
    fi
fi

# test for this outside of bc_build_docs so vars get set properly
if test "x$XSLTPROC" = "x" ; then
    XSLTPROC=:
else
    if test "x$APACHEFOP" = "x" ; then
	docbook_processing="yes (man/html only)"
	docbook_pdf_processing="no"
	APACHEFOP=:
    else
        docbook_processing="yes (man/html/pdf)"
	docbook_pdf_processing="yes"
	# set FOP_OPTS to avoid dock bounce on osX
	APACHEFOP="FOP_OPTS=-Djava.awt.headless=true $APACHEFOP"
    fi
fi
AC_SUBST(XSLTPROC)
AC_SUBST(APACHEFOP)
AC_MSG_CHECKING(for Docbook processing functionality)
AC_MSG_RESULT($docbook_processing)
AC_MSG_CHECKING(for Docbook pdf generation functionality)
AC_MSG_RESULT($docbook_pdf_processing)

# XXX nasty temp hack for backwards cake compatibility. code needs to be fixed.
if test "x$host_os" = "xirix6.5" ; then
    # XXX the code that relies on this will be going away, but until then..
    if test "x$build_brlcad_64bit" = "xyes" ; then
	CFLAGS="$CFLAGS -DIRIX64=65"
    else
	CFLAGS="$CFLAGS -DIRIX=6"
    fi
fi

# XXX want to include strict flags pervasively throughout if strict
# building is enabled.  alas, can't enable this until src/other has
# NOWARN (-w) added to the build: CFLAGS="$CFLAGS \${STRICT_FLAGS}"

dnl search the BRL-CAD include directory (required for non-srcdir builds).
dnl should come after the system services checks otherwise or our headers
dnl may conflict.
CPPFLAGS="$CPPFLAGS -DBRLCADBUILD=1 -I\${top_srcdir}/include $OPENNURBS_CPPFLAGS"

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

AM_CONDITIONAL(BUILD_TCL, [test "x$build_brlcad_tcl" = "xyes"])
AM_CONDITIONAL(BUILD_TK, [test "x$build_brlcad_tk" = "xyes"])
AM_CONDITIONAL(BUILD_ITCL, [test "x$build_brlcad_itcl" = "xyes"])
AM_CONDITIONAL(BUILD_ITK, [test "x$build_brlcad_itk" = "xyes"])
AM_CONDITIONAL(BUILD_IWIDGETS, [test "x$build_brlcad_iwidgets" = "xyes"])
AM_CONDITIONAL(BUILD_TKHTML, [test "x$build_brlcad_tkhtml" = "xyes"])
AM_CONDITIONAL(BUILD_TKTABLE, [test "x$build_brlcad_tktable" = "xyes"])
AM_CONDITIONAL(BUILD_TKPNG, [test "x$build_brlcad_tkpng" = "xyes"])
AM_CONDITIONAL(BUILD_PNG, [test "x$build_brlcad_png" = "xyes"])
AM_CONDITIONAL(BUILD_REGEX, [test "x$build_brlcad_regex" = "xyes"])
AM_CONDITIONAL(BUILD_ZLIB, [test "x$build_brlcad_zlib" = "xyes"])
AM_CONDITIONAL(BUILD_URT, [test "x$build_brlcad_urt" = "xyes"])
AM_CONDITIONAL(BUILD_OPENNURBS, [test "x$build_brlcad_opennurbs" = "xyes"])
AM_CONDITIONAL(BUILD_BOOST, [test "x$ac_cv_header_limits" = "xyes" && test "x$ac_cv_header_istream" = "xyes"])
AM_CONDITIONAL(BUILD_STEP, [test "x$build_brlcad_step" = "xyes"])
AM_CONDITIONAL(BUILD_TERMLIB, [test "x$build_brlcad_termlib" = "xyes"])
AM_CONDITIONAL(BUILD_TNT, [test "x$build_brlcad_tnt" = "xyes"])
AM_CONDITIONAL(BUILD_JOVE, [test "x$build_brlcad_jove" = "xyes"])
AM_CONDITIONAL(BUILD_CUBIT, [test "x$bc_build_cubit" = "xyes"])
AM_CONDITIONAL(BUILD_UG, [test "x$bc_build_ug" = "xyes"])

# libfb and libdm interfaces
AM_CONDITIONAL(WITH_X11, [test "x$build_against_x11" != "xno"])
AM_CONDITIONAL(WITH_OPENGL, [test "x$build_against_opengl" != "xno" && test "x$build_against_x11" != "xno"])
AM_CONDITIONAL(WITH_WGL, [test "x$build_against_wgl" != "xno"])
AM_CONDITIONAL(WITH_TK, [test "x$build_against_tk" != "xno"])

# XXX need to refactor the above fb/dm interfaces to have a better
# separation of their external 'with' deps and the interfaces
AM_CONDITIONAL(BUILD_RTGL, [test "x$build_rtgl" = "xyes"])

AM_CONDITIONAL(WITH_JAVA, [test "x$build_against_java" != "xno"])
AM_CONDITIONAL(WITH_PROE, [test "x$build_against_proe" != "xno"])
AM_CONDITIONAL(WITH_PARSERS, [test "x$build_parsers" != "xno"])

AM_CONDITIONAL(BUILD_DOCS, [test "x$docbook_processing" != "xno"])
AM_CONDITIONAL(BUILD_PDF_DOCS, [test "x$docbook_pdf_processing" != "xno"])
AM_CONDITIONAL(INSTALL_MODELS, [test "x$bc_install_geometry" = "xyes"])
AM_CONDITIONAL(ONLY_BENCHMARK, [test "x$bc_only_benchmark" != "xno"])
AM_CONDITIONAL(ONLY_RTS, [test "x$bc_only_rts" != "xno"])

# Language conditionals
AM_CONDITIONAL(EN_DOCS, [test "x$en_docs" = "xyes"])
AM_CONDITIONAL(ES_DOCS, [test "x$es_docs" = "xyes"])


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

dnl SMP-aware (i.e. PARALLEL) build
if test "x$bc_build_parallel" != "xno" ; then
	AC_DEFINE(PARALLEL, 1, [Define to enable SMP architecture parallel computation support ])
fi


#
# set up the BRL-CAD libraries
#

ANALYZE='${top_builddir}/src/libanalyze/libanalyze.la'
BN='${top_builddir}/src/libbn/libbn.la'
BU='${top_builddir}/src/libbu/libbu.la'
CURSOR='${top_builddir}/src/libcursor/libcursor.la'
DM='${top_builddir}/src/libdm/libdm.la'
FB='${top_builddir}/src/libfb/libfb.la'
FFT='${top_builddir}/src/libfft/libfft.la'
ICV='${top_builddir}/src/libicv/libicv.la'
GCV='${top_builddir}/src/libgcv/libgcv.la'
GED='${top_builddir}/src/libged/libged.la'
MULTISPECTRAL='${top_builddir}/src/libmultispectral/libmultispectral.la'
OPTICAL='${top_builddir}/src/liboptical/liboptical.la'
ORLE='${top_builddir}/src/liborle/liborle.la'
PC='${top_builddir}/src/libpc/libpc.la'
PKG='${top_builddir}/src/libpkg/libpkg.la'
RENDER='${top_builddir}/src/adrt/librender.la'
RT='${top_builddir}/src/librt/librt.la'
SYSV='${top_builddir}/src/libsysv/libsysv.la'
TCLCAD='${top_builddir}/src/libtclcad/libtclcad.la'
TERMIO='${top_builddir}/src/libtermio/libtermio.la'
TIE='${top_builddir}/src/adrt/libtie.la'
WDB='${top_builddir}/src/libwdb/libwdb.la'
BRLCAD='${top_builddir}/src/libbrlcad.la'

STEPCORE='${top_builddir}/src/other/step/src/clstepcore/libstepcore.la'
STEPDAI='${top_builddir}/src/other/step/src/cldai/libstepdai.la'
STEPEDITOR='${top_builddir}/src/other/step/src/cleditor/libstepeditor.la'
STEPUTILS='${top_builddir}/src/other/step/src/clutils/libsteputils.la'
STEPEXPRESS='${top_builddir}/src/other/step/src/express/libexpress.la'
STEPEXPPP='${top_builddir}/src/other/step/src/exppp/libexppp.la'
STEP_LIBS="${STEPCORE} ${STEPDAI} ${STEPEDITOR} ${STEPUTILS} ${STEPEXPRESS}"
STEP_FEDEX='${top_builddir}/src/other/step/src/express/fedex'
STEP_FEDEX_PLUS='${top_builddir}/src/other/step/src/fedex_plus/fedex_plus'
STEP_EXPPP='${top_builddir}/src/other/step/src/exppp/exppp'
STEP_AP203='${top_srcdir}/src/other/step/data/ap203/203wseds.exp'

ORLE_LIBS=
PKG_LIBS="${LIBSOCKET} ${LIBNSL} ${LIBNETWORK}"
SYSV_LIBS=
TERMIO_LIBS="${TERMLIB}"
CURSOR_LIBS="${TERMLIB}"
FFT_LIBS="${LIBM}"
ANALYZE_LIBS="${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS}"
BU_LIBS="${TCL} ${PNG} ${LIBZ} ${LIBM} ${LIBMALLOC} ${LIBTHREAD}"
BN_LIBS="${BU} ${BU_LIBS} ${TCL} ${LIBM}"
RT_LIBS="${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${REGEX} ${LIBM} ${SYSV} ${OPENNURBS}"
ICV_LIBS="{BN} ${BN_LIBS} ${BU} ${BU_LIBS}"
GCV_LIBS="${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS}"
GED_LIBS="${WDB} ${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${PNG} ${TCL} ${REGEX} ${ANALYZE}"
WDB_LIBS="${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS}"
FB_LIBS="${BU} ${BU_LIBS} ${PKG} ${PKG_LIBS} ${TCL}" # additional/optional FB_LIBS: ${TK} ${X_LIBS} ${LIBGL}
OPTICAL_LIBS="${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS} ${TCL}"
MULTISPECTRAL_LIBS="${OPTICAL} ${OPTICAL_LIBS} ${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS}"
PC_LIBS="${LIBSTDCXX} ${BU} ${BU_LIBS}"
if test "x$build_against_x11" = "xno" ; then
    if test "x$bc_build_aquatk" = "xyes" ; then
       DM_LIBS="${GED} ${GED_LIBS} ${RT} ${RT_LIBS} ${FB} ${FB_LIBS} ${PNG} ${FRAMEWORK_CARBON} ${TK}"
       TCLCAD_LIBS="${GED} ${GED_LIBS} ${DM} ${DM_LIBS} ${FB} ${FB_LIBS} ${ITCL} ${ITK} ${TCLSTUB} ${TCL} ${TK}"
    else
       DM_LIBS="${GED} ${GED_LIBS} ${RT} ${RT_LIBS} ${FB} ${FB_LIBS} ${PNG} ${FRAMEWORK_CARBON}"
       TCLCAD_LIBS="${GED} ${GED_LIBS} ${DM} ${DM_LIBS} ${FB} ${FB_LIBS} ${ITCL} ${TCLSTUB} ${TCL}"
    fi
else
    DM_LIBS="${GED} ${GED_LIBS} ${RT} ${RT_LIBS} ${FB} ${FB_LIBS} ${PNG} ${TK} ${X_LIBS} ${FRAMEWORK_CARBON}"
    TCLCAD_LIBS="${GED} ${GED_LIBS} ${DM} ${DM_LIBS} ${FB} ${FB_LIBS} ${ITK} ${ITCL} ${TK} ${TCLSTUB} ${TCL}"
fi
BRLCAD_LIBS="${GED} ${GED_LIBS} ${WDB} ${WDB_LIBS} ${RT} ${RT_LIBS} ${BN} ${BN_LIBS} ${BU} ${BU_LIBS}"

AC_SUBST(ANALYZE)
AC_SUBST(BN)
AC_SUBST(BU)
AC_SUBST(CURSOR)
AC_SUBST(DM)
AC_SUBST(FB)
AC_SUBST(FFT)
AC_SUBST(ICV)
AC_SUBST(GCV)
AC_SUBST(GED)
AC_SUBST(MULTISPECTRAL)
AC_SUBST(OPTICAL)
AC_SUBST(ORLE)
AC_SUBST(PKG)
AC_SUBST(PC)
AC_SUBST(RENDER)
AC_SUBST(RT)
AC_SUBST(SYSV)
AC_SUBST(TCLCAD)
AC_SUBST(TERMIO)
AC_SUBST(TIE)
AC_SUBST(WDB)
AC_SUBST(BRLCAD)

AC_SUBST(STEPCORE)
AC_SUBST(STEPDAI)
AC_SUBST(STEPEDITOR)
AC_SUBST(STEPUTILS)
AC_SUBST(STEPEXPRESS)
AC_SUBST(STEPEXPPP)
AC_SUBST(STEP_LIBS)
AC_SUBST(STEP_FEDEX)
AC_SUBST(STEP_FEDEX_PLUS)
AC_SUBST(STEP_EXPPP)
AC_SUBST(STEP_AP203)

AC_SUBST(ANALYZE_LIBS)
AC_SUBST(BN_LIBS)
AC_SUBST(BU_LIBS)
AC_SUBST(CURSOR_LIBS)
AC_SUBST(DM_LIBS)
AC_SUBST(FB_LIBS)
AC_SUBST(FFT_LIBS)
AC_SUBST(ICV_LIBS)
AC_SUBST(GCV_LIBS)
AC_SUBST(GED_LIBS)
AC_SUBST(MULTISPECTRAL_LIBS)
AC_SUBST(OPTICAL_LIBS)
AC_SUBST(ORLE_LIBS)
AC_SUBST(PC_LIBS)
AC_SUBST(PKG_LIBS)
AC_SUBST(RT_LIBS)
AC_SUBST(SYSV_LIBS)
AC_SUBST(TCLCAD_LIBS)
AC_SUBST(TERMIO_LIBS)
AC_SUBST(WDB_LIBS)
AC_SUBST(BRLCAD_LIBS)


AM_CONDITIONAL(HAVE_DOXYGEN, false)

dnl sample applications
SAMPLE_APPLICATIONS_DIR='${BRLCAD_DATA}/sample_applications'
AC_SUBST(SAMPLE_APPLICATIONS_DIR)

# stash the top builddir before configuring subdirectories
bc_top_builddir="$ac_top_builddir"

# stash the current configuration args before adding additional ones
# for subconfigure
bc_configure_args="$ac_configure_args"

dnl the way to turn on debug for tcl/tk configure
if test "x$bc_use_debug" != "xno" ; then
   ac_configure_args="$ac_configure_args --enable-symbols"
fi

dnl configure Tcl
if test "x$build_brlcad_tcl" = "xyes" ; then
    TCL_PATH="$PWD/${ac_top_builddir}/src/other/tcl/unix"
    ac_configure_args="$ac_configure_args --with-tcl=\"$TCL_PATH\""
    if test "x$bc_build_64bit" = "xyes" ; then
	ac_configure_args="$ac_configure_args --enable-64bit"
    fi
    AC_CONFIG_SUBDIRS([src/other/tcl/unix])
    TCL_PATH="`echo ${TCL_PATH} | sed 's/\\/\\//\\//g;s/\\//\\\\\\//g'`"
fi
AC_SUBST(TCL_PATH)

dnl configure Tk
if test "x$build_brlcad_tk" = "xyes" ; then
    if test "x$bc_build_aquatk" = "xyes" ; then
	ac_configure_args="$ac_configure_args --enable-aqua --disable-xft"
    fi
    TK_PATH="$PWD/${ac_top_builddir}/src/other/tk/unix"
    ac_configure_args="$ac_configure_args --with-tk=\"$TK_PATH\""
    AC_CONFIG_SUBDIRS([src/other/tk/unix])
    TK_PATH="`echo ${TK_PATH} | sed 's/\\/\\//\\//g;s/\\//\\\\\\//g'`"
fi
AC_SUBST(TK_PATH)

dnl configure libpng
if test "x$build_brlcad_png" = "xyes" ; then
   if test "x$LIBZ_CPPFLAGS" != "x" ; then
	LIBZ_PATH="$PWD/${ac_top_builddir}/src/other/libz"
	ac_configure_args="$ac_configure_args --with-sub-libs=\"${LIBZ_PATH}/libz.la\" --with-sub-cppflags=\"-I${LIBZ_PATH}\" --with-sub-cflags=\"$CFLAGS\" --with-sub-ldflags=\"$LDFLAGS\""
    else
	ac_configure_args="$ac_configure_args --with-sub-libs='$LIBZLIB' --with-sub-cflags=\"$CFLAGS\" --with-sub-ldflags=\"$LDFLAGS\""
   fi
   AC_CONFIG_SUBDIRS([src/other/libpng])
fi

dnl configure enigma
AC_CONFIG_SUBDIRS([misc/enigma])

dnl configure step
if test "x$build_brlcad_step" = "xyes" ; then
    AC_CONFIG_SUBDIRS([src/other/step])
fi

dnl configure tkhtml
if test "x$build_brlcad_tkhtml" = "xyes" ; then
    ac_configure_args="$ac_configure_args --exec-prefix=\"$bc_prefix\""
    # this is a hack to all tkhtml to see when the tk headers are in odd
    # places. it seems to ignore the CPPFLAGS in it's configure phase.
    if test ! "x$TKINCDIR" = "x" ; then
	    ac_configure_args="$ac_configure_args --with-tkinclude=$TKINCDIR"
    fi
    AC_CONFIG_SUBDIRS([src/other/tkhtml])
fi

dnl configure tktable
if test "x$build_brlcad_tktable" = "xyes" ; then
    ac_configure_args="$ac_configure_args --exec-prefix=\"$bc_prefix\""
    if test ! "x$TKINCDIR" = "x" ; then
	    ac_configure_args="$ac_configure_args --with-tkinclude=$TKINCDIR"
    fi
    AC_CONFIG_SUBDIRS([src/other/tktable])
fi

AC_CONFIG_FILES([
	Makefile
	bench/Makefile
	regress/Makefile
	regress/mged/Makefile
	db/Makefile
	db/include/Makefile
	doc/Makefile
	doc/docbook/Makefile
	doc/docbook/fop.xconf
	doc/html/Makefile
	doc/html/ReleaseNotes/Makefile
	doc/html/ReleaseNotes/Rel5.0/Makefile
	doc/html/ReleaseNotes/Rel5.0/Summary/Makefile
	doc/html/ReleaseNotes/Rel6.0/Makefile
	doc/html/manuals/Anim_Tutorial/Makefile
	doc/html/manuals/Makefile
	doc/html/manuals/cadwidgets/Makefile
	doc/html/manuals/libbu/Makefile
	doc/html/manuals/libdm/Makefile
	doc/html/manuals/librt/Makefile
	doc/html/manuals/mged/Makefile
	doc/html/manuals/mged/animmate/Makefile
	doc/html/manuals/shaders/Makefile
	doc/legal/Makefile
	doc/pad_file.xml
	include/Makefile
	include/conf/Makefile
	m4/Makefile
	misc/Makefile
	misc/brlcad-config
	misc/brlcad.spec
	misc/macosx/Makefile
	misc/macosx/Resources/Makefile
	misc/macosx/Resources/ReadMe.rtfd/Makefile
	misc/macosx/Resources/Welcome.rtfd/Makefile
	misc/pkgconfig/Makefile
	misc/pkgconfig/libanalyze.pc
	misc/pkgconfig/libbn.pc
	misc/pkgconfig/libbrlcad.pc
	misc/pkgconfig/libbu.pc
	misc/pkgconfig/libdm.pc
	misc/pkgconfig/libfb.pc
	misc/pkgconfig/libfft.pc
	misc/pkgconfig/libged.pc
	misc/pkgconfig/libgcv.pc
	misc/pkgconfig/libicv.pc
	misc/pkgconfig/libmultispectral.pc
	misc/pkgconfig/liboptical.pc
	misc/pkgconfig/libpc.pc
	misc/pkgconfig/libpkg.pc
	misc/pkgconfig/librt.pc
	misc/pkgconfig/libwdb.pc
	pix/Makefile
	sh/Makefile
	src/Makefile
	src/adrt/Makefile
	src/anim/Makefile
	src/archer/Makefile
	src/archer/plugins/Commands/Makefile
	src/archer/plugins/Core/Makefile
	src/archer/plugins/Makefile
	src/archer/plugins/Utility/Makefile
	src/archer/plugins/Utility/attrGroupsDisplayUtilityP/Makefile
	src/archer/plugins/Utility/botUtilityP/Makefile
	src/archer/plugins/Wizards/Makefile
	src/archer/plugins/Wizards/humanwizard/Makefile
	src/archer/plugins/Wizards/tankwizard/Makefile
	src/archer/plugins/Wizards/tankwizard/images/Makefile
	src/archer/plugins/Wizards/tirewizard/Makefile
	src/brlman/Makefile
	src/brlman/brlman.sh
	src/burst/Makefile
	src/bwish/Makefile
	src/canon/Makefile
	src/conv/Makefile
	src/conv/iges/Makefile
	src/conv/intaval/Makefile
	src/conv/step/Makefile
	src/external/Makefile
	src/external/Cubit/Makefile
	src/external/ProEngineer/Makefile
	src/external/ProEngineer/mk
	src/external/ProEngineer/protk.dat
	src/external/Unigraphics/Makefile
	src/fb/Makefile
	src/fbed/Makefile
	src/fbserv/Makefile
	src/gtools/Makefile
	src/gtools/beset/Makefile
	src/halftone/Makefile
	src/irprep/Makefile
	src/lgt/Makefile
	src/libanalyze/Makefile
	src/libbn/Makefile
	src/libbu/Makefile
	src/libcursor/Makefile
	src/libdm/Makefile
	src/libfb/Makefile
	src/libfft/Makefile
	src/libgcv/Makefile
	src/libgcv/wfobj/Makefile
	src/libged/Makefile
	src/libicv/Makefile
	src/libmultispectral/Makefile
	src/liboptical/Makefile
	src/liborle/Makefile
	src/libpc/Makefile
	src/libpkg/Makefile
	src/librt/Makefile
	src/librtserver/Makefile
	src/libsysv/Makefile
	src/libtclcad/Makefile
	src/libtermio/Makefile
	src/libwdb/Makefile
	src/mged/Makefile
	src/mged/points/Makefile
	src/nirt/Makefile
	src/nirt/sfiles/Makefile
	src/other/Makefile
	src/other/URToolkit/Makefile
	src/other/URToolkit/cnv/Makefile
	src/other/URToolkit/cnv/rletoabA62/Makefile
	src/other/URToolkit/cnv/rletogif/Makefile
	src/other/URToolkit/man/Makefile
	src/other/URToolkit/tools/Makefile
	src/other/URToolkit/tools/clock/Makefile
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
	src/other/jove/teach-jove
	src/other/libregex/Makefile
	src/other/libtermlib/Makefile
	src/other/libutahrle/Makefile
	src/other/libutahrle/include/Makefile
	src/other/libz/Makefile
	src/other/openNURBS/Makefile
	src/other/tcl/Makefile
	src/other/tk/Makefile
	src/other/tkpng/Makefile
	src/other/tkpng/pkgIndex.tcl
	src/other/tnt/Makefile
	src/proc-db/Makefile
	src/remrt/Makefile
	src/rt/Makefile
	src/rttherm/Makefile
	src/shapes/Makefile
	src/sig/Makefile
	src/tab/Makefile
	src/tclscripts/Makefile
	src/tclscripts/archer/Makefile
	src/tclscripts/archer/images/Makefile
	src/tclscripts/boteditor/Makefile
	src/tclscripts/geometree/Makefile
	src/tclscripts/hv3/Makefile
	src/tclscripts/lib/Makefile
	src/tclscripts/mged/Makefile
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
	src/tclscripts/sdialogs/Makefile
	src/tclscripts/sdialogs/scripts/Makefile
	src/tclscripts/swidgets/Makefile
	src/tclscripts/swidgets/images/Makefile
	src/tclscripts/swidgets/scripts/Makefile
	src/tclscripts/util/Makefile
	src/util/Makefile
	src/vdeck/Makefile
	src/vfont/Makefile
])

# let the BC_CONFIG_CACHE know we completed successfully (must come before AC_OUTPUT)
BC_CONFIG_COMPLETE

# GENERATE EVERYTHING
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


# AC_OUTPUT won't update the brlcad_config.h.in template timestamp if
# the file is unchanged.  forcibly update the timestamp so autoheader
# doesn't try to invoke.
if test ! "x`cat include/brlcad_config.h.in`" = "x" ; then
    touch include/brlcad_config.h.in
fi


dnl **********************
dnl *** Report Summary ***
dnl **********************

AC_MSG_RESULT([Done.])
AC_MSG_RESULT([])
BC_BOLD
AC_MSG_RESULT([BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE])
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
AC_MSG_RESULT([Build Tcl ............................: $build_brlcad_tcl])
AC_MSG_RESULT([Build Tk .............................: $build_brlcad_tk])
AC_MSG_RESULT([Build Itcl/Itk .......................: $build_brlcad_itcl])
AC_MSG_RESULT([Build IWidgets .......................: $build_brlcad_iwidgets])
AC_MSG_RESULT([Build tkhtml .........................: $build_brlcad_tkhtml])
AC_MSG_RESULT([Build tkpng ..........................: $build_brlcad_tkpng])
AC_MSG_RESULT([Build libpng .........................: $build_brlcad_png])
AC_MSG_RESULT([Build libregex .......................: $build_brlcad_regex])
AC_MSG_RESULT([Build zlib ...........................: $build_brlcad_zlib])
AC_MSG_RESULT([Build termlib ........................: $build_brlcad_termlib])
AC_MSG_RESULT([Build Utah Raster Toolkit.............: $build_brlcad_urt])
AC_MSG_RESULT([Build Template Numerical Toolkit......: $build_brlcad_tnt])
AC_MSG_RESULT([Build openNURBS.......................: $build_brlcad_opennurbs])
AC_MSG_RESULT([Build NIST STEP Class Libraries.......: $build_brlcad_step])
AC_MSG_RESULT([])
AC_MSG_RESULT([X11 support (optional)................: $build_against_x11])
AC_MSG_RESULT([OpenGL support (optional).............: $build_against_opengl])
AC_MSG_RESULT([librtserver JDK support (optional)....: $build_against_java])
AC_MSG_RESULT([Enable run-time debugging (optional)..: $bc_build_runtime_debug])
AC_MSG_RESULT([])
AC_MSG_RESULT([Build 64-bit release .................: $build_brlcad_64bit])
AC_MSG_RESULT([Build optimized release ..............: $bc_use_optimized])
AC_MSG_RESULT([Build debug release ..................: $bc_use_debug])
AC_MSG_RESULT([Build profile release ................: $bc_use_profiling])
AC_MSG_RESULT([Build SMP-capable release ............: $bc_build_parallel])
AC_MSG_RESULT([Build static libraries ...............: $enable_static])
AC_MSG_RESULT([Build shared/dynamic libraries .......: $enable_shared])
AC_MSG_RESULT([Print verbose compilation warnings ...: $bc_build_warnings])
AC_MSG_RESULT([Print verbose compilation progress ...: $bc_build_progress])
AC_MSG_RESULT([])
AC_MSG_RESULT([Only build benchmark suite ...........: $bc_only_benchmark])
AC_MSG_RESULT([Only build librtserver ...............: $bc_only_rts])
AC_MSG_RESULT([Install example geometry models ......: $bc_install_geometry])
AC_MSG_RESULT([Install extra docs ...................: $docbook_processing])
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
