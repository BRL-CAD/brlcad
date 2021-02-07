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
RESTORE_CACHED_BUILD_FLAGS(_CMAKE_DEFAULT)

# Quiet all warnings in this directory
DISABLE_WARNINGS()

# For src/other, be liberal about standards
unset(CMAKE_C_EXTENSIONS)
unset(CMAKE_C_STANDARD_REQUIRED)
unset(CMAKE_C_STANDARD)
unset(CMAKE_CXX_EXTENSIONS)
unset(CMAKE_CXX_STANDARD_REQUIRED)
unset(CMAKE_CXX_STANDARD)

###############################################################################
# We never want our build products to be identified by searches here - we
# either want system installed versions or nothing.  Set CMAKE_IGNORE_PATH
# values accordingly.  NOTE: we must specify EXPLICITY all directories to
# ignore as CMAKE_IGNORE_PATH does not automatically ignore subdirectories:
# https://cmake.org/cmake/help/latest/variable/CMAKE_IGNORE_PATH.html#variable:CMAKE_IGNORE_PATH
set(CMAKE_IGNORE_PATH
  ${CMAKE_BINARY_DIR}/${BIN_DIR}
  ${CMAKE_BINARY_DIR}/${LIB_DIR}
  ${CMAKE_INSTALL_PREFIX}/${BIN_DIR}
  ${CMAKE_INSTALL_PREFIX}/${LIB_DIR}
  )

###############################################################################
# Unlike the misc/tools directory components built in src/other are part of
# the distribution, not just the compilation.  Hence we need to make sure of
# some basic compatibility between the build settings.

# We don't want to try to mix 32 and 64 bit builds so despite clearing the
# parent build flags,  set the 32/64 bit flag specifically if we have it from
# the parent build.
if(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64 AND DEFINED 64BIT_FLAG)
  ADD_NEW_FLAG(C 64BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 64BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 64BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 64BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "64BIT" AND NOT CMAKE_CL_64 AND DEFINED 64BIT_FLAG)
if(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT MSVC AND DEFINED 32BIT_FLAG)
  ADD_NEW_FLAG(C 32BIT_FLAG ALL)
  ADD_NEW_FLAG(CXX 32BIT_FLAG ALL)
  ADD_NEW_FLAG(SHARED_LINKER 32BIT_FLAG ALL)
  ADD_NEW_FLAG(EXE_LINKER 32BIT_FLAG ALL)
endif(${CMAKE_WORD_SIZE} MATCHES "32BIT" AND NOT MSVC AND DEFINED 32BIT_FLAG)

###############################################################################

# Set a variable that will tell build logic in subdirectories that there is a
# parent build.  This will in some cases disable attempts to define settings
# such as RPATH values that we need the parent build to manage.
set(HAVE_PARENT_BUILD 1)

# Ideally we wouldn't need this, but in a few cases we're using BRL-CAD's
# include dirs.
include_directories(
  "${BRLCAD_BINARY_DIR}/include"
  "${BRLCAD_SOURCE_DIR}/include"
  )

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

# If the option is available (and in a number of cases we make sure it is)
# don't install the headers as part of the BRL-CAD package.  We compile
# against these, but we may have adjusted them compared to system versions
# and we don't want them mixing with the system version for an external
# code via it including our version of the header
set(SKIP_INSTALL_HEADERS TRUE CACHE  BOOL "Don't install src/other headers" FORCE)
mark_as_advanced(SKIP_INSTALL_HEADERS)

# libregex library -  often needed by tools, so do this one first.
set(regex_DESCRIPTION "
Option for enabling and disabling compilation of the Regular
Expression Library provided with BRL-CAD's source distribution.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
set(REGEX_PREFIX_STR "brl_")
THIRD_PARTY(libregex REGEX regex regex_DESCRIPTION ALIASES ENABLE_REGEX)
BRLCAD_INCLUDE_FILE(regex.h HAVE_REGEX_H)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libregex/regex.h")
SetTargetFolder(regex "Third Party Libraries")
SetTargetFolder(regex-static "Third Party Libraries")
if (NOT BRLCAD_REGEX_BUILD)
  set(REGEX_LIBRARIES ${REGEX_LIBRARY} CACHE STRING "REGEX_LIBRARIES" FORCE)
  set(REGEX_INCLUDE_DIRS "${REGEX_INCLUDE_DIR}" CACHE STRING "REGEX include directory" FORCE)
else (NOT BRLCAD_REGEX_BUILD)
  # Our own code (at least the Creo plugin, maybe others) may require our
  # regex.h to build if compiling against an installed BRL-CAD's libs.
  # Explicitly override the general suppression of header installs in this
  # specific case.
  get_directory_property(REGEX_HDRS DIRECTORY libregex DEFINITION REGEX_HDRS)
  install(FILES ${REGEX_HDRS} DESTINATION ${INCLUDE_DIR})
endif (NOT BRLCAD_REGEX_BUILD)

# Same deal for zlib Library - common requirement, deal with it up front
set(zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
set(Z_PREFIX_STR "brl_")
THIRD_PARTY(libz ZLIB zlib zlib_DESCRIPTION ALIASES ENABLE_ZLIB ENABLE_LIBZ)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libz/CTestTestfile.cmake")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libz/zconf.h")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libz/zlib.h")
SetTargetFolder(zlib "Third Party Libraries")
SetTargetFolder(zlib-static "Third Party Libraries")
SetTargetFolder(example "Third Party Executables")
SetTargetFolder(minigzip "Third Party Executables")
if(BRLCAD_ZLIB_BUILD)
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
  set(Z_PREFIX_STR "${Z_PREFIX_STR}" CACHE STRING "prefix for zlib functions" FORCE)
  # Our own code (at least logic using OpenNURBS, maybe others) can require our
  # zlib.h to build if compiling against an installed BRL-CAD's libs.
  # Explicitly override the general suppression of header installs in this
  # specific case.
  get_directory_property(ZLIB_PUBLIC_HDRS DIRECTORY libz DEFINITION ZLIB_PUBLIC_HDRS)
  install(FILES ${ZLIB_PUBLIC_HDRS} DESTINATION ${INCLUDE_DIR})
else(BRLCAD_ZLIB_BUILD)
  set(Z_PREFIX_STR "" CACHE STRING "clear prefix for zlib functions" FORCE)
  set(Z_PREFIX_STR)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY_RELEASE} CACHE STRING "ZLIB_LIBRARIES" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE STRING "ZLIB include directory" FORCE)
