dnl                    C O N F I G U R E . A C
dnl BRL-CAD
dnl
dnl Copyright (c) 2004-2007 United States Government as represented by
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

dnl minimum version of autoconf required.  should coincide with
dnl setting in autogen.sh script.
AC_PREREQ(2.52)

AC_INIT(BRL-CAD, 7.9.0, [http://brlcad.org], brlcad)
AC_REVISION($Revision$)

AC_CONFIG_AUX_DIR(misc)

dnl See HACKING for details on how to properly update the version
MAJOR_VERSION=7
MINOR_VERSION=9
PATCH_VERSION=0
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(PATCH_VERSION)

dnl just in case
PACKAGE_NAME="BRL-CAD"
AC_SUBST(PACKAGE_NAME)

# force locale setting to C so things like date output as expected
LC_ALL=C

# save the command-line in case there's a need to restart
this="$0"
args="$*"

CONFIG_DAY=`date +%d`
CONFIG_MONTH=`date +%m`
CONFIG_YEAR=`date +%Y`
CONFIG_DATE="${CONFIG_YEAR}${CONFIG_MONTH}${CONFIG_DAY}"
CONFIG_TIME="`date '+%H %M %S'`"
BRLCAD_VERSION="$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION"
AC_DEFINE_UNQUOTED([BRLCAD_VERSION], "$BRLCAD_VERSION", "Version of BRL-CAD")
AC_SUBST(CONFIG_DAY)
AC_SUBST(CONFIG_MONTH)
AC_SUBST(CONFIG_YEAR)
AC_SUBST(CONFIG_DATE)
AC_SUBST(CONFIG_TIME)
AC_SUBST(BRLCAD_VERSION)

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
fi

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
	bc_prefix="$ac_default_prefix"
else
	# allow a complete override
	bc_prefix="$prefix"
fi
AC_DEFINE_UNQUOTED([BRLCAD_ROOT], "${bc_prefix}", "Location BRL-CAD will install to")
AC_MSG_RESULT($bc_prefix)
if test ! "x$BRLCAD_ROOT" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([BRLCAD_ROOT should only be used to override an install directory at runtime])
	AC_MSG_WARN([BRLCAD_ROOT is presently set to "${BRLCAD_ROOT}"])
	AC_MSG_NOTICE([It is highly recommended that BRLCAD_ROOT be unset and not used])
	if test "x$BRLCAD_ROOT" = "x$bc_prefix" ; then
		AC_MSG_WARN([BRLCAD_ROOT is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable BRLCAD_ROOT conflicts with --prefix ***])
	fi
else
	BRLCAD_ROOT="${bc_prefix}"
fi
AC_SUBST(BRLCAD_ROOT)

# set up our default shared/non-shared data install directories
#         e.g. /usr/local/share/brlcad/7.4.0
AC_MSG_CHECKING([where BRL-CAD resources are to be installed])
if test "x$datadir" = "x\${prefix}/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${BRLCAD_VERSION}"
elif test "x$datadir" = "x${prefix}/share" ; then
	bc_data_dir="${bc_prefix}/share/brlcad/${BRLCAD_VERSION}"
else
	# allow a complete override
	bc_data_dir="${datadir}"
fi
AC_DEFINE_UNQUOTED([BRLCAD_DATA], "${bc_data_dir}", "Location BRL-CAD resources will install to")
AC_MSG_RESULT($bc_data_dir)
if test ! "x$BRLCAD_DATA" = "x" ; then
	AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	AC_MSG_WARN([BRLCAD_DATA should only be used to override an install directory at runtime])
	AC_MSG_WARN([BRLCAD_DATA is presently set to "${BRLCAD_DATA}"])
	AC_MSG_NOTICE([It is highly recommended that BRLCAD_DATA be unset and not used])
	if test "x$BRLCAD_DATA" = "x$bc_data_dir" ; then
		AC_MSG_WARN([BRLCAD_DATA is not necessary and may cause unexpected behavior])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	else
		AC_MSG_ERROR([*** Environment variable BRLCAD_DATA conflicts with --prefix or --datadir ***])
	fi
else
	BRLCAD_DATA="${bc_data_dir}"
fi
AC_SUBST(BRLCAD_DATA)

# if this is not a CVS checkout, disable dependency tracking for a
# faster build. it's not likely that the user is doing development
# from a source release. at least they really should not given the
# extra steps that are necessary for making a proper useful diff.
AC_MSG_CHECKING([whether dependency tracking should be enabled])
if test -d $srcdir/CVS ; then
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

# a secondary automated automake may blow away COPYING & INSTALL due
# to an automake bug in older versions, so restore those files if
# needed.
BC_RESTORE_CLOBBERED([COPYING INSTALL], [misc])

# automatically enable and load our configure cache file if available
BC_CONFIG_CACHE([config.cache.${host_os}.${ac_hostname}])


dnl ***********************
dnl *** Check arguments ***
dnl ***********************

BC_CONFIGURE_STAGE([arguments], [1 of 9])

dnl *** enable options ***

# enable everything
BC_ARG_ENABLE([everything], [almost-everything], [Turn on compilation of almost everything], [auto])

# automatic flag updates
BC_ARG_ENABLE([auto_flags], [automatic-flags], [Automatically configure build flags], [yes])

# only build benchmark
BC_ARG_ENABLE([only_benchmark], [only-benchmark], [Only build the components necessary for the benchmark], [no])

# only build librtserver
BC_ARG_ENABLE([only_rts], [only-rtserver], [Only build the components necessary for the raytrace server], [no])

# run-time debugging
BC_ARG_ENABLE([build_runtime_debug], [runtime-debug], [Enable run-time debug checking], [yes])

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

# force enable/disable building of termlib
BC_ARG_ENABLE([build_termlib], [termlib-build], [Build the included termlib library], [auto])

# force enable/disable building of Tcl
BC_ARG_ENABLE([build_tcl], [tcl-build], [Build the included Tcl], [auto])

# force enable/disable building of Tk
BC_ARG_ENABLE([build_tk], [tk-build], [Build the included Tk], [auto])

# force enable/disable building of incrTcl
BC_ARG_ENABLE([build_itcl], [itcl-build], [Build the included incrTcl], [auto])

# force enable/disable building of iwidgets
BC_ARG_ENABLE([build_iwidgets], [iwidgets-build], [Install the included iwidgets], [auto])

# force enable/disable building of blt
BC_ARG_ENABLE([build_blt], [blt-build], [Build the included BLT], [auto])

# tkimg
BC_ARG_ENABLE([build_tkimg], [tkimg-build], [Build the included tkImg], [auto])

# jove
BC_ARG_ENABLE([build_jove], [jove-build], [Enable compilation of jove], [auto])

# endgame framework simulation module
BC_ARG_ENABLE([build_ef], [ef-build], [Enable compilation of Endgame Framework module], [no])

# pro/e plugin
BC_ARG_ENABLE([build_proe], [proe-plugin-build], [Enable compilation of Pro/Engineer plugin], [no])

# unigraphics plugin
BC_ARG_ENABLE([build_ug], [unigraphics-build], [Enable compilation of Unigraphics importer], [no])

# ADRT (enables SDL and Python with options if they're not enabled)
BC_ARG_ENABLE([build_adrt], [adrt-build], [Enable compilation of ADRT], [auto])

# example geometry
BC_ARG_ENABLE([install_geometry], [models-install], [Enable installation of example geometry models], [yes])

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


dnl *** with options ***

# provides convenience argument handlers for specifying CFLAGS,
# LDFLAGS, CPPFLAGS, and LIBS using --with argument handlers.
BC_WITH_FLAG_ARGS

# use the jdk
BC_ARG_WITH([with_java], [jdk], [Specify use of or path to the Java Development Kit], [auto])

# use x11
BC_ARG_WITH([with_x11], [x11], [Specify use of or path to X11], [auto])

# use opengl
BC_ARG_WITH([with_opengl], [opengl], [Specify use of or path to OpenGL], [auto])

# use wgl
BC_ARG_WITH([with_wgl], [wgl], [Specify use of or path to WindowsGL], [auto])

# use sdl
BC_ARG_WITH([with_sdl], [sdl], [Specify use of or path to SDL], [auto])

# use python
BC_ARG_WITH([with_python], [python], [Specify use of or path to python], [auto])

###
# argument aliases
# they need to go below here in order for --help to consolidate the
# blank line that it inserts to exactly one line in the right place
###

# aliases for enabling configuration of everything (help uses almost-everything)
BC_ARG_ALIAS([everything], [all])
BC_ARG_ALIAS([everything], [all-build])
BC_ARG_ALIAS([everything], [build-all])
BC_ARG_ALIAS([everything], [all-builds])
BC_ARG_ALIAS([everything], [everything])
BC_ARG_ALIAS([everything], [everything-build])
BC_ARG_ALIAS([everything], [build-everything])

# aliases for allowing configure to modify the flags automatically (help uses automatic-flags)
BC_ARG_ALIAS([auto_flags], [auto-flags])
BC_ARG_ALIAS([auto_flags], [auto-flag])
BC_ARG_ALIAS([auto_flags], [automatic-flag])

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

# 64-bit compilation (help uses 64bit-build)
BC_ARG_ALIAS([build_64bit], [64])
BC_ARG_ALIAS([build_64bit], [64bit])
BC_ARG_ALIAS([build_64bit], [64-bit])
BC_ARG_ALIAS([build_64bit], [64-build])
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
BC_ARG_ALIAS([build_urt], [rle])
BC_ARG_ALIAS([build_urt], [utah])
BC_ARG_ALIAS([build_urt], [utahrle])
BC_ARG_ALIAS([build_urt], [libutah])
BC_ARG_ALIAS([build_urt], [librle])
BC_ARG_ALIAS([build_urt], [libutahrle])
BC_ARG_ALIAS([build_urt], [utahrastertoolkit])
BC_ARG_ALIAS([build_urt], [utah-raster-toolkit])
BC_ARG_ALIAS([build_urt], [urtoolkit-build])
BC_ARG_ALIAS([build_urt], [rle-build])
BC_ARG_ALIAS([build_urt], [utah-build])
BC_ARG_ALIAS([build_urt], [utahrle-build])
BC_ARG_ALIAS([build_urt], [libutah-build])
BC_ARG_ALIAS([build_urt], [librle-build])
BC_ARG_ALIAS([build_urt], [libutahrle-build])
BC_ARG_ALIAS([build_urt], [utahrastertoolkit-build])
BC_ARG_ALIAS([build_urt], [utah-raster-toolkit-build])

# aliases to force building openNURBS (help uses opennurbs-build)
BC_ARG_ALIAS([build_opennurbs], [libopennurbs])
BC_ARG_ALIAS([build_opennurbs], [libopenurbs])
BC_ARG_ALIAS([build_opennurbs], [libnurbs])
BC_ARG_ALIAS([build_opennurbs], [open-nurbs])
BC_ARG_ALIAS([build_opennurbs], [openurbs])
BC_ARG_ALIAS([build_opennurbs], [nurbs])
BC_ARG_ALIAS([build_opennurbs], [libopennurbs-build])
BC_ARG_ALIAS([build_opennurbs], [libopenurbs-build])
BC_ARG_ALIAS([build_opennurbs], [libnurbs-build])
BC_ARG_ALIAS([build_opennurbs], [open-nurbs-build])
BC_ARG_ALIAS([build_opennurbs], [openurbs-build])
BC_ARG_ALIAS([build_opennurbs], [nurbs-build])

# aliases to force building tcl (help uses tcl-build)
BC_ARG_ALIAS([build_tcl], [tcl])
BC_ARG_ALIAS([build_tcl], [libtcl])
BC_ARG_ALIAS([build_tcl], [libtcl-build])

# aliases to force building tk (help uses tk-build)
BC_ARG_ALIAS([build_tk], [tk])
BC_ARG_ALIAS([build_tk], [libtk])
BC_ARG_ALIAS([build_tk], [libtk-build])

# aliases to force building itcl (help uses itcl-build)
BC_ARG_ALIAS([build_itcl], [itcl])
BC_ARG_ALIAS([build_itcl], [itk])
BC_ARG_ALIAS([build_itcl], [libitcl])
BC_ARG_ALIAS([build_itcl], [libitk])
BC_ARG_ALIAS([build_itcl], [incrtcl])
BC_ARG_ALIAS([build_itcl], [itk-build])
BC_ARG_ALIAS([build_itcl], [libitcl-build])
BC_ARG_ALIAS([build_itcl], [libitk-build])
BC_ARG_ALIAS([build_itcl], [incrtcl-build])

# aliases to force building iwidgets (help uses iwidgets-build)
BC_ARG_ALIAS([build_iwidgets], [iwidgets])
BC_ARG_ALIAS([build_iwidgets], [install-iwidgets])

# aliases to force building blt (help uses blt-build)
BC_ARG_ALIAS([build_blt], [blt])
BC_ARG_ALIAS([build_blt], [libblt])
BC_ARG_ALIAS([build_blt], [libblt-build])

# aliases to force building tkimg (help uses tkimg-build)
BC_ARG_ALIAS([build_tkimg], [tkimg])
BC_ARG_ALIAS([build_tkimg], [libtkimg])
BC_ARG_ALIAS([build_tkimg], [libtkimg-build])

# aliases to force building termlib (help uses termlib-build)
BC_ARG_ALIAS([build_termlib], [termlib])
BC_ARG_ALIAS([build_termlib], [termcap])
BC_ARG_ALIAS([build_termlib], [libtermlib])
BC_ARG_ALIAS([build_termlib], [libtermcap])
BC_ARG_ALIAS([build_termlib], [termcap-build])
BC_ARG_ALIAS([build_termlib], [libtermlib-build])
BC_ARG_ALIAS([build_termlib], [libtermcap-build])

# jove aliases (help uses jove-build)
BC_ARG_ALIAS([build_jove], [jove])

# endgame framework module aliases (help uses ef-build)
BC_ARG_ALIAS([build_ef], [ef])
BC_ARG_ALIAS([build_ef], [endgame])
BC_ARG_ALIAS([build_ef], [endgameframework])
BC_ARG_ALIAS([build_ef], [endgame-framework])
BC_ARG_ALIAS([build_ef], [endgame-build])
BC_ARG_ALIAS([build_ef], [endgameframework-build])
BC_ARG_ALIAS([build_ef], [endgame-framework-build])

# pro/e plugin aliases (help uses proe-plugin-build)
BC_ARG_ALIAS([build_proe], [proe])
BC_ARG_ALIAS([build_proe], [proe-build])
BC_ARG_ALIAS([build_proe], [proe-plugin])
BC_ARG_ALIAS([build_proe], [proengineer])
BC_ARG_ALIAS([build_proe], [proengineer-plugin])
BC_ARG_ALIAS([build_proe], [pro-engineer])
BC_ARG_ALIAS([build_proe], [pro-engineer-plugin])
BC_ARG_ALIAS([build_proe], [proengineer-build])
BC_ARG_ALIAS([build_proe], [proengineer-plugin-build])
BC_ARG_ALIAS([build_proe], [pro-engineer-build])
BC_ARG_ALIAS([build_proe], [pro-engineer-plugin-build])

# unigraphics (NX) importer aliases (help uses unigraphics-build)
BC_ARG_ALIAS([build_ug], [ug])
BC_ARG_ALIAS([build_ug], [ug-build])
BC_ARG_ALIAS([build_ug], [unigraphics])
BC_ARG_ALIAS([build_ug], [unigraphics-importer-build])
BC_ARG_ALIAS([build_ug], [ug-importer])
BC_ARG_ALIAS([build_ug], [ug-importer-build])
BC_ARG_ALIAS([build_ug], [nx])
BC_ARG_ALIAS([build_ug], [nx-build])
BC_ARG_ALIAS([build_ug], [nx-importer])
BC_ARG_ALIAS([build_ug], [nx-importer-build])

# ADRT (help uses adrt-build)
BC_ARG_ALIAS([build_adrt], [adrt])

# install geometry aliases (help uses models-install)
BC_ARG_ALIAS([install_geometry], [models])
BC_ARG_ALIAS([install_geometry], [install-models])
BC_ARG_ALIAS([install_geometry], [geometry-install])
BC_ARG_ALIAS([install_geometry], [geometry])
BC_ARG_ALIAS([install_geometry], [install-geometry])
BC_ARG_ALIAS([install_geometry], [example-models-install])
BC_ARG_ALIAS([install_geometry], [example-models])
BC_ARG_ALIAS([install_geometry], [install-example-models])
BC_ARG_ALIAS([install_geometry], [example-geometry-install])
BC_ARG_ALIAS([install_geometry], [example-geometry])
BC_ARG_ALIAS([install_geometry], [install-example-geometry])

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
BC_ARG_ALIAS([build_warnings], [warnings-verbose])
BC_ARG_ALIAS([build_warnings], [verbose-warnings])
BC_ARG_ALIAS([build_warnings], [build-warnings])
BC_ARG_ALIAS([build_warnings], [warnings-build])

# verbose compilation progress status (help uses progress)
BC_ARG_ALIAS([build_progress], [verbose-progress])
BC_ARG_ALIAS([build_progress], [progress-verbose])
BC_ARG_ALIAS([build_progress], [build-progress])
BC_ARG_ALIAS([build_progress], [progress-build])

# with java aliases (help uses jdk)
BC_ARG_WITH_ALIAS([with_java], [java])

# with x11 aliases (help uses x11)
BC_ARG_WITH_ALIAS([with_x11], [x])

# with opengl aliases (help uses opengl)
BC_ARG_WITH_ALIAS([with_opengl], [ogl])
BC_ARG_WITH_ALIAS([with_opengl], [gl])

# with wgl aliases (help uses wgl)
BC_ARG_WITH_ALIAS([with_wgl], [windowsgl])

# with sdl aliases (help uses sdl)
BC_ARG_WITH_ALIAS([with_sdl], [libsdl])

# with python aliases (help uses python)
BC_ARG_WITH_ALIAS([with_python], [py])


###
# set up path searching
###

dnl *** X11 ***
if test "x$bc_with_x11_val" != "x" ; then
	BC_RETRY_DIRECTORY([$bc_with_x11_val])
elif test "x$bc_with_x11" = "xyes" -o "x$bc_with_x11" = "xauto" ; then
	# search /usr/X11R6 for X11
	BC_SEARCH_DIRECTORY([/usr/X11R6], [X11])

	# if we hit a wall, retry configure with these
	BC_RETRY_DIRECTORY([/opt/X11R6], [X11])
	BC_RETRY_DIRECTORY([/opt/X11], [X11])
	BC_RETRY_DIRECTORY([/usr/X11], [X11])
fi
AC_PATH_X
AC_PATH_XTRA

dnl automatically scan /usr/local (e.g. BSD uses /usr/local for ports)
BC_SEARCH_DIRECTORY([/usr/local])

dnl search several additional locations if configure happens to fail
dnl to find a required component
BC_RETRY_DIRECTORY([/usr/freeware], [freeware])
BC_RETRY_DIRECTORY([/opt])
BC_RETRY_DIRECTORY([/sw], [fink])


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
	if test "x$bc_build_runtime_debug" = "x$bc_build_runtime_debug_default" ; then
		bc_build_runtime_debug="$set_everything"
	fi
	if test "x$bc_build_regex" = "x$bc_build_regex_default" ; then
		bc_build_regex="$set_everything"
	fi
	if test "x$bc_build_png" = "x$bc_build_png_default" ; then
		bc_build_png="$set_everything"
	fi
	if test "x$bc_build_zlib" = "x$bc_build_zlib_default" ; then
		bc_build_zlib="$set_everything"
	fi
	if test "x$bc_build_urt" = "x$bc_build_urt_default" ; then
		bc_build_urt="$set_everything"
	fi
	if test "x$bc_build_opennurbs" = "x$bc_build_opennurbs_default" ; then
		bc_build_opennurbs="$set_everything"
	fi
	if test "x$bc_build_termlib" = "x$bc_build_termlib_default" ; then
		bc_build_termlib="$set_everything"
	fi
	if test "x$bc_build_tcl" = "x$bc_build_tcl_default" ; then
		bc_build_tcl="$set_everything"
	fi
	if test "x$bc_build_tk" = "x$bc_build_tk_default" ; then
		bc_build_tk="$set_everything"
	fi
	if test "x$bc_build_itcl" = "x$bc_build_itcl_default" ; then
		bc_build_itcl="$set_everything"
	fi
	if test "x$bc_build_iwidgets" = "x$bc_build_iwidgets_default" ; then
		bc_build_iwidgets="$set_everything"
	fi
	if test "x$bc_build_blt" = "x$bc_build_blt_default" ; then
		bc_build_blt="$set_everything"
	fi
	if test "x$bc_build_tkimg" = "x$bc_build_tkimg_default" ; then
		bc_build_tkimg="$set_everything"
	fi
	if test "x$bc_build_jove" = "x$bc_build_jove_default" ; then
		bc_build_jove="$set_everything"
	fi
	# Singe the Endgame Framework "simulation module" will only
	# compile if you have it installed, don't enable it even if
	# requesting everything.  fine to disable, though.
	if test "x$bc_build_ef" = "x$bc_build_ef_default" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_ef=no
		fi
	fi
	# Since the Pro/E plugin is primarily tested on only one
	# platform and requires Pro/Toolkit, don't enable it even if
	# requesting everything.  fine to disable, though.
	if test "x$bc_build_proe" = "x$bc_build_proe_default" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_proe=no
		fi
	fi
	# Since the Unigraphics importer is primarily tested on only one
	# platform and requires UGOPEN, don't enable it even if
	# requesting everything.  fine to disable, though.
	if test "x$bc_build_ug" = "x$bc_build_ug_default" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_ug=no
		fi
	fi
	# ADRT is known to not build right now on most platforms, so
	# only allow it to be disabled and not enabled.
	# User must specify --enable-adrt to get ADRT to build
	if test "x$bc_build_adrt" = "x$bc_build_adrt_default" ; then
		if test "x$set_everything" = "xno" ; then
			bc_build_adrt="no"
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
	fi
	if test "x$bc_everything" = "xyes" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Requesting ONLY benchmark and EVERYTHING?])
		AC_MSG_WARN([Building everything])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		bc_only_benchmark=no
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
	fi
