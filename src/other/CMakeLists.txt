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

# Most third party items have a list calling out files for distcheck -
# these are stored in files in the dlists directory.  Ignore that
# directory for distcheck
file(GLOB dlists "*.dist")
foreach(ITEM ${dlists})
  get_filename_component(dlist ${ITEM} NAME)
  CMAKEFILES(${dlist})
endforeach(ITEM ${dlists})

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

# ExternalProject_Add uses configure_file internally - we don't want warnings
# about those files, so suppress our warning system
set(SUPPRESS_GENERATED_TAG TRUE)

# Custom patch utility for external RPath preparation
include(ExternalProject)
# Note- make sure CMAKE_BINARY_DIR and CMAKE_INSTALL_PREFIX are finalized BEFORE including this file!
include(${BRLCAD_CMAKE_DIR}/ExternalProject_Target.cmake)

# External build outputs can be verbose - capture them to files instead of
# outputting to console
#set(LOG_OPTS
#  LOG_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/ExternalProject_logs"
#  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON
#  LOG_MERGED_STDOUTERR ON LOG_OUTPUT_ON_FAILURE ON
#  )

set(LOG_OPTS)

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

# In case we need to pass some sort of reasonable parallel
# flag to a build system, check our processor count
include(ProcessorCount)
ProcessorCount(N)
math(EXPR pcnt "${N} / 2")
if (NOT pcnt)
  set(pcnt 1)