endif(BRLCAD_ZLIB_BUILD)

# libpng Library - Checks for ZLIB, so need to handle libpng AFTER
# zlib to set the variables if using a local copy of zlib.  If not
# using a local copy of zlib, FindZLIB results will be identical in
# both cases so there is no danger of harming the libpng setup.  The
# PNG CMake system needs some options set and some variables
# translated to fit BRL-CAD's standard assumptions, so handle that
# here as well.  BRL-CAD needs PNG's IO - turn it on (i.e. turn off
# the disabling flags)
set(SKIP_INSTALL_FILES ON CACHE BOOL "Don't install files from libpng" FORCE)
mark_as_advanced(SKIP_INSTALL_FILES)
set(SKIP_INSTALL_EXECUTABLES ON CACHE BOOL "Don't install files from libpng" FORCE)
mark_as_advanced(SKIP_INSTALL_EXECUTABLES)
set(PNG_NO_CONSOLE_IO OFF CACHE BOOL "Option to disable Console IO in PNG" FORCE)
mark_as_advanced(PNG_NO_CONSOLE_IO)
set(PNG_NO_STDIO OFF CACHE BOOL "Option to disable STDIO in PNG" FORCE)
mark_as_advanced(PNG_NO_STDIO)
set(SKIP_INSTALL_EXPORT ON CACHE BOOL "We dont't want export for this application" FORCE)
mark_as_advanced(SKIP_INSTALL_EXPORT)
set(PNG_MAN_DIR ${MAN_DIR} CACHE STRING "Set PNG_MAN_DIR to the global MAN_DIR" FORCE)
mark_as_advanced(PNG_MAN_DIR)
set(PNG_TESTS 0 CACHE STRING "Disable building png test executables" FORCE)
mark_as_advanced(PNG_TESTS)
set(PNG_PREFIX "brl_" CACHE STRING "BRL-CAD prefix for libpng" FORCE)
mark_as_advanced(PNG_PREFIX)
set(ld-version-script OFF CACHE STRING "Disable linker script" FORCE)
mark_as_advanced(ld-version-script)
mark_as_advanced(PNG_PREFIX)
mark_as_advanced(PNG_FRAMEWORK)
mark_as_advanced(DFA_XTRA)
mark_as_advanced(AWK)
set(CMAKE_INSTALL_LIBDIR ${LIB_DIR})
set(PNG_LIB_NAME png_brl)
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
set(CMAKE_FIND_FRAMEWORK LAST)
THIRD_PARTY(libpng PNG png png_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_PNG)
if(BRLCAD_PNG_BUILD)

  set(PNG_LIBRARY png CACHE STRING "PNG_LIBRARY" FORCE)
  set(PNG_LIBRARIES png CACHE STRING "PNG_LIBRARIES" FORCE)
  set(PNG_PNG_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libpng;${CMAKE_CURRENT_SOURCE_DIR}/libpng" CACHE STRING "PNG include directory" FORCE)
  mark_as_advanced(PNG_PNG_INCLUDE_DIR)
  set(PNG_INCLUDE_DIR ${PNG_PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)
  set(PNG_INCLUDE_DIRS ${PNG_INCLUDE_DIR} CACHE STRING "PNG include directory" FORCE)

  # On Haiku the find_library call sets this variable directly, which is not
  # how we handle things elsewhere in BRL-CAD - unset the variable in that
  # situation so the remainder of the build logic works normally
  if("${M_LIBRARY}" MATCHES "NOTFOUND")
    unset(M_LIBRARY CACHE)
  endif("${M_LIBRARY}" MATCHES "NOTFOUND")

  SetTargetFolder(png "Third Party Libraries")
  SetTargetFolder(png_static "Third Party Libraries")
  SetTargetFolder(genfiles "Third Party Libraries")
else(BRLCAD_PNG_BUILD)
  set(PNG_LIBRARIES ${PNG_LIBRARY_RELEASE} CACHE STRING "PNG_LIBRARIES" FORCE)
  set(PNG_INCLUDE_DIRS "${PNG_PNG_INCLUDE_DIR}" CACHE STRING "PNG include directory" FORCE)
endif(BRLCAD_PNG_BUILD)

# Since SKIP_INSTALL_FILES doesn't have a PNG prefix, we'll unset it once we're done
unset(SKIP_INSTALL_FILES CACHE)
unset(SKIP_INSTALL_FILES)
unset(SKIP_INSTALL_EXECUTABLES CACHE)
unset(SKIP_INSTALL_EXECUTABLES)

DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/CTestTestfile.cmake")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng.pc")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng-config")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng${CMAKE_SHARED_LIBRARY_SUFFIX}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng${CMAKE_STATIC_LIBRARY_SUFFIX}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng16-config")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/libpng16.pc")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/scripts/genchk.cmake")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/scripts/genout.cmake")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libpng/scripts/gensrc.cmake")
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng.pc)
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng-config)
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng${CMAKE_SHARED_LIBRARY_SUFFIX})
DISTCLEAN(${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libpng${CMAKE_STATIC_LIBRARY_SUFFIX})

# Other PNG options to mark as advanced
mark_as_advanced(PNGARG)
mark_as_advanced(PNG_DEBUG)
mark_as_advanced(PNG_SHARED)
mark_as_advanced(PNG_STATIC)
mark_as_advanced(uname_executable)

# libnetpbm Library support for pnm,ppm,pbm, etc. image files
set(netpbm_DESCRIPTION "
Option for enabling and disabling compilation of the netpbm library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libnetpbm NETPBM netpbm netpbm_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_NETPBM)
if(BRLCAD_NETPBM_BUILD)
  mark_as_advanced(NETPBM_INCLUDE_DIRS)
  mark_as_advanced(NETPBM_LIBRARIES)
  SetTargetFolder(netpbm "Third Party Libraries")
  SetTargetFolder(netpbm-static "Third Party Libraries")
endif(BRLCAD_NETPBM_BUILD)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libnetpbm/version.h")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libnetpbm/compile.h")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/libnetpbm/pm_config.h")

# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on
# and the include directory for utahrle are different, so override the
# macro's setting of UTAHRLE_INCLUDE_DIR here.
set(UTAHRLE_HAVE_COMMON_H 1)
set(utahrle_DESCRIPTION "
Option for enabling and disabling compilation of the Utah Raster
Toolkit library provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libutahrle UTAHRLE utahrle utahrle_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL3
  ALIASES ENABLE_UTAHRLE FLAGS NOSYS)
if(BRLCAD_UTAHRLE_BUILD)
  SetTargetFolder(utahrle "Third Party Libraries")
  SetTargetFolder(utahrle-static "Third Party Libraries")
  set(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_INCLUDE_DIRS "${UTAH_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
  set(BRLCAD_UTAHRLE_INCLUDE_DIR "${UTAHRLE_INCLUDE_DIR}" CACHE STRING "directory with rle.h header" FORCE)
  mark_as_advanced(UTAHRLE_INCLUDE_DIR)
  mark_as_advanced(BRLCAD_UTAHRLE_INCLUDE_DIR)
endif(BRLCAD_UTAHRLE_BUILD)

####################################################################
#                Tcl/Tk and related extensions
####################################################################

function(TCL_RESET_VARS)
  unset(TCL_LIBRARY CACHE)
  unset(TCL_STUB_LIBRARY CACHE)
  unset(TCL_FOUND CACHE)
  unset(TCLSH_FOUND CACHE)
  unset(TCL_LIBRARY CACHE)
  unset(TCL_INCLUDE_PATH CACHE)
  unset(TCL_TCLSH CACHE)
  unset(TCL_STUB_LIBRARY CACHE)
endfunction(TCL_RESET_VARS)

function(TK_RESET_VARS)
  unset(TK_LIBRARY CACHE)
  unset(TK_STUB_LIBRARY CACHE)
  unset(TK_FOUND CACHE)
  unset(TCLTK_FOUND CACHE)
  unset(TK_LIBRARY CACHE)
  unset(TK_INCLUDE_PATH CACHE)
  unset(TK_WISH CACHE)
  unset(TK_STUB_LIBRARY CACHE)
  unset(TTK_STUB_LIBRARY CACHE)
endfunction(TK_RESET_VARS)


# Set the expected graphics system (if known) before any find_package
# calls related to Tcl
if(BRLCAD_ENABLE_AQUA)
  set(TK_ENABLE_AQUA ON CACHE STRING "BRL-CAD setting to enable AQUA" FORCE)
  set(TCL_TK_SYSTEM_GRAPHICS "aqua" CACHE STRING "Tk system graphics type" FORCE)
endif(BRLCAD_ENABLE_AQUA)

# Make sure we actually look for these each time...
TCL_RESET_VARS()
TK_RESET_VARS()

# The new FindTCL needs to be told to look for Tk
if(BRLCAD_ENABLE_TK)
  set(TCL_ENABLE_TK ON)
endif(BRLCAD_ENABLE_TK)

set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl tcl_DESCRIPTION ALIASES ENABLE_TCL FIND_NAME TCL REQUIRED_VARS "BRLCAD_ENABLE_TCL;BRLCAD_LEVEL2")

if(BRLCAD_TCL_BUILD)

  # Let the BRL-CAD compilation know we have Tcl
  CONFIG_H_APPEND(BRLCAD "#define HAVE_TCL_H 1\n")

  # There is no system init.tcl that we need to provide a path for
  CONFIG_H_APPEND(BRLCAD "#define TCL_SYSTEM_INITTCL_PATH \"\"\n")

  # Set the necessary variables ourselves since FindTCL.cmake doesn't know about our build
  set(TCL_LIBRARY tcl CACHE STRING "TCL_LIBRARY" FORCE)
  set(TCL_STUB_LIBRARY tclstub CACHE STRING "TCL_LIBRARY" FORCE)
  set(TCL_TCLSH tclsh CACHE STRING "Tcl shell" FORCE)
  set(TCL_VERSION_MAJOR "8" CACHE STRING "Tcl MAJOR version" FORCE)
  set(TCL_VERSION_MINOR "6" CACHE STRING "Tcl MINOR version" FORCE)

  # For the include path, use the TCL_INCLUDE_PATH set in the tcl build itself;
  # we are not installing headers, so we have to look in the source tree
  get_directory_property(TCL_INCLUDE_PATH DIRECTORY tcl DEFINITION TCL_INCLUDE_PATH)
  set(TCL_INCLUDE_PATH "${TCL_INCLUDE_PATH}" CACHE STRING "Tcl include path" FORCE)

  # Let Distclean know it will have work to do
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/tcl8")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/tcl8.6")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tcl.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclDecls.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclOO.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclOODecls.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclPlatDecls.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclTomMath.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tclTomMathDecls.h")

  # Files generated on the Mac
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/Tcl-Info.plist")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/Tclsh-Info.plist")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/include/tclConfig.h")

  # Group the targets in Visual Studio project files
  SetTargetFolder(tclsh "Third Party Executables")
  SetTargetFolder(tcl "Third Party Libraries")
  SetTargetFolder(tclstub "Third Party Libraries")