fi

# enabling/disabling ADRT implies enabling/disabling the SDL and Python options
if test "x$bc_build_adrt" = "xyes" ; then
	if test "x$bc_with_sdl" = "xauto" ; then
		bc_with_sdl=yes
	elif test "x$bc_with_sdl" = "xno" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([ADRT presently requires SDL support])
		AC_MSG_WARN([The SDL option has been enabled (--with-sdl=yes)])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		bc_with_sdl=yes
	fi
	if test "x$bc_with_python" = "xauto" ; then
		bc_with_python=yes
	elif test "x$bc_with_python" = "xno" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([ADRT presently requires Python support])
		AC_MSG_WARN([The Python option has been enabled (--with-python=yes)])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
		bc_with_python=yes
	fi
elif test "x$bc_build_adrt" = "xno" ; then
	if test "x$bc_with_sdl" = "xyes" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([SDL was requested, yet ADRT was disabled.])
		AC_MSG_WARN([Since only ADRT utilizes SDL, it will not be utilized (--with-sdl=no)])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	fi
	if test "x$bc_with_python" = "xyes" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Python was requested, yet ADRT was disabled.])
		AC_MSG_WARN([Since only ADRT utilizes Python, it will not be utilized (--with-python=no)])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	fi
	bc_with_sdl=no
	bc_with_python=no
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

