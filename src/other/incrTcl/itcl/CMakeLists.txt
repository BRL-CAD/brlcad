#                     C M A K E L I S T S . T X T
# ITCL
#
# Copyright (c) 2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# *******************************************************************
# ***                   ITCL CMakeLists.txt                       ***
# *******************************************************************

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

# set CMake project name
PROJECT(ITCL)

# Without Tcl libraries AND sources, we can't build Tk.  Use TCL_SRC_PREFIX
# and TCL_BIN_PREFIX to identify the location of the Tcl we are to use.
IF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX)
   SET(TCL_INCLUDE_DIRS ${TCL_SRC_PREFIX}/generic ${TCL_SRC_PREFIX}/libtommath)
   IF(WIN32)
      SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/win)
   ELSE(WIN32)
      SET(TCL_INCLUDE_DIRS ${TCL_INCLUDE_DIRS} ${TCL_SRC_PREFIX}/unix)
   ENDIF(WIN32)
   # Locate the TCL libraries.  We allow these to be overridden so that a parent
   # build can use a CMake target instead of an actual library path.
   IF(NOT TCL_LIBRARY)
      find_library(TCL_LIBRARY tcl PATHS ${TCL_BIN_PREFIX}/lib)
   ENDIF(NOT TCL_LIBRARY)
   IF(NOT TCL_STUB_LIBRARY)
      find_library(TCL_STUB_LIBRARY tclstub PATHS ${TCL_BIN_PREFIX}/lib)
   ENDIF(NOT TCL_STUB_LIBRARY)
   IF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY)
      MESSAGE(FATAL_ERROR "Could not find Tcl libraries in ${TCL_BIN_PREFIX} - please specify the parent directory for the libraries in the TCL_BIN_PREFIX variable (e.g. \${TCL_BIN_PREFIX}/lib)")
   ENDIF(NOT TCL_LIBRARY OR NOT TCL_STUB_LIBRARY)
ELSE(TCL_SRC_PREFIX and TCL_BIN_PREFIX)
   MESSAGE(FATAL_ERROR "Please specify the source and library locations of Tcl using the TCL_SRC_PREFIX and TCL_BIN_PREFIX variables.")
ENDIF(TCL_SRC_PREFIX AND TCL_BIN_PREFIX)
MARK_AS_ADVANCED(TCL_LIBRARY)
MARK_AS_ADVANCED(TCL_STUB_LIBRARY)

# build shared libs by default
set(BUILD_SHARED_LIBS ON)

# build static libs by default
set(BUILD_STATIC_LIBS ON)

# version numbers
SET(ITCL_VERSION_MAJOR 3)
SET(ITCL_VERSION_MINOR 4)
SET(ITCL_PATCH_LEVEL ${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR})

SET(ITCL_VERSION "${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR}")

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

#----------------------------------------------------------------------------
# The location in which to install Tcl.  Only do this if CMAKE_INSTALL_PREFIX 
# hasn't been set already, to try and allow parent builds (if any) some control.
IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   IF(NOT WIN32)
      IF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
         SET(CMAKE_INSTALL_PREFIX "/usr/local/tcldev-${TCL_VERSION}")
      ELSE ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
         SET(CMAKE_INSTALL_PREFIX "/usr")
      ENDIF ("${CMAKE_BUILD_TYPE}" MATCHES "Debug")
   ENDIF(NOT WIN32)
   SET(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE STRING "TCL install prefix" FORCE)
   SET(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT 0)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

#-----------------------------------------------------------------------------
# Output directories.

IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
   IF(WIN32)
      SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all libraries.")
   ELSE(WIN32)
      SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
   ENDIF(WIN32)
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
   SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
   SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

#----------------------------------------------------------------------------
# First, get some standard options out of the way - things that are constant
# between various platforms or pertain to specific OS definitions
add_definitions(-DPACKAGE_NAME="itcl")
add_definitions(-DPACKAGE_TARNAME="itcl")
add_definitions(-DPACKAGE_VERSION="${ITCL_PATCH_LEVEL}")
add_definitions(-DPACKAGE_BUGREPORT="")
add_definitions(-DSTDC_HEADERS=1)

# We're building Itcl
add_definitions(-DBUILD_itcl)