elseif(BRLCAD_ENABLE_TCL)
  # We're using the system Tcl, but need some extra information.  FindTCL.cmake
  # doesn't give us a stub library location, so we need to go find it ourselves.
  get_filename_component(TCL_LIB_PATH "${TCL_LIBRARY}" DIRECTORY)
  find_library(TCL_STUB_LIBRARY tclstub NAMES tclstub86 tclstub8.6 PATHS ${TCL_LIB_PATH})

  # Make sure we've got the header file
  BRLCAD_INCLUDE_FILE(tcl.h HAVE_TCL_H)

  # We're going to need the path to the system init.tcl for btclsh and bwish -
  # extract it from the system Tcl
  set(inittcl_script "
set filename \"${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl\"
set fileID [open $filename \"w\"]
puts $fileID $auto_path
close $fileID
exit
  ")
  set(inittcl_scriptfile "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl.tcl")
  file(WRITE ${inittcl_scriptfile} ${inittcl_script})
  execute_process(COMMAND ${TCL_TCLSH} ${inittcl_scriptfile} OUTPUT_VARIABLE EXECOUTPUT)
  file(READ "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl" tcl_inittcl_raw)
  string(REGEX REPLACE "\n" "" tcl_inittcl_paths_1 ${tcl_inittcl_raw})
  string(REGEX REPLACE " " ";" tcl_inittcl_paths ${tcl_inittcl_paths_1})
  find_path(tcl_inittcl NAMES init.tcl PATHS ${tcl_inittcl_paths})
  mark_as_advanced(tcl_inittcl)
  CONFIG_H_APPEND(BRLCAD "#define TCL_SYSTEM_INITTCL_PATH \"${tcl_inittcl}\"\n")
endif(BRLCAD_TCL_BUILD)
mark_as_advanced(TCL_COMPILATION_TRACING)
mark_as_advanced(TCL_COMPILE_STATS)
mark_as_advanced(TCL_ENABLE_64BIT)
mark_as_advanced(TCL_TIMEZONE_DATA)
mark_as_advanced(TCL_STUB_LIBRARY)
mark_as_advanced(M_LIBRARY)

# Depending on the Tcl/Tk results, look for packages required by BRL-CAD.  If
# building local Tcl/Tk, go ahead and enable all the packages as well - otherwise,
# check the found Tcl/Tk and enable what it doesn't supply.  If doing a non-graphical
# build, only compile extensions that don't use Tk.
include("${BRLCAD_CMAKE_DIR}/ThirdParty_TCL.cmake")

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

THIRD_PARTY_TCL_PACKAGE(Tk tk "${TCL_TCLSH}" "tcl" "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL2" "tk" tk_ALIASES tk_DESCRIPTION)

if(BRLCAD_TK_BUILD)

  # Let the BRL-CAD compilation know we have Tcl
  CONFIG_H_APPEND(BRLCAD "#define HAVE_TK_H 1\n")

  # Set the necessary variables ourselves since FindTCL.cmake doesn't know about our build
  set(TK_LIBRARY tk CACHE STRING "TK_LIBRARY" FORCE)
  set(TK_STUB_LIBRARY tkstub CACHE STRING "TK_LIBRARY" FORCE)
  set(TK_WISH wish CACHE STRING "TK_WISH" FORCE)

  # For the include path, use the TK_INCLUDE_PATH set in the tk build itself;
  # we are not installing headers, so we have to look in the source tree
  get_directory_property(TK_INCLUDE_PATH DIRECTORY tk DEFINITION TK_INCLUDE_PATH)
  set(TK_INCLUDE_PATH "${TK_INCLUDE_PATH}" CACHE STRING "Tk include path" FORCE)

  get_directory_property(TK_SYSTEM_GRAPHICS DIRECTORY tk DEFINITION TK_SYSTEM_GRAPHICS)
  set(TK_SYSTEM_GRAPHICS "${TK_SYSTEM_GRAPHICS}" CACHE STRING "Tk system graphics type" FORCE)

  # Let Distclean know it will have work to do both here and in
  # the lib directory
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/Makefile")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man1")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man3")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/mann")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/library/Makefile")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/wish.exe.manifest")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/tk8.6")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/tk8.6.10")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tk.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tkDecls.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/tkPlatDecls.h")

  # Group the targets in Visual Studio project files
  SetTargetFolder(wish "Third Party Executables")
  SetTargetFolder(tk "Third Party Libraries")
  SetTargetFolder(tkstub "Third Party Libraries")
  SetTargetFolder(Tk_pkgIndex "Third Party Libraries")

elseif(BRLCAD_ENABLE_TK)

  set(HAVE_TK_H 1)

  # Packages probably still need to know the system graphics package
  if (BRLCAD_ENABLE_AQUA)
    set(TK_SYSTEM_GRAPHICS aqua)
  elseif (BRLCAD_ENABLE_X11)
    set(TK_SYSTEM_GRAPHICS x11)
  elseif (WIN32)
    set(TK_SYSTEM_GRAPHICS win32)
  endif ()
  set(TK_SYSTEM_GRAPHICS "${TK_SYSTEM_GRAPHICS}" CACHE STRING "Tk system graphics type" FORCE)

else(BRLCAD_TK_BUILD)

  # If we're not using Tk, make sure the variables are empty - CMake won't be
  # happy if they're set to NOTFOUND and used, and FindTCL.cmake may have set
  # them.
  TK_RESET_VARS()
  unset(TK_SYSTEM_GRAPHICS CACHE)