AC_MSG_CHECKING(whether to automatically set build flags)
AC_MSG_RESULT($bc_auto_flags)

AC_MSG_CHECKING(whether to only build benchmark)
AC_MSG_RESULT($bc_only_benchmark)

AC_MSG_CHECKING(whether to only build the raytrace server)
AC_MSG_RESULT($bc_only_rts)

AC_MSG_CHECKING(whether to use run-time debug checks)
AC_MSG_RESULT($bc_build_runtime_debug)

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

AC_MSG_CHECKING(whether to build the Tcl library)
AC_MSG_RESULT($bc_build_tcl)

AC_MSG_CHECKING(whether to build the Tk library)
AC_MSG_RESULT($bc_build_tk)

AC_MSG_CHECKING(whether to build the incrTcl library)
AC_MSG_RESULT($bc_build_itcl)

AC_MSG_CHECKING(whether to install the iwidgets library)
AC_MSG_RESULT($bc_build_iwidgets)

AC_MSG_CHECKING(whether to build the BLT library)
AC_MSG_RESULT($bc_build_blt)

AC_MSG_CHECKING(whether to build the tkImg library)
AC_MSG_RESULT($bc_build_tkimg)

AC_MSG_CHECKING(whether to build the termlib library)
AC_MSG_RESULT($bc_build_termlib)

AC_MSG_CHECKING(whether to compile jove or not)
AC_MSG_RESULT($bc_build_jove)

AC_MSG_CHECKING(whether to compile the Endgame Framework module)
AC_MSG_RESULT($bc_build_ef)

AC_MSG_CHECKING(whether to compile the Pro-Engineer plugin)
AC_MSG_RESULT($bc_build_proe)

AC_MSG_CHECKING(whether to compile the Unigraphics importer)
AC_MSG_RESULT($bc_build_ug)

AC_MSG_CHECKING(whether to compile ADRT)
AC_MSG_RESULT($bc_build_adrt)

AC_MSG_CHECKING(whether to install example geometry databases)
AC_MSG_RESULT($bc_install_geometry)

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

AC_MSG_CHECKING(whether to compile with the Java Development Kit)
AC_MSG_RESULT($bc_with_java)

AC_MSG_CHECKING(whether to compile the X11 interfaces)
AC_MSG_RESULT($bc_with_x11)

AC_MSG_CHECKING(whether to compile the OpenGL interfaces)
AC_MSG_RESULT($bc_with_opengl)

AC_MSG_CHECKING(whether to compile the WindowsGL interfaces)
AC_MSG_RESULT($bc_with_wgl)

AC_MSG_CHECKING(whether to compile against SDL for ADRT)
AC_MSG_RESULT($bc_with_sdl)

AC_MSG_CHECKING(whether to compile against Python for ADRT)
AC_MSG_RESULT($bc_with_python)


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
AM_PROG_CC_STDC

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

# AN_MAKEVAR([AR], [BC_PROG_AR])
# AN_PROGRAM([ar], [BC_PROG_AR])
# AC_DEFUN([BC_PROG_AR], [AC_CHECK_TOOL(AR, ar, :)])
# BC_PROG_AR

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

AC_DEFINE(EXTERN_STDOUT,,[Stdout is extern])

# test for sdl-config, auto for with-sdl is 'no'
SDL_CONFIG=""
if test "x$bc_with_sdl" = "xyes" ; then
	AC_PATH_PROG(SDL_CONFIG, sdl-config, , $PATH)
	if test "x$SDL_CONFIG" = "x" ; then
		if test "x$bc_with_sdl" = "xyes" ; then
			AC_MSG_NOTICE([sdl-config is presently required, searching more extensively])
			BC_RETRY_CONFIGURE([$this], [$args])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_WARN([Unable to find sdl-config])
		fi
	fi
fi

# test for python interpreter, auto for with-python is 'no'
PYTHON=""
if test "x$bc_with_python" = "xyes" ; then
	AM_PATH_PYTHON(2.0,
		       PYTHON_CFLAGS=-I`$PYTHON -c "import distutils.sysconfig;print distutils.sysconfig.get_config_var('INCLUDEPY')"`
		       PYTHON_LIBS=`$PYTHON -c "import distutils.sysconfig; print distutils.sysconfig.get_config_var('LIBPL')+'/'+distutils.sysconfig.get_config_var('LIBRARY')+' '+distutils.sysconfig.get_config_var('LIBS')"`
		       )
fi

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
    dnl check if the BSD c_r threading library link works
    [AC_CHECK_LIB(c_r, pthread_create, thread_link_works=yes ; LIBTHREAD="-pthread",
	dnl try solaris threading
	[AC_CHECK_LIB(thread, thr_create, thread_link_works=yes ; LIBTHREAD="-lthread",
	    dnl pthreading is a good default if none of the others are available
	    dnl XXX need to test HPUX -- may require -lcma with pthreads
	    [AC_CHECK_LIB(pthread, pthread_create, thread_link_works=yes ; LIBTHREAD="-lpthread")]
	)]
    )]
)

dnl check whether we ended up with functioning posix threading.
AC_MSG_CHECKING([whether we are using POSIX threading])
posix_threading=no
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBTHREAD"
AC_TRY_LINK([#include <pthread.h>], [
    pthread_t th;
    pthread_join(th, 0);
    pthread_attr_init(0);
    pthread_cleanup_push(0,0);
    pthread_create(0,0,0,0);
    pthread_cleanup_pop(0);
], [posix_threading=yes])
LDFLAGS="$PRELDFLAGS"
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

dnl check if X11 libraries link
LIBX11=""
LIBXEXT=""
LIBXI=""
ice_link_works=no
sm_link_works=no
xt_link_works=no
xmu_link_works=no
if test "x$bc_with_x11" != "xno" ; then
    AC_CHECK_LIB(X11, XCreateWindow, LIBX11="-lX11")
    AC_CHECK_LIB(Xext, XShapeCombineMask, LIBEXT="-lXext")
    AC_CHECK_LIB(Xi, XGetExtensionVersion, LIBXI="-lXi")
    AC_CHECK_LIB(ICE, IceOpenConnection, ice_link_works=yes)
    AC_CHECK_LIB(SM, SmcOpenConnection, sm_link_works=yes)
    AC_CHECK_LIB(Xt, XtMalloc, xt_link_works=yes)
    AC_CHECK_LIB(Xmu, XmuLookupStandardColormap, xmu_link_works=yes)
fi # bc_with_x11

dnl check if a math library links
m_link_works=no
LIBM=""
AC_CHECK_LIB(m, cos, m_link_works=yes ; LIBM="-lm")

dnl detect a useable system regular expression library
LIBREGEX=""
AC_CHECK_LIB(c, regcomp, LIBREGEX="-lc",
    [AC_CHECK_LIB(regex, regcomp, LIBREGEX="-lregex",
	[AC_CHECK_LIB(compat, regcomp, LIBREGEX="-lcompat")]
    )]
)

dnl check if a png library links
LIBPNG=""
AC_CHECK_LIB(png, png_access_version_number, LIBPNG="-lpng")

dnl check if zlib library links
LIBZLIB=""
AC_CHECK_LIB(z, deflate, LIBZLIB="-lz")

dnl check if utahrle library links
utahrle_link_works=no
UTAHRLE=""
# checking for rle_hdr_init is NOT sufficient to get urt 3.x
PRELIBS="$LIBS"
LIBS="$LIBS $LIBM"
AC_CHECK_LIB(utahrle, rle_getraw, utahrle_link_works=yes ; UTAHRLE="-lutahrle $LIBM",
    [AC_CHECK_LIB(rle, rle_getraw, utahrle_link_works=yes ; UTAHRLE="-lrle $LIBM")]
)
LIBS="$PRELIBS"

dnl check if openNURBS library links
LIBOPENNURBS=""
AC_CHECK_LIB(openNURBS, onmalloc, LIBOPENNURBS="-lopenNURBS")

