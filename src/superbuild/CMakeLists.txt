# Find the executable extension, if there is one
get_filename_component(EXE_EXT "${CMAKE_COMMAND}" EXT)

# If targets are built, we want to group them in build tools that support
# doing so.
function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# Debugging flag for ExternalProject tools
set(EXTPROJ_VERBOSE 0)

# Superbuilds use ExternalProject_Add, and BRL-CAD defines some additional
# targets for managing them.
include(ExternalProject)
# Note- make sure CMAKE_BINARY_DIR and CMAKE_INSTALL_PREFIX are finalized
# BEFORE including this file!
include(ExternalProject_Target)

# Load some CMake macros to handle the special case of third party libraries.
include(ThirdParty)
include(ThirdPartyExecutable)


# In case we need to pass some sort of reasonable parallel
# flag to a build system, check our processor count
include(ProcessorCount)
ProcessorCount(N)
math(EXPR pcnt "${N} / 2")
if (NOT pcnt)
  set(pcnt 1)
endif (NOT pcnt)


# With MSVC, some of the non-CMake subbuilds are going to need the vcvars bat
# file to set up the environment.
if(MSVC)
  get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  get_filename_component(COMPILER_ROOT "${COMPILER_DIR}" NAME)
  set(VCVARS_BAT "${COMPILER_DIR}/vcvars${COMPILER_ROOT}.bat")
  if(NOT EXISTS "${VCVARS_BAT}")
    # Try the VS2017 location (https://stackoverflow.com/q/43372235)
    set(COMPILER_ROOT "")
    foreach(DIRS RANGE 0 5)
      get_filename_component(COMPILER_DIR "${COMPILER_DIR}" DIRECTORY)
    endforeach(DIRS RANGE 0 5)
    set(COMPILER_DIR "${COMPILER_DIR}/Auxiliary/Build")
    if(CMAKE_CL_64)
      set(VCVARS_BAT "${COMPILER_DIR}/vcvars64.bat")
    else(CMAKE_CL_64)
      set(VCVARS_BAT "${COMPILER_DIR}/vcvars32.bat")
    endif(CMAKE_CL_64)
    if(NOT EXISTS "${VCVARS_BAT}")
      message(FATAL_ERROR "Could not find vcvars bat file in ${COMPILER_DIR}")
    endif(NOT EXISTS "${VCVARS_BAT}")
  endif(NOT EXISTS "${VCVARS_BAT}")
endif(MSVC)



# For now, turn everything on:
set(BRLCAD_LEVEL2 ON)
set(BRLCAD_LEVEL3 ON)


set(BRLCAD_DEPS)