endif(BRLCAD_TK_BUILD)
mark_as_advanced(TK_INCLUDE_PATH)
mark_as_advanced(TK_LIBRARY)
mark_as_advanced(TK_STUB_LIBRARY)
mark_as_advanced(TK_SYSTEM_GRAPHICS)
mark_as_advanced(TK_WISH)

# Now that Tcl/Tk is settled, define the HAVE_TK flag for the config.h
# file

if(BRLCAD_ENABLE_TK)
  set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
  CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TK\n")
else(BRLCAD_ENABLE_TK)
  set(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)
endif(BRLCAD_ENABLE_TK)
mark_as_advanced(HAVE_TK)

# NOTE: Itcl/Itk 4 has a known behavior change that will cause an
# issue with Archer:
#
# http://core.tcl.tk/itcl/tktview/61952ca33e9e58df9e0519a0b4583a144e8ec77b
#
# Consequently we need to provide the older v3
set(ITCL_DIR itcl3)
set(ITCL_VER "3.4")
include("${CMAKE_CURRENT_SOURCE_DIR}/itcl3.dist")
CMAKEFILES_IN_DIR(itcl3_ignore_files itcl3)
set(ITK_DIR itk3)
set(ITK_VER "3.4")
include("${CMAKE_CURRENT_SOURCE_DIR}/itk3.dist")
CMAKEFILES_IN_DIR(itk3_ignore_files itk3)

set(itcl_ALIASES ENABLE_ITCL)
set(itcl_DESCRIPTION "
Option for enabling and disabling compilation of the IncrTcl package
for Tcl objects provided with BRL-CAD's source distribution.  Default
is AUTO, auto-enabling if the BRLCAD_TCL option is set to BUNDLED and
testing first for a system version if BRLCAD_TCL is set to AUTO or
SYSTEM.  If BRLCAD_ITCL is set to BUNDLED, local copy is built even if
a system version is present.
")
THIRD_PARTY_TCL_PACKAGE(Itcl ${ITCL_DIR} "${TCL_TCLSH}" "tcl" "BRLCAD_ENABLE_TCL;BRLCAD_LEVEL2" "" itcl_ALIASES itcl_DESCRIPTION)
if(BRLCAD_ITCL_BUILD)
  set(ITCL_LIBRARY itcl CACHE STRING "ITCL_LIBRARY" FORCE)
  set(ITCL_STUB_LIBRARY itclstub CACHE STRING "ITCL_STUB_LIBRARY" FORCE)
  set(ITCL_VERSION "${ITCL_VER}" CACHE STRING "ITCL_VERSION" FORCE)
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/itcl${ITCL_VER}")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itcl.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itclDecls.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itclInt.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itclIntDecls.h")
  SetTargetFolder(itcl "Third Party Libraries")
  SetTargetFolder(itclstub "Third Party Libraries")
  SetTargetFolder(itcl_pkgIndex "Third Party Libraries")
else(BRLCAD_ITCL_BUILD)
  # We have Itcl package AND Itcl library - we're good.
  set(ITCL_VERSION "${ITCL_PACKAGE_VERSION}" CACHE STRING "ITCL_VERSION" FORCE)
  set(ITCL_LIBRARY ${ITCL_LIBRARY} CACHE STRING "ITCL_LIBRARY" FORCE)
