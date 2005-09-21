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
dnl to the AC_PREREQ version shown below unless extensive testing has
dnl been performed.
dnl
dnl Strive to tame the chaos that is so easily achieved.
dnl

AC_PREREQ(2.52)

AC_INIT(BRL-CAD, 7.4.1, [http://brlcad.org], brlcad)
AC_REVISION($Revision$)

AC_CONFIG_AUX_DIR(misc)
#AM_CONFIG_HEADER([config.h])

dnl See HACKING for details on how to properly update the version
MAJOR_VERSION=7
MINOR_VERSION=4
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
AC_DEFINE_UNQUOTED([BRLCAD_VERSION], "$BRLCAD_VERSION", "Version of BRL-CAD")
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
echo "***${separator}***"
echo "*** ${title} ***"
echo "***${separator}***"

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

dnl be sure to search these directories for components
BC_SEARCH_DIRECTORY([/usr/X11R6], [X11])

dnl BSD uses /usr/local by default for ports
BC_SEARCH_DIRECTORY([/usr/local])

dnl search several additional locations if configure happens to fail
dnl to find a required component
BC_RETRY_DIRECTORY([/usr/freeware], [freeware])
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
AC_MSG_RESULT([(1 of 9)])

dnl *** enable options ***

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

# force enable/disable building of termlib
BC_ARG_ENABLE([build_termlib], [termlib-build], [Build the included termlib library], [auto])

# force enable/disable building of Tcl
BC_ARG_ENABLE([build_tcl], [tcl-build], [Build the included Tcl], [yes])

# force enable/disable building of Tk
BC_ARG_ENABLE([build_tk], [tk-build], [Build the included Tk], [yes])

# tkimg
BC_ARG_ENABLE([build_tkimg], [tkimg-build], [Build the included tkimg], [yes])

# force enable/disable building of incrTcl
BC_ARG_ENABLE([build_itcl], [itcl-build], [Build the included incrTcl], [yes])

# force enable/disable building of iwidgets
BC_ARG_ENABLE([build_iwidgets], [iwidgets-build], [Install the included iwidgets], [yes])

# force enable/disable building of blt
BC_ARG_ENABLE([build_blt], [blt-build], [Build the included blt], [yes])

# doxygen - XXX unimplemented
bc_use_doxygen=no
#BC_ARG_ENABLE([use_doxygen], [doxygen], [Enable documentation generation with doxygen], [no])
#AC_MSG_CHECKING(whether to generate documentation with doxygen)
#AC_MSG_RESULT($bc_use_doxygen)

# jove
BC_ARG_ENABLE([build_jove], [jove], [Enable compilation of jove], [no])

# pro/e plugin
BC_ARG_ENABLE([build_proe], [pro-engineer-plugin], [Enable compilation of Pro/Engineer plugin], [no])

# example geometry
BC_ARG_ENABLE([install_geometry], [models-install], [Enable installation of example geometry models], [yes])

# optimized
BC_ARG_ENABLE([use_optimized], [optimized], [Enable optimized compilation], [no])

# warnings
BC_ARG_ENABLE([use_warnings], [warnings], [Enable verbose compilation warnings], [no])

# debug
BC_ARG_ENABLE([use_debug], [debug], [Enable debug symbols], [yes])

# profiling
BC_ARG_ENABLE([use_profiling], [profiling], [Enable profiling], [no])

dnl *** with options ***

# provides convenience argument handlers for specifying CFLAGS,
# LDFLAGS, CPPFLAGS, and LIBS using --with argument handlers.
BC_WITH_FLAG_ARGS

# use the jdk
BC_ARG_WITH([with_java], [jdk], [Specify path to the Java Development Kit], [no])


###
# argument aliases
# they need to go below here in order for --help to consolidate the
# blank line that it inserts to exactly one line in the right place
###

# aliases for building only the benchmark (help uses only-benchmark)
BC_ARG_ALIAS([only_benchmark], [only-bench])
BC_ARG_ALIAS([only_benchmark], [only-benchmarks])
BC_ARG_ALIAS([only_benchmark], [bench-only])
BC_ARG_ALIAS([only_benchmark], [benchmark-only])
BC_ARG_ALIAS([only_benchmark], [benchmarks-only])
AC_MSG_CHECKING(whether to only build benchmark)
AC_MSG_RESULT($bc_only_benchmark)

# aliases for building only the raytrace server (help uses only-rtserver)
BC_ARG_ALIAS([only_rts], [only-rts])
BC_ARG_ALIAS([only_rts], [only-librtserver])
BC_ARG_ALIAS([only_rts], [rts-only])
BC_ARG_ALIAS([only_rts], [rtserver-only])
BC_ARG_ALIAS([only_rts], [librtserver-only])
AC_MSG_CHECKING(whether to only build the raytrace server)
AC_MSG_RESULT($bc_only_rts)

# run-time debugging (help uses runtime-debug)
BC_ARG_ALIAS([build_runtime_debug], [run-time-debug])
BC_ARG_ALIAS([build_runtime_debug], [runtime-debugging])
BC_ARG_ALIAS([build_runtime_debug], [run-time-debugging])
AC_MSG_CHECKING(whether to use run-time debug checks)
AC_MSG_RESULT($bc_build_runtime_debug)

# 64-bit compilation (help uses 64bit-build)
BC_ARG_ALIAS([build_64bit], [64])
BC_ARG_ALIAS([build_64bit], [64bit])
BC_ARG_ALIAS([build_64bit], [64-bit])
BC_ARG_ALIAS([build_64bit], [64-build])
BC_ARG_ALIAS([build_64bit], [64-bit-build])
AC_MSG_CHECKING(whether to compile in 64-bit mode)
AC_MSG_RESULT($bc_build_64bit)

# force building libregex (help uses regex-build)
BC_ARG_ALIAS([build_regex], [regex])
BC_ARG_ALIAS([build_regex], [libregex])
BC_ARG_ALIAS([build_regex], [libregex-build])
AC_MSG_CHECKING(whether to build the regular expression library)
AC_MSG_RESULT($bc_build_regex)

# aliases to force building libpng (help uses png-build)
BC_ARG_ALIAS([build_png], [png])
BC_ARG_ALIAS([build_png], [libpng])
BC_ARG_ALIAS([build_png], [libpng-build])
AC_MSG_CHECKING(whether to build the PNG library)
AC_MSG_RESULT($bc_build_png)

# aliases to force building zlib (help uses zlib-build)
BC_ARG_ALIAS([build_zlib], [zlib])
BC_ARG_ALIAS([build_zlib], [libz])
BC_ARG_ALIAS([build_zlib], [libz-build])
AC_MSG_CHECKING(whether to build the zlib compression library)
AC_MSG_RESULT($bc_build_zlib)

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
AC_MSG_CHECKING(whether to build the Utah Raster Toolkit)
AC_MSG_RESULT($bc_build_urt)

# aliases to force building tcl (help uses tcl-build)
BC_ARG_ALIAS([build_tcl], [tcl])
BC_ARG_ALIAS([build_tcl], [libtcl])
BC_ARG_ALIAS([build_tcl], [libtcl-build])
AC_MSG_CHECKING(whether to build the Tcl library)
AC_MSG_RESULT($bc_build_tcl)

# aliases to force building tk (help uses tk-build)
BC_ARG_ALIAS([build_tk], [tk])
BC_ARG_ALIAS([build_tk], [libtk])
BC_ARG_ALIAS([build_tk], [libtk-build])
AC_MSG_CHECKING(whether to build the Tk library)
AC_MSG_RESULT($bc_build_tk)

# aliases to force building tkimg (help uses tkimg-build)
BC_ARG_ALIAS([build_tkimg], [tkimg])
BC_ARG_ALIAS([build_tkimg], [tkimg-build])
AC_MSG_CHECKING(whether to build the Tkimg library)
AC_MSG_RESULT($bc_build_tkimg)

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
AC_MSG_CHECKING(whether to build the incrTcl library)
AC_MSG_RESULT($bc_build_itcl)

# aliases to force building iwidgets (help uses iwidgets-build)
BC_ARG_ALIAS([build_iwidgets], [iwidgets])
BC_ARG_ALIAS([build_iwidgets], [install-iwidgets])
AC_MSG_CHECKING(whether to install the iwidgets library)
AC_MSG_RESULT($bc_build_iwidgets)

# aliases to force building blt (help uses blt-build)
BC_ARG_ALIAS([build_blt], [blt])
BC_ARG_ALIAS([build_blt], [libblt])
BC_ARG_ALIAS([build_blt], [blt-build])
BC_ARG_ALIAS([build_blt], [libblt-build])
AC_MSG_CHECKING(whether to build the Blt library)
AC_MSG_RESULT($bc_build_blt)

# aliases to force building termlib (help uses termlib-build)
BC_ARG_ALIAS([build_termlib], [termlib])
BC_ARG_ALIAS([build_termlib], [termcap])
BC_ARG_ALIAS([build_termlib], [libtermlib])
BC_ARG_ALIAS([build_termlib], [libtermcap])
BC_ARG_ALIAS([build_termlib], [termcap-build])
BC_ARG_ALIAS([build_termlib], [libtermlib-build])
BC_ARG_ALIAS([build_termlib], [libtermcap-build])
AC_MSG_CHECKING(whether to build the termlib library)
AC_MSG_RESULT($bc_build_termlib)

# jove aliases (help uses jove)
BC_ARG_ALIAS([build_jove], [jove-build])
AC_MSG_CHECKING(whether to compile jove or not)
AC_MSG_RESULT($bc_build_jove)

# pro/e plugin aliases (help uses pro-engineer-plugin)
BC_ARG_ALIAS([build_proe], [pro-engineer])
BC_ARG_ALIAS([build_proe], [proe])
BC_ARG_ALIAS([build_proe], [proe-plugin])
AC_MSG_CHECKING(whether to compile the Pro-Engineer plugin)
AC_MSG_RESULT($bc_build_proe)

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
AC_MSG_CHECKING(whether to install example geometry databases)
AC_MSG_RESULT($bc_install_geometry)

# optimized aliases (help uses optimized)
BC_ARG_ALIAS([use_optimized], [optimize])
BC_ARG_ALIAS([use_optimized], [optimization])
BC_ARG_ALIAS([use_optimized], [optimizations])
AC_MSG_CHECKING(whether to enable optimized compilation)
AC_MSG_RESULT($bc_use_optimized)

# verbose warning aliases (help uses warnings)
BC_ARG_ALIAS([use_warnings], [verbose])
BC_ARG_ALIAS([use_warnings], [warning])
BC_ARG_ALIAS([use_warnings], [warnings])
BC_ARG_ALIAS([use_warnings], [verbose-warnings])
BC_ARG_ALIAS([use_warnings], [warnings-verbose])
AC_MSG_CHECKING(whether to enable verbose compilation warnings)
AC_MSG_RESULT($bc_use_warnings)

# debug aliases (help uses debug)
BC_ARG_ALIAS([use_debug], [debugging])
AC_MSG_CHECKING(whether to disable debug mode compilation)
AC_MSG_RESULT($bc_use_debug)

# profiling aliases (help uses profiling)
BC_ARG_ALIAS([use_profiling], [profile])
BC_ARG_ALIAS([use_profiling], [profiled])
AC_MSG_CHECKING(whether to enable profile mode compilation)
AC_MSG_RESULT($bc_use_profiling)

# with java aliases (help uses jdk)
BC_ARG_WITH_ALIAS([with_java], [java])
AC_MSG_CHECKING(whether to compile with the Java Development Kit)
AC_MSG_RESULT($bc_with_java)


# make sure multiple "only" options were not given
if test "x$bc_only_benchmark" != "xno" ; then
	if test "x$bc_only_rts" != "xno" ; then
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_WARN([Requesting BOTH benchmark and librtserver "only"?])
		AC_MSG_WARN([Building both benchmark and librtserver])
		AC_MSG_NOTICE([{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{])
	fi
fi

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
AC_MSG_RESULT([(2 of 9)])

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

if test "x$bc_use_doxygen" = "xyes" ; then
	AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
	if test "x$DOXYGEN" = "x" ; then
		BC_RETRY_CONFIGURE([$0])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_ERROR([*** Unable to find doxygen ***])
	fi
fi
AC_MSG_CHECKING(whether to enable doxygen)
AC_MSG_RESULT($bc_use_doxygen)


dnl ***************************
dnl *** Check for libraries ***
dnl ***************************

AC_MSG_CHECKING([... l i b r a r i e s ])
AC_MSG_RESULT([(3 of 9)])

dnl check if there is a threading library that links
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

dnl check if a math library links
m_link_works=no
LIBM=""
AC_CHECK_LIB(m, cos, m_link_works=yes ; LIBM="-lm")

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

dnl check if libtcl library links
tcl_link_works=no
LIBTCL=""
AC_CHECK_LIB(tcl8.6, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.6",
    [AC_CHECK_LIB(tcl8.5, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.5",
	[AC_CHECK_LIB(tcl8.4, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl8.4",
	    [AC_CHECK_LIB(tcl, Tcl_Main, tcl_link_works=yes ; LIBTCL="-ltcl")]
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

dnl check if libitcl library links
itcl_link_works=no
LIBITCL=""
AC_CHECK_LIB(itcl3.3, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.3",
    [AC_CHECK_LIB(itcl3.2, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl3.2",
        [AC_CHECK_LIB(itcl, Itcl_Init, itcl_link_works=yes ; LIBITCL="-litcl")]
    )]
)

dnl check if libitk library links
itk_link_works=no
LIBITK=""
AC_CHECK_LIB(itk3.3, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.3",
    [AC_CHECK_LIB(itk3.2, Itk_Init, itk_link_works=yes ; LIBITK="-litk3.2",
        [AC_CHECK_LIB(itk, Itk_Init, itk_link_works=yes ; LIBITK="-litk")]
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

dnl check for java
JAVA_CFLAGS=
JAVA_LDFLAGS=
if test "x$bc_with_java" = "xyes" ; then
	JAVA_CFLAGS="-I$bc_with_java_val/include -I$bc_with_java_val/include/`uname | tr [A-Z] [a-z]`"
fi
# XXX this should go away and become proper library or compiler option checks
case "$target" in
	*-*-darwin*)
		JAVA_CFLAGS="-framework JavaVM"
		JAVA_LDFLAGS="-shrext .jnilib -framework JavaVM"
		bc_with_java=yes
		;;
esac
AC_SUBST(JAVA_CFLAGS)
AC_SUBST(JAVA_LDFLAGS)


dnl *************************
dnl *** Check for headers ***
dnl *************************

AC_MSG_CHECKING([... h e a d e r s ])
AC_MSG_RESULT([(4 of 9)])

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
	sys/socket.h \
	sys/stat.h \
	sys/statfs.h \
	sys/sysctl.h \
	sys/sysinfo.h \
	sys/sysmp.h \
	sys/time.h \
	sys/timeb.h \
	sys/types.h \
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
	wchar.h \
	xosdefs.h \
)

AC_CHECK_HEADER(sys/sched.h,
		AC_DEFINE(HAVE_SYS_SCHED_H, 1, Define to 1 if you have the <sys/sched.h> header file.),,
		[
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#include <sys/sched.h>
])

# XXX cached opengl header checks cause bad things to happen without a configure ERROR halt

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

#################### Begin java check ##############################
OLD_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $JAVA_CFLAGS"
found_jni_h=no
AC_CHECK_HEADER(JavaVM/jni.h, 
		AC_DEFINE(HAVE_JAVAVM_JNI_H,1,[Define to 1 if you have the '<JavaVM/jni.h>' header file])
		found_jni_h=yes	)
AC_CHECK_HEADER(jni.h,
		AC_DEFINE(HAVE_JNI_H,1,[Define to 1 if you have the '<jni.h>' header file])
		found_jni_h=yes )
search_for_jni_h_more=no
if test "x$found_jni_h" = "xno" ; then
	if test "x$bc_with_java" != "xno" ; then
		bc_with_java=no
		search_for_jni_h_more=yes
		if test "x$bc_only_benchmark" != "xno" ; then
			if test "x$bc_only_rts" = "xno" ; then
				search_for_jni_h_more=no
			fi
		fi
	fi
else
	# even though --with-jdk was not given, header was found and is needed
	if test "x$bc_only_rts" != "xno" ; then
		bc_with_java=yes
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
		bc_with_java=yes
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
CPPFLAGS="$OLD_CPPFLAGS"
###################### End java check ##############################


dnl **********************************
dnl *** Check for types/structures ***
dnl **********************************

AC_MSG_CHECKING([... t y p e s ])
AC_MSG_RESULT([(5 of 9)])

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

AC_MSG_CHECKING([... c o m p i l e r ])
AC_MSG_RESULT([(6 of 9)])

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
if test "x$bc_use_optimized" != "xno" ; then
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
if test "x$bc_use_debug" != "xno" ; then
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
if test "x$bc_use_profiling" != "xno" ; then
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
if test "x$bc_use_warnings" != "xno" ; then
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
AC_MSG_RESULT([(7 of 9)])

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
	getenv \
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


dnl *********************************
dnl *** Check for system services ***
dnl *********************************

AC_MSG_CHECKING([... s e r v i c e s ])
AC_MSG_RESULT([(8 of 9)])

dnl *** SDL ***
AC_PATH_PROG(SDL_CONFIG_PROG, sdl-config)
if test "x$SDL_CONFIG_PROG" != "x" ; then
	AM_PATH_SDL(1.2.0,,AC_MSG_ERROR([Cannot find SDL.]))
fi
AC_SUBST(SDL_CFLAGS)
AC_SUBST(SDL_LIBS)


dnl *** java ***
if test "x$bc_only_rts" != "xno" ; then
	AC_MSG_CHECKING(if java seems to be available)
	if test "x$bc_with_java" = "xno" ; then
		AC_MSG_RESULT(no)
		BC_RETRY_CONFIGURE([$0])
		AC_MSG_NOTICE([}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}])
		AC_MSG_NOTICE([Try adding --with-jdk=/path/to/jdk to find jni.h])
		AC_MSG_ERROR([*** Cannot locate jni.h for building librtserver ***])
	else 
		AC_MSG_NOTICE([$bc_with_java ($bc_with_java_val)])
	fi
	AC_MSG_RESULT(yes)
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
			BC_RETRY_CONFIGURE([$0])
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
			BC_RETRY_CONFIGURE([$0])
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
if test "x$build_brlcad_tcl" = "xyes" ; then
	TCL='${top_builddir}/src/other/libtcl/libtcl${TCL_VERSION}.la '"${SOCKET}"
	AC_MSG_RESULT(yes)
else
	if test "x$tcl_link_works" = "xyes" ; then
		TCL="$LIBTCL"
	fi
	AC_MSG_RESULT(no)
fi
AC_SUBST(TCL)


dnl *** Tk ***
dnl See if libtk seems to work
tk_works=no
AC_MSG_CHECKING(for Tk library functionality)
PRELIBS="$LIBS"
if test "x$tk_link_works" = "xyes" ; then
	LIBS="$LIBS $LIBTK $LIBTCL"
fi
AC_TRY_RUN([
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
			BC_RETRY_CONFIGURE([$0])
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
build_brlcad_tk=yes
AC_MSG_CHECKING(whether to build Tk)
dnl Need to update version number(s) in src/other/libtk/Makefile.am too
TK_VERSION="8.4"
AC_DEFINE_UNQUOTED([TK_VERSION], "$TK_VERSION", "Version of Tk")
AC_SUBST(TK_VERSION)
TK_LIBRARY='${prefix}/lib/tk${TK_VERSION}'
AC_SUBST(TK_LIBRARY)
if test "x$build_brlcad_tk" = "xyes" ; then
	TK='${top_builddir}/src/other/libtk/libtk${TK_VERSION}.la'
	AC_MSG_RESULT(yes)
else
	TK="$LIBTK"
	AC_MSG_RESULT(no)
fi
AC_SUBST(TK)

dnl figure out whether we need to build Tkimg
TKIMG_VERSION="1.3"
AC_SUBST(TKIMG_VERSION)
if test "x$bc_build_tkimg" = "xyes" ; then
   build_brlcad_tkimg=yes
   TKIMG='${top_builddir}/src/other/tkimg/tkimg${TKIMG_VERSION}.la'
   AC_MSG_RESULT(yes)
else
   build_brlcad_tkimg=no
   TKIMG=""
   AC_MSG_RESULT(no)
fi

dnl *** incrTcl ***
dnl See if libitcl and libitk seem to work, make sure it's not dependant upon libtcl
itcl_works=no
AC_MSG_CHECKING(for incrTcl library functionality)
PRELIBS="$LIBS"
PRECPPFLAGS="$CPPFLAGS"
if test "x$itcl_link_works" = "xyes" ; then
	LIBS="$LIBS $LIBITCL $LIBTCL"
	CPPFLAGS="$CPPFLAGS -Iinclude"
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
	CPPFLAGS="$PRECPPFLAGS"
fi
AC_MSG_RESULT($itcl_works)

dnl figure out whether we need to build incrTcl
build_brlcad_itcl="no (using system)"
if test "x$bc_build_itcl" = "xyes" ; then
	build_brlcad_itcl=yes
else
	if test "x$bc_build_itcl" = "xno" ; then
		if test "x$itcl_works" = "xno" ; then
			BC_RETRY_CONFIGURE([$0])
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
    const char *cmd = "package require iwidgets";
    interp = Tcl_CreateInterp();
    if (Itcl_Init(interp) == TCL_ERROR) {
	return 1;
    }
    if (Tcl_Eval(interp, cmd) != TCL_OK || interp->result[0] != '\0') {
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
			BC_RETRY_CONFIGURE([$0])
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

dnl figure out whether we need to build Blt
BLT_VERSION="2.4"
AC_SUBST(BLT_VERSION)
BLT_LIBRARY='${prefix}/lib/blt${BLT_VERSION}'
AC_SUBST(BLT_LIBRARY)
if test "x$bc_build_blt" = "xyes" ; then
   build_brlcad_blt=yes
   BLT='${top_builddir}/src/other/blt/libblt${BLT_VERSION}.la'
   AC_MSG_RESULT(yes)
else
   build_brlcad_blt=no
   BLT=""
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
			BC_RETRY_CONFIGURE([$0])
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

# XXX nasty temp hack for backwards cake compatibility.
if test "x$host_os" = "xirix6.5" ; then
	# XXX the code that relies on this will be going away, but until then..
	if test "x$build_brlcad_64bit" = "xyes" ; then
		CFLAGS="$CFLAGS -DIRIX64=65"
	else
		CFLAGS="$CFLAGS -DIRIX=6"
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


dnl **************************************
dnl *** Configure Makefiles and output ***
dnl **************************************

AC_MSG_CHECKING([... o u t p u t ])
AC_MSG_RESULT([(9 of 9)])

AM_CONDITIONAL(BUILD_TCL, [test "x$build_brlcad_tcl" = "xyes"])
AM_CONDITIONAL(BUILD_TK, [test "x$build_brlcad_tk" = "xyes"])
AM_CONDITIONAL(BUILD_TKIMG, [test "x$build_brlcad_tkimg" = "xyes"])
AM_CONDITIONAL(BUILD_ITCL, [test "x$build_brlcad_itcl" = "xyes"])
AM_CONDITIONAL(BUILD_IWIDGETS, [test "x$build_brlcad_iwidgets" = "xyes"])
AM_CONDITIONAL(BUILD_BLT, [test "x$build_brlcad_blt" = "xyes"])
AM_CONDITIONAL(BUILD_PNG, [test "x$build_brlcad_png" = "xyes"])
AM_CONDITIONAL(BUILD_REGEX, [test "x$build_brlcad_regex" = "xyes"])
AM_CONDITIONAL(BUILD_ZLIB, [test "x$build_brlcad_zlib" = "xyes"])
AM_CONDITIONAL(BUILD_URT, [test "x$build_brlcad_urt" = "xyes"])
AM_CONDITIONAL(BUILD_TERMLIB, [test "x$build_brlcad_termlib" = "xyes"])
AM_CONDITIONAL(BUILD_JOVE, [test "x$bc_build_jove" = "xyes"])
AM_CONDITIONAL(BUILD_PROE, [test "x$bc_build_proe" = "xyes"])
AM_CONDITIONAL(INSTALL_MODELS, [test "x$bc_install_geometry" = "xyes"])

AM_CONDITIONAL(JAVA, [test "x$bc_with_java" != "xno"])

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

# hack to work around the irix compiler crashing on the libtool btclsh
# relink attempt when we get to tclscripts.
BTCLSH_DEPENDENCY=""
if test "x$build_vendor" = "xsgi" ; then
    if test "x$compiler" = "xmipspro" ; then
	BTCLSH_DEPENDENCY=".libs/lt-btclsh"
    fi
fi
AC_SUBST(BTCLSH_DEPENDENCY)

dnl Need to update version number(s) in src/other/libtcl/Makefile.am
TCL_LIBRARY='${prefix}/lib/tcl${TCL_VERSION}'
AC_SUBST(TCL_LIBRARY)
TCL_PACKAGE_PATH="'${libdir}'"
AC_SUBST(TCL_PACKAGE_PATH)
TCLSTUB='${top_builddir}/src/other/libtcl/generic/libtclstub.la'
AC_SUBST(TCLSTUB)

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

TKSTUB='${top_builddir}/src/other/libtk/generic/libtkstub.la'
AC_SUBST(TKSTUB)

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
	doc/html/manuals/ReleaseNotes/Makefile
	doc/html/manuals/archer/Makefile
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
	misc/brlcad.spec
	misc/macosx/Makefile
	misc/macosx/Resources/Makefile
	misc/macosx/Resources/ReadMe.rtfd/Makefile
	misc/macosx/Resources/Welcome.rtfd/Makefile
	misc/pro-engineer/Makefile
	misc/pro-engineer/mk
	misc/pro-engineer/protk.dat
	misc/vfont/Makefile
	misc/win32-msvc/Makefile
	pix/Makefile
	sh/Makefile
	src/Makefile
	src/adrt/Makefile
	src/adrt/bench/Makefile
	src/adrt/doc/Makefile
	src/adrt/libcommon/Makefile
	src/adrt/librender/Makefile
	src/adrt/libtexture/Makefile
	src/adrt/libtie/Makefile
	src/adrt/libtienet/Makefile
	src/adrt/libutil/Makefile
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
	src/archer/plugins/Makefile
	src/archer/plugins/Wizards/Makefile
	src/archer/plugins/Wizards/tankwizardIA/Makefile
	src/archer/plugins/Wizards/tankwizardIA/images/Makefile
	src/brlman/Makefile
	src/brlman/awf
	src/brlman/brlman
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
	src/other/tkimg/Makefile
	src/other/tkimg/base/Makefile
	src/other/tkimg/png/Makefile
	src/other/tkimg/pngtcl/Makefile
	src/other/tkimg/zlibtcl/Makefile
	src/other/blt/Makefile
	src/other/blt/src/Makefile
	src/other/blt/library/Makefile
	src/other/blt/library/dd_protocols/Makefile
	src/patch/Makefile
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
	src/unigraphics/Makefile
	src/util/Makefile
	src/vas4/Makefile
	src/vdeck/Makefile
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
echo Build Tcl ............................: $build_brlcad_tcl
echo Build Tk .............................: $build_brlcad_tk
echo Build Tkimg ..........................: $build_brlcad_tkimg
echo Build Itcl/Itk .......................: $build_brlcad_itcl
echo Build IWidgets .......................: $build_brlcad_iwidgets
echo Build Blt ..........................: $build_brlcad_blt
echo Build libpng .........................: $build_brlcad_png
echo Build libregex .......................: $build_brlcad_regex
echo Build zlib ...........................: $build_brlcad_zlib
echo Build termlib ........................: $build_brlcad_termlib
echo Build Utah Raster Toolkit.............: $build_brlcad_urt
echo Build jove ...........................: $bc_build_jove
echo 
echo Java Developer Kit support ...........: $bc_with_java
echo X11 support ..........................: yes
echo Open GL support ......................: yes
echo Enable run-time debugging ............: $bc_build_runtime_debug
echo
echo Build 64-bit release .................: $build_brlcad_64bit
echo Build optimized release ..............: $bc_use_optimized
echo Build debug release ..................: $bc_use_debug
echo Build profile release ................: $bc_use_profiling
echo Build static libraries ...............: $enable_static
echo Build shared/dynamic libraries .......: $enable_shared
echo Print verbose compilation warnings ...: $bc_use_warnings
echo
echo Only build benchmark suite ...........: $bc_only_benchmark
echo Only build librtserver ...............: $bc_only_rts
echo Generate doxygen documentation .......: $bc_use_doxygen
echo Install example geometry models ......: $bc_install_geometry
echo
if test "x$time_elapsed" != "xunknown" ; then
echo Elapsed configuration time ...........: $time_elapsed
fi
echo ---
echo "$0 complete, type 'make' to begin building"
echo
