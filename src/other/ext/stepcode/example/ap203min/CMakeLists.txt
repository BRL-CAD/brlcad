#
# CMakeLists.txt for AP203 Minimum
#
#  This file is released to the public domain.  Any part of this file may be
# freely copied in part or in full for any purpose.  No acknowledgment is required
# for the use of this file.
#
project(AP203Minimum)
cmake_minimum_required(VERSION 3.6.3)

# Set STEPCODE_ROOT_DIR to point to the root of the STEPcode source tree.
if(NOT DEFINED STEPCODE_ROOT_DIR)
  message(FATAL_ERROR "STEPCODE_ROOT_DIR is not defined! Set via cmake-gui or on command line:\n    cmake .. -DSTEPCODE_ROOT_DIR=/path/to/sc")
endif(NOT DEFINED STEPCODE_ROOT_DIR)

# STEPCODE_ROOT_DIR is relative or absolute path?
if(EXISTS "${CMAKE_BINARY_DIR}/${STEPCODE_ROOT_DIR}/SC_VERSION.txt")
  set(STEPCODE_ROOT_DIR "${CMAKE_BINARY_DIR}/${STEPCODE_ROOT_DIR}")
  message("** STEPCODE_ROOT_DIR is a relative path; converted to absolute path: ${STEPCODE_ROOT_DIR}.")
else()
  if(NOT EXISTS "${STEPCODE_ROOT_DIR}/SC_VERSION.txt")
    message(FATAL_ERROR "****  Cannot locate STEPCODE_ROOT_DIR - try an absolute path.")
  endif(NOT EXISTS "${STEPCODE_ROOT_DIR}/SC_VERSION.txt")
endif(EXISTS "${CMAKE_BINARY_DIR}/${STEPCODE_ROOT_DIR}/SC_VERSION.txt")


# Use STEPcode as library, but build from this build process.
set(SC_IS_SUBBUILD TRUE)

# Build type, DEBUG or RELEASE
if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif(NOT DEFINED CMAKE_BUILD_TYPE)
set(SC_BUILD_TYPE ${CMAKE_BUILD_TYPE})

# Path to STEPcode
set(STEPCODE_BUILD_DIR ${CMAKE_BINARY_DIR}/sc CACHE PATH "STEPcode build subdir")

# STEP schema to build and link against.
set(SCHEMA ap203)
set(SC_BUILD_SCHEMAS ${SCHEMA} CACHE INTERNAL "Schema, internal, immutable" FORCE)

# Parse out schema name and library.
set(SCHEMA_PATH "${STEPCODE_ROOT_DIR}/data/${SCHEMA}/*.exp")
file(GLOB SCHEMA_FILE "${SCHEMA_PATH}")

if(NOT EXISTS ${SCHEMA_FILE})
  message(FATAL_ERROR "Cannot find the schema; cannot continue. Looked for ${SCHEMA_PATH} from ${CMAKE_CURRENT_LIST_DIR}")
endif(NOT EXISTS ${SCHEMA_FILE})


get_filename_component(SCHEMA_SN ${SCHEMA_FILE} NAME)
string(REGEX REPLACE "\(.*\).[Ee][Xx][Pp]" "sdai_\\1" SCHEMA_LINK_NAME ${SCHEMA_SN})
set(STEPCODE_LIBRARIES base stepcore stepeditor stepdai steputils ${SCHEMA_LINK_NAME})

# Add STEPCode project to CMake build.
add_subdirectory(${STEPCODE_ROOT_DIR} "${CMAKE_CURRENT_BINARY_DIR}/sc" EXCLUDE_FROM_ALL)

# Set up STEPcode include directories.
set(STEPCODE_INCLUDE_DIR
  ${STEPCODE_ROOT_DIR}/src/base
  ${STEPCODE_ROOT_DIR}/src/clstepcore
  ${STEPCODE_ROOT_DIR}/src/cldai
  ${STEPCODE_ROOT_DIR}/src/clutils
  ${STEPCODE_ROOT_DIR}/src/cleditor
  ${STEPCODE_BUILD_DIR}/include
  ${STEPCODE_ROOT_DIR}/include
  ${CMAKE_BINARY_DIR}
  ${STEPCODE_BUILD_DIR}/schemas/${SCHEMA_LINK_NAME}
 )

# Reset sane values for build destination.  Otherwise, everything ends up in ${CMAKE_BINARY_DIR}/sc
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(
  ${STEPCODE_INCLUDE_DIR}
 )

# Application sources and headers
set(SRCS ap203min.cpp)

set(HDRS )

add_executable(${PROJECT_NAME} ${SRCS} ${HDRS})

target_link_libraries(${PROJECT_NAME} ${STEPCODE_LIBRARIES})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

