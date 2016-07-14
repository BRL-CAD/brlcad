#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.

#-----------------------------------------------------------------------
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
#-----------------------------------------------------------------------

AC_INIT([tdbcodbc], [1.0.4])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

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

#TEA_PATH_TKCONFIG
#TEA_LOAD_TKCONFIG

#--------------------------------------------------------------------
# Load the tclooConfig.sh file on Tcl versions where oo is an
# extension.
#--------------------------------------------------------------------

if test "${TCL_MAJOR_VERSION}" -lt 8 ; then
    AC_MSG_ERROR([${PACKAGE_NAME} ${PACKAGE_VERSION} requires Tcl 8.6 or newer.
Found configuration for Tcl ${TCL_VERSION}.])
fi
if test "${TCL_MAJOR_VERSION}" -eq 8 ; then
   if test "${TCL_MINOR_VERSION}" -lt 6 ; then
      AC_MSG_ERROR([${PACKAGE_NAME} ${PACKAGE_VERSION} requires Tcl 8.6 or newer.
Found configuration for Tcl ${TCL_VERSION}.])
   fi
   TCLOO_CFLAGS=''
fi
TCL_VERSION_REQ=8.6; AC_SUBST(TCL_VERSION_REQ)

TEA_PATH_CONFIG(tdbc)
TEA_LOAD_CONFIG(tdbc)
# The next bit probably ought to be in TEA_LOAD_CONFIG
AC_MSG_WARN([Looking for "${tdbc_BIN_DIR}/Makefile"])
if test -f "${tdbc_BIN_DIR}/Makefile" ; then
    AC_MSG_WARN([Found Makefile - using build include spec and lib specs for tdbc])
    tdbc_INCLUDE_SPEC=${tdbc_BUILD_INCLUDE_SPEC}
    tdbc_LIBRARY_PATH=${tdbc_BUILD_LIBRARY_PATH}
fi
AC_SUBST(tdbc_LIBRARY_PATH)
AC_SUBST(TDBC_VERSION)
AC_SUBST(tdbc_BIN_DIR)
AC_SUBST(TDBC_LIB_FILE)

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC and a few others to create the basic setup
# necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER
AC_C_INLINE
AC_CHECK_TYPE([long long],[
    AC_DEFINE([HAVE_LONG_LONG],[1])
    AC_CHECK_SIZEOF([long long])
],[],[])
AC_CHECK_SIZEOF([long])
AC_DEFINE([DONT_TD_VOID],[1])

#-----------------------------------------------------------------------
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

TEA_ADD_SOURCES(generic/tdbcodbc.c generic/odbcStubInit.c)
TEA_ADD_HEADERS(generic/fakesql.h generic/odbcStubs.h)
if test "${TCL_MAJOR_VERSION}" -eq 8 ; then
  if test "${TCL_MINOR_VERSION}" -eq 5 ; then
    TEA_ADD_INCLUDES([${TCLOO_INCLUDE_SPEC}])
    TEA_ADD_LIBS([${TCLOO_STUB_LIB_SPEC}])
  fi
fi
TEA_ADD_CFLAGS([${TCLOO_CFLAGS}])
TEA_ADD_INCLUDES([${tdbc_INCLUDE_SPEC}])
if test "x${TEA_PLATFORM}" = "xwindows" -a "x${GCC}" != "xyes" ; then
    TEA_ADD_LIBS([\"`${CYGPATH} ${tdbc_STUB_LIB_PATH}`\"])
else
    TEA_ADD_LIBS([${tdbc_STUB_LIB_SPEC}])
fi
TEA_ADD_CFLAGS([${tdbc_CFLAGS}])
TEA_ADD_STUB_SOURCES()
TEA_ADD_TCL_SOURCES([library/tdbcodbc.tcl])

#--------------------------------------------------------------------
#
# You can add more files to clean if your extension creates any extra
# files by extending CLEANFILES.
# Add pkgIndex.tcl if it is generated in the Makefile instead of ./configure
# and change Makefile.in to move it from CONFIG_CLEAN_FILES to BINARIES var.
#
# A few miscellaneous platform-specific items:
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

#CLEANFILES="$CLEANFILES pkgIndex.tcl"

#--------------------------------------------------------------------
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS

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

AC_DEFINE(USE_TCL_STUBS, 1, [Use Tcl stubs])

#--------------------------------------------------------------------
# We need the INT2PTR and PTR2INT macros, so make sure that intptr_t
# is defined.
#--------------------------------------------------------------------

AC_CHECK_HEADERS(stdint.h sys/types.h)
AC_CHECK_TYPE([intptr_t], [
    AC_DEFINE([HAVE_INTPTR_T], 1, [Do we have the intptr_t type?])], [
    AC_CACHE_CHECK([for pointer-size signed integer type], tcl_cv_intptr_t, [
    for tcl_cv_intptr_t in "int" "long" "long long" none; do
	if test "$tcl_cv_intptr_t" != none; then
	    AC_COMPILE_IFELSE([AC_LANG_BOOL_COMPILE_TRY([AC_INCLUDES_DEFAULT],
		    [[sizeof (void *) <= sizeof ($tcl_cv_intptr_t)]])],
		[tcl_ok=yes], [tcl_ok=no])
	    test "$tcl_ok" = yes && break; fi
    done])
    if test "$tcl_cv_intptr_t" != none; then
	AC_DEFINE_UNQUOTED([intptr_t], [$tcl_cv_intptr_t], [Signed integer
	   type wide enough to hold a pointer.])
    fi
])
AC_CHECK_TYPE([uintptr_t], [
    AC_DEFINE([HAVE_UINTPTR_T], 1, [Do we have the uintptr_t type?])], [
    AC_CACHE_CHECK([for pointer-size unsigned integer type], tcl_cv_uintptr_t, [
    for tcl_cv_uintptr_t in "unsigned int" "unsigned long" "unsigned long long" \
	    none; do
	if test "$tcl_cv_uintptr_t" != none; then
	    AC_COMPILE_IFELSE([AC_LANG_BOOL_COMPILE_TRY([AC_INCLUDES_DEFAULT],
		    [[sizeof (void *) <= sizeof ($tcl_cv_uintptr_t)]])],
		[tcl_ok=yes], [tcl_ok=no])
	    test "$tcl_ok" = yes && break; fi
    done])
    if test "$tcl_cv_uintptr_t" != none; then
	AC_DEFINE_UNQUOTED([uintptr_t], [$tcl_cv_uintptr_t], [Unsigned integer
	   type wide enough to hold a pointer.])
    fi
])

#--------------------------------------------------------------------
# Locate the ws2_32 library; we need it for ntohs and friends
#--------------------------------------------------------------------

AC_CHECK_LIB(ws2_32,main,TEA_ADD_LIBS([-lws2_32]),[
    AC_CHECK_LIB(winsock,main,TEA_ADD_LIBS([-lwinsock]))
])

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Determine the name of the tclsh and/or wish executables in the
# Tcl and Tk build directories or the location they were installed
# into. These paths are used to support running test cases only,
# the Makefile should not be making use of these paths to generate
# a pkgIndex.tcl file or anything else at extension build time.
#--------------------------------------------------------------------

TEA_PROG_TCLSH

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------

CONFIGURE_OUTPUTS="Makefile pkgIndex.tcl config.cache config.log config.status"
AC_SUBST(CONFIGURE_OUTPUTS)

AC_OUTPUT([Makefile pkgIndex.tcl])
