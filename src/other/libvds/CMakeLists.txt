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
IF(COMMAND CMAKE_POLICY)
   CMAKE_POLICY(SET CMP0003 NEW)
   CMAKE_POLICY(SET CMP0007 OLD)
   IF(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION} VERSION_GREATER 2.8.3)
      CMAKE_POLICY(SET CMP0017 OLD)
   ENDIF(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}.${CMAKE_PATCH_VERSION} VERSION_GREATER 2.8.3)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(VDS)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
endif(NOT APPLE)
SET(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#---------------------------------------------------------------------
# Output directories - this is where built library and executable
# files will be placed after building but prior to install.
IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
	IF(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/bin	CACHE INTERNAL "Single output directory for building all libraries.")
	ELSE(WIN32)
		SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY
			${VDS_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all	libraries.")
	ENDIF(WIN32)
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
	SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
	SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${VDS_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all	executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)


#---------------------------------------------------------------------
# Configure install locations.  Don't set these if they have already
# been set by some other means (like a higher level CMakeLists.txt file
# including this one.)

# The location in which to install executables.
IF(NOT BIN_DIR)
   SET(BIN_DIR bin)
ENDIF(NOT BIN_DIR)

# The location in which to install header files.
IF(NOT INCLUDE_DIR)
   SET(INCLUDE_DIR include)
ENDIF(NOT INCLUDE_DIR)

# The location in which to install libraries.
IF(NOT LIB_DIR)
   IF(NOT WIN32)
      SET(LIB_DIR lib)
   ELSE(NOT WIN32)
      SET(LIB_DIR bin)
   ENDIF(NOT WIN32)
ENDIF(NOT LIB_DIR)

# The location in which to install Manual pages
IF(NOT MAN_DIR)
   SET(MAN_DIR man)
ENDIF(NOT MAN_DIR)

#---------------------------------------------------------------------

SET(CORE_SRCS
	build.c
	dynamic.c
	render.c
	util.c
	file.c
	)

SET(STD_SRCS
	stdvds.c
	stdfold.c
	stdvis.c
	stdrender.c
	)

SET(VDS_HDRS
	vds.h
	vdsprivate.h
	path.h
	vector.h
	stdvds.h
	)


## Include -DVDS_DEBUGPRINT for verbose operation.
## Note that assertions are suppressed when VDS_DEBUGPRINT is not defined
#add_definitions(-DVDS_DEBUGPRINT)

add_library(vds SHARED ${CORE_SRCS})

add_library(stdvds SHARED ${STD_SRCS})

ADD_SUBDIRECTORY(polyview)
