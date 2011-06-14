#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.
#
# RCS: @(#) $Id$

#-----------------------------------------------------------------------
# Sample configure.in for Tcl Extensions.
# See the TEA sample extension for description of structure and items.
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# This very first macro is used to verify that the configure script can 
# find the sources.  The argument to AC_INIT should be a unique filename
# for this package, and can be a relative path, such as:
#
# AC_INIT(generic/tcl.h)
#-----------------------------------------------------------------------

AC_INIT([shellicon], [2.3])
AC_SUBST([PACKAGE_PATCHLEVEL], [2.3.2])
AC_DEFINE_UNQUOTED(PACKAGE_PATCHLEVEL, "$PACKAGE_PATCHLEVEL")

TEA_INIT([3.9])

AC_CONFIG_AUX_DIR(tclconfig)

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

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

TEA_ADD_SOURCES([shellicon.c])
TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([-I\"`${CYGPATH} ${srcdir}/../generic`\"])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([])

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

#TEA_PUBLIC_TCL_HEADERS
TEA_PRIVATE_TCL_HEADERS

#TEA_PUBLIC_TK_HEADERS
TEA_PRIVATE_TK_HEADERS

#--------------------------------------------------------------------
# For Unix/Tk builds, make sure that the X libraries/headers are found.
#--------------------------------------------------------------------

TEA_PATH_X

# Needed for OS X ppx/intel image handling
AC_C_BIGENDIAN

#--------------------------------------------------------------------
# __CHANGE__
# A few miscellaneous platform-specific items:
#
# Define a special symbol for Windows (BUILD_sample in this case) so
# that we create the export library with the dll.  See sha1.h on how
# to use this.
#
# Windows creates a few extra files that need to be cleaned up.
# You can add more files to clean if your extension creates any extra
# files.
#
# Define any extra compiler flags in the PACKAGE_CFLAGS variable.
# These will be appended to the current set of compiler flags for
# your system.
#--------------------------------------------------------------------

OUTPUTFILES=Makefile

if test "${TEA_PLATFORM}" = "windows" ; then
    TEA_ADD_LIBS([gdi32.lib user32.lib comctl32.lib ole32.lib shell32.lib])

    # Find rc.exe or windres.exe.
    # Defined in winrc.m4.
    TREECTRL_PROG_RC

    # X86|AMD64|IA64 for manifest
    AC_SUBST(MACHINE)

    # PKG_MANIFEST -> VC_MANIFEST_EMBED_DLL -> MAKE_SHARED_LIB
    TEA_ADD_MANIFEST([shellicon.dll.manifest])

    # Create shellicon.dll.manifest from shellicon.dll.manifest.in.
    # shellicon.dll.manifest is included by shellicon.rc.
    # If building with a Microsoft compiler that generates
    # shelliconNN.dll.manifest (to pick the correct MSVCRT runtime) then
    # that manifest is merged with mine using mt.exe (see the Makefile
    # rule for PKG_LIB_FILE).
    OUTPUTFILES="Makefile shellicon.dll.manifest"

    TREECTRL_ADD_RC([shellicon.rc])
fi
if test "${TEA_WINDOWINGSYSTEM}" = "aqua"; then
    TEA_ADD_LIBS([-framework Carbon])
fi

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
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

if test "${enable_symbols+set}" = set && test "$enable_symbols" != no; then
    TEA_ADD_CFLAGS([-DTREECTRL_DEBUG])
fi

#--------------------------------------------------------------------
# Everyone should be linking against the Tcl stub library.  If you
# can't for some reason, remove this definition.  If you aren't using
# stubs, you also need to modify the SHLIB_LD_LIBS setting below to
# link against the non-stubbed Tcl library.  Add Tk too if necessary.
#--------------------------------------------------------------------

AC_DEFINE(USE_TCL_STUBS)
AC_DEFINE(USE_TK_STUBS)

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

TEA_PROG_TCLSH
TEA_PROG_WISH

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------

AC_OUTPUT([$OUTPUTFILES])