#--------------------------------------------------------------------
# We need to enable the threading macros found in tcl.h and tclInt.h.
# The use of the threading features is determined by the core the
# extension is loaded into, but we need to compile with these macros
# turned on.
#--------------------------------------------------------------------
add_definitions(-DTCL_THREADS=1)

MACRO(TCL_CHECK_INCLUDE_FILE filename var)
   CHECK_INCLUDE_FILE(${filename} ${var})
   IF(${var})
      add_definitions(-D${var}=1)
   ENDIF(${var})
ENDMACRO(TCL_CHECK_INCLUDE_FILE)

TCL_CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
TCL_CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
TCL_CHECK_INCLUDE_FILE(sys/fstatfs.h HAVE_SYS_FSTATFS_H)
IF(NOT HAVE_SYS_FSTATFS_H)
   add_definitions(-DNO_FSTATFS=1)
ENDIF(NOT HAVE_SYS_FSTATFS_H)
TCL_CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
TCL_CHECK_INCLUDE_FILE(strings.h HAVE_STRINGS_H)
TCL_CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
TCL_CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
TCL_CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)


SET(ITCL_GENERIC_SRCS
	generic/itclStubInit.c
	generic/itclStubLib.c
	generic/itcl_bicmds.c
	generic/itcl_class.c
	generic/itcl_cmds.c
	generic/itcl_ensemble.c
	generic/itcl_linkage.c
	generic/itcl_methods.c
	generic/itcl_migrate.c
	generic/itcl_objects.c
	generic/itcl_parse.c
	generic/itcl_util.c
)

SET(ITCL_WIN_SRCS
	win/dllEntryPoint.c
)

SET(ITCL_STUB_SRCS
	generic/itclStubLib.c
)


SET(ITCL_INCLUDE_DIRS ${ITCL_SOURCE_DIR}/generic ${ITCL_BINARY_DIR}/include)
MARK_AS_ADVANCED(ITCL_INCLUDE_DIRS)

include_directories(
	${TCL_INCLUDE_DIRS}
	${ITCL_INCLUDE_DIRS}
	${CMAKE_BINARY_DIR}/include
	${CMAKE_SOURCE_DIR}/include
)

IF(WIN32)
   add_library(itcl ${ITCL_GENERIC_SRCS} ${ITCL_WIN_SRCS})
ELSE(WIN32)
   add_library(itcl ${ITCL_GENERIC_SRCS})
ENDIF(WIN32)
target_link_libraries(itcl ${M_LIBRARY} ${TCL_STUB_LIBRARIES} )
SET_TARGET_PROPERTIES(itcl PROPERTIES PREFIX "lib")
SET_TARGET_PROPERTIES(itcl PROPERTIES VERSION ${ITCL_VERSION} SOVERSION ${ITCL_VERSION_MAJOR})
SET_PROPERTY(TARGET itcl APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)
install(TARGETS itcl DESTINATION ${LIB_DIR})

add_library(itclstub STATIC ${ITCL_STUB_SRCS})
install(TARGETS itclstub DESTINATION ${LIB_DIR})
SET_PROPERTY(TARGET itclstub APPEND PROPERTY COMPILE_DEFINITIONS USE_TCL_STUBS)

GET_TARGET_PROPERTY(ITCL_LIBLOCATION itcl LOCATION_${CMAKE_BUILD_TYPE})
GET_FILENAME_COMPONENT(ITCL_LIBNAME ${ITCL_LIBLOCATION} NAME)
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded Itcl ${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR} [list load [file join $dir .. .. ${LIB_DIR} ${ITCL_LIBNAME}] Itcl]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/itcl${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR})

FILE(WRITE ${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR}/pkgIndex.tcl "package ifneeded Itcl ${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR} [list load [file join $dir ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${ITCL_LIBNAME}] Itcl]")

install(FILES library/itcl.tcl DESTINATION lib/itcl${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/library/itcl.tcl ${CMAKE_BINARY_DIR}/lib/itcl${ITCL_VERSION_MAJOR}.${ITCL_VERSION_MINOR}/itcl.tcl COPYONLY)

SET(ITCL_HDRS
	generic/itcl.h
	generic/itclDecls.h
	generic/itclInt.h
	generic/itclIntDecls.h
	)
install(FILES ${ITCL_HDRS} DESTINATION include)
