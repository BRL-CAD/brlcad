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
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.3)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  CMAKE_POLICY(SET CMP0007 OLD)
  if("${CMAKE_VERSION}" VERSION_GREATER 2.8.3)
    CMAKE_POLICY(SET CMP0017 OLD)
  endif("${CMAKE_VERSION}" VERSION_GREATER 2.8.3)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
    CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
endif(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(VDS)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
endif(NOT APPLE)
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable
# files will be placed after building but prior to install.
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  if(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/bin	CACHE INTERNAL "Single output directory for building all libraries.")
  else(WIN32)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
      ${VDS_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all	libraries.")
  endif(WIN32)
endif(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
endif(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all	executables.")
endif(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)


#---------------------------------------------------------------------
# Configure install locations.  Don't set these if they have already
# been set by some other means (like a higher level CMakeLists.txt file
# including this one.)

# The location in which to install executables.
if(NOT BIN_DIR)
  set(BIN_DIR bin)
endif(NOT BIN_DIR)

# The location in which to install header files.
if(NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif(NOT INCLUDE_DIR)

# The location in which to install libraries.
if(NOT LIB_DIR)
  if(NOT WIN32)
    set(LIB_DIR lib)
  else(NOT WIN32)
    set(LIB_DIR bin)
  endif(NOT WIN32)
endif(NOT LIB_DIR)

# The location in which to install Manual pages
if(NOT MAN_DIR)
  set(MAN_DIR man)
endif(NOT MAN_DIR)

#---------------------------------------------------------------------
include(CheckLibraryExists)
CHECK_LIBRARY_EXISTS(m cos "" HAVE_M_LIBRARY)
if(HAVE_M_LIBRARY)
  set(SYS_LIBS ${SYS_LIBS} m)
endif(HAVE_M_LIBRARY)

#---------------------------------------------------------------------

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