endif(BRLCAD_ITCL_BUILD)
CONFIG_H_APPEND(BRLCAD "#cmakedefine ITCL_VERSION	\"${ITCL_VERSION}\"\n")
mark_as_advanced(ITCL_LIBRARY)
mark_as_advanced(ITCL_STUB_LIBRARY)
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
THIRD_PARTY_TCL_PACKAGE(Itk ${ITK_DIR} "${TK_WISH}" "tcl;itcl;tk" "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" itk_ALIASES itk_DESCRIPTION)
if(BRLCAD_ITK_BUILD)
  SetTargetFolder(itk "Third Party Libraries")
  SetTargetFolder(itkstub "Third Party Libraries")
  SetTargetFolder(itk_pkgIndex "Third Party Libraries")
  # does not handle C library settings.
  set(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
  set(ITK_VERSION "3.4" CACHE STRING "ITK_VERSION" FORCE)
  DISTCLEAN("${CMAKE_BINARY_DIR}/${LIB_DIR}/itk${ITK_VER}")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itk.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/itkDecls.h")
else(BRLCAD_ITK_BUILD)
  # We have Itcl/Itk packages AND Itcl/Itk libraries - we're good.
  set(ITK_VERSION "${ITCL_VERSION}" CACHE STRING "ITK_VERSION" FORCE)
  set(ITK_LIBRARY ${ITK_LIBRARY} CACHE STRING "ITK_LIBRARY" FORCE)
  if(ITK_LIBRARY)
    get_filename_component(ITK_BIN_PREFIX ${ITK_LIBRARY} PATH)
  endif(ITK_LIBRARY)
endif(BRLCAD_ITK_BUILD)
mark_as_advanced(ITK_VERSION)
mark_as_advanced(ITK_LIBRARY)
CONFIG_H_APPEND(BRLCAD "#cmakedefine ITK_VERSION	\"${ITK_VERSION}\"\n")

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
set(IWIDGETS_VER "4.1.1")
THIRD_PARTY_TCL_PACKAGE(Iwidgets iwidgets "${TK_WISH}" "tcl;tk;itcl;itk" "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" iwidgets_ALIASES iwidgets_DESCRIPTION)
if(BRLCAD_IWIDGETS_BUILD)
  set(IWIDGETS_VERSION "${IWIDGETS_VER}" CACHE STRING "IWIDGETS_VERSION" FORCE)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/iwidgets/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/iwidgets/iwidgets.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Iwidgets${IWIDGETS_VER}")
ELSE(BRLCAD_IWIDGETS_BUILD)
  set(IWIDGETS_VERSION "${IWIDGETS_PACKAGE_VERSION}" CACHE STRING "IWIDGETS_VERSION" FORCE)
endif(BRLCAD_IWIDGETS_BUILD)
CONFIG_H_APPEND(BRLCAD "#define IWIDGETS_VERSION \"${IWIDGETS_VERSION}\"\n")
mark_as_advanced(IWIDGETS_VERSION)
mark_as_advanced(COMPAT_SRCS)

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
THIRD_PARTY_TCL_PACKAGE(Tkhtml tkhtml "${TK_WISH}" "tcl;tk" "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tkhtml_ALIASES tkhtml_DESCRIPTION)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tkhtml/pkgIndex.tcl")
DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tkhtml3.0")
SetTargetFolder(Tkhtml "Third Party Libraries")
SetTargetFolder(tkhtml_n_gen "Compilation Utilities")

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
THIRD_PARTY_TCL_PACKAGE(Tktable tktable "${TK_WISH}" "tcl;tk" "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tktable_ALIASES tktable_DESCRIPTION)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/pkgIndex.tcl")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/tktable_cfg.h")
DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tktable2.10")
SetTargetFolder(Tktable "Third Party Libraries")
SetTargetFolder(tktable_header_gen "Compilation Utilities")

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
#THIRD_PARTY_TCL_PACKAGE(tktreectrl tktreectrl "${TK_WISH}" "tcl;tk"
#  "BRLCAD_ENABLE_TCL;BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tktreectrl_ALIASES tktreectrl_DESCRIPTION)
#DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/pkgIndex.tcl")
#DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/tktreectrl_cfg.h")
#DISTCLEAN("${CMAKE_BINARY_DIR}/lib/TkTreeCtrl2.4")

# OpenNURBS Library
set(opennurbs_DESCRIPTION "
Option for enabling and disabling compilation of the openNURBS library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openNURBS OPENNURBS openNURBS opennurbs_DESCRIPTION ALIASES
  ENABLE_OPENNURBS FLAGS NOSYS)
SetTargetFolder(openNURBS "Third Party Libraries")
SetTargetFolder(openNURBS-static "Third Party Libraries")
SetTargetFolder(openNURBS-obj "Third Party Libraries")
if(BRLCAD_OPENNURBS_BUILD)
  set(OPENNURBS_INCLUDE_DIRS "${OPENNURBS_INCLUDE_DIR};${ZLIB_INCLUDE_DIRS}" CACHE STRING "Require opennurbs header includes" FORCE)
  set(OPENNURBS_LIBRARIES "${OPENNURBS_LIBRARY};${ZLIB_LIBRARIES}" CACHE STRING "Require opennurbs libraries" FORCE)
  mark_as_advanced(OPENNURBS_INCLUDE_DIRS)
  mark_as_advanced(OPENNURBS_LIBRARIES)
  get_directory_property(openNURBS_headers_orig DIRECTORY openNURBS DEFINITION OPENNURBS_HEADERS)
  set(openNURBS_headers)
  foreach(onhfile ${openNURBS_headers_orig})
    set(openNURBS_headers ${openNURBS_headers} openNURBS/${onhfile})
  endforeach(onhfile ${openNURBS_headers_orig})
  BRLCAD_MANAGE_FILES(openNURBS_headers ${INCLUDE_DIR}/openNURBS)
endif(BRLCAD_OPENNURBS_BUILD)


if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
#=== LIBBSON ==================================================
# Libbson needed for binary attributes, always use this local version
set(libbson_DESCRIPTION "
Option for enabling and disabling compilation of the Libbson library
provided with BRL-CAD's source code.  Default is BUNDLED, using
the included other/src version.
")
THIRD_PARTY(libbson BSON Libbson libbson_DESCRIPTION ALIASES
  ENABLE_BSON FLAGS NOSYS UNDOCUMENTED)
if(BRLCAD_BSON_BUILD)
  SetTargetFolder(libBSON "Third Party Libraries")
  SetTargetFolder(libBSON-static "Third Party Libraries")
endif(BRLCAD_BSON_BUILD)
#=== END LIBBSON ==================================================
else(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  include("${CMAKE_CURRENT_SOURCE_DIR}/libbson.dist")
  CMAKEFILES_IN_DIR(libbson_ignore_files libbson)
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)

# STEPcode
# Need Lemon/Perplex to build STEP - conditionalize

# Set some options for the SCL build
option(SC_PYTHON_GENERATOR "Compile exp2python" OFF)
option(SC_ENABLE_TESTING "Enable unittesting framework" OFF)
option(SC_ENABLE_COVERAGE "Enable code coverage test" OFF)

set(SC_IS_SUBBUILD ON)
set(SC_SKIP_EXEC_INSTALL ON)
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
THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION
  REQUIRED_VARS LEMON_EXECUTABLE PERPLEX_EXECUTABLE BRLCAD_LEVEL3 BRLCAD_ENABLE_STEP
  ALIASES ${sc_ALIASES} RESET_VARS
  EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET FLAGS NOSYS)
if(BRLCAD_SC_BUILD)
  set(EXP2CXX_EXEC exp2cxx CACHE STRING "Express to C++ executable" FORCE)
  set(EXP2CXX_EXECUTABLE_TARGET exp2cxx CACHE STRING "Express to C++ executable target" FORCE)
  mark_as_advanced(EXP2CXX_EXEC)
  mark_as_advanced(EXP2CXX_EXECUTABLE_TARGET)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_cf.h.in")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_cf.h")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_version_string.h")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/src/express/ExpParser_expparse/expparse.y")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/src/express/express_md5gen.cmake")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/src/express/express_verify.cmake")
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
SetTargetFolder(exp2cxx "Third Party Executables")
SetTargetFolder(core "Compilation Utilities")
SetTargetFolder(print_attrs "Compilation Utilities")
SetTargetFolder(print_schemas "Compilation Utilities")
SetTargetFolder(exppp "Third Party Executables")
SetTargetFolder(libexppp "Third Party Libraries")
SetTargetFolder(check-express "Third Party Executables")
SetTargetFolder(express "Third Party Libraries")
SetTargetFolder(express_md5gen "Third Party Executables")
SetTargetFolder(express_verify "Third Party Libraries")
SetTargetFolder(base "Third Party Libraries")
SetTargetFolder(stepcore "Third Party Libraries")
SetTargetFolder(stepdai "Third Party Libraries")
SetTargetFolder(stepeditor "Third Party Libraries")
SetTargetFolder(steputils "Third Party Libraries")
SetTargetFolder(version_string "Compilation Utilities")

