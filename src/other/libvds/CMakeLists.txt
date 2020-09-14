##########################################################################
#   Copyright 2011  United States Government as represented by the U.S.
#   Army Research Laboratory.
#
#   Copyright 1999 The University of Virginia.
#   All Rights Reserved.
#
#   Permission to use, copy, modify and distribute this software and its
#   documentation without fee, and without a written agreement, is
#   hereby granted, provided that the above copyright notice and the
#   complete text of this comment appear in all copies, and provided that
#   the University of Virginia and the original authors are credited in
#   any publications arising from the use of this software.
#
#   IN NO EVENT SHALL THE UNIVERSITY OF VIRGINIA OR ANY AUTHOR OF THIS
#   SOFTWARE BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL,
#   INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
#   ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
#   IF THE UNIVERSITY OF VIRGINIA AND/OR THE AUTHOR OF THIS SOFTWARE
#   HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
#
##########################################################################

# This file contains the top level CMakeLists.txt logic for the
# BRL-CAD software package.
# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(VDS)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (NOT BIN_DIR)
  set(BIN_DIR bin)
endif (NOT BIN_DIR)

if (NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT INCLUDE_DIR)

if (NOT LIB_DIR)
  set(LIB_DIR lib)
endif (NOT LIB_DIR)

if (NOT DEFINED BUILD_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ON)
endif (NOT DEFINED BUILD_SHARED_LIBS)

if (NOT DEFINED BUILD_STATIC_LIBS)
  set(BUILD_STATIC_LIBS OFF)
endif (NOT DEFINED BUILD_STATIC_LIBS)

include(CheckLibraryExists)
check_library_exists(m cos "" HAVE_M_LIBRARY)
if (HAVE_M_LIBRARY)
  set(SYS_LIBS ${SYS_LIBS} m)
endif (HAVE_M_LIBRARY)

if (BUILD_SHARED_LIBS)
  add_library(vds SHARED vds.c)
  if (SYS_LIBS)
    target_link_libraries(vds ${SYS_LIBS})
  endif (SYS_LIBS)
  if (MSVC)
    set_property(TARGET vds APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_EXPORTS")
  endif (MSVC)
  install(TARGETS vds
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_SHARED_LIBS)

if (BUILD_STATIC_LIBS)
  add_library(vds-static STATIC vds.c)
  if (CMAKE_CL_64)
    set_target_properties(vds-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  endif (CMAKE_CL_64)
  if (MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(vds-static PROPERTIES PREFIX "lib")
  else (MSVC)
    set_target_properties(vds-static PROPERTIES OUTPUT_NAME "vds")
  endif (MSVC)
  install(TARGETS vds-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

if (NOT SKIP_INSTALL_HEADERS)
  install(FILES vds.h DESTINATION ${INCLUDE_DIR})
endif (NOT SKIP_INSTALL_HEADERS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