dnl check if libtcl library links
tcl_link_works=no
LIBTCL=""
AC_CHECK_LIB(tcl8.6, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.6",
    [AC_CHECK_LIB(tcl8.5, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.5",
	[AC_CHECK_LIB(tcl8.4, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.4",
	    [AC_CHECK_LIB(tcl84, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl84",
		[AC_CHECK_LIB(tcl, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl")]
	    )]
	)]
    )]
)

dnl check if libtclstub library links
tclstub_link_works=no
LIBTCLSTUB=""
AC_CHECK_LIB(tclstub8.6, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub8.6",
    [AC_CHECK_LIB(tclstub8.5, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub8.5",
	[AC_CHECK_LIB(tclstub8.4, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub8.4",
	    [AC_CHECK_LIB(tclstub84, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub84",
		[AC_CHECK_LIB(tclstub, Tcl_InitStubs, tclstub_link_works=yes ; LIBTCLSTUB="-ltclstub")]
	    )]
	)]
    )]
)

dnl check if libtk library links
tk_link_works=no
LIBTK=""
AC_CHECK_LIB(tk8.6, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk8.6",
    [AC_CHECK_LIB(tk8.5, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk8.5",
	[AC_CHECK_LIB(tk8.4, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk8.4",
	    [AC_CHECK_LIB(tk, Tk_MainLoop, tk_link_works=yes ; LIBTK="-ltk")]
	)]
    )]
)

dnl check if libtkstub library links
tkstub_link_works=no
LIBTKSTUB=""
PRELIBS="$LIBS"
LIBS="$LIBS $LIBTCLSTUB"
AC_CHECK_LIB(tkstub8.6, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub8.6",
    [AC_CHECK_LIB(tkstub8.5, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub8.5",
	[AC_CHECK_LIB(tkstub8.4, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub8.4",
	    [AC_CHECK_LIB(tkstub84, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub84",
		[AC_CHECK_LIB(tkstub, Tk_InitStubs, tkstub_link_works=yes ; LIBTKSTUB="-ltkstub")]
	    )]
	)]
    )]
)
LIBS="$PRELIBS"

dnl check if libitcl library links
itcl_link_works=no
LIBITCL=""
AC_CHECK_LIB(itcl3.3, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.3",
    [AC_CHECK_LIB(itcl3.2, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.2",
	[AC_CHECK_LIB(itcl32, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl32",
	    [AC_CHECK_LIB(itcl, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl")]
	)]
    )]
)

dnl check if libitk library links
itk_link_works=no
LIBITK=""
AC_CHECK_LIB(itk3.3, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.3",
    [AC_CHECK_LIB(itk3.2, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.2",
	[AC_CHECK_LIB(itk32, Itk_Init, itk_link_works=yes ; LIBITK="-litk32",
	    [AC_CHECK_LIB(itk, Itk_Init, itk_link_works=yes ; LIBITK="-litk")]
	)]
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

dnl check if an OpenGL graphics library links
gl_link_works=no
LIBGL=""
if test "x$bc_with_opengl" != "xno" ; then
    AC_CHECK_LIB(GL, glEnable, gl_link_works=yes ; LIBGL="-lGL",
		 [AC_CHECK_LIB(GL2, glEnable, gl_link_works=yes ; LIBGL="-lGL2")]
    )
fi

dnl check if the Windows WGL graphics library links
wgl_link_works=no
LIBWGL=""
if test "xbc_with_wgl" != "xno" ; then
    AC_CHECK_LIB(opengl32, glEnable, wgl_link_works=yes ; LIBWGL="-lopengl32")
fi

dnl check if the SGI graphics library links
sgigl_link_works=no
LIBSGIGL=""
AC_CHECK_LIB(gl, getvideo, sgigl_link_works=yes ; LIBSGIGL="-lgl")

dnl check if the Solaris lexer library links
l_link_works=no
LIBL=""
AC_CHECK_LIB(l, yyless, l_link_works=yes ; LIBL="-ll")

dnl check for java
JAVA_CFLAGS=
JAVA_LDFLAGS=
if test "x$bc_with_java" = "xyes" ; then
    if test "x$bc_with_java_val" != "x" ; then
	JAVA_CFLAGS="-I$bc_with_java_val/include -I$bc_with_java_val/include/`uname | tr [A-Z] [a-z]`"
    fi
fi
# XXX this should go away and become proper library or compiler option checks
if test ! "x$bc_auto_flags" = "xno" ; then
    case "$target" in
	*-*-darwin*)
		JAVA_CFLAGS="-framework JavaVM"
		JAVA_LDFLAGS="-shrext .jnilib -framework JavaVM"
		;;
    esac
fi
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)


dnl *************************
dnl *** Check for headers ***
dnl *************************

BC_CONFIGURE_STAGE([headers], [4 of 9])

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS( \
	arpa/inet.h \
	curses.h \
	dslib.h \
	errno.h \
	fcntl.h \
	float.h \
	getopt.h \
	gl/device.h \
	ieeefp.h \
	inttypes.h \
	io.h \
	itcl.h \
	itk.h \
	langinfo.h \
	libgen.h \
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
	rle.h \
	sched.h \
	setjmp.h \
	sgtty.h \
	signal.h \
	socket.h \
	stdarg.h \
	stddef.h \
	stdint.h \
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
	sys/prctl.h \
	sys/resource.h \
	sys/select.h \
	sys/socket.h \
	sys/stat.h \
	sys/statfs.h \
	sys/sysctl.h \
	sys/sysinfo.h \
	sys/sysmp.h \
	sys/time.h \
	sys/timeb.h \
	sys/types.h \
	sys/un.h \
	sys/unistd.h \
	sys/wait.h \
	syslog.h \
	tcl.h \
	termcap.h \
	terminfo.h \
	termio.h \
	termios.h \
	termlib.h \
	time.h \
	tinfo.h \
	tk.h \
	ulocks.h \
	unistd.h \
	utime.h \
	values.h \
	varargs.h \
	waitflags.h \
	wchar.h \
	winsock.h
)

# optionally check for X11 headers
if test "x$bc_with_x11" != "xno" ; then
    AC_CHECK_HEADERS( \
	X11/Xlib.h \
	X11/Xosdefs.h \
    )
fi # bc_with_x11

# optionally check for OpenGL headers
if test "x$bc_with_opengl" != "xno" -o "x$bc_with_wgl" != "xno" ; then
    AC_CHECK_HEADERS( \
	GL/gl.h \
	GL/glx.h \
    )
fi # bc_with_opengl

AC_CHECK_HEADER(sys/sched.h,
		AC_DEFINE(HAVE_SYS_SCHED_H, 1, Define to 1 if you have the <sys/sched.h> header file.),,
		[
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#include <sys/sched.h>
])

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
	fi
    fi # search_for_jni_h_more
    CPPFLAGS="$PRECPPFLAGS"
fi # bc_with_java


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

dnl XXX - union wait testing from tcl8.4's configure.in, should be revamped.
AC_MSG_CHECKING([union wait])
AC_CACHE_VAL(tcl_cv_union_wait,
    AC_TRY_LINK([
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
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

BC_CONFIGURE_STAGE([compiler], [6 of 9])

if test ! "x$bc_auto_flags" = "xno" ; then

    dnl try to use -pipe to speed up the compiles
    BC_COMPILER_AND_LINKER_RECOGNIZES([-pipe])

    dnl check for -fno-strict-aliasing
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fno-strict-aliasing])

    dnl check for -fno-common (libtcl needs it on darwin)
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fno-common])

    dnl check for -fexceptions
    dnl this is needed to resolve __Unwind_Resume when compiling and
    dnl linking against openNURBS in librt for some binaries, for
    dnl example rttherm
    BC_COMPILER_AND_LINKER_RECOGNIZES([-fexceptions], [fexceptions], [nokeep])
    if test "x$bc_fexceptions_works" = "xyes" ; then
       EXCEPTIONS="-fexceptions"
       AC_SUBST(EXCEPTIONS)
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
		if test "x$bc_use_profiling" = "xyes" ; then
			dnl profiling was requested, so abort
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Profiling was enabled but -pg does not work])
			AC_MSG_ERROR([*** Don't know how to profile with this compiler ***])
		fi
		AC_MSG_WARN([Don't know how to profile with this compiler])
	else
		# convert 'auto' to 'no' even though it works
		bc_use_profiling=no
	fi
fi

dnl debug flags
if test "x$bc_use_debug" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-g], [debug_flag])
	if test "x$bc_debug_flag_works" = "xno" ; then
		if test "x$bc_use_debug" = "xyes" ; then
			dnl debug was requested, so abort
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Debug was enabled but -g does not work])
			AC_MSG_ERROR([*** Don't know how to debug with this compiler ***])
		fi
		AC_MSG_WARN([Do not know how to debug with this compiler])
	else
		# convert 'auto' to 'yes'
		bc_use_debug="yes"
	fi
fi

dnl optimization flags
if test "x$bc_use_optimized" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-O3], [o3_flag])
	if test "x$bc_o3_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to compile optimized with this compiler])
	fi


# XXX -fast seems to provoke stack corruption in the shadow
# computations, possible that the corruption is indeed valid and
# there's something that needs to be checked/changed.
#
#	BC_COMPILER_AND_LINKER_RECOGNIZES([-fast], [mac_opt_flag])
#	if test "x$bc_mac_opt_flag_works" = "xno" ; then
#		BC_COMPILER_AND_LINKER_RECOGNIZES([-fast -mcpu=7450], [mac_opt_flag])
#	fi
	# don't set extra optimization flags if auto-flags was turned off
	if test ! "x$bc_auto_flags" = "xno" ; then
		if test "x$bc_mac_opt_flag_works" = "xno" ; then
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
fi

dnl verbose warning flags
if test "x$bc_build_warnings" != "xno" ; then
	BC_COMPILER_AND_LINKER_RECOGNIZES([-W -Wall -Wundef -Wfloat-equal -Wshadow -Wunreachable-code -Winline -Wconversion], [warning])
	# XXX also of interest
	# -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -pedantic -ansi -Werror

	if test "x$bc_warning_flag_works" = "xno" ; then
		AC_MSG_WARN([Don't know how to output verbose warnings with this compiler])
	fi
fi

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE
AC_C_BIGENDIAN
if test "x$ac_cv_c_bigendian" = "xyes" ; then
	AC_DEFINE(NATURAL_IEEE, , [64-bit floating point storage technique is big endian])
else
	if test "x$ac_cv_c_bigendian" = "xno" ; then
	       AC_DEFINE(REVERSE_IEEE, , [64-bit floating point storage technique is little endian])
	else
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_ERROR([*** Don't know the endian of this machine! ***])
	fi
fi

dnl AC_C_PROTOTYPES
AM_C_PROTOTYPES

AC_DEFINE(USE_PROTOTYPES,1,heh)

# figure out what size pointers the compiler is actually generating
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)
AC_CHECK_SIZEOF(void *, 4)
pointer_size="$ac_cv_sizeof_void_p"

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

BC_CONFIGURE_STAGE([functions], [7 of 9])

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
	finite \
	floor \
	fsync \
	ftime \
	ftruncate \
	getcwd \
	getenv \
	gethostbyaddr \
	gethostbyname \
	gethostname \
	getopt \
	getpagesize \
	getpass \
	getprogname \
	gettimeofday \
	inet_ntoa \
	isascii \
	isfinite \
	localtime_r \
	lrand48 \
	lseek \
	memchr \
	memmove \
	memset \
	mkdir \
	mkfifo \
	modf \
	munmap \
	nl_langinfo \
	panic \
	pipe \
	pow \
	prtcl \
	putenv \
	raise \
	re_comp \
	realpath \
	regcomp \
	rmdir \
	sbrk \
	select \
	setenv \
	setlinebuf \
	setlocale \
	setpgid \
	setprogname \
	setvbuf \
	socket \
	sqrt \
	srand48 \
	stat \
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
	strtok \
	strtol \
	strtoul \
	strtoull \
	sysctl \
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
AH_VERBATIM([OUR_STRSEP],
[#ifndef HAVE_STRSEP
extern char *strsep(char **stringp, const char *delim);
#endif])


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
fi
AC_MSG_CHECKING([whether we have the essentials to make parsers])
if test "x$build_parsers" = "xyes" ; then
    AC_DEFINE(BC_WITH_PARSERS, 1, [Define to enable compilation of parsers])
    AC_MSG_RESULT(yes)
else
    AC_MSG_RESULT(no)
fi


dnl *** libpng ***
png_works=no
AC_MSG_CHECKING(for portable network graphics functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBPNG"
AC_TRY_RUN([
#include <png.h>
main() {
	int i = png_access_version_number();
	return 0;
}
	],
	[png_works=yes]
)
LDFLAGS="$PRELDFLAGS"
AC_MSG_RESULT($png_works)

dnl figure out whether we need to build libpng
build_brlcad_png="no (using system)"
if test "x$bc_build_png" = "xyes" ; then
	build_brlcad_png=yes
else
	if test "x$bc_build_png" = "xno" ; then
		if test "x$png_works" != "xyes" ; then
			AC_MSG_NOTICE([libpng was disabled, but no system png library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
if test "x$build_brlcad_png" = "xyes" ; then
	PNG='${top_builddir}/src/other/libpng/libpng.la'
	AC_MSG_RESULT(yes)
else
	PNG="$LIBPNG"
	AC_MSG_RESULT(no)
fi
AC_SUBST(PNG)


dnl *** libregex ***
dnl See if regular expression support works
regex_works=no
AC_MSG_CHECKING(for regular expression functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBREGEX"
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
	],
	[regex_works=yes]
)
LDFLAGS="$PRELDFLAGS"
AC_MSG_RESULT($regex_works)

dnl figure out whether we need to build libregex
build_brlcad_regex="no (using system)"
if test "x$bc_build_regex" = "xyes" ; then
	build_brlcad_regex=yes
else
	if test "x$bc_build_regex" = "xno" ; then
		if test "x$regex_works" != "xyes" ; then
			AC_MSG_NOTICE([libregex was disabled, but no system regular expression library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
if test "x$build_brlcad_regex" = "xyes" ; then
	REGEX='${top_builddir}/src/other/libregex/libregex.la'
	AC_MSG_RESULT(yes)
else
	REGEX="$LIBREGEX"
	AC_MSG_RESULT(no)
fi
AC_SUBST(REGEX)


dnl *** libz ***
dnl See if zlib library support works
zlib_works=no
AC_MSG_CHECKING(for zlib compression functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBZLIB"
AC_TRY_RUN([
#include <zlib.h>
int main () {
  char *zlibver = ZLIB_VERSION;
  return 0;
}
	],
	[zlib_works=yes]
)
LDFLAGS="$PRELDFLAGS"
AC_MSG_RESULT($zlib_works)

dnl figure out whether we need to build libz
build_brlcad_zlib="no (using system)"
if test "x$bc_build_zlib" = "xyes" ; then
	build_brlcad_zlib=yes
else
	if test "x$bc_build_zlib" = "xno" ; then
		if test "x$zlib_works" != "xyes" ; then
			AC_MSG_NOTICE([libz was disabled, but no system zlib compression library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
if test "x$build_brlcad_zlib" = "xyes" ; then
	LIBZ='${top_builddir}/src/other/libz/libz.la'
	AC_MSG_RESULT(yes)
else
	LIBZ="$LIBZLIB"
	AC_MSG_RESULT(no)
fi
AC_SUBST(LIBZ)


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
	],
	[urt_works=yes]
)
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
			BC_RETRY_CONFIGURE([$this], [$args])
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
if test "x$build_brlcad_urt" = "xyes" ; then
	RLE='${top_builddir}/src/other/libutahrle/libutahrle.la'
	AC_MSG_RESULT(yes)
else
	if test "x$utahrle_link_works" = "xyes" ; then
		RLE="$UTAHRLE"
	fi
	AC_MSG_RESULT(no)
fi
AC_SUBST(RLE)


dnl *** openNURBS ***
dnl See if openNURBS library support works
opennurbs_works=no
AC_MSG_CHECKING(for openNURBS functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBOPENNURBS"
AC_LANG_PUSH([C++])
AC_TRY_RUN([
#include <opennurbs.h>
int main () {
  int version = ON::Version();
  return 0;
}
	],
	[opennurbs_works=yes]
)
AC_LANG_POP
LDFLAGS="$PRELDFLAGS"
AC_MSG_RESULT($opennurbs_works)

dnl figure out whether we need to build openNURBS
build_brlcad_opennurbs="no (using system)"
if test "x$bc_build_opennurbs" = "xyes" ; then
	build_brlcad_opennurbs=yes
else
	if test "x$bc_build_opennurbs" = "xno" ; then
		if test "x$opennurbs_works" != "xyes" ; then
			AC_MSG_NOTICE([openNURBS was disabled, but no system openNURBS library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-opennurbs-build and/or --enable-c++])
			AC_MSG_ERROR([*** Building openNURBS was disabled, yet no system openNURBS library was found ***])
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
if test "x$build_brlcad_opennurbs" = "xyes" ; then
	OPENNURBS='${top_builddir}/src/other/openNURBS/libopenNURBS.la -lstdc++'
	OPENNURBS_CPPFLAGS='-I${top_srcdir}/src/other/openNURBS'
	AC_MSG_RESULT(yes)
else
	OPENNURBS="$LIBOPENNURBS -lstdc++"
	OPENNURBS_CPPFLAGS=''
	AC_MSG_RESULT(no)
fi
AC_SUBST(OPENNURBS)
AC_SUBST(OPENNURBS_CPPFLAGS)


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
#elif TCl_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4
    fprintf(stderr, "minor version < 4\n");
    return 1;
#endif
    return 0;
}
	],
	[tcl_works=yes]
)
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
			AC_MSG_NOTICE([libtcl was disabled, but no system Tcl library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
TCL_VERSION="8.4"
AC_DEFINE_UNQUOTED([TCL_VERSION], "$TCL_VERSION", "Version of Tcl")
AC_SUBST(TCL_VERSION)
TCL_MAJOR_VERSION=8
AC_SUBST(TCL_MAJOR_VERSION)
TCL_MINOR_VERSION=4
AC_SUBST(TCL_MINOR_VERSION)
TCL_PATCH_LEVEL=".6"
AC_SUBST(TCL_PATCH_LEVEL)
TCL=""
TCLSTUB=""
if test "x$build_brlcad_tcl" = "xyes" ; then
	AC_MSG_RESULT(yes)
	TCL='${top_builddir}/src/other/libtcl/libtcl${TCL_VERSION}.la '"${SOCKET}"
	TCLSTUB='${top_builddir}/src/other/libtcl/generic/libtclstub.la'
else
	AC_MSG_RESULT(no)
	if test "x$tcl_link_works" = "xyes" ; then
		TCL="$LIBTCL"
	else
		AC_MSG_WARN([Unable to link against a system Tcl library])
	fi
	if test "x$tclstub_link_works" = "xyes" ; then
		TCLSTUB="$LIBTCLSTUB"
	else
		AC_MSG_WARN([Unable to link against a system Tclstub library])
	fi
fi
AC_SUBST(TCL)
AC_SUBST(TCLSTUB)


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
int main() {
    Tcl_Interp *interp;
    interp = Tcl_CreateInterp();
    if (Tcl_Init(interp) == TCL_ERROR) {
	fprintf(stderr, "Tcl_Init returned error\n");
	return 1;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
	fprintf(stderr, "Tk_Init returned error\n");
	return 1;
    }

#if TK_MAJOR_VERSION < 8
    fprintf(stderr, "major version < 8\n");
    return 1;
#elif TK_MAJOR_VERSION == 8 && TK_MINOR_VERSION < 4
    fprintf(stderr, "minor version < 4\n");
    return 1;
#endif
    return 0;
}
	],
	[tk_works=yes]
)
if test "x$tk_link_works" = "xyes" ; then
	LIBS="$PRELIBS"
fi
AC_MSG_RESULT($tk_works)

dnl figure out whether we need to build Tk
build_brlcad_tk="no (using system)"
if test "x$bc_build_tk" = "xyes" ; then
	build_brlcad_tk=yes
else
	if test "x$bc_build_tk" = "xno" ; then
		if test "x$tk_works" = "xno" ; then
			AC_MSG_NOTICE([libtk was disabled, but no system Tk library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-tk-build])
			AC_MSG_ERROR([*** Tk was disabled, yet no usable libtk system library was found ***])
		fi
	else
		dnl automatic detection
		if test "x$tk_works" = "xno" ; then
			build_brlcad_tk=yes
		fi
	fi
fi
AC_MSG_CHECKING(whether to build Tk)
dnl Need to update version number(s) in src/other/libtk/Makefile.am too
TK_VERSION="8.4"
AC_DEFINE_UNQUOTED([TK_VERSION], "$TK_VERSION", "Version of Tk")
AC_SUBST(TK_VERSION)
TK_LIBRARY='${prefix}/lib/tk${TK_VERSION}'
AC_SUBST(TK_LIBRARY)
TK=""
TKSTUB=""
if test "x$build_brlcad_tk" = "xyes" ; then
	AC_MSG_RESULT(yes)
	TK='${top_builddir}/src/other/libtk/libtk${TK_VERSION}.la'
	TKSTUB='${top_builddir}/src/other/libtk/generic/libtkstub.la'
else
	AC_MSG_RESULT(no)
	if test "x$tk_link_works" = "xyes" ; then
		TK="$LIBTK"
	else
		AC_MSG_WARN([Unable to link against a system Tk library])
	fi
	if test "x$tkstub_link_works" = "xyes" ; then
		TKSTUB="$LIBTKSTUB"
	else
		AC_MSG_WARN([Unable to link against a system Tkstub library])
	fi
fi
AC_SUBST(TK)
AC_SUBST(TKSTUB)


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
#elif ITCl_MAJOR_VERSION == 3 && ITCL_MINOR_VERSION < 2
    fprintf(stderr, "minorversion < 2\n");
    return 1;
#endif
    return 0;
}
	],
	[itcl_works=yes]
)
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
			AC_MSG_NOTICE([incrTcl was disabled, but no system incrTcl library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
ITCL_VERSION="3.3"
AC_SUBST(ITCL_VERSION)
ITCL=""
if test "x$build_brlcad_itcl" = "xyes" ; then
	ITCL='${top_builddir}/src/other/incrTcl/libitcl${ITCL_VERSION}.la'
	ITK='${top_builddir}/src/other/incrTcl/libitk${ITCL_VERSION}.la'
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


dnl *** iwidgets ***
dnl See if the iwidgets are available
iwidgets_works=no
AC_MSG_CHECKING(for iwidgets functionality)
PRELIBS="$LIBS"
if test "x$tcl_link_works" = "xyes" ; then
	LIBS="$LIBS $LIBTCL"
fi
dnl this test is probably not sufficient if iwidgets is not locateable
dnl in the tcl auto_path.
AC_TRY_RUN([
#ifdef HAVE_TCL_H
#  include <tcl.h>
#endif
int main() {
    Tcl_Interp *interp;
    const char *cmd = "package require Iwidgets";
    interp = Tcl_CreateInterp();
    if (Itcl_Init(interp) == TCL_ERROR) {
	return 1;
    }
    if (Tcl_Eval(interp, cmd) != TCL_OK || interp->result[0] == '\0') {
	return 1;
    }
    return 0;
}
	],
	[iwidgets_works=yes]
)
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
			AC_MSG_NOTICE([iwidgets was disabled, but no system iwidgets package was found])
			BC_RETRY_CONFIGURE([$this], [$args])
			AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
			AC_MSG_NOTICE([Try adding --enable-iwidgets-build])
			AC_MSG_ERROR([*** iwidgets was disabled, yet no usable iwidgets system package was found ***])
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


dnl *** BLT ***
dnl figure out whether we need to build BLT
BLT_VERSION="2.4"
AC_SUBST(BLT_VERSION)
BLT_LIBRARY='${prefix}/lib/blt${BLT_VERSION}'
AC_SUBST(BLT_LIBRARY)
AC_MSG_CHECKING(whether to build libblt)
if test ! "x$bc_build_blt" = "xno" ; then
   build_brlcad_blt=yes
   BLT='${top_builddir}/src/other/blt/libblt${BLT_VERSION}.la'
   AC_MSG_RESULT(yes)
else
   build_brlcad_blt=no
   BLT=""
   AC_MSG_RESULT(no)
fi


dnl *** tkImg ***
dnl figure out whether we need to build tkImg
TKIMG_VERSION="1.3"
AC_SUBST(TKIMG_VERSION)
AC_MSG_CHECKING(whether to build tkimg)
if test ! "x$bc_build_tkimg" = "xno" ; then
   build_brlcad_tkimg=yes
   TKIMG='${top_builddir}/src/other/tkimg/tkimg${TKIMG_VERSION}.la'
   AC_MSG_RESULT(yes)
else
   build_brlcad_tkimg=no
   TKIMG=""
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
	],
	[thread_works=yes]
)
AC_MSG_RESULT($thread_works)


dnl *** libtermlib ***
dnl See if termlib library support works
termlib_works=no
AC_MSG_CHECKING(for termlib functionality)
PRELDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS $LIBTERMLIB"
AC_TRY_RUN([
#ifdef HAVE_TERMLIB_H
#  include <termlib.h>
#else
#  if HAVE_NCURSES_H
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
#  if HAVE_TERM_H
#    include <term.h>
#  endif
#endif
int main () {
  char buffer[2048] = {0};
  int result = tgetent(buffer, "vt100");
  return 0;
}
	],
	[termlib_works=yes]
)
LDFLAGS="$PRELDFLAGS"
AC_MSG_RESULT($termlib_works)

dnl figure out whether we need to build libtermlib
build_brlcad_termlib="no (using system)"
if test "x$bc_build_termlib" = "xyes" ; then
	build_brlcad_termlib=yes
else
	if test "x$bc_build_termlib" = "xno" ; then
		if test "x$termlib_works" != "xyes" ; then
			AC_MSG_NOTICE([termlib was disabled, but no system terminal library was found])
			BC_RETRY_CONFIGURE([$this], [$args])
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
if test "x$build_brlcad_termlib" = "xyes" ; then
	TERMLIB='${top_builddir}/src/other/libtermlib/libtermlib.la'
	AC_MSG_RESULT(yes)
else
	TERMLIB="$LIBTERMLIB"
	AC_MSG_RESULT(no)
fi
AC_SUBST(TERMLIB)


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

dnl *** libds ***
AC_MSG_CHECKING(for SGI /dev/scsi library availability)
AC_MSG_RESULT([$ds_link_works])
AC_SUBST(LIBDS)

dnl *** libgl ***
AC_MSG_CHECKING(whether to link with the OpenGL library)
AC_MSG_RESULT($gl_link_works)
AC_SUBST(LIBGL)

dnl *** libopengl32 ***
AC_MSG_CHECKING(whether to link with the WindowsGL library)
AC_MSG_RESULT($wgl_link_works)
AC_SUBST(LIBWGL)

dnl *** sgigl ***
AC_MSG_CHECKING(whether to link with the SGI graphics library)
AC_MSG_RESULT($sgigl_link_works)
AC_SUBST(LIBSGIGL)
if test "x$sgigl_link_works" = "xyes" ; then
	AC_DEFINE(HAS_SGIGL, 1, Irix GL is present.)
fi

dnl *** libl Solaris lexer library ***
AC_MSG_CHECKING(whether to link with the Solaris lexer library)
AC_MSG_RESULT($l_link_works)
AC_SUBST(LIBL)


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
	    BC_RETRY_CONFIGURE([$this], [$args])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
	    AC_MSG_ERROR([*** Cannot locate jni.h for building librtserver ***])
	fi
	if test "x$bc_with_java" = "xyes" ; then
	    AC_MSG_RESULT(no)
	    AC_MSG_NOTICE([searching more extensively])
	    BC_RETRY_CONFIGURE([$this], [$args])
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_ERROR([Failed to locate a functioning jni.h for Java])
	fi

	if test "x$bc_with_java" != "xauto" ; then
	    AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
	    AC_MSG_WARN([Unable to locate a functioning Java])
	    AC_MSG_WARN([Compilation of librtserver will be disabled])
	    AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
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


dnl *** OpenGL interface compilation ***
build_against_opengl="no"
if test "x$bc_with_opengl" != "xno" ; then
    AC_MSG_CHECKING(for OpenGL functionality)
    opengl_works=no
    PRELDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $LIBGL"
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
	       ],
	       [opengl_works=yes]
	      )
    LDFLAGS="$PRELDFLAGS"
    AC_MSG_RESULT($opengl_works)

    dnl figure out whether we need to build against opengl
    if test "x$opengl_works" = "xyes" ; then
	build_against_opengl="yes"
    else
	if test "x$bc_with_opengl" = "xyes" ; then
	    AC_MSG_NOTICE([OpenGL was requested, but no system OpenGL libraries were found])
	    BC_RETRY_CONFIGURE([$this], [$args])
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


dnl *** WindowsGL interface compilation ***
build_against_wgl="no"
if test "x$bc_with_wgl" != "xno" ; then
    AC_MSG_CHECKING(for WindowsGL functionality)
    wgl_works=no
    PRELDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $LIBWGL"
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
	       ],
	       [wgl_works=yes]
	      )
    LDFLAGS="$PRELDFLAGS"
    AC_MSG_RESULT($wgl_works)

    dnl figure out whether we need to build against wgl
    if test "x$wgl_works" = "xyes" ; then
	build_against_wgl="yes"
    else
	if test "x$bc_with_wgl" = "xyes" ; then
	    AC_MSG_NOTICE([WindowsGL was requested, but no system WGL OpenGL libraries were found])
	    BC_RETRY_CONFIGURE([$this], [$args])
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


dnl *** X11 interface compilation ***
build_against_x11="no"
if test "x$bc_with_x11" != "xno" ; then
    # XXX ugly hack (that needs to die)
    X_PREFIX=/usr/X11R6
    if test "x$bc_with_x11_val" != "x" ; then
	X_PREFIX="$bc_with_x11_val"
    fi
    X_CFLAGS="-I${X_PREFIX}/include"
    X_LIBS="-L${X_PREFIX}/lib $LIBX11 $LIBXI"

    x11_works=no
    AC_MSG_CHECKING(for X11 header functionality)
    PRELDFLAGS="$LDFLAGS"
    LDFLAGS="$LDFLAGS $X_LIBS"
    AC_TRY_RUN([
#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#endif
main() {
       XPoint pt;
       pt.x = pt.y = 0;
       return pt.x;
}
		],[x11_works=yes])
    AC_MSG_RESULT($x11_works)
    if test "x$x11_works" = "xyes" ; then
	AC_MSG_CHECKING(for X11 link functionality)
	AC_TRY_RUN([
#ifdef HAVE_X11_XLIB_H
#  include <X11/Xlib.h>
#endif
main() {
	XModifierKeymap *xmkm;
	xmkm=XNewModifiermap(1);
	return xmkm==NULL;
}
		    ],,x11_works=no)
	AC_MSG_RESULT($x11_works)
    fi
    LDFLAGS="$PRELDFLAGS"

    dnl figure out whether we need to build against X11
    if test "x$bc_with_x11" != "xno" ; then
	if test "x$x11_works" = "xyes" ; then
	    build_against_x11="yes"
	else
	    if test "x$bc_with_x11" = "xyes" ; then
		BC_RETRY_CONFIGURE([$this], [$args])
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
AC_SUBST(X_PREFIX)
AC_SUBST(X_CFLAGS)
AC_SUBST(X_LIBS)
AC_MSG_CHECKING(whether to build against X11)
AC_MSG_RESULT($build_against_x11)


dnl *** SDL availability  ***
sdl_works="no"
if test "x$bc_with_sdl" != "xno" ; then
	if test "x$SDL_CONFIG" != "x" ; then
		AM_PATH_SDL(1.2.0,,AC_MSG_ERROR([Cannot find SDL.]))
		AC_SUBST(SDL_CFLAGS)
		AC_SUBST(SDL_LIBS)
		sdl_works="yes"
	fi
fi
AC_MSG_CHECKING(for SDL functionality)
AC_MSG_RESULT($sdl_works)


dnl *** Python availability ***
python_works="no"
AC_MSG_CHECKING(for Python functionality)
if test "x$bc_with_python" != "xno" ; then
	if test "x$PYTHON" != "x" ; then
		python_works="yes"
	fi
fi
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)
AC_MSG_RESULT($python_works)


dnl *** ADRT compilation ***
dnl figure out whether we have everything we need to build ADRT
dnl requires sdl and python
build_brlcad_adrt="no"
if test "x$bc_build_adrt" = "xauto" ; then
	if test "x$python_works" = "xyes" ; then
		if test "x$sdl_works" = "xyes" ; then
			build_brlcad_adrt="yes"
		else
			build_brlcad_adrt="no (need sdl)"
		fi
	else
		if test "x$sdl_works" = "xyes" ; then
			build_brlcad_adrt="no (need python)"
		else
			build_brlcad_adrt="no (need python and sdl)"
		fi
	fi
elif test "x$bc_build_adrt" = "xyes" ; then
	build_brlcad_adrt="yes"
	if test "x$sdl_works" = "xno" ; then
		AC_MSG_NOTICE([ADRT requires SDL (and Python)])
		BC_RETRY_CONFIGURE([$this], [$args])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_NOTICE([Try adding --disable-adrt-build])
		AC_MSG_ERROR([*** Using ADRT was requested, yet no system SDL library was found ***])
	elif test "x$python_works" = "xno" ; then
		AC_MSG_NOTICE([ADRT requires Python (and SDL)])
		BC_RETRY_CONFIGURE([$this], [$args])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_NOTICE([Try adding --disable-adrt-build])
		AC_MSG_ERROR([*** Using ADRT was requested, yet no system Python interpreter was found ***])
	fi
fi


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
	],
	[dials_and_buttons=yes]
)
if test "x$dials_and_buttons" = "xyes" ; then
	AC_DEFINE(IR_KNOBS, 8, [Turn on SGI knobs])
	AC_DEFINE(IR_BUTTONS, 32, [Turn on SGI buttons])