###############################################################################
# BRL-CAD and some of its dependencies require the lemon, re2c and perplex
# tools for compilation.  They are not installed, but must be available.  We
# ensure they can be found by building them up front if not present on the
# system.
###############################################################################

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
set(lemon_DESCRIPTION "
Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(lemon LEMON lemon REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_LEMON" DESCRIPTION lemon_DESCRIPTION)
if (${CMAKE_PROJECT_NAME}_LEMON_BUILD)

  ExternalProject_Add(LEMON_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/lemon"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    )

  list(APPEND BRLCAD_DEPS LEMON_BLD)

  set(LEMON_TEMPLATE "${CMAKE_SOURCE_DIR}/misc/tools/lemon/lempar.c" CACHE STRING "Lemon template file" FORCE)
  SetTargetFolder(LEMON_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_LEMON_BUILD)

set(re2c_DESCRIPTION "
Option for enabling and disabling compilation of the re2c scanner
utility provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY_EXECUTABLE(re2c RE2C re2c REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_RE2C" DESCRIPTION re2c_DESCRIPTION)
if (${CMAKE_PROJECT_NAME}_RE2C_BUILD)

  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND RE2C_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  ExternalProject_Add(RE2C_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/re2c"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
    DEPENDS ${RE2C_DEPS}
    )

  list(APPEND BRLCAD_DEPS RE2C_BLD)

  SetTargetFolder(RE2C_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_RE2C_BUILD)

set(perplex_DESCRIPTION "
Option for enabling and disabling compilation of the perplex scanner
generator provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.  perplex requires a working re2c.
")
THIRD_PARTY_EXECUTABLE(perplex PERPLEX perplex REQUIRED "BRLCAD_LEVEL2" ALIASES "ENABLE_PERPLEX" DESCRIPTION perplex_DESCRIPTION NOSYS)
if (${CMAKE_PROJECT_NAME}_PERPLEX_BUILD)

  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND PERPLEX_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  if (TARGET RE2C_BLD)
    set(RE2C_TARGET RE2C_BLD)
    list(APPEND PERPLEX_DEPS RE2C_BLD)
  endif (TARGET RE2C_BLD)

  ExternalProject_Add(PERPLEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../misc/tools/perplex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
    -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
    -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
    DEPENDS ${PERPLEX_DEPS}
    )

  list(APPEND BRLCAD_DEPS PERPLEX_BLD)

  SetTargetFolder(PERPLEX_BLD "Third Party Libraries")
endif (${CMAKE_PROJECT_NAME}_PERPLEX_BUILD)



# Make sure we load FindPERPLEX.cmake to be able to define PERPLEX targets.
include(${BRLCAD_CMAKE_DIR}/FindPERPLEX.cmake)

###############################################################################
#                 zlib compression/decompression library
###############################################################################

set (zlib_DESCRIPTION "
Option for enabling and disabling compilation of the zlib library
provided with BRL-CAD's source distribution.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(libz ZLIB zlib zlib_DESCRIPTION ALIASES ENABLE_ZLIB ENABLE_LIBZ)

if (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

  set(ZLIB_VERSION 1.2.11)

  set(Z_PREFIX_STR "brl_")
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
  set(Z_PREFIX_STR "${Z_PREFIX_STR}" CACHE STRING "prefix for zlib functions" FORCE)

  if (MSVC)
    set(ZLIB_BASENAME z_brl)
    set(ZLIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(ZLIB_BASENAME libz_brl)
    set(ZLIB_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${ZLIB_VERSION})
  endif (MSVC)

  ExternalProject_Add(ZLIB_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libz"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZ_PREFIX_STR=${Z_PREFIX_STR}
    )
  ExternalProject_Target(zlib ZLIB_BLD
    OUTPUT_FILE ${ZLIB_BASENAME}${ZLIB_SUFFIX}
    STATIC_OUTPUT_FILE ${ZLIB_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(ZLIB_BLD ${INCLUDE_DIR}
    zconf.h
    zlib.h
    )

  list(APPEND BRLCAD_DEPS ZLIB_BLD)

  set(ZLIB_LIBRARIES zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)

  SetTargetFolder(ZLIB_BLD "Third Party Libraries")
  SetTargetFolder(zlib "Third Party Libraries")

else (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

  set(Z_PREFIX_STR "" CACHE STRING "clear prefix for zlib functions" FORCE)
  set(Z_PREFIX_STR)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY_RELEASE} CACHE STRING "ZLIB_LIBRARIES" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}" CACHE STRING "ZLIB include directory" FORCE)

endif (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

###############################################################################
#                               libregex
###############################################################################

set (regex_DESCRIPTION "
Option for enabling and disabling compilation of the Regular
Expression Library provided with BRL-CAD's source distribution.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libregex REGEX regex regex_DESCRIPTION ALIASES ENABLE_REGEX)

if (${CMAKE_PROJECT_NAME}_REGEX_BUILD)

  set(REGEX_VERSION "1.0.4")
  if (MSVC)
    set(REGEX_BASENAME regex_brl)
    set(REGEX_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(REGEX_BASENAME libregex_brl)
    set(REGEX_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${REGEX_VERSION})
  endif (MSVC)

  # Platform differences in default linker behavior make it difficult to
  # guarantee that our libregex symbols will override libc. We'll avoid the
  # issue by renaming our libregex symbols to be incompatible with libc.
  ExternalProject_Add(REGEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libregex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DREGEX_PREFIX_STR=libregex_
    )

  ExternalProject_Target(regex REGEX_BLD
    OUTPUT_FILE ${REGEX_BASENAME}${REGEX_SUFFIX}
    STATIC_OUTPUT_FILE ${REGEX_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${REGEX_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(REGEX_BLD ${INCLUDE_DIR}
    regex.h
    )

  list(APPEND BRLCAD_DEPS REGEX_BLD)

  set(REGEX_LIBRARIES regex CACHE STRING "Building bundled libregex" FORCE)
  set(REGEX_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

  SetTargetFolder(REGEX_BLD "Third Party Libraries")
  SetTargetFolder(regex "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_REGEX_BUILD)


###############################################################################
#           netpbm library - support for pnm,ppm,pbm, etc. image files
###############################################################################

set(netpbm_DESCRIPTION "
Option for enabling and disabling compilation of the netpbm library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libnetpbm NETPBM netpbm netpbm_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_NETPBM)

if (${CMAKE_PROJECT_NAME}_NETPBM_BUILD)

  if (MSVC)
    set(NETPBM_BASENAME regex_brl)
  else (MSVC)
    set(NETPBM_BASENAME libregex_brl)
  endif (MSVC)

  ExternalProject_Add(NETPBM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libnetpbm"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(netpbm NETPBM_BLD
    OUTPUT_FILE ${NETPBM_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${NETPBM_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(NETPBM_BLD ${INCLUDE_DIR}
    netpbm/bitio.h
    netpbm/colorname.h
    netpbm/pam.h
    netpbm/pammap.h
    netpbm/pbm.h
    netpbm/pbmfont.h
    netpbm/pgm.h
    netpbm/pm.h
    netpbm/pm_gamma.h
    netpbm/pm_system.h
    netpbm/pnm.h
    netpbm/ppm.h
    netpbm/ppmcmap.h
    netpbm/ppmfloyd.h
    )

  list(APPEND BRLCAD_DEPS NETPBM_BLD)

  set(NETPBM_LIBRARIES netpbm CACHE STRING "Building bundled netpbm" FORCE)
  set(NETPBM_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/netpbm" CACHE STRING "Directory containing netpbm headers." FORCE)

  SetTargetFolder(NETPBM_BLD "Third Party Libraries")
  SetTargetFolder(netpbm "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_NETPBM_BUILD)


###############################################################################
#           libpng - Portable Network Graphics image file support
###############################################################################

# NOTE: we need to have libpng's internal call to find_package looking for zlib
# locate our local copy if we have one.  Defining the ZLIB_ROOT prefix for
# find_package and setting the library file to our custom library name is
# intended to do this (requires CMake 3.12).

set(png_DESCRIPTION "
Option for enabling and disabling compilation of the Portable Network
Graphics library provided with BRL-CAD's source distribution.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")

# We generally don't want the Mac framework libpng...
set(CMAKE_FIND_FRAMEWORK LAST)

THIRD_PARTY(libpng PNG png png_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL2 ALIASES ENABLE_PNG)

if (${CMAKE_PROJECT_NAME}_PNG_BUILD)

  set(PNG_VERSION_MAJOR 16)
  set(PNG_VERSION_MINOR 37)
  set(PNG_LIB_NAME png_brl)

  if (MSVC)
    set(PNG_BASENAME ${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(PNG_BASENAME lib${PNG_LIB_NAME})
    set(PNG_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0)
  endif (MSVC)

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  ExternalProject_Add(PNG_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libpng"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
               -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
               -DPNG_STATIC=${BUILD_STATIC_LIBS} -DSKIP_INSTALL_FILES=ON -DSKIP_INSTALL_EXECUTABLES=ON
               -DSKIP_INSTALL_EXPORT=ON -DPNG_TESTS=OFF -Dld-version-script=OFF
	       -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}> -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
               -DPNG_LIB_NAME=${PNG_LIB_NAME} -DPNG_PREFIX=brl_
    DEPENDS ${ZLIB_TARGET}
    )
  ExternalProject_Target(png PNG_BLD
    OUTPUT_FILE ${PNG_BASENAME}${PNG_SUFFIX}
    STATIC_OUTPUT_FILE ${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR};${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    LINK_TARGET "${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${PNG_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(PNG_BLD ${INCLUDE_DIR}
    png.h
    pngconf.h
    pnglibconf.h
    libpng${PNG_VERSION_MAJOR}/png.h
    libpng${PNG_VERSION_MAJOR}/pngconf.h
    libpng${PNG_VERSION_MAJOR}/pnglibconf.h
    )

  list(APPEND BRLCAD_DEPS PNG_BLD)

  set(PNG_LIBRARIES png CACHE STRING "Building bundled libpng" FORCE)
  set(PNG_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

  SetTargetFolder(PNG_BLD "Third Party Libraries")
  SetTargetFolder(png "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_PNG_BUILD)


###############################################################################
#           libutahrle - Runtime Length Encoding image file support
###############################################################################

set(utahrle_DESCRIPTION "
Option for enabling and disabling compilation of the Utah Raster
Toolkit library provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libutahrle UTAHRLE utahrle utahrle_DESCRIPTION REQUIRED_VARS BRLCAD_LEVEL3 ALIASES ENABLE_UTAHRLE FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_UTAHRLE_BUILD)

  set(UTAHRLE_MAJOR_VERSION 19)
  set(UTAHRLE_MINOR_VERSION 0)
  set(UTAHRLE_PATCH_VERSION 1)
  set(UTAHRLE_VERSION ${UTAHRLE_MAJOR_VERSION}.${UTAHRLE_MINOR_VERSION}.${UTAHRLE_PATCH_VERSION})

  if (MSVC)
    set(UTAHRLE_BASENAME utahrle)
    set(UTAHRLE_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(UTAHRLE_BASENAME libutahrle)
    set(UTAHRLE_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_VERSION})
  endif (MSVC)


  ExternalProject_Add(UTAHRLE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libutahrle"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )

  ExternalProject_Target(utahrle UTAHRLE_BLD
    OUTPUT_FILE ${UTAHRLE_BASENAME}${UTAHRLE_SUFFIX}
    STATIC_OUTPUT_FILE ${UTAHRLE_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${UTAHRLE_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${UTAHRLE_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_MAJOR_VERSION}"
    LINK_TARGET "${UTAHRLE_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${UTAHRLE_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(UTAHRLE_BLD ${INCLUDE_DIR}
    rle.h
    rle_code.h
    rle_config.h
    rle_put.h
    rle_raw.h
    )

  list(APPEND BRLCAD_DEPS UTAHRLE_BLD)

  set(UTAHRLE_LIBRARIES utahrle CACHE STRING "Building bundled libutahrle" FORCE)
  set(UTAHRLE_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing utahrle headers." FORCE)

  SetTargetFolder(UTAHRLE_BLD "Third Party Libraries")
  SetTargetFolder(utahrle "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_UTAHRLE_BUILD)


###############################################################################
#           openNURBS - Non-Uniform Rational B-Spline support
###############################################################################

set(opennurbs_DESCRIPTION "
Option for enabling and disabling compilation of the openNURBS library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(openNURBS OPENNURBS openNURBS opennurbs_DESCRIPTION ALIASES ENABLE_OPENNURBS FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_OPENNURBS_BUILD)

  set(OPENNURBS_MAJOR_VERSION 2012)
  set(OPENNURBS_MINOR_VERSION 10)
  set(OPENNURBS_PATCH_VERSION 245)
  set(OPENNURBS_VERSION ${OPENNURBS_MAJOR_VERSION}.${OPENNURBS_MINOR_VERSION}.${OPENNURBS_PATCH_VERSION})

  if (MSVC)
    set(OPENNURBS_BASENAME openNURBS)
    set(OPENNURBS_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
  else (MSVC)
    set(OPENNURBS_BASENAME libopenNURBS)
    set(OPENNURBS_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_VERSION})
  endif (MSVC)

  if (TARGET ZLIB_BLD)
    set(ZLIB_TARGET ZLIB_BLD)
  endif (TARGET ZLIB_BLD)

  ExternalProject_Add(OPENNURBS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/openNURBS"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}> -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
    DEPENDS ${ZLIB_TARGET}
    )
  ExternalProject_Target(openNURBS OPENNURBS_BLD
    OUTPUT_FILE ${OPENNURBS_BASENAME}${OPENNURBS_SUFFIX}
    STATIC_OUTPUT_FILE ${OPENNURBS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX};${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_MAJOR_VERSION}"
    LINK_TARGET "${OPENNURBS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "${OPENNURBS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )

  ExternalProject_ByProducts(OPENNURBS_BLD ${INCLUDE_DIR}
    openNURBS/opennurbs.h
    openNURBS/opennurbs_3dm.h
    openNURBS/opennurbs_3dm_attributes.h
    openNURBS/opennurbs_3dm_properties.h
    openNURBS/opennurbs_3dm_settings.h
    openNURBS/opennurbs_annotation.h
    openNURBS/opennurbs_annotation2.h
    openNURBS/opennurbs_arc.h
    openNURBS/opennurbs_arccurve.h
    openNURBS/opennurbs_archive.h
    openNURBS/opennurbs_array.h
    openNURBS/opennurbs_array_defs.h
    openNURBS/opennurbs_base32.h
    openNURBS/opennurbs_base64.h
    openNURBS/opennurbs_beam.h
    openNURBS/opennurbs_bezier.h
    openNURBS/opennurbs_bitmap.h
    openNURBS/opennurbs_bounding_box.h
    openNURBS/opennurbs_box.h
    openNURBS/opennurbs_brep.h
    openNURBS/opennurbs_circle.h
    openNURBS/opennurbs_color.h
    openNURBS/opennurbs_compress.h
    openNURBS/opennurbs_cone.h
    openNURBS/opennurbs_crc.h
    openNURBS/opennurbs_curve.h
    openNURBS/opennurbs_curveonsurface.h
    openNURBS/opennurbs_curveproxy.h
    openNURBS/opennurbs_cylinder.h
    openNURBS/opennurbs_defines.h
    openNURBS/opennurbs_detail.h
    openNURBS/opennurbs_dimstyle.h
    openNURBS/opennurbs_dll_resource.h
    openNURBS/opennurbs_ellipse.h
    openNURBS/opennurbs_error.h
    openNURBS/opennurbs_evaluate_nurbs.h
    openNURBS/opennurbs_extensions.h
    openNURBS/opennurbs_font.h
    openNURBS/opennurbs_fpoint.h
    openNURBS/opennurbs_fsp.h
    openNURBS/opennurbs_fsp_defs.h
    openNURBS/opennurbs_geometry.h
    openNURBS/opennurbs_group.h
    openNURBS/opennurbs_hatch.h
    openNURBS/opennurbs_hsort_template.h
    openNURBS/opennurbs_instance.h
    openNURBS/opennurbs_intersect.h
    openNURBS/opennurbs_knot.h
    openNURBS/opennurbs_layer.h
    openNURBS/opennurbs_light.h
    openNURBS/opennurbs_line.h
    openNURBS/opennurbs_linecurve.h
    openNURBS/opennurbs_linestyle.h
    openNURBS/opennurbs_linetype.h
    openNURBS/opennurbs_lookup.h
    openNURBS/opennurbs_mapchan.h
    openNURBS/opennurbs_massprop.h
    openNURBS/opennurbs_material.h
    openNURBS/opennurbs_math.h
    openNURBS/opennurbs_matrix.h
    openNURBS/opennurbs_memory.h
    openNURBS/opennurbs_mesh.h
    openNURBS/opennurbs_nurbscurve.h
    openNURBS/opennurbs_nurbssurface.h
    openNURBS/opennurbs_object.h
    openNURBS/opennurbs_object_history.h
    openNURBS/opennurbs_objref.h
    openNURBS/opennurbs_offsetsurface.h
    openNURBS/opennurbs_optimize.h
    openNURBS/opennurbs_plane.h
    openNURBS/opennurbs_planesurface.h
    openNURBS/opennurbs_pluginlist.h
    openNURBS/opennurbs_point.h
    openNURBS/opennurbs_pointcloud.h
    openNURBS/opennurbs_pointgeometry.h
    openNURBS/opennurbs_pointgrid.h
    openNURBS/opennurbs_polycurve.h
    openNURBS/opennurbs_polyedgecurve.h
    openNURBS/opennurbs_polyline.h
    openNURBS/opennurbs_polylinecurve.h
    openNURBS/opennurbs_qsort_template.h
    openNURBS/opennurbs_rand.h
    openNURBS/opennurbs_rendering.h
    openNURBS/opennurbs_revsurface.h
    openNURBS/opennurbs_rtree.h
    openNURBS/opennurbs_sphere.h
    openNURBS/opennurbs_string.h
    openNURBS/opennurbs_sumsurface.h
    openNURBS/opennurbs_surface.h
    openNURBS/opennurbs_surfaceproxy.h
    openNURBS/opennurbs_system.h
    openNURBS/opennurbs_textlog.h
    openNURBS/opennurbs_texture.h
    openNURBS/opennurbs_texture_mapping.h
    openNURBS/opennurbs_torus.h
    openNURBS/opennurbs_unicode.h
    openNURBS/opennurbs_userdata.h
    openNURBS/opennurbs_uuid.h
    openNURBS/opennurbs_version.h
    openNURBS/opennurbs_viewport.h
    openNURBS/opennurbs_workspace.h
    openNURBS/opennurbs_x.h
    openNURBS/opennurbs_xform.h
    openNURBS/opennurbs_zlib.h
    )

  list(APPEND BRLCAD_DEPS OPENNURBS_BLD)

endif (${CMAKE_PROJECT_NAME}_OPENNURBS_BUILD)

###############################################################################
#                  Screened Poisson Reconstruction Library
###############################################################################

set(spsr_DESCRIPTION "
Option for enabling and disabling compilation of the Screened Poisson
Surface Reconstruction library provided with BRL-CAD's source code.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libspsr SPSR libspsr spsr_DESCRIPTION ALIASES ENABLE_SPSR FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_SPSR_BUILD)

  if (MSVC)
    set(SPSR_BASENAME SPSR)
  else (MSVC)
    set(SPSR_BASENAME libSPSR)
  endif (MSVC)

  ExternalProject_Add(SPSR_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libspsr"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(spsr SPSR_BLD
    OUTPUT_FILE ${SPSR_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${SPSR_BASENAME}-static${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(SPSR_BLD ${INCLUDE_DIR}
    SPSR/SPSR.h
    SPSR/cvertex.h
    )

  set(SPSR_LIBRARIES spsr CACHE STRING "Building bundled spsr" FORCE)
  set(SPSR_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/spsr" CACHE STRING "Directory containing spsr headers." FORCE)

  list(APPEND BRLCAD_DEPS SPSR_BLD)
  
  SetTargetFolder(SPSR_BLD "Third Party Libraries")
  SetTargetFolder(spsr "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_SPSR_BUILD)



###############################################################################
#       STEPcode - support for reading and writing STEP files
###############################################################################

set(sc_DESCRIPTION "
Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(stepcode SC stepcode sc_DESCRIPTION
  REQUIRED_VARS BRLCAD_LEVEL3
  ALIASES ${sc_ALIASES}
  RESET_VARS EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET
  FLAGS NOSYS)

if(BRLCAD_SC_BUILD)


  set(SC_MAJOR_VERSION 2)
  set(SC_MINOR_VERSION 0)
  set(SC_PATCH_VERSION 0)
  set(SC_VERSION ${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}.${SC_PATCH_VERSION})

  if (MSVC)
    set(SC_PREFIX "")
    set(SC_SUFFIX "")
  else (MSVC)
    set(SC_PREFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(SC_SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}.${SC_VERSION}")
  endif (MSVC)


  set(LEMON_TARGET)
  if (TARGET LEMON_BLD)
    set(LEMON_TARGET LEMON_BLD)
    list(APPEND SC_DEPS LEMON_BLD)
  endif (TARGET LEMON_BLD)

  set(RE2C_TARGET)
  if (TARGET RE2C_BLD)
    set(RE2C_TARGET RE2C_BLD)
    list(APPEND SC_DEPS RE2C_BLD)
  endif (TARGET RE2C_BLD)

  set(PERPLEX_TARGET)
  if (TARGET PERPLEX_BLD)
    set(PERPLEX_TARGET PERPLEX_BLD)
    list(APPEND SC_DEPS PERPLEX_BLD)
  endif (TARGET PERPLEX_BLD)

  ExternalProject_Add(STEPCODE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/stepcode"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}>
	       -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
	       -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
	       -DPERPLEX_ROOT=$<$<BOOL:${PERPLEX_TARGET}>:${CMAKE_BINARY_DIR}>
               -DSC_IS_SUBBUILD=ON -DSC_PYTHON_GENERATOR=OFF
               -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
               -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR}
    DEPENDS ${SC_DEPS}
    )
 set(STEPCODE_LIBS base express exppp stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    ExternalProject_Target(lib${SCLIB} STEPCODE_BLD
      OUTPUT_FILE ${SC_PREFIX}${SCLIB}${SC_SUFFIX}
      SYMLINKS "${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX};${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
      LINK_TARGET "${SC_PREFIX}${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )
  endforeach(SCLIB ${STEPCODE_LIBS})
  set(STEPCODE_EXECS check-express exppp exp2cxx)
  foreach(SCEXEC ${STEPCODE_EXECS})
    ExternalProject_Target(${SCEXEC} STEPCODE_BLD
      OUTPUT_FILE ${SCEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )
  endforeach(SCEXEC ${STEPCODE_EXECS})

  set(EXP2CXX_EXEC exp2cxx CACHE STRING "Express to C++ executable" FORCE)
  mark_as_advanced(EXP2CXX_EXEC)
  set(EXP2CXX_EXECUTABLE_TARGET exp2cxx CACHE STRING "Express to C++ executable target" FORCE)
  mark_as_advanced(EXP2CXX_EXECUTABLE_TARGET)
  ExternalProject_ByProducts(STEPCODE_BLD ${INCLUDE_DIR}
    stepcode/cldai/sdaiApplication_instance_set.h
    stepcode/cldai/sdaiSession_instance.h
    stepcode/cldai/sdaiObject.h
    stepcode/cldai/sdaiString.h
    stepcode/cldai/sdaiEntity_extent.h
    stepcode/cldai/sdaiEnum.h
    stepcode/cldai/sdaiModel_contents.h
    stepcode/cldai/sdaiBinary.h
    stepcode/cldai/sdaiEntity_extent_set.h
    stepcode/cldai/sdaiModel_contents_list.h
    stepcode/cldai/sdaiDaObject.h
    stepcode/ordered_attrs.h
    stepcode/exppp/exppp.h
    stepcode/express/hash.h
    stepcode/express/error.h
    stepcode/express/linklist.h
    stepcode/express/basic.h
    stepcode/express/memory.h
    stepcode/express/lexact.h
    stepcode/express/type.h
    stepcode/express/caseitem.h
    stepcode/express/entity.h
    stepcode/express/resolve.h
    stepcode/express/schema.h
    stepcode/express/stmt.h
    stepcode/express/expr.h
    stepcode/express/dict.h
    stepcode/express/expbasic.h
    stepcode/express/alg.h
    stepcode/express/variable.h
    stepcode/express/express.h
    stepcode/express/object.h
    stepcode/express/symbol.h
    stepcode/express/scope.h
    stepcode/sc_export.h
    stepcode/sc_cf.h
    stepcode/clutils/Str.h
    stepcode/clutils/gennodearray.h
    stepcode/clutils/gennode.h
    stepcode/clutils/errordesc.h
    stepcode/clutils/gennodelist.h
    stepcode/clutils/sc_hash.h
    stepcode/clutils/dirobj.h
    stepcode/cleditor/cmdmgr.h
    stepcode/cleditor/editordefines.h
    stepcode/cleditor/SdaiHeaderSchemaClasses.h
    stepcode/cleditor/seeinfodefault.h
    stepcode/cleditor/SdaiHeaderSchema.h
    stepcode/cleditor/SdaiSchemaInit.h
    stepcode/cleditor/STEPfile.h
    stepcode/sc_version_string.h
    stepcode/sc_stdbool.h
    stepcode/base/sc_getopt.h
    stepcode/base/sc_trace_fprintf.h
    stepcode/base/sc_benchmark.h
    stepcode/base/sc_memmgr.h
    stepcode/clstepcore/STEPundefined.h
    stepcode/clstepcore/mgrnodelist.h
    stepcode/clstepcore/STEPattribute.h
    stepcode/clstepcore/STEPaggregate.h
    stepcode/clstepcore/ExpDict.h
    stepcode/clstepcore/read_func.h
    stepcode/clstepcore/needFunc.h
    stepcode/clstepcore/mgrnodearray.h
    stepcode/clstepcore/mgrnode.h
    stepcode/clstepcore/dispnode.h
    stepcode/clstepcore/sdai.h
    stepcode/clstepcore/STEPcomplex.h
    stepcode/clstepcore/instmgr.h
    stepcode/clstepcore/baseType.h
    stepcode/clstepcore/sdaiSelect.h
    stepcode/clstepcore/SubSuperIterators.h
    stepcode/clstepcore/dictdefs.h
    stepcode/clstepcore/SingleLinkList.h
    stepcode/clstepcore/STEPattributeList.h
    stepcode/clstepcore/dispnodelist.h
    stepcode/clstepcore/sdaiApplication_instance.h
    stepcode/clstepcore/Registry.h
    stepcode/clstepcore/complexSupport.h
    )

  list(APPEND BRLCAD_DEPS STEPCODE_BLD)

  SetTargetFolder(STEPCODE_BLD "Third Party Libraries")
  SetTargetFolder(stepcode "Third Party Libraries")
endif(BRLCAD_SC_BUILD)

###############################################################################
#      VDSlib - A View-Dependent Simplification and Rendering Library
###############################################################################

#  For the moment, this is marked NOSYS - it's possible that some Debian
#  systems would have 0.9 of vdslib installed, but it's unmaintained and we're
#  likely to be making changes.  If our own copy of VDSlib ever spins back off
#  into its own project, revisit the NOSYS

set(libvds_DESCRIPTION "
Option for enabling and disabling compilation of the libvds triangle
simplification library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libvds VDS libvds libvds_DESCRIPTION ALIASES ENABLE_VDS FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_VDS_BUILD)

  if (MSVC)
    set(VDS_BASENAME vds)
  else (MSVC)
    set(VDS_BASENAME libvds)
  endif (MSVC)


  ExternalProject_Add(VDS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libvds"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(vds VDS_BLD
    OUTPUT_FILE ${VDS_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${VDS_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(VDS_BLD ${INCLUDE_DIR}
    vds.h
    )

  set(VDS_LIBRARIES vds CACHE STRING "Building bundled netpbm" FORCE)
  set(VDS_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing vds headers." FORCE)

  list(APPEND BRLCAD_DEPS VDS_BLD)
  
  SetTargetFolder(VDS_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_VDS_BUILD)


###############################################################################
#               gdiam - approximate tight bounding boxes
###############################################################################

set(libgdiam_DESCRIPTION "
Option for enabling and disabling compilation of the libgdiam approximate
tight bounding box library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libgdiam GDIAM libgdiam libgdiam_DESCRIPTION ALIASES ENABLE_GDIAM FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_GDIAM_BUILD)

  if (MSVC)
    set(GDIAM_BASENAME gdiam)
  else (MSVC)
    set(GDIAM_BASENAME libgdiam)
  endif (MSVC)

  ExternalProject_Add(GDIAM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/libgdiam"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(gdiam GDIAM_BLD
    OUTPUT_FILE ${GDIAM_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${GDIAM_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(GDIAM_BLD ${INCLUDE_DIR}
    gdiam.hpp
    )
  
  list(APPEND BRLCAD_DEPS GDIAM_BLD)

  SetTargetFolder(GDIAM_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_GDIAM_BUILD)


###############################################################################
#             Poly2Tri - 2D constrained Delaunay triangulation
###############################################################################

set(poly2tri_DESCRIPTION "
Option for enabling and disabling compilation of the poly2tri 2D constrained
Delaunay triangulation library provided with BRL-CAD's source code.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing first
for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(poly2tri POLY2TRI poly2tri poly2tri_DESCRIPTION ALIASES ENABLE_POLY2TRI FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_POLY2TRI_BUILD)

  if (MSVC)
    set(POLY2TRI_BASENAME poly2tri)
  else (MSVC)
    set(POLY2TRI_BASENAME libpoly2tri)
  endif (MSVC)

  ExternalProject_Add(POLY2TRI_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../other/poly2tri"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(poly2tri POLY2TRI_BLD
    OUTPUT_FILE ${POLY2TRI_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE ${POLY2TRI_BASENAME}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(POLY2TRI_BLD ${INCLUDE_DIR}
    poly2tri/poly2tri.h
    poly2tri/common/shapes.h
    poly2tri/sweep/cdt.h
    poly2tri/sweep/advancing_front.h
    poly2tri/sweep/sweep.h
    poly2tri/sweep/sweep_context.h
    )

  list(APPEND BRLCAD_DEPS POLY2TRI_BLD)
  
  SetTargetFolder(POLY2TRI_BLD "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_POLY2TRI_BUILD)


###############################################################################
#                    Primary BRL-CAD Compilation
###############################################################################

# Set variables based on what we're building, so we know what to tell the main
# BRL-CAD build.
set(TARGET_LIST
  LEMON
  RE2C
  PERPLEX
  REGEX
  ZLIB
  PNG
  NETPBM
  UTAHRLE
  OPENNURBS
  SPSR
  STEPCODE
  VDS
  GDIAM
  POLY2TRI
  )
foreach(T ${TARGET_LIST})
if (TARGET ${T}_BLD)
  set(${T}_TARGET ${T}_BLD)
endif (TARGET ${T}_BLD)
endforeach(T ${TARGET_LIST})

# We've built everything we needed to build - now we can build BRL-CAD itself
ExternalProject_Add(BRLCAD_BLD
  SOURCE_DIR "${BRLCAD_SOURCE_DIR}"
  PREFIX ${BRLCAD_BINARY_DIR}/brlcad
  BINARY_DIR ${BRLCAD_BINARY_DIR}/brlcad-build
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
  -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
  -DSUBBUILD=ON
  -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
  -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
  -DPERPLEX_ROOT=$<$<BOOL:${PERPLEX_TARGET}>:${CMAKE_BINARY_DIR}> -DPERPLEX_TEMPLATE=$<$<BOOL:${PERPLEX_TARGET}>:${PERPLEX_TEMPLATE}>
  -DREGEX_ROOT=$<$<BOOL:${REGEX_TARGET}>:${CMAKE_BINARY_DIR}> -DREGEX_LIBRARY=$<$<BOOL:${REGEX_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
  -DZLIB_ROOT=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}> -DZLIB_LIBRARY=$<$<BOOL:${ZLIB_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
  -DPNG_ROOT=$<$<BOOL:${PNG_TARGET}>:${CMAKE_BINARY_DIR}> -DPNG_LIBRARY=$<$<BOOL:${PNG_TARGET}>:${CMAKE_BINARY_DIR}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}>
  -DNETPBM_ROOT=$<$<BOOL:${NETPBM_TARGET}>:${CMAKE_BINARY_DIR}>
  -DUTAHRLE_ROOT=$<$<BOOL:${UTAHRLE_TARGET}>:${CMAKE_BINARY_DIR}>
  -DOPENNURBS_ROOT=$<$<BOOL:${OPENNURBS_TARGET}>:${CMAKE_BINARY_DIR}>
  -DSPSR_ROOT=$<$<BOOL:${SPSR_TARGET}>:${CMAKE_BINARY_DIR}>
  -DSTEPCODE_ROOT=$<$<BOOL:${STEPCODE_TARGET}>:${CMAKE_BINARY_DIR}>
  -DVDS_ROOT=$<$<BOOL:${VDS_TARGET}>:${CMAKE_BINARY_DIR}>
  -DGDIAM_ROOT=$<$<BOOL:${GDIAM_TARGET}>:${CMAKE_BINARY_DIR}>
  -DPOLY2TRI_ROOT=$<$<BOOL:${POLY2TRI_TARGET}>:${CMAKE_BINARY_DIR}>
  DEPENDS ${BRLCAD_DEPS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