endif (NOT pcnt)

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

  # Platform differences in default linker behavior make it difficult to
  # guarantee that our libregex symbols will override libc. We'll avoid the
  # issue by renaming our libregex symbols to be incompatible with libc.
  ExternalProject_Add(REGEX_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libregex"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DREGEX_PREFIX_STR=libregex_
    )

  ExternalProject_Target(regex REGEX_BLD
    OUTPUT_FILE libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0.4
    STATIC_OUTPUT_FILE libregex${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libregex${CMAKE_SHARED_LIBRARY_SUFFIX};libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "libregex${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libregex${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  ExternalProject_ByProducts(REGEX_BLD ${INCLUDE_DIR}
    regex.h
    )

  set(REGEX_LIBRARIES regex CACHE STRING "Building bundled libregex" FORCE)
  set(REGEX_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

  SetTargetFolder(REGEX_BLD "Third Party Libraries")
  SetTargetFolder(regex "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_REGEX_BUILD)

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

  ExternalProject_Add(ZLIB_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libz"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DZ_PREFIX_STR=brl_
    )
  if (NOT MSVC)
    ExternalProject_Target(zlib ZLIB_BLD
      OUTPUT_FILE libz${CMAKE_SHARED_LIBRARY_SUFFIX}.${ZLIB_VERSION}
      STATIC_OUTPUT_FILE libz${CMAKE_STATIC_LIBRARY_SUFFIX}
      SYMLINKS "libz${CMAKE_SHARED_LIBRARY_SUFFIX};libz${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
      LINK_TARGET "libz${CMAKE_SHARED_LIBRARY_SUFFIX}"
      )
  else (NOT MSVC)
    ExternalProject_Target(zlib ZLIB_BLD
      OUTPUT_FILE zlib${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE zlibstatic${CMAKE_STATIC_LIBRARY_SUFFIX}
      LINK_TARGET_DEBUG "zlibd${CMAKE_SHARED_LIBRARY_SUFFIX}"
      STATIC_LINK_TARGET_DEBUG "zlibstaticd${CMAKE_STATIC_LIBRARY_SUFFIX}"
      )
  endif (NOT MSVC)

  ExternalProject_ByProducts(ZLIB_BLD ${INCLUDE_DIR}
    zconf.h
    zlib.h
    )

  set(ZLIB_LIBRARIES zlib CACHE STRING "Building bundled zlib" FORCE)
  set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)

  SetTargetFolder(ZLIB_BLD "Third Party Libraries")
  SetTargetFolder(zlib "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_ZLIB_BUILD)

###############################################################################
#                 LZ4 compression/decompression library
###############################################################################

set(lz4_DESCRIPTION "
Option for enabling and disabling compilation of the lz4 data
compression library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(lz4 LZ4 lz4 lz4_DESCRIPTION ALIASES ENABLE_LZ4 FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_LZ4_BUILD)

  ExternalProject_Add(LZ4_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lz4"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(lz4 LZ4_BLD
    OUTPUT_FILE liblz4${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE liblz4${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(LZ4_BLD ${INCLUDE_DIR}
    lz4.h
    lz4hc.h
    lz4frame.h
    )

  set(LZ4_LIBRARIES lz4 CACHE STRING "Building bundled lz4" FORCE)
  set(LZ4_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing lz4 headers." FORCE)

  SetTargetFolder(LZ4_BLD "Third Party Libraries")
  SetTargetFolder(lz4 "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_LZ4_BUILD)

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

  ExternalProject_Add(NETPBM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libnetpbm"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(netpbm NETPBM_BLD
    OUTPUT_FILE libnetpbm${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libnetpbm${CMAKE_STATIC_LIBRARY_SUFFIX}
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
# find_package is intended to do this (requires CMake 3.12).

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
set(PNG_VERSION_MAJOR 16)
set(PNG_VERSION_MINOR 37)

if (${CMAKE_PROJECT_NAME}_PNG_BUILD)
  ExternalProject_Add(PNG_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libpng"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DCMAKE_INSTALL_LIBDIR=${LIB_DIR}
    	       -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
	       -DPNG_STATIC=${BUILD_STATIC_LIBS} -DZLIB_ROOT=${CMAKE_BINARY_DIR} -DSKIP_INSTALL_FILES=ON
	       -DSKIP_INSTALL_EXECUTABLES=ON -DSKIP_INSTALL_EXPORT=ON -DPNG_TESTS=OFF -Dld-version-script=OFF
	       -DPNG_PREFIX=brl_

    DEPENDS ${ZLIB_LIBRARIES}
    )
  ExternalProject_Target(png PNG_BLD
    OUTPUT_FILE libpng${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR}.${PNG_VERSION_MINOR}.0
    STATIC_OUTPUT_FILE libpng16${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libpng${CMAKE_SHARED_LIBRARY_SUFFIX};libpng${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX};libpng${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR};libpng${CMAKE_STATIC_LIBRARY_SUFFIX}"
    LINK_TARGET "libpng${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libpng${CMAKE_STATIC_LIBRARY_SUFFIX}"
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

  set(PNG_LIBRARIES netpbm CACHE STRING "Building bundled libpng" FORCE)
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

  ExternalProject_Add(UTAHRLE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libutahrle"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )

  ExternalProject_Target(utahrle UTAHRLE_BLD
    OUTPUT_FILE libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_VERSION}
    STATIC_OUTPUT_FILE libutahrle${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX};libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_MAJOR_VERSION}"
    LINK_TARGET "libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libutahrle${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(UTAHRLE_BLD ${INCLUDE_DIR}
    rle.h
    rle_code.h
    rle_config.h
    rle_put.h
    rle_raw.h
    )

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

  ExternalProject_Add(OPENNURBS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openNURBS"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    DEPENDS ${ZLIB_LIBRARIES}
    )

  ExternalProject_Target(openNURBS OPENNURBS_BLD
    OUTPUT_FILE libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_VERSION}
    STATIC_OUTPUT_FILE libopenNURBS${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX};libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}.${OPENNURBS_MAJOR_VERSION}"
    LINK_TARGET "libopenNURBS${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libopenNURBS${CMAKE_STATIC_LIBRARY_SUFFIX}"
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

  set(OPENNURBS_LIBRARIES openNURBS CACHE STRING "openNURBS" FORCE)
  set(OPENNURBS_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/openNURBS;${ZLIB_INCLUDE_DIRS}" CACHE STRING "Directory containing openNURBS headers." FORCE)

  SetTargetFolder(OPENNURBS_BLD "Third Party Libraries")
  SetTargetFolder(openNURBS "Third Party Libraries")

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

  ExternalProject_Add(SPSR_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libspsr"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(spsr SPSR_BLD
    OUTPUT_FILE libSPSR${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libSPSR-static${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(SPSR_BLD ${INCLUDE_DIR}
    SPSR/SPSR.h
    SPSR/cvertex.h
    )

  set(SPSR_LIBRARIES spsr CACHE STRING "Building bundled spsr" FORCE)
  set(SPSR_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/spsr" CACHE STRING "Directory containing spsr headers." FORCE)

  SetTargetFolder(SPSR_BLD "Third Party Libraries")
  SetTargetFolder(spsr "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_SPSR_BUILD)


###############################################################################
#     libbson - for binary attributes, always use this local version
###############################################################################

# For testing
# set(BRLCAD_ENABLE_BINARY_ATTRIBUTES ON)

set(libbson_DESCRIPTION "
Option for enabling and disabling compilation of the Libbson library
provided with BRL-CAD's source code.  Default is BUNDLED, using
the included other/src version.
")
THIRD_PARTY(libbson BSON Libbson libbson_DESCRIPTION ALIASES ENABLE_BSON REQUIRED_VARS BRLCAD_ENABLE_BINARY_ATTRIBUTES FLAGS NOSYS UNDOCUMENTED)

if (${CMAKE_PROJECT_NAME}_BSON_BUILD)

  set(BSON_MAJOR_VERSION 1)
  set(BSON_MINOR_VERSION 3)
  set(BSON_PATCH_VERSION 5)
  set(BSON_VERSION ${BSON_MAJOR_VERSION}.${BSON_MINOR_VERSION}.${BSON_PATCH_VERSION})

  ExternalProject_Add(BSON_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libbson"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )

  ExternalProject_Target(bson BSON_BLD
    OUTPUT_FILE libbson-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}.${BSON_VERSION}
    STATIC_OUTPUT_FILE libbson-1.0${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libbson-1.0${CMAKE_SHARED_LIBRARY_SUFFIX};libbson-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}.${BSON_MAJOR_VERSION}"
    LINK_TARGET "libbson-1.0${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libbson-1.0${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )
  ExternalProject_ByProducts(BSON_BLD ${INCLUDE_DIR}
    libbson-1.0/bson-endian.h
    libbson-1.0/bson-md5.h
    libbson-1.0/bson-value.h
    libbson-1.0/bson-stdint.h
    libbson-1.0/bson-reader.h
    libbson-1.0/bson-context.h
    libbson-1.0/bson-string.h
    libbson-1.0/bson-error.h
    libbson-1.0/bson-macros.h
    libbson-1.0/bson-compat.h
    libbson-1.0/bson-version.h
    libbson-1.0/bson-oid.h
    libbson-1.0/bson-version-functions.h
    libbson-1.0/bson-iter.h
    libbson-1.0/bson-stdint-win32.h
    libbson-1.0/bson-atomic.h
    libbson-1.0/bson-memory.h
    libbson-1.0/bcon.h
    libbson-1.0/bson-json.h
    libbson-1.0/bson-keys.h
    libbson-1.0/bson-utf8.h
    libbson-1.0/bson-types.h
    libbson-1.0/bson.h
    libbson-1.0/bson-writer.h
    libbson-1.0/bson-config.h
    libbson-1.0/bson-clock.h
    )

  set(BSON_LIBRARIES utahrle CACHE STRING "Building bundled libbson" FORCE)
  set(BSON_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing bson headers." FORCE)

  SetTargetFolder(BSON_BLD "Third Party Libraries")
  SetTargetFolder(utahrle "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_BSON_BUILD)

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
  REQUIRED_VARS LEMON_EXECUTABLE PERPLEX_EXECUTABLE BRLCAD_LEVEL3
  ALIASES ${sc_ALIASES}
  RESET_VARS EXP2CXX_EXEC EXP2CXX_EXECUTABLE_TARGET
  FLAGS NOSYS)

if(BRLCAD_SC_BUILD)

  set(LEMON_TARGET)
  if (TARGET lemon)
    set(LEMON_TARGET lemon)
  endif (TARGET lemon)

  set(PERPLEX_TARGET)
  if (TARGET lemon)
    set(PERPLEX_TARGET lemon)
  endif (TARGET lemon)

  ExternalProject_Add(STEPCODE_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stepcode"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DPERPLEX_ROOT=${CMAKE_BINARY_DIR} -DLEMON_ROOT=${CMAKE_BINARY_DIR}
	       -DLEMON_TEMPLATE=${CMAKE_BINARY_DIR}/share/lemon/lempar.c
	       -DSC_IS_SUBBUILD=ON -DSC_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DSC_PYTHON_GENERATOR=OFF
	       -DSC_ENABLE_TESTING=OFF -DSC_ENABLE_COVERAGE=OFF -DSC_BUILD_SCHEMAS=
	       -DINCLUDE_INSTALL_DIR=${INCLUDE_DIR} -DBIN_INSTALL_DIR=${BIN_DIR} -DLIB_INSTALL_DIR=${LIB_DIR}
    DEPENDS ${LEMON_TARGET} ${PERPLEX_TARGET}
    )

  set(STEPCODE_LIBS base express exppp stepcore stepeditor stepdai steputils)
  foreach(SCLIB ${STEPCODE_LIBS})
    ExternalProject_Target(lib${SCLIB} STEPCODE_BLD
      OUTPUT_FILE lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2.0.0
      SYMLINKS "lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX};lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}.2"
      LINK_TARGET "lib${SCLIB}${CMAKE_SHARED_LIBRARY_SUFFIX}"
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

  ExternalProject_Add(VDS_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libvds"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(vds VDS_BLD
    OUTPUT_FILE libvds${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libvds${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(VDS_BLD ${INCLUDE_DIR}
    vds.h
    )

  set(VDS_LIBRARIES vds CACHE STRING "Building bundled netpbm" FORCE)
  set(VDS_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing vds headers." FORCE)

  SetTargetFolder(VDS_BLD "Third Party Libraries")
  SetTargetFolder(vds "Third Party Libraries")

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

  ExternalProject_Add(GDIAM_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libgdiam"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(gdiam GDIAM_BLD
    OUTPUT_FILE libgdiam${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libgdiam${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(GDIAM_BLD ${INCLUDE_DIR}
    gdiam.hpp
    )

  set(GDIAM_LIBRARIES gdiam CACHE STRING "Building bundled gdiam" FORCE)
  set(GDIAM_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing gdiam headers." FORCE)

  SetTargetFolder(GDIAM_BLD "Third Party Libraries")
  SetTargetFolder(gdiam "Third Party Libraries")

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

  ExternalProject_Add(POLY2TRI_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/poly2tri"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(poly2tri POLY2TRI_BLD
    OUTPUT_FILE libpoly2tri${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libpoly2tri${CMAKE_STATIC_LIBRARY_SUFFIX}
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

  set(POLY2TRI_LIBRARIES poly2tri CACHE STRING "Building bundled poly2tri" FORCE)
  set(POLY2TRI_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing poly2tri headers." FORCE)

  SetTargetFolder(POLY2TRI_BLD "Third Party Libraries")
  SetTargetFolder(poly2tri "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_POLY2TRI_BUILD)

###############################################################################
#         RPly - support for reading and writign PLY geometry files
###############################################################################

set(rply_DESCRIPTION "
Option for enabling and disabling compilation of the RPly library for PLY file
input and ouput provided with BRL-CAD's source code.  Default is AUTO,
responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a
system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(rply RPLY rply rply_DESCRIPTION ALIASES ENABLE_RPLY FLAGS NOSYS)

if (${CMAKE_PROJECT_NAME}_RPLY_BUILD)

  ExternalProject_Add(RPLY_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rply"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(rply RPLY_BLD
    OUTPUT_FILE librply${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE librply${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  ExternalProject_ByProducts(RPLY_BLD ${INCLUDE_DIR}
    rply.h
    )

  set(RPLY_LIBRARIES rply CACHE STRING "Building bundled rply" FORCE)
  set(RPLY_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing rply headers." FORCE)

  SetTargetFolder(RPLY_BLD "Third Party Libraries")
  SetTargetFolder(rply "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_RPLY_BUILD)


###############################################################################
#               PROJ - generic coordinate transformation
###############################################################################

set(proj4_DESCRIPTION "
Option for enabling and disabling compilation of the PROJ.4 geographic
projection library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(proj-4 PROJ4 proj4 proj4_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_PROJ4)

if (${CMAKE_PROJECT_NAME}_PROJ4_BUILD)

  set(PROJ_MAJOR_VERSION 4)
  set(PROJ_MINOR_VERSION 9)
  set(PROJ_API_VERSION 12)
  set(PROJ_VERSION ${PROJ_MAJOR_VERSION}.${PROJ_MINOR_VERSION}.${PROJ_API_VERSION})

  ExternalProject_Add(PROJ4_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proj-4"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )
  ExternalProject_Target(proj PROJ4_BLD
    OUTPUT_FILE libproj${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_VERSION}
    STATIC_OUTPUT_FILE libproj${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "libproj${CMAKE_SHARED_LIBRARY_SUFFIX};libproj${CMAKE_SHARED_LIBRARY_SUFFIX}.${PROJ_API_VERSION}"
    LINK_TARGET "libproj${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libproj${CMAKE_STATIC_LIBRARY_SUFFIX}"
    RPATH
    )

  ExternalProject_ByProducts(PROJ4_BLD ${DATA_DIR}
    proj/epsg
    proj/esri
    proj/world
    proj/esri.extra
    proj/other.extra
    proj/IGNF
    proj/nad27
    proj/GL27
    proj/nad83
    proj/nad.lst
    proj/proj_def.dat
    proj/CH
    )

  ExternalProject_ByProducts(PROJ4_BLD ${INCLUDE_DIR}/proj
    projects.h
    proj_api.h
    geodesic.h
    )

  set(PROJ4_LIBRARIES proj CACHE STRING "Building bundled proj" FORCE)
  set(PROJ4_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/proj" CACHE STRING "Directory containing proj headers." FORCE)

  SetTargetFolder(PROJ4_BLD "Third Party Libraries")
  SetTargetFolder(proj "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_PROJ4_BUILD)

###############################################################################
#  GDAL -  translator library for raster and vector geospatial data formats
###############################################################################
set(gdal_DESCRIPTION "
Option for enabling and disabling compilation of the GDAL geographic
library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(gdal GDAL gdal gdal_DESCRIPTION REQUIRED_VARS "BRLCAD_ENABLE_GDAL;BRLCAD_LEVEL2" ALIASES ENABLE_GDAL)

if (${CMAKE_PROJECT_NAME}_GDAL_BUILD)

  ExternalProject_Add(GDAL_BLD
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gdal"
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DZLIB_ROOT=${CMAKE_BINARY_DIR} -DPNG_ROOT=${CMAKE_BINARY_DIR} -DPROJ4_ROOT=${CMAKE_BINARY_DIR}
    DEPENDS ${PROJ4_LIBRARIES}
    )
  ExternalProject_Target(gdal GDAL_BLD
    OUTPUT_FILE libgdal${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libgdal${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )

  set(GDAL_EXECUTABLES gdalinfo gdallocationinfo gdal_translate gdaltransform gdaldem gdalwarp gdalbuildvrt)
  foreach(GDALEXEC ${GDAL_EXECUTABLES})
    ExternalProject_Target(${GDALEXEC} STEPCODE_BLD
      OUTPUT_FILE ${GDALEXEC}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )
  endforeach(GDALEXEC ${GDAL_EXECUTABLES})

  ExternalProject_ByProducts(GDAL_BLD ${DATA_DIR}/gdal
    LICENSE.TXT
    GDALLogoBW.svg
    GDALLogoColor.svg
    GDALLogoGS.svg
    compdcs.csv
    coordinate_axis.csv
    cubewerx_extra.wkt
    datum_shift.csv
    default.rsc
    ecw_cs.wkt
    ellipsoid.csv
    epsg.wkt
    esri_StatePlane_extra.wkt
    esri_Wisconsin_extra.wkt
    esri_extra.wkt
    gcs.csv
    gcs.override.csv
    gdal_datum.csv
    gdalicon.png
    gdalvrt.xsd
    geoccs.csv
    gml_registry.xml
    gmlasconf.xml
    gmlasconf.xsd
    gt_datum.csv
    gt_ellips.csv
    header.dxf
    inspire_cp_BasicPropertyUnit.gfs
    inspire_cp_CadastralBoundary.gfs
    inspire_cp_CadastralParcel.gfs
    inspire_cp_CadastralZoning.gfs
    jpfgdgml_AdmArea.gfs
    jpfgdgml_AdmBdry.gfs
    jpfgdgml_AdmPt.gfs
    jpfgdgml_BldA.gfs
    jpfgdgml_BldL.gfs
    jpfgdgml_Cntr.gfs
    jpfgdgml_CommBdry.gfs
    jpfgdgml_CommPt.gfs
    jpfgdgml_Cstline.gfs
    jpfgdgml_ElevPt.gfs
    jpfgdgml_GCP.gfs
    jpfgdgml_LeveeEdge.gfs
    jpfgdgml_RailCL.gfs
    jpfgdgml_RdASL.gfs
    jpfgdgml_RdArea.gfs
    jpfgdgml_RdCompt.gfs
    jpfgdgml_RdEdg.gfs
    jpfgdgml_RdMgtBdry.gfs
    jpfgdgml_RdSgmtA.gfs
    jpfgdgml_RvrMgtBdry.gfs
    jpfgdgml_SBAPt.gfs
    jpfgdgml_SBArea.gfs
    jpfgdgml_SBBdry.gfs
    jpfgdgml_WA.gfs
    jpfgdgml_WL.gfs
    jpfgdgml_WStrA.gfs
    jpfgdgml_WStrL.gfs
    netcdf_config.xsd
    nitf_spec.xml
    nitf_spec.xsd
    ogrvrt.xsd
    osmconf.ini
    ozi_datum.csv
    ozi_ellips.csv
    pci_datum.txt
    pci_ellips.txt
    pcs.csv
    pcs.override.csv
    plscenesconf.json
    prime_meridian.csv
    projop_wparm.csv
    ruian_vf_ob_v1.gfs
    ruian_vf_st_uvoh_v1.gfs
    ruian_vf_st_v1.gfs
    ruian_vf_v1.gfs
    s57agencies.csv
    s57attributes.csv
    s57expectedinput.csv
    s57objectclasses.csv
    seed_2d.dgn
    seed_3d.dgn
    stateplane.csv
    trailer.dxf
    unit_of_measure.csv
    vdv452.xml
    vdv452.xsd
    vertcs.csv
    vertcs.override.csv
    )

  set(GDAL_LIBRARIES gdal CACHE STRING "Building bundled gdal" FORCE)
  set(GDAL_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/gdal" CACHE STRING "Directory containing GDAL headers." FORCE)

  SetTargetFolder(GDAL_BLD "Third Party Libraries")
  SetTargetFolder(gdal "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_GDAL_BUILD)

###############################################################################
#                                 Tcl/Tk
###############################################################################

set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl tcl_DESCRIPTION ALIASES ENABLE_TCL REQUIRED_VARS BRLCAD_LEVEL2)

if (NOT ${CMAKE_PROJECT_NAME}_TCL_BUILD)
  # Trying to manage all the component pieces of a Tcl install (mixing and
  # matching system and local) is way too complex for too little benefit.
  # For the most part this is an all or nothing arrangement - either the
  # system version is adequate, or we're on our own.  If we think we have a
  # system version, check to see if it has everything we need.  If not, we're
  # rolling our own.
  #
  # (TODO - Might make an exception for tkhtml since that's not very standard
  # in the broader ecosystem...
endif (NOT ${CMAKE_PROJECT_NAME}_TCL_BUILD)

# If we're building, build it all
if (${CMAKE_PROJECT_NAME}_TCL_BUILD)

  ############
  #    Tcl
  ############

  set(TCL_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TCL_BLD-prefix/src/TCL_BLD")
  set(TCL_MINOR_VERSION 6)
  set(ITCL_VERSION 4.2.0)

  if (NOT MSVC)

    set(TCL_PATCH_FILES "${TCL_SRC_DIR}/unix/configure" "${TCL_SRC_DIR}/macosx/configure" "${TCL_SRC_DIR}/unix/tcl.m4")

    ExternalProject_Add(TCL_BLD
      URL "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TCL_PATCH_FILES}
      CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TCL_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR}
      BUILD_COMMAND make -j${pcnt}
      INSTALL_COMMAND make install
      DEPENDS ${ZLIB_LIBRARIES}
      )
  else (NOT MSVC)

    ExternalProject_Add(TCL_BLD
      SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tcl"
      BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
      CONFIGURE_COMMAND ""
      BINARY_DIR ${TCL_SRC_DIR}/win
      BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR}
      INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR}
      )

  endif (NOT MSVC)

  ExternalProject_Target(tcl TCL_BLD
    OUTPUT_FILE libtcl8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
    STATIC_OUTPUT_FILE libtclstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
    RPATH
    )
  ExternalProject_Target(tclsh TCL_BLD
    OUTPUT_FILE tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
    RPATH EXEC
    )
  ExternalProject_ByProducts(TCL_BLD ${LIB_DIR} FIXPATH
    tclConfig.sh
    tclooConfig.sh
    )
  ExternalProject_ByProducts(TCL_BLD ${INCLUDE_DIR}
    tclDecls.h
    tcl.h
    tclOODecls.h
    tclOO.h
    tclPlatDecls.h
    tclTomMathDecls.h
    tclTomMath.h
    )

  set(TCL_LIBRARIES tcl CACHE STRING "Building bundled tcl" FORCE)
  set(TCL_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

  SetTargetFolder(TCL_BLD "Third Party Libraries")
  SetTargetFolder(tcl "Third Party Libraries")

  if(BRLCAD_ENABLE_TK)

    set(HAVE_TK 1 CACHE STRING "C level Tk flag" FORCE)
    CONFIG_H_APPEND(BRLCAD "#cmakedefine HAVE_TK\n")

    ############
    #    Tk
    ############


    set(TK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TK_BLD-prefix/src/TK_BLD")
    set(TK_MINOR_VERSION 6)

    if (NOT MSVC)

      set(TK_PATCH_FILES "${TK_SRC_DIR}/unix/configure" "${TK_SRC_DIR}/macosx/configure" "${TK_SRC_DIR}/unix/tcl.m4")

      ExternalProject_Add(TK_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TK_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TK_SRC_DIR}/unix/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --disable-xft --enable-64bit --enable-rpath
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TCL_BLD ${PNG_LIBRARIES}
	)
    else (NOT MSVC)

      ExternalProject_Add(TK_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${TK_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR}
	DEPENDS TCL_BLD ${PNG_LIBRARIES}
	)

    endif (NOT MSVC)

    ExternalProject_Target(tk TK_BLD
      OUTPUT_FILE libtk8.${TK_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      STATIC_OUTPUT_FILE libtkstub8.${TK_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      RPATH
      )
    ExternalProject_Target(wish TK_BLD
      OUTPUT_FILE wish8.${TK_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      RPATH EXEC
      )

    ExternalProject_ByProducts(TK_BLD ${LIB_DIR} FIXPATH
      tkConfig.sh
      )

    ExternalProject_ByProducts(TK_BLD ${INCLUDE_DIR}
      tkDecls.h
      tk.h
      tkPlatDecls.h
      )

    set(TK_LIBRARIES tk CACHE STRING "Building bundled tk" FORCE)
    set(TK_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing tcl headers." FORCE)

    SetTargetFolder(TK_BLD "Third Party Libraries")
    SetTargetFolder(tk "Third Party Libraries")

    ############
    #    Itk
    ############

    if (NOT MSVC)

      set(ITK_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/ITK_BLD-prefix/src/ITK_BLD")
      set(ITK_VERSION 4.1.0)

      set(ITK_PATCH_FILES "${ITK_SRC_DIR}/configure" "${ITK_SRC_DIR}/tclconfig/tcl.m4")

      ExternalProject_Add(ITK_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/itk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${ITK_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${ITK_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(ITK_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${ITK_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)


    ExternalProject_Target(itk ITK_BLD
      IMPORT_PREFIX itk${ITK_VERSION}
      OUTPUT_FILE itk${ITK_VERSION}/libitk${ITK_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      )

    ExternalProject_ByProducts(ITK_BLD ${INCLUDE_DIR}
      itk.h
      itkInt.h
      )

    ExternalProject_ByProducts(ITK_BLD ${LIB_DIR}
      itk${ITK_VERSION}/itk.tcl
      itk${ITK_VERSION}/pkgIndex.tcl
      itk${ITK_VERSION}/Widget.itk
      itk${ITK_VERSION}/Archetype.itk
      itk${ITK_VERSION}/Toplevel.itk
      itk${ITK_VERSION}/tclIndex
      )

    SetTargetFolder(ITK_BLD "Third Party Libraries")
    SetTargetFolder(itk "Third Party Libraries")

    ################
    #    IWidgets
    ################

    if (NOT MSVC)

      set(IWIDGETS_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/IWIDGETS_BLD-prefix/src/IWIDGETS_BLD")
      set(IWIDGETS_VERSION 4.1.1)

      set(IWIDGETS_PATCH_FILES "${IWIDGETS_SRC_DIR}/configure" "${IWIDGETS_SRC_DIR}/tclconfig/tcl.m4")

      ExternalProject_Add(IWIDGETS_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/iwidgets"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${IWIDGETS_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${IWIDGETS_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-itcl=${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION} --with-itk=${ITK_SRC_DIR}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(IWIDGETS_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${IWIDGETS_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)

    ExternalProject_ByProducts(IWIDGETS_BLD ${LIB_DIR}
      iwidgets${IWIDGETS_VERSION}/iwidgets.tcl
      )

    SetTargetFolder(IWIDGETS_BLD "Third Party Libraries")
    SetTargetFolder(iwidgets "Third Party Libraries")

    #############
    #   TkTable
    #############

    if (NOT MSVC)

      set(TKTABLE_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/TKTABLE_BLD-prefix/src/TKTABLE_BLD")
      set(TKTABLE_VERSION 2.10)

      set(TKTABLE_PATCH_FILES "${TKTABLE_SRC_DIR}/configure" "${TKTABLE_SRC_DIR}/tclconfig/tcl.m4")

      ExternalProject_Add(TKTABLE_BLD
	URL "${CMAKE_CURRENT_SOURCE_DIR}/tktable"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	PATCH_COMMAND rpath_replace "${CMAKE_BUILD_RPATH}" ${TKTABLE_PATCH_FILES}
	CONFIGURE_COMMAND CPPFLAGS=-I${CMAKE_BINARY_DIR}/${INCLUDE_DIR} LDFLAGS=-L${CMAKE_BINARY_DIR}/${LIB_DIR} ${TKTABLE_SRC_DIR}/configure --prefix=${CMAKE_BINARY_DIR} --with-tcl=${CMAKE_BINARY_DIR}/${LIB_DIR} --with-tk=${CMAKE_BINARY_DIR}/${LIB_DIR}
	BUILD_COMMAND make -j${pcnt}
	INSTALL_COMMAND make install
	DEPENDS TK_BLD TCL_BLD
	)
    else (NOT MSVC)

      ExternalProject_Add(TKTABLE_BLD
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tk"
	BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE} ${LOG_OPTS}
	CONFIGURE_COMMAND ""
	BINARY_DIR ${TKTABLE_SRC_DIR}/win
	BUILD_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	INSTALL_COMMAND ${VCVARS_BAT} && nmake -f makefile.vc install INSTALLDIR=${CMAKE_BINARY_DIR} TCLDIR=${TCL_SRC_DIR} TKDIR=${TK_SRC_DIR}
	DEPENDS TK_BLD TCL_BLD
	)

    endif (NOT MSVC)


    ExternalProject_Target(tktable TKTABLE_BLD
      IMPORT_PREFIX Tktable${TKTABLE_VERSION}
      OUTPUT_FILE Tktable${TKTABLE_VERSION}/libTktable${TKTABLE_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      )

    ExternalProject_ByProducts(TKTABLE_BLD ${LIB_DIR}
      Tktable2.10/README.txt
      Tktable2.10/html/tkTable.html
      Tktable2.10/pkgIndex.tcl
      Tktable2.10/tkTable.tcl
      Tktable2.10/tktable.py
      Tktable2.10/license.txt
      )

    SetTargetFolder(TKTABLE_BLD "Third Party Libraries")
    SetTargetFolder(tktable "Third Party Libraries")

    #############
    #   Tkhtml
    #############

    set(TKHTML_VERSION 3.0)

    cmake_set_rpath(SUFFIX "Tkhtml${TKHTML_VERSION}")
    message("CMAKE_BUILD_RPATH: ${CMAKE_BUILD_RPATH}")
    ExternalProject_Add(TKHTML_BLD
      SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tkhtml
      CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR}
      -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/${LIB_DIR} -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH}
      -DTCL_INCLUDE_DIRS=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTCL_INCLUDE_PATH=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTCL_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtcl8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      -DTCL_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtclstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      -DTCL_TCLSH=${CMAKE_BINARY_DIR}/${BIN_DIR}/tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTCL_TCLSH_EXECUTABLE=${CMAKE_BINARY_DIR}/${BIN_DIR}/tclsh8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTCL_TK_CONF_PREFIX=${CMAKE_BINARY_DIR}/${LIB_DIR}
      -DTCL_TK_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtk8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      -DTCL_TK_STUB_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtkstub8.${TCL_MINOR_VERSION}${CMAKE_STATIC_LIBRARY_SUFFIX}
      -DTCL_WISH_EXECUTABLE=${CMAKE_BINARY_DIR}/${BIN_DIR}/wish8.${TCL_MINOR_VERSION}${CMAKE_EXECUTABLE_SUFFIX}
      -DTK_INCLUDE_PATH=${CMAKE_BINARY_DIR}/${INCLUDE_DIR}
      -DTK_LIBRARY=${CMAKE_BINARY_DIR}/${LIB_DIR}/libtk8.${TCL_MINOR_VERSION}${CMAKE_SHARED_LIBRARY_SUFFIX}
      DEPENDS TCL_BLD TK_BLD
      )
    message("LIB_DIR: ${LIB_DIR}")
    ExternalProject_Target(tkhtml TKHTML_BLD
      SUBDIR Tkhtml${TKHTML_VERSION}
      OUTPUT_FILE libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}.${TKHTML_VERSION}
      SYMLINKS "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
      LINK_TARGET "libTkhtml${CMAKE_SHARED_LIBRARY_SUFFIX}"
      RPATH
      )
    cmake_set_rpath("")

    ExternalProject_ByProducts(TKHTML_BLD ${LIB_DIR}/Tkhtml${TKHTML_VERSION}
      pkgIndex.tcl
      )

    SetTargetFolder(TKHTML_BLD "Third Party Libraries")
    SetTargetFolder(tkhtml "Third Party Libraries")

  else(BRLCAD_ENABLE_TK)

    set(HAVE_TK 0 CACHE STRING "C level Tk flag" FORCE)

  endif(BRLCAD_ENABLE_TK)

  mark_as_advanced(HAVE_TK)

endif (${CMAKE_PROJECT_NAME}_TCL_BUILD)

if (0)

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
THIRD_PARTY_TCL_PACKAGE(Tktable tktable "${TCL_WISH_EXECUTABLE}" "tcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tktable_ALIASES tktable_DESCRIPTION)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/pkgIndex.tcl")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/tktable_cfg.h")
DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tktable2.10")
SetTargetFolder(Tktable "Third Party Libraries")
SetTargetFolder(tktable_header_gen "Compilation Utilities")

endif (0)

if (0)
# FreeType Libraries
set(freetype_DESCRIPTION "
Option for enabling and disabling compilation of the FreeType
libraries provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
# We don't want the Freetype distribution build targets
set(FREETYPE_NO_DIST 1)
THIRD_PARTY(freetype FREETYPE ft freetype_DESCRIPTION BRLCAD_LEVEL3 ALIASES ENABLE_FREETYPE REQUIRED_VARS "BRLCAD_ENABLE_OSG;BRLCAD_LEVEL3" FIND_NAME Freetype)
if(BRLCAD_FREETYPE_BUILD)
  set(FREETYPE_LIBRARY freetype CACHE STRING "libfreetype" FORCE)
  set(FREETYPE_INCLUDE_DIR_freetype2
    "${BRLCAD_SOURCE_DIR}/src/other/freetype/include"
    CACHE STRING "FreeType freetype2 include dir" FORCE
    )
  set(FREETYPE_INCLUDE_DIR_ft2build
    "${CMAKE_CURRENT_BINARY_DIR}/freetype/include"
    CACHE STRING "FreeType ft2build include dir" FORCE
    )
  set(FREETYPE_INCLUDE_DIRS
    "${FREETYPE_INCLUDE_DIR_freetype2};${FREETYPE_INCLUDE_DIR_ft2build}"
    CACHE STRING "Directories containing FreeType headers." FORCE)
  set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}" CACHE STRING "FreeType Libraries")
  SetTargetFolder(freetype "Third Party Libraries")

  # If we enable XFT support Tk when building our own freetype, XFT often pulls
  # in conflicting system libs anyway, which causes conflicts.  Make sure, if
  # we do have to build Tk, that we don't try enabling XFT.
  set(TK_DISABLE_XFT 1 CACHE STRING "Disable due to building local libpng" FORCE)
  mark_as_advanced(TK_DISABLE_XFT)
endif(BRLCAD_FREETYPE_BUILD)
if(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftconfig.h")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftoption.h")
endif(NOT "${CMAKE_CURRENT_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftconfig.h-new")
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/freetype/include/freetype/config/ftoption.h-new")
mark_as_advanced(FREETYPE_INCLUDE_DIRS)
mark_as_advanced(FREETYPE_INCLUDE_DIR_freetype2)
mark_as_advanced(FREETYPE_INCLUDE_DIR_ft2build)
mark_as_advanced(FREETYPE_LIBRARIES)


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

endif (0)

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

# Done with ExternalProject_Add
set(SUPPRESS_GENERATED_TAG FALSE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