if(BRLCAD_ENABLE_GECODE)
  set(gecode_DESCRIPTION "
  Option for enabling and disabling compilation of the Gecode Constraint
  Solving Libraries provided with BRL-CAD's source code.  Default is AUTO,
  responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
  first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
  ")
  set(GECODE_USE_QT OFF)
  mark_as_advanced(GECODE_USE_QT)
  THIRD_PARTY(gecode GECODE gecode gecode_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL3 ALIASES ENABLE_GECODE)
  if(BRLCAD_GECODE_BUILD)
    set(GECODE_LIBRARIES gecodesupport gecodekernel gecodesearch gecodeint
      gecodeset gecodefloat gecodeminimodel gecodedriver gecodeflatzinc)
    set(GECODE_LIBRARIES "${GECODE_LIBRARIES}"  CACHE STRING "Gecode libraries in BRL-CAD" FORCE)
    set(GECODE_INCLUDE_DIR "${BRLCAD_BINARY_DIR}/src/other/gecode;${BRLCAD_SOURCE_DIR}/src/other/gecode"  CACHE STRING "Gecode headers in BRL-CAD" FORCE)
  endif(BRLCAD_GECODE_BUILD)
endif(BRLCAD_ENABLE_GECODE)

# OpenSceneGraph Libraries
set(openscenegraph_DESCRIPTION "
Option for enabling and disabling compilation of the OpenSceneGraph
libraries provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openscenegraph OSG osg openscenegraph_DESCRIPTION
  ALIASES ENABLE_OPENSCENEGRAPH REQUIRED_VARS "BRLCAD_ENABLE_OSG;BRLCAD_LEVEL3" FIND_NAME OpenSceneGraph FIND_COMPONENTS
  osgText osgViewer FLAGS NOSYS)
if(BRLCAD_OSG_BUILD)
  set(OSG_LIBRARY osg CACHE STRING "libosg" FORCE)
  set(OSGUTIL_LIBRARY osgUtil CACHE STRING "osgutil" FORCE)
  set(OSGDB_LIBRARY osgDB CACHE STRING "osgdb" FORCE)
  set(OSGGA_LIBRARY osgGA CACHE STRING "osgGA" FORCE)
  set(OSGTEXT_LIBRARY osgText CACHE STRING "osg Text library" FORCE)
  set(OSGVIEWER_LIBRARY osgViewer CACHE STRING "osg Viewer library" FORCE)
  set(OPENTHREADS_LIBRARY OpenThreads CACHE STRING "OpenThreads library" FORCE)
  set(OPENTHREADS_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/openscenegraph/src/OpenThreads/include"
    "${BRLCAD_SOURCE_DIR}/src/other/openscenegraph/src/OpenThreads/include"
    CACHE STRING "OpenThreads include dirs" FORCE
    )
  set(OSG_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/openscenegraph/include"
    "${BRLCAD_SOURCE_DIR}/src/other/openscenegraph/include"
    CACHE STRING "OpenSceneGraph include dirs" FORCE
    )
  set(OSG_INCLUDE_DIR "${OSG_INCLUDE_DIR}" CACHE STRING "Directory containing OpenSceneGraph headers." FORCE)
  set(OSG_LIBRARIES "${OSG_LIBRARY};${OSGUTIL_LIBRARY};${OSGDB_LIBRARY};${OSGGA_LIBRARY};${OSGTEXT_LIBRARY};${OSGVIEWER_LIBRARY};${OPENTHREADS_LIBRARY}" CACHE STRING "OpenSceneGraph Libraries")
  SetTargetFolder(osg "Third Party Libraries")
  SetTargetFolder(osgDB "Third Party Libraries")
  SetTargetFolder(osgGA "Third Party Libraries")
  SetTargetFolder(osgText "Third Party Libraries")
  SetTargetFolder(osgViewer "Third Party Libraries")
  SetTargetFolder(osgUtil "Third Party Libraries")
  SetTargetFolder(osgWidget "Third Party Libraries")
  SetTargetFolder(osgdb_freetype "Third Party Libraries")
  SetTargetFolder(osgdb_osg "Third Party Libraries")
  SetTargetFolder(osgdb_png "Third Party Libraries")
else(BRLCAD_OSG_BUILD)
  if (BRLCAD_ENABLE_OSG)
    find_package(OpenThreads)
  endif (BRLCAD_ENABLE_OSG)
endif(BRLCAD_OSG_BUILD)

# Adaptagrams is not yet integrated as a src/other subbuild, but there
# is code that will make use of it if it is available.  If that code
# becomes sufficiently useful, Adaptagrams will be integrated.  In the
# meantime, locate the find logic for that package here.
# Adaptagrams library
#if(BRLCAD_LEVEL2)
# find_package(ADAPTAGRAMS)
# if(ADAPTAGRAMS_FOUND)
#   CONFIG_H_APPEND(BRLCAD "#define HAVE_ADAPTAGRAMS 1\n")
# endif(ADAPTAGRAMS_FOUND)
# set(ADAPTAGRAMS_LIBRARIES "${ADAPTAGRAMS_LIBRARIES}" CACHE STRING "Adaptagrams libs" FORCE)
# set(ADAPTAGRAMS_FOUND "${ADAPTAGRAMS_FOUND}" CACHE BOOL "Adaptagrams status" FORCE)
# mark_as_advanced(ADAPTAGRAMS_FOUND)
# mark_as_advanced(ADAPTAGRAMS_LIBRARIES)
#endif(BRLCAD_LEVEL2)

# Poly2Tri CDT library
# Note - this really should be embedded as part of libbg - it is simple, can be
# compiled as part of libbg with a few source adjustments, and there is an API
# for this feature exposed by libbg.  The only reason it is still maintained as
# a separate library is the fast cdt feature of libbrep, which is using this
# API directly instead of going through libbg.  Care will have to be taken when
# making that shift, and it may even be that we'll discover a need to adjust the
# libbg API in doing so to avoid performance issues.  We should eventually
# address those issues though, if for no other reason than to discourage other
# direct uses of this API instead of bg_nested_polygon_triangulate.
#
# May also want to investigate https://github.com/MetricPanda/fast-poly2tri, but
# the removal of Steiner point insertion would probably need to be addressed...
add_subdirectory(poly2tri)
include("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri.dist")
CMAKEFILES_IN_DIR(poly2tri_ignore_files poly2tri)
DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/Makefile")
set(P2T_LIBRARY "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(POLY2TRI_LIBRARIES "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(P2T_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
set(POLY2TRI_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
SetTargetFolder(poly2tri "Third Party Libraries")
SetTargetFolder(poly2tri-static "Third Party Libraries")
mark_as_advanced(P2T_LIBRARY)
mark_as_advanced(P2T_INCLUDE_DIR)
mark_as_advanced(P2T_SHARED)
mark_as_advanced(P2T_STATIC)
mark_as_advanced(P2T_TESTS)

# PROJ.4 library - used by GDAL
set(proj4_DESCRIPTION "
Option for enabling and disabling compilation of the PROJ.4 geographic
projection library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(proj-4 PROJ4 proj4 proj4_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_PROJ4)
if(BRLCAD_PROJ4_BUILD)
  set(PROJ4_LIBRARY proj CACHE STRING "libproj" FORCE)
  set(PROJ4_LIBRARIES proj CACHE STRING "libproj" FORCE)
  set(PROJ4_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/proj-4;${CMAKE_CURRENT_SOURCE_DIR}/proj-4/src" CACHE STRING "proj-4 includes" FORCE)
  set(PROJ4_INCLUDE_DIRS "${PROJ4_INCLUDE_DIR}")
  set(PROJ4_VERSION "4.9.3")
  set(PROJ4_FOUND ON)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/proj-4/proj_config.h")
  SetTargetFolder(proj "Third Party Libraries/GDAL")
  SetTargetFolder(proj-static "Third Party Libraries/GDAL")
  # Need data files in order for PROJ-4 to work...
  get_directory_property(PROJ_DICTIONARY DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/proj-4/nad DEFINITION PROJ_DICTIONARY)
  set(proj_dict)
  foreach(pfile ${PROJ_DICTIONARY})
    set(proj_dict ${proj_dict} proj-4/nad/${pfile})
  endforeach(pfile ${PROJ_DICTIONARY})
  BRLCAD_MANAGE_FILES("${proj_dict}" "${DATA_DIR}/proj")
endif(BRLCAD_PROJ4_BUILD)
mark_as_advanced(PROJ4_INCLUDE_DIRS)
mark_as_advanced(PROJ4_LIBRARY)
mark_as_advanced(USE_THREAD)

# GDAL library
set(gdal_DESCRIPTION "
Option for enabling and disabling compilation of the GDAL geographic
library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(gdal GDAL gdal gdal_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_GDAL)
if(BRLCAD_GDAL_BUILD)
  set(GDAL_LIBRARY gdal CACHE STRING "libgdal" FORCE)
  set(GDAL_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/gdal"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/port"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/gcore"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/alg"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/ogr"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/ogr/ogrsf_frmts"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/gnm"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/apps"
    "${BRLCAD_SOURCE_DIR}/src/other/gdal/frmts/vrt"
    )
  set(GDAL_INCLUDE_DIR "${GDAL_INCLUDE_DIR}" CACHE STRING "libgdal includes" FORCE)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/gdal/cpl_config.h")
  SetTargetFolder(gdal "Third Party Libraries/GDAL")
  SetTargetFolder(gdal-static "Third Party Libraries/GDAL")
else(BRLCAD_GDAL_BUILD)
  set(GDAL_INCLUDE_DIR "${GDAL_INCLUDE_DIRS}" CACHE STRING "libgdal includes" FORCE)
  set(GDAL_LIBRARIES "${GDAL_LIBRARY}" CACHE STRING "libgdal" FORCE)
endif(BRLCAD_GDAL_BUILD)
mark_as_advanced(GDAL_CONFIG)
mark_as_advanced(GDAL_INCLUDE_DIR)
mark_as_advanced(GDAL_LIBRARY)
mark_as_advanced(GDAL_INCLUDE_DIRS)
mark_as_advanced(GDAL_LIBRARIES)

# GCT is a collection of algorithms for geometry processing and conversion
#add_subdirectory(gct)
include("${CMAKE_CURRENT_SOURCE_DIR}/gct.dist")
CMAKEFILES_IN_DIR(gct_ignore_files gct)

# linenoise is used by applications directly
include(${CMAKE_CURRENT_SOURCE_DIR}/linenoise.dist)
CMAKEFILES_IN_DIR(linenoise_ignore_files linenoise)

# Eigen is header only
include("${CMAKE_CURRENT_SOURCE_DIR}/Eigen.dist")
CMAKEFILES_IN_DIR(Eigen_ignore_files Eigen)

CMAKEFILES(README)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