fi
AC_MSG_RESULT($dials_and_buttons)


# XXX nasty temp hack for backwards cake compatibility. code needs to be fixed.
if test "x$host_os" = "xirix6.5" ; then
	# XXX the code that relies on this will be going away, but until then..
	if test "x$build_brlcad_64bit" = "xyes" ; then
		CFLAGS="$CFLAGS -DIRIX64=65"
	else
		CFLAGS="$CFLAGS -DIRIX=6"
	fi
fi

dnl search the BRL-CAD include directory (required for non-srcdir builds).
dnl should come after the system services checks otherwise or our headers
dnl may conflict.
CPPFLAGS="$CPPFLAGS -DBRLCADBUILD=1 -I\$(top_srcdir)/include"

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


dnl **************************************
dnl *** Configure Makefiles and output ***
dnl **************************************

BC_CONFIGURE_STAGE([output], [9 of 9])

AM_CONDITIONAL(BUILD_TCL, [test "x$build_brlcad_tcl" = "xyes"])
AM_CONDITIONAL(BUILD_TK, [test "x$build_brlcad_tk" = "xyes"])
AM_CONDITIONAL(BUILD_ITCL, [test "x$build_brlcad_itcl" = "xyes"])
AM_CONDITIONAL(BUILD_IWIDGETS, [test "x$build_brlcad_iwidgets" = "xyes"])
AM_CONDITIONAL(BUILD_BLT, [test "x$build_brlcad_blt" = "xyes"])
AM_CONDITIONAL(BUILD_TKIMG, [test "x$build_brlcad_tkimg" = "xyes"])
AM_CONDITIONAL(BUILD_PNG, [test "x$build_brlcad_png" = "xyes"])
AM_CONDITIONAL(BUILD_REGEX, [test "x$build_brlcad_regex" = "xyes"])
AM_CONDITIONAL(BUILD_ZLIB, [test "x$build_brlcad_zlib" = "xyes"])
AM_CONDITIONAL(BUILD_URT, [test "x$build_brlcad_urt" = "xyes"])
AM_CONDITIONAL(BUILD_OPENNURBS, [test "x$build_brlcad_opennurbs" = "xyes"])
AM_CONDITIONAL(BUILD_TERMLIB, [test "x$build_brlcad_termlib" = "xyes"])
AM_CONDITIONAL(BUILD_JOVE, [test "x$build_brlcad_jove" = "xyes"])
AM_CONDITIONAL(BUILD_EF, [test "x$bc_build_ef" = "xyes"])
AM_CONDITIONAL(BUILD_PROE, [test "x$bc_build_proe" = "xyes"])
AM_CONDITIONAL(BUILD_UG, [test "x$bc_build_ug" = "xyes"])
AM_CONDITIONAL(BUILD_ADRT, [test "x$build_brlcad_adrt" = "xyes"])
AM_CONDITIONAL(INSTALL_MODELS, [test "x$bc_install_geometry" = "xyes"])

