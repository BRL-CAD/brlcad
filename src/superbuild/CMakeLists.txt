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

# If targets are built, we want to group them in build tools that support
# doing so.
function(SetTargetFolder targetname folder)
  if(TARGET ${targetname})
    set_target_properties(${targetname} PROPERTIES FOLDER "${folder}")
  endif(TARGET ${targetname})
endfunction(SetTargetFolder)

# By default, changes in src/other files won't trigger a rebuild.  Setting this
# to 1 at configure time changes that, but at the expense of running the build
# step every time.  This may be fast if there is nothing to update in the
# project, but it will be slower than skipping the step completely.
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

include(${CMAKE_CURRENT_SOURCE_DIR}/tools.cmake)

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

# libutahrle - Runtime Length Encoding image file support
include(${CMAKE_CURRENT_SOURCE_DIR}/utahrle.cmake)

# openNURBS - Non-Uniform Rational B-Spline support
include(${CMAKE_CURRENT_SOURCE_DIR}/openNURBS.cmake)

# Screened Poisson Reconstruction Library
include(${CMAKE_CURRENT_SOURCE_DIR}/spsr.cmake)

# STEPcode - support for reading and writing STEP files
include(${CMAKE_CURRENT_SOURCE_DIR}/stepcode.cmake)

# VDSlib - A View-Dependent Simplification and Rendering Library
include(${CMAKE_CURRENT_SOURCE_DIR}/vds.cmake)

# gdiam - approximate tight bounding boxes
include(${CMAKE_CURRENT_SOURCE_DIR}/gdiam.cmake)

# Poly2Tri - 2D constrained Delaunay triangulation
include(${CMAKE_CURRENT_SOURCE_DIR}/poly2tri.cmake)

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
include(${CMAKE_CURRENT_SOURCE_DIR}/tktable.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/tkhtml.cmake)

###############################################################################
#                    Primary BRL-CAD Compilation
###############################################################################

# Set variables based on what we're building, so we know what to tell the main
# BRL-CAD build.  Note we don't need to do this for Tk or the other Tcl
# packages, as they will key of of the Tcl result.
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
  BSON
  PROJ4
  GDAL
  TCL
  )
foreach(T ${TARGET_LIST})
if (TARGET ${T}_BLD)
  set(${T}_TARGET ${T}_BLD)
endif (TARGET ${T}_BLD)
endforeach(T ${TARGET_LIST})

if(BRLCAD_DEPS)
  add_custom_target(deps DEPENDS ${BRLCAD_DEPS})
endif(BRLCAD_DEPS)

# We've built everything we needed to build - now we can build BRL-CAD itself

ExternalProject_Add(BRLCAD_BLD
  SOURCE_DIR "${BRLCAD_SOURCE_DIR}"
  PREFIX ${BRLCAD_BINARY_DIR}/brlcad
  BINARY_DIR ${BRLCAD_BINARY_DIR}/brlcad-build
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
  -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
  -DSUBBUILD=ON -DBRLCAD_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
  -DLEMON_ROOT=$<$<BOOL:${LEMON_TARGET}>:${CMAKE_BINARY_DIR}> -DLEMON_TEMPLATE=$<$<BOOL:${LEMON_TARGET}>:${LEMON_TEMPLATE}>
  -DRE2C_ROOT=$<$<BOOL:${RE2C_TARGET}>:${CMAKE_BINARY_DIR}>
  -DPERPLEX_ROOT=$<$<BOOL:${PERPLEX_TARGET}>:${CMAKE_BINARY_DIR}> -DPERPLEX_TEMPLATE=${CMAKE_SOURCE_DIR}/misc/tools/perplex/perplex_template.c
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
  -DBSON_ROOT=$<$<BOOL:${BSON_TARGET}>:${CMAKE_BINARY_DIR}>
  -DPROJ4_ROOT=$<$<BOOL:${PROJ4_TARGET}>:${CMAKE_BINARY_DIR}>
  -DGDAL_ROOT=$<$<BOOL:${GDAL_TARGET}>:${CMAKE_BINARY_DIR}>
  -DTCL_ROOT=${CMAKE_BINARY_DIR}
  DEPENDS ${BRLCAD_DEPS}
  )

if("${CMAKE_GENERATOR}" MATCHES "Make")
  add_custom_target(brlcad ALL
    COMMAND cd ${BRLCAD_BINARY_DIR}/brlcad-build && $(MAKE) && $(MAKE) install
    DEPENDS deps
    )
elseif("${CMAKE_GENERATOR}" MATCHES "Ninja")
  add_custom_target(brlcad ALL
    COMMAND cd ${BRLCAD_BINARY_DIR}/brlcad-build && ninja && ninja install
    DEPENDS deps
    )
else()
  add_custom_target(brlcad-build
    COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_BINARY_DIR}/brlcad-build
    DEPENDS deps
    )
  add_custom_target(brlcad ALL
    COMMAND ${CMAKE_COMMAND} --build ${BRLCAD_BINARY_DIR}/brlcad-build --target install
    DEPENDS brlcad-build
    )
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

