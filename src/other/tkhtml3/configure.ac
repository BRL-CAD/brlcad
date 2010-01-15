#!/bin/sh 
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run when building Tkhtml
dnl	to configure the system for the local environment.
dnl
dnl You need at least autoconf 2.59
dnl
#
# RCS: @(#) $Id$

#-----------------------------------------------------------------------
# Sample configure.ac for Tcl Extensions.  The only places you should
# need to modify this file are marked by the string __CHANGE__
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
#-----------------------------------------------------------------------

AC_INIT([Tkhtml],[3.0])
TEA_INIT([3.7])

AC_CONFIG_AUX_DIR(tclconfig)
AM_INIT_AUTOMAKE

# AX_PREFIX_CONFIG_H is a macro taken from the autotools macro archive.
# The typical configure generated 'config.h' can be renamed and 
# preprocessor symbols in it can be prefixed in order to avoid clashes 
# with Apache's config.h
# 
# As of today (27 Sept 2008) this macro is not part of autotools and
# it's in the 'm4' directory
#

AX_PREFIX_CONFIG_H([tkhtml_config.h],[tkhtml])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

#
# Init automake stuff
#
AC_DISABLE_STATIC
AM_INIT_AUTOMAKE([foreign])

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

TEA_PATH_TKCONFIG
TEA_LOAD_TKCONFIG

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC, AC_PROG_INSTALL and a few others to create
# the basic setup necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

#
# For automake to be happy, we also need AC_PROG_LIBTOOL
#
# the LIBTOOL_DEPS macro sets up for automatic updating of the
# libtool script if it becomes out of date, as per libtool docs
#
AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

#-----------------------------------------------------------------------
# __CHANGE__
# Specify stub library C source files to compile in 
# TEA_ADD_STUB_SOURCES, and runtime Tcl library files in 
# TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, and 
# PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

TEA_ADD_INCLUDES([])
TEA_ADD_LIBS([])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([])

#--------------------------------------------------------------------
# __CHANGE__
# A few miscellaneous platform-specific items:
#
# Define a special symbol for Windows (BUILD_sample in this case) so
# that we create the export library with the dll.
#
# Windows creates a few extra files that need to be cleaned up.
# You can add more files to clean if your extension creates any extra
# files.
#
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

if test "${TEA_PLATFORM}" = "windows" ; then
    AC_DEFINE(BUILD_tkhtml,1,[Define to define the BUILD_tkhtml define (?)])
    CLEANFILES="pkgIndex.tcl *.lib *.dll *.exp *.ilk *.pdb vc*.pch"
else
    CLEANFILES="pkgIndex.tcl"
fi
AC_SUBST(CLEANFILES)

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
#TEA_PRIVATE_TCL_HEADERS

TEA_PUBLIC_TK_HEADERS
#TEA_PRIVATE_TK_HEADERS
#TEA_PATH_X

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
# This auto-enables if Tcl was compiled threaded.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# Everyone should be linking against the Tcl stub library.  If you
# can't for some reason, remove this definition.  If you aren't using
# stubs, you also need to modify the SHLIB_LD_LIBS setting below to
# link against the non-stubbed Tcl library.  Add Tk too if necessary.
#--------------------------------------------------------------------

#
# stubs are causing segmentation violations when mod_tkhtml is getting
# loaded into apache.  it would be nice if we did no stubs for this and
# still did stubs for libtkhtml and libtkhtmlparser, which seem to be OK.
#
#AC_DEFINE(USE_TCL_STUBS,1,[Define to link against the Tcl stub library])
#AC_DEFINE(USE_TK_STUBS)

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Find tclsh so that we can run pkg_mkIndex to generate the pkgIndex.tcl
# file during the install process.  Don't run the TCLSH_PROG through
# ${CYGPATH} because it's being used directly by make.
# Require that we use a tclsh shell version 8.2 or later since earlier
# versions have bugs in the pkg_mkIndex routine.
# Add WISH as well if this is a Tk extension.
#--------------------------------------------------------------------

# We have to either find a working tclsh or be told where it is 
# using --with-tclsh
AC_ARG_WITH(tclsh, [  --with-tclsh=FILE       location of a working tclsh executable], with_tclsh=${withval})

#
# allow them to override the tclsh we think we found
#
if test x"${with_tclsh}" != x ; then
    AC_MSG_CHECKING([for tclsh])
    if test -f "${with_tclsh}" ; then
        TCLSH_PROG=${with_tclsh}
        AC_MSG_RESULT([manually set by --with-tclsh=$TCLSH_PROG])
    else
        AC_MSG_ERROR([No tclsh at place specified by --with-tclsh (${with_tclsh})])
    fi
    AC_SUBST(TCLSH_PROG)
else
TEA_PROG_TCLSH
fi

#TEA_PROG_WISH

# apache base directory

AC_DEFUN([HANDLE_TCL_PACKAGE_PATH],[
    # shamelessly stolen from TEA_LOAD_TCLCONFIG
    AC_MSG_CHECKING([Debian TCL_PACKAGE_PATH workaround])
        if test -f "${TCL_BIN_DIR}/tclConfig.sh" ; then
           AC_MSG_RESULT([loading])
           . "${TCL_BIN_DIR}/tclConfig.sh"
        else
           AC_MSG_RESULT([could not find ${TCL_BIN_DIR}/tclConfig.sh])
        fi
    # If we have multiple paths, pull the first one, add end slash if needed
	res=`echo ${TCL_PACKAGE_PATH} | \
             grep '[[^\]] ' | \
             sed -e 's/\([[^\]]\)\( \)\(.*\)$/\1/'  -e 's/\([[^\/]]\)$/\1\//' `
	if test x"${res}" != x; then
	    TCL_PACKAGE_PATH=${res}
            AC_SUBST(TCL_PACKAGE_PATH)
        fi
])

AC_DEFUN([GET_TKHTML_BASE], [
    TKHTML_BASE_INCLUDE=`pwd`/src	
    AC_SUBST(TKHTML_BASE_INCLUDE)
])

AC_DEFUN([TKHTML_TCL_LIB],[
    AC_ARG_WITH(
        tkhtml_target_dir,
        [ --with-tkhtml-target-dir=DIR  Tkhtml tcl library target dir ],,
		with_tkhtml_target_dir="${apache_base}/lib/tkhtml${PACKAGE_VERSION}"
    )
    AC_MSG_CHECKING(for Tkhtml tcl library target)
    TKHTML_TCL_TARGET=${with_tkhtml_target_dir}
    AC_SUBST(TKHTML_TCL_TARGET)
    AC_MSG_RESULT([$with_tkhtml_target_dir])
])

GET_TKHTML_BASE
TKHTML_TCL_LIB
DISPLAY_TKHTML_VERSION

AC_DEFINE_UNQUOTED(TKHTMLLIB_DESTDIR,"${TKHTML_TCL_TARGET}",[The path to the tkhtml tcl library])
AC_DEFINE_UNQUOTED(NAMEOFEXECUTABLE,"${TCLSH_PROG}",[The path to a working tclsh executable])

# We need to use the package path for the installation procedure.  On
#Debian linux TCL_PACKAGE_PATH may have more than one path in the
#TCL_PACKAGE_PATH variable, so we have to handle this
HANDLE_TCL_PACKAGE_PATH
AC_SUBST(TCL_PACKAGE_PATH)

# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