AM_CONDITIONAL(WITH_JAVA, [test "x$build_against_java" != "xno"])
AM_CONDITIONAL(WITH_X11, [test "x$build_against_x11" != "xno"])
AM_CONDITIONAL(WITH_OPENGL, [test "x$build_against_opengl" != "xno"])
AM_CONDITIONAL(WITH_WGL, [test "x$build_against_wgl" != "xno"])
AM_CONDITIONAL(WITH_PARSERS, [test "x$build_parsers" != "xno"])
AM_CONDITIONAL(WITH_OPENNURBS, [test "x$OPENNURBS" != "x"])

AM_CONDITIONAL(ONLY_BENCHMARK, [test "x$bc_only_benchmark" != "xno"])
AM_CONDITIONAL(ONLY_RTS, [test "x$bc_only_rts" != "xno"])

# XXX only irix can build libtkglx right now?
AM_CONDITIONAL(BUILD_LIBTKGLX, [test "x$host_os" = "xirix6.5"])


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

BU='${top_builddir}/src/libbu/libbu.la'
AC_SUBST(BU)

BN='${top_builddir}/src/libbn/libbn.la'
AC_SUBST(BN)

RT='${top_builddir}/src/librt/librt.la'
AC_SUBST(RT)

WDB='${top_builddir}/src/libwdb/libwdb.la'
AC_SUBST(WDB)

