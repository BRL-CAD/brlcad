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
cmake_minimum_required(VERSION 3.0.2)

# set CMake project name
project(VDS)

include(CheckLibraryExists)
CHECK_LIBRARY_EXISTS(m cos "" HAVE_M_LIBRARY)
if(HAVE_M_LIBRARY)
  set(SYS_LIBS ${SYS_LIBS} m)
endif(HAVE_M_LIBRARY)

set(CORE_SRCS
  build.c
  cluster.c
  dynamic.c
  render.c
  util.c
  file.c
  )

set(STD_SRCS
  stdvds.c
  stdfold.c
  stdvis.c
  )

set(VDS_HDRS
  vds.h
  vdsprivate.h
  path.h
  vector.h
  stdvds.h
  )

## Include -DVDS_DEBUGPRINT for verbose operation.
## Note that assertions are suppressed when VDS_DEBUGPRINT is not defined
#add_definitions(-DVDS_DEBUGPRINT)

add_library(libvds SHARED ${CORE_SRCS})
set_target_properties(libvds PROPERTIES VERSION 1.0.1 SOVERSION 1)
if(SYS_LIBS)
  target_link_libraries(libvds ${SYS_LIBS})
endif(SYS_LIBS)

install(TARGETS libvds
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

add_library(stdvds SHARED ${STD_SRCS})
set_target_properties(stdvds PROPERTIES VERSION 1.0.1 SOVERSION 1)
if(SYS_LIBS)
  target_link_libraries(stdvds ${SYS_LIBS})
endif(SYS_LIBS)


install(TARGETS stdvds
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(MSVC)
  set_property(TARGET libvds APPEND PROPERTY COMPILE_DEFINITIONS "VDS_DLL_EXPORTS")
endif(MSVC)

if(BUILD_STATIC_LIBS)
  add_library(libvds-static STATIC ${CORE_SRCS})
  if(CMAKE_CL_64)
    set_target_properties(libvds-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  endif(CMAKE_CL_64)

  add_library(stdvds-static STATIC ${STD_SRCS})
  if(CMAKE_CL_64)
    set_target_properties(stdvds-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  endif(CMAKE_CL_64)
endif(BUILD_STATIC_LIBS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
