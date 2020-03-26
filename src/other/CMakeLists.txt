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
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    -DREGEX_PREFIX_STR=libregex_
    )

  ExternalProject_Target(regex REGEX_BLD
    OUTPUT_FILE libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1.0.4
    STATIC_OUTPUT_FILE libregex${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${LIB_DIR}/libregex${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/libregex${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
    LINK_TARGET "libregex${CMAKE_SHARED_LIBRARY_SUFFIX}"
    STATIC_LINK_TARGET "libregex${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  ExternalProject_ByProducts(REGEX_BLD ${INCLUDE_DIR}
    regex.h
    )

  set(REGEX_LIBRARIES regex CACHE STRING "Building bundled libregex" FORCE)
  set(REGEX_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

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
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
               -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
	       -DZ_PREFIX_STR=brl_
    )
  if (NOT MSVC)
    ExternalProject_Target(zlib ZLIB_BLD
      OUTPUT_FILE libz${CMAKE_SHARED_LIBRARY_SUFFIX}.${ZLIB_VERSION}
      STATIC_OUTPUT_FILE libz${CMAKE_STATIC_LIBRARY_SUFFIX}
      SYMLINKS "${LIB_DIR}/libz${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/libz${CMAKE_SHARED_LIBRARY_SUFFIX}.1"
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
  set(ZLIB_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing zlib headers." FORCE)

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
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE}
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
  set(LZ4_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing lz4 headers." FORCE)

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
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE}
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
  set(NETPBM_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}/netpbm" CACHE STRING "Directory containing netpbm headers." FORCE)

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
    SYMLINKS "${LIB_DIR}/libpng${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/libpng${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/libpng${PNG_VERSION_MAJOR}${CMAKE_SHARED_LIBRARY_SUFFIX}.${PNG_VERSION_MAJOR};${LIB_DIR}/libpng${CMAKE_STATIC_LIBRARY_SUFFIX}"
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
  set(PNG_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing libpng headers." FORCE)

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
    BUILD_ALWAYS ${EXTERNAL_BUILD_UPDATE}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR} -DLIB_DIR=${LIB_DIR} -DBIN_DIR=${BIN_DIR}
    -DCMAKE_INSTALL_RPATH=${CMAKE_BUILD_RPATH} -DBUILD_STATIC_LIBS=${BUILD_STATIC_LIBS}
    )

  ExternalProject_Target(utahrle UTAHRLE_BLD
    OUTPUT_FILE libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_VERSION}
    STATIC_OUTPUT_FILE libutahrle${CMAKE_STATIC_LIBRARY_SUFFIX}
    SYMLINKS "${LIB_DIR}/libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX};${LIB_DIR}/libutahrle${CMAKE_SHARED_LIBRARY_SUFFIX}.${UTAHRLE_MAJOR_VERSION}"
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

  set(UTAHRLE_LIBRARIES utahrle CACHE STRING "Building bundled libregex" FORCE)
  set(UTAHRLE_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/${INCLUDE_DIR}" CACHE STRING "Directory containing regex headers." FORCE)

  SetTargetFolder(UTAHRLE_BLD "Third Party Libraries")
  SetTargetFolder(utahrle "Third Party Libraries")

endif (${CMAKE_PROJECT_NAME}_UTAHRLE_BUILD)


if (0)

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
  set(OPENNURBS_INCLUDE_DIRS "${OPENNURBS_INCLUDE_DIR};${ZLIB_INCLUDE_DIR}" CACHE STRING "Require opennurbs header includes" FORCE)
  set(OPENNURBS_LIBRARIES "${OPENNURBS_LIBRARY};${ZLIB_LIBRARY}" CACHE STRING "Require opennurbs libraries" FORCE)
  mark_as_advanced(OPENNURBS_INCLUDE_DIRS)
  mark_as_advanced(OPENNURBS_LIBRARIES)
  get_directory_property(openNURBS_headers_orig DIRECTORY openNURBS DEFINITION OPENNURBS_HEADERS)
  set(openNURBS_headers)
  foreach(onhfile ${openNURBS_headers_orig})
    set(openNURBS_headers ${openNURBS_headers} openNURBS/${onhfile})
  endforeach(onhfile ${openNURBS_headers_orig})
  BRLCAD_MANAGE_FILES(openNURBS_headers ${INCLUDE_DIR}/openNURBS)
endif(BRLCAD_OPENNURBS_BUILD)

# Screened Poisson Reconstruction Library
set(spsr_DESCRIPTION "
Option for enabling and disabling compilation of the Screened Poisson
Surface Reconstruction library provided with BRL-CAD's source code.
Default is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option
and testing first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.
")
THIRD_PARTY(libspsr SPSR libspsr spsr_DESCRIPTION ALIASES
  ENABLE_SPSR FLAGS NOSYS)
if(BRLCAD_SPSR_BUILD)
  SetTargetFolder(PoissonRecon "Third Party Executables")
  SetTargetFolder(SurfaceTrimmer "Third Party Executables")
  SetTargetFolder(libSPSR "Third Party Libraries")
  SetTargetFolder(libSPSR-static "Third Party Libraries")
endif(BRLCAD_SPSR_BUILD)

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

function(SC_MANAGE_HDRS src_dir src_var target_dir)
  get_directory_property(sc_headers_orig DIRECTORY ${src_dir} DEFINITION ${src_var})
  set(sc_headers)
  foreach(scfile ${sc_headers_orig})
    get_filename_component(scf ${scfile} NAME)
    set(sc_headers ${sc_headers} ${src_dir}/${scf})
  endforeach(scfile ${sc_headers_orig})
  BRLCAD_MANAGE_FILES("${sc_headers}" "${target_dir}" FOLDER stepcode)
endfunction(SC_MANAGE_HDRS)

function(SC_MANAGE_HDRS2 src_dir src_var target_dir)
  get_directory_property(sc_headers_orig DIRECTORY ${src_dir} DEFINITION ${src_var})
  set(sc_headers)
  foreach(scfile ${sc_headers_orig})
    set(sc_headers ${sc_headers} ${src_dir}/${scfile})
  endforeach(scfile ${sc_headers_orig})
  BRLCAD_MANAGE_FILES("${sc_headers}" "${target_dir}" FOLDER stepcode)
endfunction(SC_MANAGE_HDRS2)

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
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_cf.h.in")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/include/sc_version_string.h")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/stepcode/src/express/ExpParser_expparse/expparse.y")

  # stepcode has a lot of installed headers...
  SC_MANAGE_HDRS(stepcode/src/base SC_BASE_HDRS ${INCLUDE_DIR}/stepcode/base)
  SC_MANAGE_HDRS(stepcode/src/cldai SC_CLDAI_HDRS ${INCLUDE_DIR}/stepcode/cldai)
  SC_MANAGE_HDRS(stepcode/src/cleditor SC_CLEDITOR_HDRS ${INCLUDE_DIR}/stepcode/cleditor)
  SC_MANAGE_HDRS(stepcode/src/clstepcore SC_CLSTEPCORE_HDRS ${INCLUDE_DIR}/stepcode/clstepcore)
  SC_MANAGE_HDRS(stepcode/src/clutils SC_CLUTILS_HDRS ${INCLUDE_DIR}/stepcode/clutils)
  SC_MANAGE_HDRS2(stepcode/include express_HDRS ${INCLUDE_DIR}/stepcode/express)
  SC_MANAGE_HDRS2(stepcode/include exppp_HDRS ${INCLUDE_DIR}/stepcode/exppp)
  BRLCAD_MANAGE_FILES(stepcode/include/ordered_attrs.h ${INCLUDE_DIR}/stepcode)
  BRLCAD_MANAGE_FILES(stepcode/include/sc_export.h ${INCLUDE_DIR}/stepcode)
  BRLCAD_MANAGE_FILES(stepcode/include/sc_stdbool.h ${INCLUDE_DIR}/stepcode)
  get_directory_property(scbdir DIRECTORY stepcode/include DEFINITION SC_BINARY_DIR)
  get_directory_property(sciidir DIRECTORY stepcode/include DEFINITION INCLUDE_INSTALL_DIR)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sc_gen_hdrs.sentinel"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${scbdir}/${sciidir}/sc_cf.h ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/stepcode/sc_cf.h
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${scbdir}/${sciidir}/sc_version_string.h ${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/stepcode/sc_version_string.h
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/sc_gen_hdrs.sentinel"
    DEPENDS ${scbdir}/${sciidir}/sc_cf.h
    )
  add_custom_target(sc_gen_hdrs_cp ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/sc_gen_hdrs.sentinel")
  BRLCAD_ADD_DIR_LIST_ENTRY(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" sc_gen_hdrs_cp)
  set_target_properties(sc_gen_hdrs_cp PROPERTIES FOLDER "BRL-CAD File Setup/stepcode")
  add_dependencies(express sc_gen_hdrs_cp)
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/stepcode/sc_cf.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/${INCLUDE_DIR}/stepcode/sc_version_string.h")

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

# VDSlib - A View-Dependent Simplification and Rendering Library For
# the moment, this is marked NOSYS - it's possible that some Debian
# systems would have 0.9 of vdslib installed, but it's unmaintained
# and we're likely to be making changes.  If our own copy of VDSlib
# ever spins back off into its own project, revisit the NOSYS
set(libvds_DESCRIPTION "
Option for enabling and disabling compilation of the libvds triangle
simplification library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libvds VDS libvds libvds_DESCRIPTION ALIASES ENABLE_VDS FLAGS NOSYS)
set(LIBVDS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libvds" CACHE STRING "Directory containing libvds headers." FORCE)
mark_as_advanced(LIBVDS_INCLUDE_DIR)
SetTargetFolder(libvds "Third Party Libraries")
SetTargetFolder(libvds-static "Third Party Libraries")
SetTargetFolder(stdvds "Third Party Libraries")
SetTargetFolder(stdvds-static "Third Party Libraries")


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

# gdiam
set(libgdiam_DESCRIPTION "
Option for enabling and disabling compilation of the libgdiam approximate
tight bounding box library provided with BRL-CAD's source code.  Default
is AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and
testing first for a system version if BRLCAD_BUNDLED_LIBS is also
AUTO.
")
THIRD_PARTY(libgdiam GDIAM libgdiam libgdiam_DESCRIPTION ALIASES ENABLE_GDIAM FLAGS NOSYS)
set(LIBGDIAM_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libgdiam" CACHE STRING "Directory containing libgdiam headers." FORCE)
mark_as_advanced(LIBGDIAM_INCLUDE_DIR)
SetTargetFolder(libgdiam "Third Party Libraries")
SetTargetFolder(libgdiam-static "Third Party Libraries")

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
add_subdirectory(poly2tri)
include("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri.dist")
CMAKEFILES_IN_DIR(poly2tri_ignore_files poly2tri)
DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/poly2tri/Makefile")
set(P2T_LIBRARY "p2t" CACHE STRING "Poly2Tri library" FORCE)
set(P2T_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
get_directory_property(poly2tri_headers DIRECTORY poly2tri DEFINITION LIBP2T_PUBLIC_HDRS)
foreach(phfile ${poly2tri_headers})
  get_filename_component(phfile_dir ${phfile} DIRECTORY)
  BRLCAD_MANAGE_FILES(poly2tri/${phfile} ${INCLUDE_DIR}/${phfile_dir})
endforeach(phfile ${poly2tri_headers})
SetTargetFolder(p2t "Third Party Libraries")
SetTargetFolder(p2t-static "Third Party Libraries")
mark_as_advanced(P2T_LIBRARY)
mark_as_advanced(P2T_INCLUDE_DIR)
mark_as_advanced(P2T_SHARED)
mark_as_advanced(P2T_STATIC)
mark_as_advanced(P2T_TESTS)

# RPLY I/O library
add_subdirectory(rply)
include("${CMAKE_CURRENT_SOURCE_DIR}/rply.dist")
CMAKEFILES_IN_DIR(rply_ignore_files rply)
DISTCLEAN("${CMAKE_CURRENT_SOURCE_DIR}/rply/Makefile")
set(RPLY_LIBRARIES "rply" CACHE STRING "RPLY library" FORCE)
set(RPLY_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/rply" CACHE STRING "Directory containing rply header" FORCE)
SetTargetFolder(rply "Third Party Libraries")
SetTargetFolder(rply-static "Third Party Libraries")
mark_as_advanced(RPLY_LIBRARIES)
mark_as_advanced(RPLY_INCLUDE_DIRS)

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
  set(PROJ4_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/proj-4;${CMAKE_CURRENT_SOURCE_DIR}/proj-4/src" CACHE STRING "proj-4 includes" FORCE)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/proj-4/proj_config.h")
  SetTargetFolder(proj "Third Party Libraries/GDAL")
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
endif(BRLCAD_GDAL_BUILD)
mark_as_advanced(GDAL_CONFIG)
mark_as_advanced(GDAL_INCLUDE_DIR)
mark_as_advanced(GDAL_LIBRARY)



# Tcl/Tk presents a number of complexities for BRL-CAD and CMake.
# BRL-CAD requires far more knowledge of the details of a Tcl/Tk
# installation than most programs, which unfortunately means the
# standard FindTCL.cmake is not sufficient. We have our own FindTCL
# routine which handles Tcl/Tk and its stubs, so trigger this using
# the THIRD_PARTY macro for TCL.

include("${BRLCAD_SOURCE_DIR}/misc/CMake/TCL_PKGINDEX.cmake")

# Set Tcl/Tk requirements for BRL-CAD so FindTCL.cmake knows what to reject
set(TCL_PATH_NOMATCH_PATTERNS "/usr/brlcad;brlcad-install;${CMAKE_INSTALL_PREFIX}" CACHE STRING "Paths to avoid when looking for tcl/tk" FORCE)
set(TCL_MIN_VERSION "8.5" CACHE STRING "Minimum acceptable Tcl/Tk version" FORCE)

if(BRLCAD_ENABLE_TK)
  OPTION(TCL_REQUIRE_TK "Look for Tk installation, not just Tcl." ON)
else(BRLCAD_ENABLE_TK)
  OPTION(TCL_REQUIRE_TK "Look for Tk installation, not just Tcl." OFF)
endif(BRLCAD_ENABLE_TK)

# Turn off the man pages
set(TCL_DISABLE_MANPAGES ON CACHE BOOL "disable Tcl man page generation" FORCE)
mark_as_advanced(TCL_DISABLE_MANPAGES)
set(TK_DISABLE_MANPAGES ON CACHE BOOL "disable Tk man page generation" FORCE)
mark_as_advanced(TK_DISABLE_MANPAGES)

set(tcl_DESCRIPTION "
Option for enabling and disabling compilation of the Tcl library
provided with BRL-CAD's source code.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system
version if BRLCAD_BUNDLED_LIBS is also AUTO.
")

THIRD_PARTY(tcl TCL tcl tcl_DESCRIPTION ALIASES ENABLE_TCL FIND_NAME BRLCADTCL REQUIRED_VARS BRLCAD_LEVEL2)

if(BRLCAD_TCL_BUILD)

  SetTargetFolder(tclsh "Third Party Executables")
  SetTargetFolder(tcl "Third Party Libraries")
  SetTargetFolder(tclstub "Third Party Libraries")
  # Set the variables ourselves since FindTCL.cmake doesn't know about our build
  set(TCL_SRC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/tcl")
  set(TCL_BIN_PREFIX "${CMAKE_BINARY_DIR}/${LIB_DIR}")
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
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/Makefile")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/man1")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/man3")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/doc/mann")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tcl/library/Makefile")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/tcl8")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/tcl8.5")

  # manage installed headers
  get_directory_property(tcl_headers_orig DIRECTORY tcl DEFINITION TCL_HDRS)
  set(tcl_headers)
  foreach(tclh ${tcl_headers_orig})
    get_filename_component(tclhf ${tclh} NAME)
    set(tcl_headers ${tcl_headers} tcl/generic/${tclhf})
  endforeach(tclh ${tcl_headers_orig})
  BRLCAD_MANAGE_FILES(tcl_headers ${INCLUDE_DIR})

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
    file(READ "${CMAKE_BINARY_DIR}/CMakeTmp/tcl_inittcl" tcl_inittcl_raw)
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
if(BRLCAD_ENABLE_AQUA)
  set(TK_ENABLE_AQUA ON CACHE STRING "BRL-CAD setting to enable AQUA" FORCE)
endif(BRLCAD_ENABLE_AQUA)
THIRD_PARTY_TCL_PACKAGE(Tk tk "${TCL_TCLSH_EXECUTABLE}" "tcl" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL2" "tk" tk_ALIASES tk_DESCRIPTION)
if(BRLCAD_TK_BUILD)
  SetTargetFolder(wish "Third Party Executables")
  SetTargetFolder(tk "Third Party Libraries") 
  SetTargetFolder(tkstub "Third Party Libraries") 
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
    set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/tk/win" "${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib" CACHE STRING "Tcl include paths" FORCE)
  endif(WIN32)
  set(TCL_WISH_EXECUTABLE wish CACHE STRING "Tk shell" FORCE)
  set(TK_WISH wish CACHE STRING "Tk shell" FORCE)
  if(WIN32)
    set(TK_SYSTEM_GRAPHICS "win32" CACHE STRING "Tk system graphics" FORCE)
  elseif(APPLE)
    if(OPENGL_USE_AQUA)
      set(TK_SYSTEM_GRAPHICS "aqua" CACHE STRING "Tk system graphics" FORCE)
    ELSE(OPENGL_USE_AQUA)
      set(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
    endif(OPENGL_USE_AQUA)
  elseif(UNIX)
    set(TK_SYSTEM_GRAPHICS "x11" CACHE STRING "Tk system graphics" FORCE)
  endif(WIN32)
  # If we're building our own libpng, that tends to conflict with fontconfig,
  # which is needed for Tk's freefype enabled build.  So when we do build
  # libpng we need to disable the Tk freetype support to allow reliable
  # building, at least until we can provide our own fontconfig as well as
  # our own freetype...
  CONFIG_H_APPEND(BRLCAD "#define HAVE_TK_H 1\n")
  mark_as_advanced(TK-ENABLE_FREETYPE)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/Makefile")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man1")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/man3")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/doc/mann")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/library/Makefile")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tk/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/tk8.5")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/tk8.5.19")

  # manage installed headers
  get_directory_property(tk_headers_orig DIRECTORY tk DEFINITION TK_HDRS)
  set(tk_headers)
  foreach(tkh ${tk_headers_orig})
    get_filename_component(tkhf ${tkh} NAME)
    set(tk_headers ${tk_headers} tk/generic/${tkhf})
  endforeach(tkh ${tk_headers_orig})
  BRLCAD_MANAGE_FILES(tk_headers ${INCLUDE_DIR})

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

set(TCL_SRC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/tcl")
set(TK_SRC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/tk")
set(ITCL_SRC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl")
if(WIN32)
  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic" "${CMAKE_CURRENT_SOURCE_DIR}/tcl/win" "${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath")
  set(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/tk/generic" "${CMAKE_CURRENT_SOURCE_DIR}/tk/xlib" "${CMAKE_CURRENT_SOURCE_DIR}/tk/win" "${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps")
ELSE(WIN32)
  set(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} "${CMAKE_CURRENT_SOURCE_DIR}/tcl/generic" "${CMAKE_CURRENT_SOURCE_DIR}/tcl/unix" "${CMAKE_CURRENT_SOURCE_DIR}/tcl/libtommath")
  set(TK_INCLUDE_PATH ${TK_INCLUDE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/tk/generic" "${CMAKE_CURRENT_SOURCE_DIR}/tk/unix" "${CMAKE_CURRENT_SOURCE_DIR}/tk/bitmaps")
endif(WIN32)
set(ITCL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/incrTcl/itcl/generic")
if(BRLCAD_TCL_BUILD)
  set(TCL_BIN_PREFIX "${CMAKE_BINARY_DIR}/${LIB_DIR}")
ELSE(BRLCAD_TCL_BUILD)
  if (NOT "${TCL_LIBRARY}" STREQUAL "")
    get_filename_component(TCL_BIN_PREFIX ${TCL_LIBRARY} PATH)
  endif (NOT "${TCL_LIBRARY}" STREQUAL "")
endif(BRLCAD_TCL_BUILD)
if(BRLCAD_ENABLE_TK)
  if(BRLCAD_TK_BUILD)
    set(TK_BIN_PREFIX "${CMAKE_BINARY_DIR}/${LIB_DIR}")
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

# Ignore the toplevel incrTcl files that aren't used by either itcl
# or itk
include(${CMAKE_CURRENT_SOURCE_DIR}/incrTcl.dist)
CMAKEFILES_IN_DIR(incrTcl_ignore_files incrTcl)

# First up, look for incrTcl's Itcl.
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
  set(ITCL_BIN_PREFIX "${CMAKE_BINARY_DIR}/${LIB_DIR}")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/incrTcl/itcl/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/itcl3.4")
  SetTargetFolder(itcl "Third Party Libraries")
  SetTargetFolder(itclstub "Third Party Libraries")

  # manage installed headers
  get_directory_property(itcl_headers_orig DIRECTORY incrTcl/itcl DEFINITION ITCL_HDRS)
  set(itcl_headers)
  foreach(itclh ${itcl_headers_orig})
    get_filename_component(itclhf ${itclh} NAME)
    set(itcl_headers ${itcl_headers} incrTcl/itcl/generic/${itclhf})
  endforeach(itclh ${itcl_headers_orig})
  BRLCAD_MANAGE_FILES(itcl_headers ${INCLUDE_DIR})

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
  SetTargetFolder(itk "Third Party Libraries")
  SetTargetFolder(itkstub "Third Party Libraries")
  # does not handle C library settings.
  set(ITK_LIBRARY itk CACHE STRING "ITK_LIBRARY" FORCE)
  set(ITK_VERSION "3.3" CACHE STRING "ITK_VERSION" FORCE)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/incrTcl/itk/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/itk3.4")

  # manage installed headers
  get_directory_property(itk_headers_orig DIRECTORY incrTcl/itk DEFINITION ITK_HDRS)
  set(itk_headers)
  foreach(itkh ${itk_headers_orig})
    get_filename_component(itkhf ${itkh} NAME)
    set(itk_headers ${itk_headers} incrTcl/itk/generic/${itkhf})
  endforeach(itkh ${itk_headers_orig})
  BRLCAD_MANAGE_FILES(itk_headers ${INCLUDE_DIR})

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
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/iwidgets/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Iwidgets4.0.1")
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
THIRD_PARTY_TCL_PACKAGE(Tkhtml tkhtml "${TCL_WISH_EXECUTABLE}" "tcl;tk" "BRLCAD_ENABLE_TK;BRLCAD_LEVEL3" "" tkhtml_ALIASES tkhtml_DESCRIPTION)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tkhtml/pkgIndex.tcl")
DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tkhtml3.0")
SetTargetFolder(Tkhtml "Third Party Libraries")
SetTargetFolder(tkhtml_n_gen "Compilation Utilities")

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
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tkpng/pkgIndex.tcl")
DISTCLEAN("${CMAKE_BINARY_DIR}/lib/tkpng0.8")
SetTargetFolder(tkpng "Third Party Libraries")

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
#DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/pkgIndex.tcl")
#DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktreectrl/tktreectrl_cfg.h")
#DISTCLEAN("${CMAKE_BINARY_DIR}/lib/TkTreeCtrl2.4")

# GCT is a collection of algorithms for geometry processing and conversion
#add_subdirectory(gct)
include("${CMAKE_CURRENT_SOURCE_DIR}/gct.dist")
CMAKEFILES_IN_DIR(gct_ignore_files gct)

# linenoise is used by applications directly
include(${CMAKE_CURRENT_SOURCE_DIR}/linenoise.dist)
CMAKEFILES_IN_DIR(linenoise_ignore_files linenoise)

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
endif (0)


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

