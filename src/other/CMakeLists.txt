#-----------------------------------------------------------------------
# Local Copies of External Libraries
#
# BRL-CAD depends on a variety of external libraries and tools -
# rather than fail if those requirements are not satisfied, we build
# local copies at need.
#
# There are three overall approaches to the handling of these
# dependencies:
#
# 1.  Auto - detect system libraries and use them if suitable,
#     otherwise build and use the local copy.  This is the default
#     approach.
#
# 2.  Bundled - regardless of system conditions, build and use all
#     bundled libraries.
#
# 3.  System - fail to build if the system libraries do not satisfy
#     requirements.  This is primarily useful for distributions that
#     want to ensure packages are using external libraries.
#
# In addition to the broad toplevel control, individual libraries can
# also be overridden - for example, if the toplevel setting is for
# Bundled libs, it is still possible to request a system library in
# individual cases.
#
#-----------------------------------------------------------------------

# Clear all BRL-CAD defined CMake flags
CLEAR_BUILD_FLAGS()

# Restore CMake's original flags
RESTORE_CACHED_BUILD_FLAGS()

# Quiet all warnings in this directory
if(BRLCAD_DISABLE_SRC_OTHER_WARN)
  add_definitions(-w)
endif(BRLCAD_DISABLE_SRC_OTHER_WARN)

# Ideally we wouldn't need this, but in a few cases we're using
# BRL-CAD's include dirs.
include_directories(
  ${BRLCAD_BINARY_DIR}/include
  ${BRLCAD_SOURCE_DIR}/include
  )

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# For lower build levels, some of the third party components are not
# needed.  define some variables we can use for testing.
set(BRLCAD_LEVEL2 0)
set(BRLCAD_LEVEL3 0)
if(NOT BRLCAD_ENABLE_TARGETS)
  set(BRLCAD_LEVEL2 1)
  set(BRLCAD_LEVEL3 1)
else(NOT BRLCAD_ENABLE_TARGETS)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 1)
    set(BRLCAD_LEVEL2 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 1)
  if(${BRLCAD_ENABLE_TARGETS} GREATER 2)
    set(BRLCAD_LEVEL3 1)
  endif(${BRLCAD_ENABLE_TARGETS} GREATER 2)
endif(NOT BRLCAD_ENABLE_TARGETS)

# At the start, clear the src/other subdirs list so repeated
# configures will correctly add the required directories
set(SRC_OTHER_ADDED_DIRS "" CACHE STRING "initialize 3rd party sub-directories list" FORCE)
mark_as_advanced(SRC_OTHER_ADDED_DIRS)

#-----------------------------------------------------------------------------
# Load some CMake macros to handle the special case of third party libraries.
include(${BRLCAD_CMAKE_DIR}/ThirdParty.cmake)


# libregex library -  often needed by tools, so do this one first.
set(regex_ALIASES ENABLE_REGEX)
set(regex_DESCRIPTION "
Option for enabling and disabling compilation of the Regular
Expression Library provided with BRL-CAD's source distribution.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libregex REGEX regex regex_DESCRIPTION ALIASES ${regex_ALIASES})
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)

CHECK_LIBRARY_EXISTS(c regcomp "" HAVE_LIBC_REGEX)
if(BUILD_SHARED_LIBS AND HAVE_LIBC_REGEX AND "${BRLCAD_REGEX}" MATCHES "BUNDLED")
  # Our tests indicate that the system libc defines regex symbols, but
  # we're still building with our local libregex.
  #
  # Platform differences in default linker behavior make it difficult
  # to guarantee that our libregex symbols will override libc. We'll
  # avoid the issue by renaming our libregex symbols to be
  # incompatible with libc.

  # First we add definitions to our config file to rename all of the
  # regex function calls in our first-party code.
  CONFIG_H_APPEND(BRLCAD "#define regcomp  libregex_regcomp\n")
  CONFIG_H_APPEND(BRLCAD "#define regerror libregex_regerror\n")
  CONFIG_H_APPEND(BRLCAD "#define regexec  libregex_regexec\n")
  CONFIG_H_APPEND(BRLCAD "#define regfree  libregex_regfree\n")

  # Second, we add the same definitions to libregex to rename the
  # function declarations and definitions to match the calls.
  #
  # Note that this is necessary despite the fact that libregex/regex.h
  # includes common.h. Our config file is intentionally omitted from
  # common.h until after the third party dirs are processed, so the
  # above definitions don't affect libregex.
  set_property(DIRECTORY libregex APPEND PROPERTY COMPILE_DEFINITIONS regcomp=libregex_regcomp)
  set_property(DIRECTORY libregex APPEND PROPERTY COMPILE_DEFINITIONS regerror=libregex_regerror)
  set_property(DIRECTORY libregex APPEND PROPERTY COMPILE_DEFINITIONS regexec=libregex_regexec)
  set_property(DIRECTORY libregex APPEND PROPERTY COMPILE_DEFINITIONS regfree=libregex_regfree)
endif(BUILD_SHARED_LIBS AND HAVE_LIBC_REGEX AND "${BRLCAD_REGEX}" MATCHES "BUNDLED")

# Same deal for zlib Library - common requirement, deal with it up front
set(zlib_ALIASES ENABLE_ZLIB ENABLE_LIBZ)
set(zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libz ZLIB zlib zlib_DESCRIPTION ALIASES ${zlib_ALIASES})
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libz/CTestTestfile.cmake)

#-----------------------------------------------------------------------------

#---------------------------------------------------------------------------------
# Local Copies of Tools
#---------------------------------------------------------------------------------

# Because we sometimes need to override system versions of tools, it
# is essential that the variables containing the executable to be run
# for each of the following tools use full paths - if just the name of
# the target is used, there is a chance that name is the same as the
# executable name itself.  In that case, if the system executable is
# in the path, it may get called instead of resolving to the compiled
# exectuable associated with the CMake target.  (Information from the
# CMake devs indicates this is not expected behavior, but at some
# point during the development efforts a problem apparently related to
# this came up.)  On single configuration systems we can reference
# specify the full path to the binary directory at CMake configure
# time, but this isn't possible for multi-configuration systems like
# Visual Studio since that path changes at run-time.  For those
# situations, CMake provides a variable CMAKE_CFG_INTDIR that will
# insert the correct logic for config-dependent paths.

