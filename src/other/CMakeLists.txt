#-----------------------------------------------------------------------
# Bundled External Libraries and other 3rd party components.
#
# In general, source code here will meet one of the following criteria:
#
# 1.  A third party library where BRL-CAD has had to make local
#     modifications that are essential to our use of the library,
#     precluding the use of a system-supplied version, and the library
#     is not readily integrable into BRL-CAD's own compilation.
#
# 2.  A header only library or some other component used in the
#     compilation of multiple parts of BRL-CAD (i.e. it isn't practical
#     to locate it with the logic that uses it.)
#
# 3.  A third party component that is not widely available in the
#     the broader software ecosystem.  Since we know we will
#     always be building those components, there isn't any gain or
#     justification for the increased complexity of maintaining a
#     superbuild detect-and-build-if-needed approach to support it.
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

# If we're using a bundled zlib with a prefix defined, we need a definition
# flag.
# TODO - is there some target property we can use to pass this automatically?
# Hammering it at high levels is almost certainly not the right approach...
if (Z_PREFIX_STR)
  add_definitions(-DZ_PREFIX)
  add_definitions(-DZ_PREFIX_STR=${Z_PREFIX_STR})
endif (Z_PREFIX_STR)

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

# libutahrle Library - The directory to perform ADD_SUBDIRECTORY on
# and the include directory for utahrle are different, so override the
# macro's setting of UTAHRLE_INCLUDE_DIRS here.
if (BRLCAD_LEVEL3)
  add_subdirectory(libutahrle)
  set(UTAHRLE_LIBRARY utahrle CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_LIBRARIES utahrle CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
  set(UTAHRLE_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/libutahrle/include" CACHE STRING "directory with rle.h header" FORCE)
  mark_as_advanced(UTAHRLE_LIBRARY)
  mark_as_advanced(UTAHRLE_LIBRARIES)
  mark_as_advanced(UTAHRLE_INCLUDE_DIR)
  mark_as_advanced(UTAHRLE_INCLUDE_DIRS)
endif (BRLCAD_LEVEL3)
include("${CMAKE_CURRENT_SOURCE_DIR}/libutahrle.dist")
CMAKEFILES_IN_DIR(libutahrle_ignore_files libutahrle)


# OpenNURBS Library
add_subdirectory(openNURBS)
SetTargetFolder(openNURBS "Third Party Libraries")
SetTargetFolder(openNURBS-static "Third Party Libraries")
SetTargetFolder(openNURBS-obj "Third Party Libraries")
set(OPENNURBS_LIBRARY openNURBS CACHE STRING "Require opennurbs libraries" FORCE)
set(OPENNURBS_LIBRARIES openNURBS CACHE STRING "Require opennurbs libraries" FORCE)
set(OPENNURBS_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Require opennurbs header includes" FORCE)
set(OPENNURBS_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/openNURBS" CACHE STRING "Require opennurbs header includes" FORCE)
mark_as_advanced(OPENNURBS_LIBRARY)
mark_as_advanced(OPENNURBS_LIBRARIES)
mark_as_advanced(OPENNURBS_INCLUDE_DIR)
mark_as_advanced(OPENNURBS_INCLUDE_DIRS)
get_directory_property(openNURBS_headers_orig DIRECTORY openNURBS DEFINITION OPENNURBS_HEADERS)
set(openNURBS_headers)
foreach(onhfile ${openNURBS_headers_orig})
  set(openNURBS_headers ${openNURBS_headers} openNURBS/${onhfile})
endforeach(onhfile ${openNURBS_headers_orig})
BRLCAD_MANAGE_FILES(openNURBS_headers ${INCLUDE_DIR}/openNURBS)
include("${CMAKE_CURRENT_SOURCE_DIR}/openNURBS.dist")
CMAKEFILES_IN_DIR(openNURBS_ignore_files openNURBS)


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
set(POLY2TRI_LIBRARY "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(POLY2TRI_LIBRARIES "poly2tri" CACHE STRING "Poly2Tri library" FORCE)
set(POLY2TRI_INCLUDE_DIR "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
set(POLY2TRI_INCLUDE_DIRS "${BRLCAD_SOURCE_DIR}/src/other/poly2tri" CACHE STRING "Directory containing poly2tri header" FORCE)
SetTargetFolder(poly2tri "Third Party Libraries")
SetTargetFolder(poly2tri-static "Third Party Libraries")
mark_as_advanced(POLY2TRI_LIBRARY)
mark_as_advanced(POLY2TRI_LIBRARIES)
mark_as_advanced(POLY2TRI_INCLUDE_DIR)
mark_as_advanced(POLY2TRI_INCLUDE_DIRS)


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
set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen CACHE STRING "Bundled system include dirs" FORCE)


if (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK)

  # Tkhtml doesn't have an active upstream
  add_subdirectory(tkhtml)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tkhtml/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tkhtml3.0")
  SetTargetFolder(Tkhtml "Third Party Libraries")
  SetTargetFolder(tkhtml_n_gen "Compilation Utilities")

  # Tktable is a borderline case.  It does exist and is used in the Tcl/Tk
  # ecosystem, but it is has the same problem we have generally with "package
  # require" Tk package testing and there is very little upstream activity...
  add_subdirectory(tktable)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/pkgIndex.tcl")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/tktable/tktable_cfg.h")
  DISTCLEAN("${CMAKE_BINARY_DIR}/lib/Tktable2.10")
  SetTargetFolder(Tktable "Third Party Libraries")
  SetTargetFolder(tktable_header_gen "Compilation Utilities")

endif (BRLCAD_ENABLE_TCL AND BRLCAD_ENABLE_TK)

include("${CMAKE_CURRENT_SOURCE_DIR}/tkhtml.dist")
CMAKEFILES_IN_DIR(tkhtml_ignore_files tkhtml)

include("${CMAKE_CURRENT_SOURCE_DIR}/tktable.dist")
CMAKEFILES_IN_DIR(tktable_ignore_files tktable)

CMAKEFILES(
  README
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