PKG='${top_builddir}/src/libpkg/libpkg.la'
AC_SUBST(PKG)

SYSV='${top_builddir}/src/libsysv/libsysv.la'
AC_SUBST(SYSV)

CURSOR='${top_builddir}/src/libcursor/libcursor.la'
AC_SUBST(CURSOR)

FB='${top_builddir}/src/libfb/libfb.la'
AC_SUBST(FB)

DM='${top_builddir}/src/libdm/libdm.la'
AC_SUBST(DM)
AC_DEFINE_UNQUOTED([USE_FBSERV], [1], [Required for libdm/mged to allow direct fbserv render connections])

FFT='${top_builddir}/src/libfft/libfft.la'
AC_SUBST(FFT)

MULTISPECTRAL='${top_builddir}/src/libmultispectral/libmultispectral.la'
AC_SUBST(MULTISPECTRAL)

OPTICAL='${top_builddir}/src/liboptical/liboptical.la'
AC_SUBST(OPTICAL)

ORLE='${top_builddir}/src/liborle/liborle.la'
AC_SUBST(ORLE)

TCLCAD='${top_builddir}/src/libtclcad/libtclcad.la'
AC_SUBST(TCLCAD)

TERMIO='${top_builddir}/src/libtermio/libtermio.la'
AC_SUBST(TERMIO)

dnl tcl

# if compiling with the MIPSpro 7.3 compiler, there is a bug when
# trying to link using an rpath longer than 256 that causes the linker
# to core dump.
AM_CONDITIONAL(LINK_STATIC_REQUIRED, [test "x$build_vendor" = "xsgi" -a "x$compiler" = "xsgi" ])

dnl Need to update version number(s) in src/other/libtcl/Makefile.am
TCL_LIBRARY='${prefix}/lib/tcl${TCL_VERSION}'
AC_SUBST(TCL_LIBRARY)
TCL_PACKAGE_PATH="'${libdir}'"
AC_SUBST(TCL_PACKAGE_PATH)

dnl XXX - hack for justins g.c file
AM_CONDITIONAL(HAVE_BRLCAD, true)
AM_CONDITIONAL(HAVE_DOXYGEN, false)

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