# For those wanting to use a system version of the LEMON parser
# generator from sqlite, remember that the presence of /usr/bin/lemon
# is not enough.  LEMON needs a template file, lempar.c, and by
# default it needs it in the same directory as /usr/bin/lemon.  The
# typical approach to avoiding that requirement is to patch lemon,
# like this Gentoo ebuild:
#
# http://gentoo-overlays.zugaina.org/gentoo-zh/portage/dev-util/lemon/
#
# LEMON packages for other major Linux/BSD distros will do the same.
# BRL-CAD's FindLEMON.cmake macros will look for the template file in
# the executable directory first, and if not there will check in
# /usr/share/lemon (the location used by several distributions.)  If
# your distribution has a working lemon with the lempar.c template
# file in a custom location, specify the full path to the template
# with the variable LEMON_TEMPLATE - something like:
#
# cmake .. -DLEMON_TEMPLATE=/etc/lemon/lempar.c
#
# This is not to tell LEMON what template to use - that information is
# usually hardcoded in LEMON itself - but to let FindLEMON.cmake know
# there is a working LEMON installation.
set(lemon_ALIASES ENABLE_LEMON)
set(lemon_DESCRIPTION "
Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
if("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
  set(LEMON_TEMPLATE "${BRLCAD_SOURCE_DIR}/src/other/lemon/lempar.c")
endif("${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED" OR "${BRLCAD_LEMON}" MATCHES "BUNDLED")
THIRD_PARTY_EXECUTABLE(lemon lemon "BRLCAD_LEVEL2" lemon_ALIASES lemon_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/lemon/Makefile)
# Unless some parent logic has already defined the macro, we need to
# load LEMON_Util to be able to define LEMON targets.
if(NOT COMMAND LEMON_TARGET)
  include(${BRLCAD_CMAKE_DIR}/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)

set(re2c_ALIASES ENABLE_RE2C)
set(re2c_DESCRIPTION "
Option for enabling and disabling compilation of the re2c scanner
utility provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(re2c re2c "BRLCAD_LEVEL2" re2c_ALIASES re2c_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/re2c/Makefile)

# Unless some parent logic has already defined the macro, we need to
# load RE2C_Util to be able to define RE2C targets.

if(NOT COMMAND RE2C_TARGET)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/RE2C_Util.cmake)
endif(NOT COMMAND RE2C_TARGET)

# Perplex is set up as a src/other sub-build, but at this time it is
# entirely a BRL-CAD effort and not distributed or maintained as its
# own project.

# For all BRL-CAD projects, we will use the perplex template in
# src/other/perplex
set(PERPLEX_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/perplex/perplex_template.c" CACHE STRING "Perplex template file" FORCE)
mark_as_advanced(PERPLEX_TEMPLATE)

set(perplex_ALIASES ENABLE_PERPLEX)
set(perplex_DESCRIPTION "
Option for enabling and disabling compilation of the perplex scanner
generator provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.  perplex requires a working re2c.
")
THIRD_PARTY_EXECUTABLE(perplex perplex "BRLCAD_LEVEL2" perplex_ALIASES perplex_DESCRIPTION NOSYS)
# Unless some parent logic has already defined the macro, we need to
# load PERPLEX_Util to be able to define PERPLEX targets.
if(NOT COMMAND PERPLEX_TARGET)
  include(${BRLCAD_SOURCE_DIR}/misc/CMake/PERPLEX_Util.cmake)
endif(NOT COMMAND PERPLEX_TARGET)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/perplex/Makefile)

# libxml and libxslt tools for DocBook processing.  We build just
# enough library support to get these two tools working.
set(xsltproc_ALIASES ENABLE_XSLTPROC)
set(xsltproc_DESCRIPTION "
Option for enabling and disabling compilation of the xsltproc XML
transformation utility provided with BRL-CAD's source distribution.
Used for DocBook documentation processing.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
Depends on BRLCAD_EXTRADOCS.
")
THIRD_PARTY_EXECUTABLE(xsltproc xmltools "BRLCAD_EXTRADOCS;BRLCAD_LEVEL3" xsltproc_ALIASES xsltproc_DESCRIPTION)

# Used only when validating XML for schema compliance
# (BRLCAD_EXTRADOCS_VALIDATE must be on to use this, although it is compiled
# regardless if the xmltools subdirectory build is added.)
set(xmllint_ALIASES ENABLE_XMLLINT)
set(xmllint_DESCRIPTION "
Option for enabling and disabling compilation of the xmllint XML
validation utility provided with BRL-CAD's source distribution.  Used
for DocBook documentation validation.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(xmllint xmltools "BRLCAD_EXTRADOCS_VALIDATE;BRLCAD_LEVEL3" xmllint_ALIASES xmllint_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/xmltools/include)

# --------------------------------------------------------------------
# Third Party Sources
# --------------------------------------------------------------------
# Ignore the boost subdir - it's only used for the boost headers and
# is not compiled
CMAKEFILES(boost)

# The incrtTcl subdirectory isn't directly referenced, but its
# subdirectories are used - mark incrTcl as ignored
CMAKEFILES(incrTcl)

# Ignore the osl subdir
CMAKEFILES(osl)

# libtermlib Library Only on WIN32 will the combination of the build
# search tests and libtermlib fail to find a valid term, so set
# variables accordingly
if(NOT WIN32)
  set(HAVE_TERMLIB ON CACHE BOOL "Have some termlib" FORCE)
ELSE(NOT WIN32)
  set(BRLCAD_TERMLIB_BUILD "System" CACHE STRING "Disable libtermlib build on WIN32." FORCE)
  mark_as_advanced(BRLCAD_TERMLIB_BUILD)
  set(HAVE_TERMLIB OFF CACHE BOOL "Have some termlib" FORCE)
endif(NOT WIN32)
mark_as_advanced(HAVE_TERMLIB)

set(termlib_ALIASES ENABLE_TERMLIB)
set(termlib_DESCRIPTION "
Option for enabling and disabling compilation of the termlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO. (Except when building with Visual Studio, where it is
disabled. Windows does not support the termlib API.)
")
THIRD_PARTY(libtermlib TERMLIB termlib termlib_DESCRIPTION REQUIRED_VARS HAVE_TERMLIB
  BRLCAD_LEVEL2 ALIASES ${termlib_ALIASES})
if(BRLCAD_TERMLIB_BUILD)
  set(HAVE_TERMCAP_H ON CACHE BOOL "src/other termlib is on" FORCE)
  CONFIG_H_APPEND(BRLCAD "#undef HAVE_TERMCAP_H\n#cmakedefine HAVE_TERMCAP_H 1\n")
  mark_as_advanced(HAVE_TERMCAP_H)
endif(BRLCAD_TERMLIB_BUILD)

# libpng Library - Checks for ZLIB, so need to handle libpng AFTER
# zlib to set the variables if using a local copy of zlib.  If not
# using a local copy of zlib, FindZLIB results will be identical in
# both cases so there is no danger of harming the libpng setup.  The
# PNG CMake system needs some options set and some variables
# translated to fit BRL-CAD's standard assumptions, so handle that
# here as well.  BRL-CAD needs PNG's IO - turn it on (i.e. turn off
# the disabling flags)

set(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
mark_as_advanced(PNG_NO_CONSOLE_IO)
set(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
mark_as_advanced(PNG_NO_STDIO)
set(SKIP_INSTALL_EXPORT ON CACHE BOOL "We dont't want export for this application" FORCE)
mark_as_advanced(SKIP_INSTALL_EXPORT)
set(PNG_MAN_DIR ${MAN_DIR} CACHE STRING "Set PNG_MAN_DIR to the global MAN_DIR" FORCE)
mark_as_advanced(PNG_MAN_DIR)
set(CMAKE_INSTALL_LIBDIR ${LIB_DIR})
set(png_ALIASES ENABLE_PNG)
set(png_DESCRIPTION "
Option for enabling and disabling compilation of the Portable Network
Graphics library provided with BRL-CAD's source distribution.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
# Note - we don't actually know the libpng build target name for the
# shared library at this point - use png as a stub and override
# once we know what PNG_LIB_NAME is.
THIRD_PARTY(libpng PNG png png_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ${png_ALIASES})
if(BRLCAD_PNG_BUILD)
  # PNG_LIB_NAME doesn't automatically propagate to toplevel due to
  # scoping of CMake variables - use get_directory_property to recover
  # the value (which is later put in the CACHE)

  get_directory_property(PNG_LIBRARY DIRECTORY libpng DEFINITION PNG_LIB_NAME)

  # On Haiku the find_library call sets this variable directly, which is not
  # how we handle things elsewhere in BRL-CAD - unset the variable in that
  # situation so the remainder of the build logic works normally
  if("${M_LIBRARY}" MATCHES "NOTFOUND")
    unset(M_LIBRARY CACHE)
  endif("${M_LIBRARY}" MATCHES "NOTFOUND")
ELSE(BRLCAD_PNG_BUILD)
  # The PNG CMake file seems to set PNG_PNG_INCLUDE_DIR rather than
  # PNG_INCLUDE_DIR, so a generic macro won't pick up the setting -
  # handle it here.
  set(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
endif(BRLCAD_PNG_BUILD)

# Make sure this value is in the cache, regardless of
# BRLCAD_PNG_BUILD, so it can be referenced unconditionally elsewhere.
set(PNG_LIBRARY "${PNG_LIBRARY}" CACHE STRING "libpng name for targets" FORCE)

DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libpng/CTestTestfile.cmake)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng.pc)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng-config)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng${CMAKE_SHARED_LIBRARY_SUFFIX})
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng${CMAKE_STATIC_LIBRARY_SUFFIX})
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng.pc)
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng-config)
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng${CMAKE_SHARED_LIBRARY_SUFFIX})
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng${CMAKE_STATIC_LIBRARY_SUFFIX})
# Other PNG options to mark as advanced
mark_as_advanced(PNGARG)
mark_as_advanced(PNG_DEBUG)
mark_as_advanced(PNG_SHARED)
mark_as_advanced(PNG_STATIC)
mark_as_advanced(PNG_TESTS)
mark_as_advanced(uname_executable)


# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on
# and the include directory for utahrle are different, so override the
# macro's setting of UTAHRLE_INCLUDE_DIR here.

set(utahrle_ALIASES ENABLE_UTAHRLE)
set(utahrle_DESCRIPTION "
Option for enabling and disabling compilation of the Utah Raster
Toolkit library provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libutahrle UTAHRLE utahrle utahrle_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL3
  ALIASES ${utahrle_ALIASES} FLAGS NOSYS)
if(BRLCAD_UTAHRLE_BUILD)
  set(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
  set(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
  mark_as_advanced(UTAHRLE_INCLUDE_DIR)
  mark_as_advanced(BRLCAD_UTAHRLE_INCLUDE_DIR)
endif(BRLCAD_UTAHRLE_BUILD)


# URTToolkit - Programs, not a library - needs libutahrle, but the
# "check for system install" logic will be different here.  Ignore for
# now, revisit later.

if(BRLCAD_LEVEL3)
  add_subdirectory(URToolkit)
endif(BRLCAD_LEVEL3)
include(${CMAKE_CURRENT_SOURCE_DIR}/URToolkit.dist)
CMAKEFILES_IN_DIR(URToolkit_ignore_files URToolkit)

# Tcl/Tk presents a number of complexities for BRL-CAD and CMake.
# BRL-CAD requires far more knowledge of the details of a Tcl/Tk
# installation than most programs, which unfortunately means the
# standard FindTCL.cmake is not sufficient. We have our own FindTCL
# routine which handles Tcl/Tk and its stubs, so trigger this using
# the THIRD_PARTY macro for TCL.

include(${BRLCAD_SOURCE_DIR}/misc/CMake/TCL_PKGINDEX.cmake)

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
set(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
set(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

if(BRLCAD_ENABLE_TK)
  set(TCL_REQUIRE_TK 1)
ELSE(BRLCAD_ENABLE_TK)
  set(TCL_REQUIRE_TK 0)
endif(BRLCAD_ENABLE_TK)

set(tcl_ALIASES ENABLE_TCL)
set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl tcl_DESCRIPTION ALIASES ${tcl_ALIASES})
if(BRLCAD_TCL_BUILD)
  # Set the variables ourselves since FindTCL.cmake doesn't know about our build
  set(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
  set(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
  set(TCL_LIBRARY tcl CACHE STRING "TCL_LIBRARY" FORCE)
  set(TCL_LIBRARIES tcl CACHE STRING "TCL_LIBRARY" FORCE)
  set(TCL_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_CONF_PREFIX" FORCE)
  set(TCL_STUB_LIBRARY tclstub CACHE STRING "TCL_LIBRARY" FORCE)
  set(TCL_STUB_LIBRARIES tclstub CACHE STRING "TCL_LIBRARY" FORCE)
  get_directory_property(TCL_INCLUDE_DIRS DIRECTORY tcl DEFINITION TCL_INCLUDE_DIRS)
  set(TCL_INCLUDE_DIRS "${TCL_INCLUDE_DIRS}" CACHE STRING "Tcl include paths" FORCE)
  set(TCL_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic" CACHE STRING "Tcl include path" FORCE)
  set(TCL_TCLSH_EXECUTABLE tclsh CACHE STRING "Tcl shell" FORCE)
  set(TCL_TCLSH tclsh CACHE STRING "Tcl shell" FORCE)
  get_directory_property(TCL_VERSION_MAJOR DIRECTORY tcl DEFINITION TCL_VERSION_MAJOR)
  set(TCL_VERSION_MAJOR "${TCL_VERSION_MAJOR}" CACHE STRING "Tcl MAJOR version" FORCE)
  get_directory_property(TCL_VERSION_MINOR DIRECTORY tcl DEFINITION TCL_VERSION_MINOR)
  set(TCL_VERSION_MINOR "${TCL_VERSION_MINOR}" CACHE STRING "Tcl MINOR version" FORCE)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_TCL_H 1\n")
  CONFIG_H_APPEND(BRLCAD "#define TCL_SYSTEM_INITTCL_PATH \"\"\n")
  include(${CMAKE_CURRENT_SOURCE_DIR}/tcl.dist)
  CMAKEFILES_IN_DIR(tcl_ignore_files tcl)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/Makefile)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/man1)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/man3)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/mann)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tcl/library/Makefile)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/tcl8)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/tcl8.5)
ELSE(BRLCAD_TCL_BUILD)
  if(${TCL_TCLSH_EXECUTABLE})
    BRLCAD_INCLUDE_FILE(tcl.h HAVE_TCL_H)
    # We're going to need the path to the system init.tcl for btclsh and bwish
    set(inittcl_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl\"
set fileID [open $filename \"w\"]
puts $fileID $auto_path
close $fileID
exit
")
    set(inittcl_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl.tcl")
    file(WRITE ${inittcl_scriptfile} ${inittcl_script})
    EXEC_PROGRAM(${TCL_TCLSH_EXECUTABLE} ARGS ${inittcl_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
    file(READ ${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl tcl_inittcl_raw)
    STRING(REGEX REPLACE "\n" "" tcl_inittcl_paths_1 ${tcl_inittcl_raw})
    STRING(REGEX REPLACE " " ";" tcl_inittcl_paths ${tcl_inittcl_paths_1})
    FIND_PATH(tcl_inittcl NAMES init.tcl PATHS ${tcl_inittcl_paths})
    mark_as_advanced(tcl_inittcl)
    CONFIG_H_APPEND(BRLCAD "#define TCL_SYSTEM_INITTCL_PATH \"${tcl_inittcl}\"\n")
  endif(${TCL_TCLSH_EXECUTABLE})
endif(BRLCAD_TCL_BUILD)
mark_as_advanced(TCL_COMPILE_DEBUG)
mark_as_advanced(TCL_COMPILE_STATS)
mark_as_advanced(TCL_DEBUG)
mark_as_advanced(TCL_ENABLE_64BIT)
mark_as_advanced(TCL_ENABLE_DLL_UNLOADING)
mark_as_advanced(TCL_ENABLE_LANGINFO)
mark_as_advanced(TCL_ENABLE_LOAD)
mark_as_advanced(TCL_OPTIMIZED)
mark_as_advanced(TCL_THREADS)
mark_as_advanced(TCL_TIMEZONE_DATA)
mark_as_advanced(M_LIBRARY)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
include(${BRLCAD_CMAKE_DIR}/ThirdParty_TCL.cmake)

# The first package to settle is Tk itself
set(tk_ALIASES ENABLE_TK)
set(tk_DESCRIPTION "
Option for enabling and disabling compilation of the Tk library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the BRLCAD_TCL option and testing first for a system version if
BRLCAD_BUNDLED_LIBS is also AUTO. Option for enabling and disabling
compilation of the Tk graphics package for Tcl provided with BRL-CAD's
source distribution.  Default is AUTO, auto-enabling if the BRLCAD_TCL
option is set to BUNDLED and testing first for a system version if
BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_TK is set to BUNDLED,
local copy is built even if a system version is present.  This option
also depends on BRLCAD_ENABLE_TK being ON.
")
THIRD_PARTY_TCL_PACKAGE(Tk tk "${TCL_TCLSH_EXECUTABLE}" "tcl" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL2" "tk" tk_ALIASES tk_DESCRIPTION)
if(BRLCAD_TK_BUILD)
  # Set the variables ourselves since FindTCL.cmake doesn't know about our build
  set(TCL_TK_LIBRARY tk CACHE STRING "TCL_TK_LIBRARY" FORCE)
  set(TCL_TK_CONF_PREFIX "NONE-CMake" CACHE STRING "TCL_TK_CONF_PREFIX" FORCE)
  set(TCL_LIBRARIES ${TCL_LIBRARIES} tk CACHE STRING "TCL_LIBRARIES" FORCE)
  set(TK_LIBRARY tk CACHE STRING "TK_LIBRARY" FORCE)
  set(TCL_STUB_LIBRARIES tkstub ${TCL_STUB_LIBRARIES} CACHE STRING "TCL_TK_STUBLIBRARIES" FORCE)
  set(TCL_TK_STUB_LIBRARY tkstub CACHE STRING "TCL_TK_STUB_LIBRARY" FORCE)
  set(TK_STUB_LIBRARY tkstub CACHE STRING "TK_STUB_LIBRARY" FORCE)
  set(TK_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tk/generic" CACHE STRING "Tk include path" FORCE)
  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TK_INCLUDE_PATH} CACHE STRING "Tcl include paths" FORCE)
  if(WIN32)
    set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib CACHE STRING "Tcl include paths" FORCE)
  endif(WIN32)
  set(TCL_WISH_EXECUTABLE wish CACHE STRING "Tk shell" FORCE)
  set(TK_WISH wish CACHE STRING "Tk shell" FORCE)
  if(WIN32)
    set(TK_SYSTEM_GRAPHICS "win32" CACHE STRING "Tk system graphics" FORCE)
  ELSEif(APPLE)
    if(OPENGL_USE_AQUA)
      set(TK_SYSTEM_GRAPHICS "aqua" CACHE STRING "Tk system graphics" FORCE)
    ELSE(OPENGL_USE_AQUA)
      set(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
    endif(OPENGL_USE_AQUA)
  ELSEif(UNIX)
    set(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
  endif(WIN32)
  CONFIG_H_APPEND(BRLCAD "#define HAVE_TK_H 1\n")
  mark_as_advanced(TK-ENABLE_FREETYPE)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/doc/Makefile)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man1)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man3)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/doc/mann)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/library/Makefile)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tk/pkgIndex.tcl)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/tk8.5)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/tk8.5.9)
ELSE(BRLCAD_TK_BUILD)
  if(BRLCAD_ENABLE_TK)
    BRLCAD_INCLUDE_FILE(tk.h HAVE_TK_H)
  endif(BRLCAD_ENABLE_TK)
endif(BRLCAD_TK_BUILD)
mark_as_advanced(TK_INCLUDE_PATH)
mark_as_advanced(TK_LIBRARY)
mark_as_advanced(TK_STUB_LIBRARY)
mark_as_advanced(TK_SYSTEM_GRAPHICS)
mark_as_advanced(TK_WISH)
mark_as_advanced(TK_ENABLE_XFT)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h
# file

if(BRLCAD_ENABLE_TK)
  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
  CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TK\n")
ELSE(BRLCAD_ENABLE_TK)
  set(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
endif(BRLCAD_ENABLE_TK)
mark_as_advanced(HAVE_TK)

# Tcl/Tk extensions need a variety of settings provided for them -
# take care of those here.  Because system Tcl/Tk installations are
# not guaranteed to have what is needed in the way of headers, go
# ahead and assign includedir settings based on the local sources even
# if system versions are enabled.  Ugly, but an unfortunate
# consequence of current Tcl/Tk coding styles

set(TCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tcl)
set(TK_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/tk)
set(ITCL_SRC_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl)
if(WIN32)
  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/win ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
  set(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib ${CMAKE_CURRENT_SOURCE_DIR}/tk/win ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
ELSE(WIN32)
  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic ${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix ${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath)
  set(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/tk/generic ${CMAKE_CURRENT_SOURCE_DIR}/tk/unix ${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps)
endif(WIN32)
set(ITCL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl/generic)
if(BRLCAD_TCL_BUILD)
  set(TCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
ELSE(BRLCAD_TCL_BUILD)
  get_filename_component(TCL_BIN_PREFIX ${TCL_LIBRARY} PATH)
endif(BRLCAD_TCL_BUILD)
if(BRLCAD_ENABLE_TK)
  if(BRLCAD_TK_BUILD)
    set(TK_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
    get_directory_property(TK_X11_INCLUDE_DIRS DIRECTORY tk DEFINITION TK_X11_INCLUDE_DIRS)
  ELSE(BRLCAD_TK_BUILD)
    if(TCL_TK_LIBRARY)
      get_filename_component(TK_BIN_PREFIX ${TCL_TK_LIBRARY} PATH)
    endif(TCL_TK_LIBRARY)
    if(X11_INCLUDE_DIR)
      set(TK_X11_INCLUDE_DIRS ${X11_INCLUDE_DIR})
    endif(X11_INCLUDE_DIR)
    if(X11_Xft_INCLUDE_PATH)
      set(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
	${X11_Xft_INCLUDE_PATH})
    endif(X11_Xft_INCLUDE_PATH)
    if(X11_Xrender_INCLUDE_PATH)
      set(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
	${X11_Xrender_INCLUDE_PATH})
    endif(X11_Xrender_INCLUDE_PATH)
    if(X11_Xscreensaver_INCLUDE_PATH)
      set(TK_X11_INCLUDE_DIRS ${TK_X11_INCLUDE_DIRS}
	${X11_Xscreensaver_INCLUDE_PATH})
    endif(X11_Xscreensaver_INCLUDE_PATH)
  endif(BRLCAD_TK_BUILD)
endif(BRLCAD_ENABLE_TK)


# First up, look for IncrTcl's Itcl.
set(itcl_ALIASES ENABLE_ITCL)
set(itcl_DESCRIPTION "
Option for enabling and disabling compilation of the IncrTcl package
for Tcl objects provided with BRL-CAD's source distribution.  Default
is AUTO, auto-enabling if the BRLCAD_TCL option is set to BUNDLED and
testing first for a system version if BRLCAD_TCL is set to AUTO or
SYSTEM.  If BRLCAD_ITCL is set to BUNDLED, local copy is built even if
a system version is present.
")
THIRD_PARTY_TCL_PACKAGE(Itcl incrTcl/itcl "${TCL_TCLSH_EXECUTABLE}" "tcl" "BRLCAD_LEVEL2" "itcl" itcl_ALIASES itcl_DESCRIPTION)
if(BRLCAD_ITCL_BUILD)
  set(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
  set(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
  get_directory_property(ITCL_INCLUDE_DIRS DIRECTORY incrTcl/itcl DEFINITION ITCL_INCLUDE_DIRS)
  set(ITCL_INCLUDE_DIRS "${ITCL_INCLUDE_DIRS}" CACHE STRING "Itcl include paths" FORCE)
  set(ITCL_VERSION "3.4" CACHE STRING "ITCL_VERSION" FORCE)
  set(ITCL_BIN_PREFIX ${CMAKE_BINARY_DIR}/${LIB_DIR})
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/incrTcl/itcl/pkgIndex.tcl)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/itcl3.4)
ELSE(BRLCAD_ITCL_BUILD)
  # We have Itcl package AND Itcl library - we're good.
  set(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
  set(ITCL_LIBRARY ${ITCL_LIBRARY} CACHE STRING "ITCL_LIBRARY" FORCE)
  if(ITCL_LIBRARY)
    get_filename_component(ITCL_BIN_PREFIX ${ITCL_LIBRARY} PATH)
  endif(ITCL_LIBRARY)
endif(BRLCAD_ITCL_BUILD)
CONFIG_H_APPEND(BRLCAD "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
mark_as_advanced(ITCL_LIBRARY)
mark_as_advanced(ITCL_STUB_LIBRARY)
mark_as_advanced(ITCL_INCLUDE_DIRS)
mark_as_advanced(ITCL_VERSION)

# Look for IncrTcl's Itk.
set(itk_ALIASES ENABLE_ITK)
set(itk_DESCRIPTION "
Option for enabling and disabling compilation of the IncrTcl itk
package for Tk objects provided with BRL-CAD's source distribution.
Default is AUTO, auto-enabling if the BRLCAD_TCL option is set to
BUNDLED and testing first for a system version if BRLCAD_TCL is set to
AUTO or SYSTEM.  If BRLCAD_ITK is set to BUNDLED, local copy is built
even if a system version is present.  This package will be disabled if
BRLCAD_ENABLE_TK is OFF.
")
THIRD_PARTY_TCL_PACKAGE(Itk incrTcl/itk "${TCL_WISH_EXECUTABLE}" "tcl;itcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "itk" itk_ALIASES itk_DESCRIPTION)
if(BRLCAD_ITK_BUILD)
  # does not handle C library settings.
  set(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
  set(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/incrTcl/itk/pkgIndex.tcl)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/itk3.4)
ELSE(BRLCAD_ITK_BUILD)
  # We have Itcl/Itk packages AND Itcl/Itk libraries - we're good.
  set(ITK_VERSION "${ITCL_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
  set(ITK_LIBRARY ${ITK_LIBRARY} CACHE STRING "ITK_LIBRARY" FORCE)
  if(ITK_LIBRARY)
    get_filename_component(ITK_BIN_PREFIX ${ITK_LIBRARY} PATH)
  endif(ITK_LIBRARY)
endif(BRLCAD_ITK_BUILD)
mark_as_advanced(ITK_VERSION)
mark_as_advanced(ITK_LIBRARY)

set(iwidgets_ALIASES ENABLE_IWIDGETS)
set(iwidgets_DESCRIPTION "
Option for enabling and disabling compilation of the IWidgets Tk
widget package provided with BRL-CAD's source distribution.  Default
is AUTO, auto-enabling if the BRLCAD_TCL option is set to BUNDLED and
testing first for a system version if BRLCAD_TCL is set to AUTO or
SYSTEM.  If BRLCAD_IWIDGETS is set to BUNDLED, local copy is built even if
a system version is present.  This package will be disabled if
BRLCAD_ENABLE_TK is OFF.
")
THIRD_PARTY_TCL_PACKAGE(Iwidgets iwidgets "${TCL_WISH_EXECUTABLE}" "tcl;tk;itcl;itk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" iwidgets_ALIASES iwidgets_DESCRIPTION)
if(BRLCAD_IWIDGETS_BUILD)
  set(IWIDGETS_VERSION "4.0.1" CACHE STRING "IWIDGETS_VERSION" FORCE)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/iwidgets/pkgIndex.tcl)
  DISTCLEAN(${CMAKE_BINARY_DIR}/lib/Iwidgets4.0.1)
ELSE(BRLCAD_IWIDGETS_BUILD)
  set(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
endif(BRLCAD_IWIDGETS_BUILD)
CONFIG_H_APPEND(BRLCAD "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
mark_as_advanced(IWIDGETS_VERSION)

set(tkhtml_ALIASES ENABLE_TKHTML)
set(tkhtml_DESCRIPTION "
Option for enabling and disabling compilation of the Tkhtml HTML
viewing package provided with BRL-CAD's source distribution.  Default
is AUTO, auto-enabling if the BRLCAD_TCL option is set to BUNDLED and
testing first for a system version if BRLCAD_TCL is set to AUTO or
SYSTEM.  If BRLCAD_TKHTML is set to BUNDLED, local copy is built even if
a system version is present.  This package will be disabled if
BRLCAD_ENABLE_TK is OFF.
")
THIRD_PARTY_TCL_PACKAGE(Tkhtml tkhtml "${TCL_WISH_EXECUTABLE}" "tcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tkhtml_ALIASES tkhtml_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tkhtml/pkgIndex.tcl)
DISTCLEAN(${CMAKE_BINARY_DIR}/lib/Tkhtml3.0)
#add_subdirectory(hv3)
#include(${CMAKE_CURRENT_SOURCE_DIR}/hv3.dist)
#CMAKEFILES_IN_DIR(hv3_ignore_files hv3)
#add_subdirectory(sqlite3)
#include(${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.dist)
#CMAKEFILES_IN_DIR(sqlite3_ignore_files sqlite3)
CMAKEFILES(hv3)
CMAKEFILES(sqlite3)

set(tkpng_ALIASES ENABLE_TKPNG)
set(tkpng_DESCRIPTION "
Option for enabling and disabling compilation of the tkpng PNG image
viewing package provided with BRL-CAD's source distribution.  Default
is AUTO, auto-enabling if the BRLCAD_TCL option is set to BUNDLED and
testing first for a system version if BRLCAD_TCL is set to AUTO or
SYSTEM.  If BRLCAD_TKPNG is set to BUNDLED, local copy is built even if
a system version is present.  This package will be disabled if
BRLCAD_ENABLE_TK is OFF.
")
THIRD_PARTY_TCL_PACKAGE(tkpng tkpng "${TCL_WISH_EXECUTABLE}" "tcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tkpng_ALIASES tkpng_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tkpng/pkgIndex.tcl)
DISTCLEAN(${CMAKE_BINARY_DIR}/lib/tkpng0.8)

set(tktable_ALIASES ENABLE_TKTABLE)
set(tktable_DESCRIPTION "
Option for enabling and disabling compilation of the Tktable graphical
table widget package provided with BRL-CAD's source distribution.
Default is AUTO, auto-enabling if the BRLCAD_TCL option is set to
BUNDLED and testing first for a system version if BRLCAD_TCL is set to
AUTO or SYSTEM.  If BRLCAD_TKTABLE is set to BUNDLED, local copy is built
even if a system version is present.  This package will be disabled if
BRLCAD_ENABLE_TK is OFF.
")
THIRD_PARTY_TCL_PACKAGE(Tktable tktable "${TCL_WISH_EXECUTABLE}" "tcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tktable_ALIASES tktable_DESCRIPTION)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tktable/pkgIndex.tcl)
DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tktable/tktable_cfg.h)
DISTCLEAN(${CMAKE_BINARY_DIR}/lib/Tktable2.10)

#set(tktreectrl_ALIASES ENABLE_TKTREECTRL)
#set(tktreectrl_DESCRIPTION "
#Option for enabling and disabling compilation of the TkTreeCtrl graphical
#table widget package provided with BRL-CAD's source distribution.
#Default is AUTO, auto-enabling if the BRLCAD_TCL option is set to
#BUNDLED and testing first for a system version if BRLCAD_TCL is set to
#AUTO or SYSTEM.  If BRLCAD_TKTREECTRL is set to BUNDLED, local copy is built
#even if a system version is present.  This package will be disabled if
#BRLCAD_ENABLE_TK is OFF.
#")
#THIRD_PARTY_TCL_PACKAGE(tktreectrl tktreectrl "${TCL_WISH_EXECUTABLE}" "tcl;tk"
#  "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tktreectrl_ALIASES tktreectrl_DESCRIPTION)
#DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/pkgIndex.tcl)
#DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/tktreectrl_cfg.h)
#DISTCLEAN(${CMAKE_BINARY_DIR}/lib/TkTreeCtrl2.4)

# OpenNURBS Library
set(opennurbs_ALIASES ENABLE_OPENNURBS)
set(opennurbs_DESCRIPTION "
Option for enabling and disabling compilation of the openNURBS library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openNURBS OPENNURBS openNURBS opennurbs_DESCRIPTION ALIASES
  ${opennurbs_ALIASES} FLAGS NOSYS)

# STEPcode 
# Need Lemon/Perplex to build STEP - conditionalize

# Set some options for the SCL build
set(SC_PYTHON_GENERATOR OFF)
set(SC_ENABLE_TESTING OFF)
set(SC_ENABLE_COVERAGE OFF)
set(SC_IS_SUBBUILD ON)
set(SC_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
set(INCLUDE_INSTALL_DIR ${INCLUDE_DIR})
set(BIN_INSTALL_DIR ${BIN_DIR})
set(LIB_INSTALL_DIR ${LIB_DIR})

set(sc_ALIASES ENABLE_SCL ENABLE_STEP ENABLE_STEP_CLASS_LIBRARIES)
set(sc_DESCRIPTION "
Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
set(IS_SUBBUILD_STASH ${IS_SUBBUILD})
set(IS_SUBBUILD ON)
set(SC_BUILD_SCHEMAS "" CACHE STRING "Disable schema builds for BRL-CAD" FORCE)
# Lots of "libraries" associated with stepcode - use "stepcode" as the
# stand-in for the build target var.
THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION REQUIRED_VARS LEMON_EXECUTABLE
  PERPLEX_EXECUTABLE BRLCAD_LEVEL3 ALIASES ${sc_ALIASES} RESET_VARS
  EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET FLAGS NOSYS)
if(BRLCAD_SC_BUILD)
  set(EXP2CXX_EXEC exp2cxx CACHE STRING "Express to C++ executable" FORCE)
  set(EXP2CXX_EXECUTABLE_TARGET exp2cxx CACHE STRING "Express to C++ executable target" FORCE)
  mark_as_advanced(EXP2CXX_EXEC)
  mark_as_advanced(EXP2CXX_EXECUTABLE_TARGET)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_cf.h.in)
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/stepcode/sc_version_string.h.txt)
endif(BRLCAD_SC_BUILD)
set(IS_SUBBUILD ${IS_SUBBUILD_STASH})
mark_as_advanced(SC_BUILD_TYPE)
mark_as_advanced(SC_BUILD_SCHEMAS)
mark_as_advanced(SC_GENERATE_LEXER_PARSER)
mark_as_advanced(SC_BUILD_SHARED_LIBS)
mark_as_advanced(SC_BUILD_STATIC_LIBS)
mark_as_advanced(SC_CPP_GENERATOR)
mark_as_advanced(SC_ENABLE_COVERAGE)
mark_as_advanced(SC_ENABLE_TESTING)
mark_as_advanced(SC_PYTHON_GENERATOR)
mark_as_advanced(SC_MEMMGR_ENABLE_CHECKS)
mark_as_advanced(SC_SDAI_ADDITIONAL_EXES_SRCS)
mark_as_advanced(SC_TRACE_FPRINTF)

# Clipper polygon clipping library - for now, we're going to use our copy
if(BRLCAD_LEVEL2)
  add_subdirectory(clipper)
  include(${CMAKE_CURRENT_SOURCE_DIR}/clipper.dist)
  CMAKEFILES_IN_DIR(clipper_ignore_files clipper)
  DISTCLEAN(${CMAKE_CURRENT_SOURCE_DIR}/clipper/Makefile)
  set(CLIPPER_LIBRARY "clipper" CACHE STRING "Clipper library" FORCE)
  set(CLIPPER_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/clipper" CACHE STRING "Directory containing clipper header" FORCE)
  mark_as_advanced(CLIPPER_LIBRARY)
  mark_as_advanced(CLIPPER_INCLUDE_DIR)
else(BRLCAD_LEVEL2)
  CMAKEFILES(clipper)
endif(BRLCAD_LEVEL2)

# dom2dox - Doc-O-Matic to Doxygen C++ source converter
include(${CMAKE_CURRENT_SOURCE_DIR}/dom2dox.dist)
CMAKEFILES_IN_DIR(dom2dox_ignore_files dom2dox)

# VDSlib - A View-Dependent Simplification and Rendering Library For
# the moment, this is marked NOSYS - it's possible that some Debian
# systems would have 0.9 of vdslib installed, but it's unmaintained
# and we're likely to be making changes.  If our own copy of VDSlib
# ever spins back off into its own project, revisit the NOSYS

set(libvds_ALIASES ENABLE_VDS)
set(libvds_DESCRIPTION "
Option for enabling and disabling compilation of the libvds triangle
simplification library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libvds VDS libvds libvds_DESCRIPTION ALIASES ${libvds_ALIASES} FLAGS NOSYS)
set(LIBVDS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libvds" CACHE STRING "Directory containing libvds headers." FORCE)

# gdiam

set(libgdiam_ALIASES ENABLE_GDIAM)
set(libgdiam_DESCRIPTION "
Option for enabling and disabling compilation of the libgdiam approximate
tight bounding box library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libgdiam GDIAM libgdiam libgdiam_DESCRIPTION ALIASES ${libgdiam_ALIASES}
  FLAGS NOSYS)
set(LIBGDIAM_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libgdiam" CACHE STRING "Directory containing libgdiam headers." FORCE)

# Bullet is not yet integrated as a src/other subbuild, but there is
# code that will make use of it if it is available.  If that code
# becomes sufficiently useful, Bullet will be integrated.  In the
# meantime, locate the find logic for that package here.

# Bullet physics library
if(BRLCAD_LEVEL2)
  find_package(Bullet)
  if(BULLET_FOUND)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_BULLET 1\n")
  endif(BULLET_FOUND)
  # Let the rest of the package know the results
  set(BULLET_LIBRARIES "${BULLET_LIBRARIES}" CACHE STRING "Bullet libs" FORCE) 
  set(BULLET_FOUND "${BULLET_FOUND}" CACHE BOOL "Bullet status" FORCE) 
  mark_as_advanced(BULLET_INCLUDE_DIR)
  mark_as_advanced(BULLET_FOUND)
  mark_as_advanced(BULLET_LIBRARIES)
endif(BRLCAD_LEVEL2)

# Adaptagrams is not yet integrated as a src/other subbuild, but there
# is code that will make use of it if it is available.  If that code
# becomes sufficiently useful, Adaptagrams will be integrated.  In the
# meantime, locate the find logic for that package here.

# Adaptagrams library
if(BRLCAD_LEVEL2)
  find_package(ADAPTAGRAMS)
  if(ADAPTAGRAMS_FOUND)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_ADAPTAGRAMS 1\n")
  endif(ADAPTAGRAMS_FOUND)
  set(ADAPTAGRAMS_LIBRARIES "${ADAPTAGRAMS_LIBRARIES}" CACHE STRING "Adaptagrams libs" FORCE) 
  set(ADAPTAGRAMS_FOUND "${ADAPTAGRAMS_FOUND}" CACHE BOOL "Adaptagrams status" FORCE) 
  mark_as_advanced(ADAPTAGRAMS_FOUND)
  mark_as_advanced(ADAPTAGRAMS_LIBRARIES)
endif(BRLCAD_LEVEL2)

# Poly2Tri CDT library
add_subdirectory(poly2tri)
include(${CMAKE_CURRENT_SOURCE_DIR}/poly2tri.dist)
CMAKEFILES_IN_DIR(poly2tri_ignore_files poly2tri)
DISTCLEAN(${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/Makefile)
set(P2T_LIBRARY "p2t" CACHE STRING "Poly2Tri library" FORCE)
set(P2T_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
mark_as_advanced(P2T_LIBRARY)
mark_as_advanced(P2T_INCLUDE_DIR)
mark_as_advanced(P2T_SHARED)
mark_as_advanced(P2T_STATIC)
mark_as_advanced(P2T_TESTS)

# The jama/tnt headers are installed by default - BRL-CAD requires the
# altered headers to build (I think?)
add_subdirectory(tnt)
set(TNT_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/tnt" CACHE STRING "Directory containing tnt headers" FORCE)
CMAKEFILES(tnt)
mark_as_advanced(TNT_INCLUDE_DIR)

# Eigen is used for advanced linear algebra functionality It consists
# entirely of headers and does not need to be installed.
#
# TODO - replace jama/tnt with Eigen
#
CMAKEFILES(Eigen)

# TCLAP is used for option parsing - consists entirely of headers
# and does not need to be installed, but does need to generate a
# config file
add_subdirectory(tclap)
CMAKEFILES(tclap)

if(BRLCAD_ENABLE_OSL)
  add_subdirectory(osl)
endif(BRLCAD_ENABLE_OSL)

CMAKEFILES(README)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

