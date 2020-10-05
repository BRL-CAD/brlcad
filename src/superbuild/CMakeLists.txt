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

project(BDEPS)

cmake_minimum_required(VERSION 3.12)

# If defined, get feature variables from parent
if (EXISTS ${CMAKE_BINARY_DIR}/brlcad_vars.cmake)
  include(${CMAKE_BINARY_DIR}/brlcad_vars.cmake)
endif (EXISTS ${CMAKE_BINARY_DIR}/brlcad_vars.cmake)

# We need some extra functionality for this...
set(BDEPS_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/CMake")
set(CMAKE_MODULE_PATH "${BDEPS_CMAKE_DIR};${CMAKE_MODULE_PATH}")

# We do not want to search in our install target; on a second
# configure pass the outputs of previous builds will be detected
# and mess up the results...
set(CMAKE_SYSTEM_IGNORE_PATH "${CMAKE_INSTALL_PREFIX}")

include(Path_Setup)
include(RPath_Setup)
cmake_set_rpath()

# If targets are built, we want to group them in build tools that support
# doing so.
function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# By default, changes in src/superbuild files won't trigger a rebuild.  Setting
# this to 1 changes that, but at the expense of running the build step every
# time.  This may be fast if there is nothing to update in the project, but it
# will be slower than skipping the step completely.
if (NOT DEFINED EXTERNAL_BUILD_UPDATE)
  set(EXTERNAL_BUILD_UPDATE 0)
endif (NOT DEFINED EXTERNAL_BUILD_UPDATE)

# Use this variable to key both CMake messages and tools like rpath_replace
if(NOT DEFINED EXTPROJ_VERBOSE)
  set(EXTPROJ_VERBOSE 0)
endif(NOT DEFINED EXTPROJ_VERBOSE)

# Superbuilds use ExternalProject_Add, and BRL-CAD defines some additional
# targets for managing them.
include(ExternalProject)
# Note- make sure CMAKE_BINARY_DIR and CMAKE_INSTALL_PREFIX are finalized
# BEFORE including this file!
include(ExternalProject_Target)

# External build outputs can be verbose - capture them to files instead of
# outputting to console
#set(LOG_OPTS
#  LOG_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/ExternalProject_logs"
#  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON
#  LOG_MERGED_STDOUTERR ON LOG_OUTPUT_ON_FAILURE ON
#  )

set(LOG_OPTS)

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


# We need to track projects that must be built before starting the main
# BRL-CAD build, since that set changes with build options and can't be
# hardcoded.
set(BRLCAD_DEPS)


###############################################################################
# BRL-CAD and some of its dependencies require the lemon, re2c and perplex
# tools for compilation.  They are not installed, but must be available.  We
# ensure they can be found by building them up front if not present on the
# system.
###############################################################################

include(${CMAKE_CURRENT_SOURCE_DIR}/perplex.cmake)

###############################################################################
# Build logic is broken out per-library, but the ordering is important.  Some
# libraries will depend on others listed here (for example, we want openNURBS
# to use our bundled zlib if it is enabled.) Developers adding, reordering or
# removing dependencies here need to make sure they are aware of impact they
# may be having on other external projects in other files.
###############################################################################

# zlib compression/decompression library
include(${CMAKE_CURRENT_SOURCE_DIR}/zlib.cmake)

# libregex regular expression matching
include(${CMAKE_CURRENT_SOURCE_DIR}/regex.cmake)

# netpbm library - support for pnm,ppm,pbm, etc. image files
include(${CMAKE_CURRENT_SOURCE_DIR}/netpbm.cmake)

# libpng - Portable Network Graphics image file support
include(${CMAKE_CURRENT_SOURCE_DIR}/png.cmake)

# STEPcode - support for reading and writing STEP files
include(${CMAKE_CURRENT_SOURCE_DIR}/stepcode.cmake)

# libbson - for binary attributes, always use this local version
include(${CMAKE_CURRENT_SOURCE_DIR}/bson.cmake)

# PROJ4 - generic coordinate transformation
include(${CMAKE_CURRENT_SOURCE_DIR}/proj4.cmake)

# GDAL -  translator library for raster and vector geospatial data formats
include(${CMAKE_CURRENT_SOURCE_DIR}/gdal.cmake)

# TCL - scripting language
include(${CMAKE_CURRENT_SOURCE_DIR}/tcl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/tk.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/itcl.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/itk.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/iwidgets.cmake)

# For anything we've built here, we need to let the parent build know
# where to go looking for key components
file(WRITE "${BRLCAD_BINARY_DIR}/superbuild.cmake" "\#superbuild outputs\n")
if (TARGET PERPLEX_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(LEMON_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(LEMON_TEMPLATE ${BRLCAD_BINARY_DIR}/${DATA_DIR}/lemon/lempar.c)\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(RE2C_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(PERPLEX_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(PERPLEX_TEMPLATE ${BRLCAD_BINARY_DIR}/${DATA_DIR}/perplex/perplex_template.c)\n")
else (TARGET PERPLEX_BLD)
  message(FATAL_ERROR "Failed to build perplex\n")
endif (TARGET PERPLEX_BLD)
if (TARGET NETPBM_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(NETPBM_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET NETPBM_BLD)
if (TARGET STEPCODE_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(STEPCODE_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET STEPCODE_BLD)
if (TARGET BSON_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(BSON_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET BSON_BLD)
if (TARGET PROJ4_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(PROJ4_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET PROJ4_BLD)
if (TARGET GDAL_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(GDAL_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET GDAL_BLD)
if (TARGET TCL_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(TCL_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
endif (TARGET TCL_BLD)

# (For REGEX, ZLIB and PNG we need to do a little extra work and supply our
# custom library names so find_package knows about them.)
if (TARGET REGEX_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(REGEX_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(REGEX_LIBRARY \"${BRLCAD_BINARY_DIR}/${LIB_DIR}/${REGEX_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\")\n")
endif (TARGET REGEX_BLD)
if (TARGET ZLIB_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(ZLIB_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(REGEX_LIBRARY \"${BRLCAD_BINARY_DIR}/${LIB_DIR}/${ZLIB_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\")\n")
endif (TARGET ZLIB_BLD)
if (TARGET PNG_BLD)
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(PNG_ROOT \"${BRLCAD_BINARY_DIR}\")\n")
  file(APPEND "${BRLCAD_BINARY_DIR}/superbuild.cmake" "set(PNG_LIBRARY \"${BRLCAD_BINARY_DIR}/${LIB_DIR}/${PNG_BASENAME}${CMAKE_SHARED_LIBRARY_SUFFIX}\")\n")
endif (TARGET PNG_BLD)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