dnl sample applications
SAMPLE_APPLICATIONS_DIR='$(BRLCAD_DATA)/sample_applications'
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
	doc/html/manuals/archer/Makefile
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
	m4/Makefile
	misc/Makefile
	misc/brlcad.spec
	misc/macosx/Makefile
	misc/macosx/Resources/Makefile
	misc/macosx/Resources/ReadMe.rtfd/Makefile
	misc/macosx/Resources/Welcome.rtfd/Makefile
	misc/vfont/Makefile
	misc/win32-msvc/Makefile
	misc/win32-msvc/Dll/Makefile
	misc/win32-msvc7/Makefile
	misc/win32-msvc7/all/Makefile
	misc/win32-msvc7/asc2g/Makefile
	misc/win32-msvc7/brlcad/Makefile
	misc/win32-msvc7/dbupgrade/Makefile
	misc/win32-msvc7/fast42g/Makefile
	misc/win32-msvc7/fb2pix/Makefile
	misc/win32-msvc7/fb2png/Makefile
	misc/win32-msvc7/fbclear/Makefile
	misc/win32-msvc7/fbserv/Makefile
	misc/win32-msvc7/g2asc/Makefile
	misc/win32-msvc7/g2iges/Makefile
	misc/win32-msvc7/g2stl/Makefile
	misc/win32-msvc7/iges2g/Makefile
	misc/win32-msvc7/itclstub/Makefile
	misc/win32-msvc7/itkstub/Makefile
	misc/win32-msvc7/libBLT/Makefile
	misc/win32-msvc7/libbn/Makefile
	misc/win32-msvc7/libbu/Makefile
	misc/win32-msvc7/libdm/Makefile
	misc/win32-msvc7/libfb/Makefile
	misc/win32-msvc7/libItcl/Makefile
	misc/win32-msvc7/libItk/Makefile
	misc/win32-msvc7/liboptical/Makefile
	misc/win32-msvc7/libpkg/Makefile
	misc/win32-msvc7/libpng/Makefile
	misc/win32-msvc7/librt/Makefile
	misc/win32-msvc7/libsysv/Makefile
	misc/win32-msvc7/libtcl/Makefile
	misc/win32-msvc7/libtclcad/Makefile
	misc/win32-msvc7/libtk/Makefile
	misc/win32-msvc7/libwdb/Makefile
	misc/win32-msvc7/libz/Makefile
	misc/win32-msvc7/mged/Makefile
	misc/win32-msvc7/nirt/Makefile
	misc/win32-msvc7/pix2fb/Makefile
	misc/win32-msvc7/png2fb/Makefile
	misc/win32-msvc7/rt/Makefile
	misc/win32-msvc7/rtcheck/Makefile
	misc/win32-msvc7/rtedge/Makefile
	misc/win32-msvc7/stl2g/Makefile
	misc/win32-msvc7/tclpipe/Makefile
	misc/win32-msvc7/tclsh/Makefile
	misc/win32-msvc7/tclstub/Makefile
	misc/win32-msvc7/tkimg/Makefile
	misc/win32-msvc7/tkimgpng/Makefile
	misc/win32-msvc7/tkstub/Makefile
	misc/win32-msvc7/wish/Makefile
	pix/Makefile
	sh/Makefile
	src/Makefile
	src/adrt/Makefile
	src/adrt/bench/Makefile
	src/adrt/doc/Makefile
	src/adrt/libcommon/Makefile
	src/adrt/libtexture/Makefile
	src/adrt/libtie/Makefile
	src/adrt/libtienet/Makefile
	src/adrt/libutil/Makefile
	src/adrt/librender/Makefile
	src/adrt/rise/Makefile
	src/adrt/rise/master/Makefile
	src/adrt/rise/observer/Makefile
	src/adrt/rise/slave/Makefile
	src/adrt/isst/Makefile
	src/adrt/isst/master/Makefile
	src/adrt/isst/observer/Makefile
	src/adrt/isst/slave/Makefile
	src/adrt/scripts/Makefile
	src/adrt/misc/Makefile
	src/anim/Makefile
	src/archer/Makefile
	src/archer/plugins/Commands/Makefile
	src/archer/plugins/Core/Makefile
	src/archer/plugins/Makefile
	src/archer/plugins/Utility/Makefile
	src/archer/plugins/Wizards/Makefile
	src/archer/plugins/Wizards/tankwizardIA/Makefile
	src/archer/plugins/Wizards/tankwizardIA/images/Makefile
	src/brlman/Makefile
	src/brlman/awf
	src/brlman/brlman
	src/burst/Makefile
	src/bwish/Makefile
	src/canon/Makefile
	src/conv/Makefile
	src/conv/comgeom/Makefile
	src/conv/iges/Makefile
	src/conv/jack/Makefile
	src/conv/off/Makefile
	src/conv/patch/Makefile
	src/external/Makefile
	src/external/EndgameFramework/Makefile
	src/external/ProEngineer/Makefile
	src/external/ProEngineer/mk
	src/external/ProEngineer/protk.dat
	src/external/Unigraphics/Makefile
	src/fb/Makefile
	src/fbed/Makefile
	src/fbserv/Makefile
	src/gtools/Makefile
	src/halftone/Makefile
	src/irprep/Makefile
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
	src/mged/points/Makefile
	src/mk/Makefile
	src/nirt/Makefile
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
	src/other/libtermlib/Makefile
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
	src/other/openNURBS/Makefile
	src/other/tkimg/Makefile
	src/other/tkimg/base/Makefile
	src/other/tkimg/png/Makefile
	src/other/tkimg/pngtcl/Makefile
	src/other/tkimg/zlibtcl/Makefile
	src/other/blt/Makefile
	src/other/blt/cf/Makefile
	src/other/blt/demos/Makefile
	src/other/blt/demos/bitmaps/Makefile
	src/other/blt/examples/Makefile
	src/other/blt/html/Makefile
	src/other/blt/library/Makefile
	src/other/blt/library/dd_protocols/Makefile
	src/other/blt/man/Makefile
	src/other/blt/src/Makefile
	src/other/blt/win/Makefile
	src/proc-db/Makefile
	src/remrt/Makefile
	src/rt/Makefile
	src/rttherm/Makefile
	src/sig/Makefile
	src/tab/Makefile
	src/tclscripts/Makefile
	src/tclscripts/archer/Makefile
	src/tclscripts/archer/images/Makefile
	src/tclscripts/archer/images/Themes/Makefile
	src/tclscripts/archer/images/Themes/Crystal/Makefile
	src/tclscripts/archer/images/Themes/Crystal_Large/Makefile
	src/tclscripts/archer/images/Themes/Windows/Makefile
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
	src/tclscripts/sdialogs/Makefile
	src/tclscripts/sdialogs/scripts/Makefile
	src/tclscripts/swidgets/Makefile
	src/tclscripts/swidgets/images/Makefile
	src/tclscripts/swidgets/scripts/Makefile
	src/tclscripts/util/Makefile
	src/util/Makefile
	src/vas4/Makefile
	src/vdeck/Makefile
])

AC_OUTPUT

#################
# patch libtool #
#################
case $host_os in
    darwin*)
	if test "x$ac_top_builddir" = "x" ; then
	    libtoolscript="libtool"
	else
	    libtoolscript="${ac_top_builddir}/libtool"
	fi
	if test -f ${libtoolscript} ; then
	    if test -w ${libtoolscript} ; then
		# remove any -all_load option.
		# provokes libtool linker bug with C++ libraries.
		sed 's/-all_load[[ 	]]*[\\\]$convenience//g' < $libtoolscript > ${libtoolscript}.sed
		if test ! "x`cat ${libtoolscript}`" = "x`cat ${libtoolscript}.sed`" ; then
		    AC_MSG_RESULT([Found -all_load in libtool script, removing])
		    cp ${libtoolscript}.sed ${libtoolscript}
		    rm -f ${libtoolscript}.sed
		fi
	    else
		AC_MSG_WARN([libtool script exists but is not writable so not attempting to edit])
	    fi
	fi
	;;
esac

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

AC_MSG_RESULT([Done.])
AC_MSG_RESULT([])
BC_BOLD
AC_MSG_RESULT([BRL-CAD Release $BRLCAD_VERSION, Build $CONFIG_DATE])
BC_UNBOLD
AC_MSG_RESULT([])
AC_MSG_RESULT([             Prefix: ${prefix}])
AC_MSG_RESULT([           Binaries: ${bindir}])
AC_MSG_RESULT([Configuration files: ${sysconfdir}])
AC_MSG_RESULT([       Manual pages: ${mandir}])
AC_MSG_RESULT([])
AC_MSG_RESULT([CC       = ${CC}])
AC_MSG_RESULT([CFLAGS   = ${CFLAGS}])
AC_MSG_RESULT([CPPFLAGS = ${CPPFLAGS}])
AC_MSG_RESULT([LDFLAGS  = ${LDFLAGS}])
AC_MSG_RESULT([LIBS     = ${LIBS}])
AC_MSG_RESULT([])
AC_MSG_RESULT([Build Tcl ............................: $build_brlcad_tcl])
AC_MSG_RESULT([Build Tk .............................: $build_brlcad_tk])
AC_MSG_RESULT([Build Itcl/Itk .......................: $build_brlcad_itcl])
AC_MSG_RESULT([Build IWidgets .......................: $build_brlcad_iwidgets])
AC_MSG_RESULT([Build BLT ............................: $build_brlcad_blt])
AC_MSG_RESULT([Build tkImg ..........................: $build_brlcad_tkimg])
AC_MSG_RESULT([Build libpng .........................: $build_brlcad_png])
AC_MSG_RESULT([Build libregex .......................: $build_brlcad_regex])
AC_MSG_RESULT([Build zlib ...........................: $build_brlcad_zlib])
AC_MSG_RESULT([Build termlib ........................: $build_brlcad_termlib])
AC_MSG_RESULT([Build Utah Raster Toolkit.............: $build_brlcad_urt])
AC_MSG_RESULT([Build openNURBS.......................: $build_brlcad_opennurbs])
AC_MSG_RESULT([Build jove ...........................: $build_brlcad_jove])
AC_MSG_RESULT([])
AC_MSG_RESULT([ADRT support .........................: $build_brlcad_adrt])
AC_MSG_RESULT([X11 support ..........................: $build_against_x11])
AC_MSG_RESULT([OpenGL support .......................: $build_against_opengl])
AC_MSG_RESULT([Java Developer Kit support ...........: $build_against_java])
AC_MSG_RESULT([Enable run-time debugging ............: $bc_build_runtime_debug])
AC_MSG_RESULT([])
AC_MSG_RESULT([Build 64-bit release .................: $build_brlcad_64bit])
AC_MSG_RESULT([Build optimized release ..............: $bc_use_optimized])
AC_MSG_RESULT([Build debug release ..................: $bc_use_debug])
AC_MSG_RESULT([Build profile release ................: $bc_use_profiling])
AC_MSG_RESULT([Build static libraries ...............: $enable_static])
AC_MSG_RESULT([Build shared/dynamic libraries .......: $enable_shared])
AC_MSG_RESULT([Print verbose compilation warnings ...: $bc_build_warnings])
AC_MSG_RESULT([Print verbose compilation progress ...: $bc_build_progress])
AC_MSG_RESULT([])
AC_MSG_RESULT([Only build benchmark suite ...........: $bc_only_benchmark])
AC_MSG_RESULT([Only build librtserver ...............: $bc_only_rts])
AC_MSG_RESULT([Install example geometry models ......: $bc_install_geometry])
AC_MSG_RESULT([])
if test "x$time_elapsed" != "xunknown" ; then
AC_MSG_RESULT([Elapsed configuration time ...........: $time_elapsed])
fi
AC_MSG_RESULT([---])
AC_MSG_RESULT([$0 complete, type 'make' to begin building])
AC_MSG_RESULT([])
