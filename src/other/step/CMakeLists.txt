#       C M A K E L I S T S . T X T   F O R   S C L
#
# This file is Copyright (c) 2010 United States Government as 
# represented by the U.S. Army Research Laboratory.
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
# ***                   SCL's CMakeLists.txt                      ***
# *******************************************************************

# This file contains the top level CMakeLists.txt logic for the 
# SCL software package.

PROJECT(SCL)

# SCL version
SET(SCL_VERSION_MAJOR 0)
SET(SCL_VERSION_MINOR 5-dev)
SET(SCL_VERSION ${SCL_VERSION_MAJOR}.${SCL_VERSION_MINOR})

# SCL ABI version. SCL_ABI_SOVERSION should be incremented
# for each release introducing API incompatibilities
SET(SCL_ABI_SOVERSION 2)
SET(SCL_ABI_VERSION ${SCL_ABI_SOVERSION}.0.0)

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

IF( NOT DEFINED SCL_BUILD_TYPE )
  SET( SCL_BUILD_TYPE "Debug" CACHE STRING "Build type" ) # By default set debug build
ENDIF( NOT DEFINED SCL_BUILD_TYPE )
SET(CMAKE_BUILD_TYPE ${SCL_BUILD_TYPE} CACHE INTERNAL "Build type, immutable" FORCE )

# Define helper macro OPTION_WITH_DEFAULT
MACRO( OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT )
  IF( NOT DEFINED ${OPTION_NAME} )
    SET( ${OPTION_NAME} ${OPTION_DEFAULT} )
  ENDIF( NOT DEFINED ${OPTION_NAME} )
  OPTION( ${OPTION_NAME} "${OPTION_STRING}" ${${OPTION_NAME}} )
ENDMACRO( OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT )

# build shared libs by default
OPTION_WITH_DEFAULT(SCL_BUILD_SHARED_LIBS "Build shared libs" ON )

# build static libs by default
OPTION_WITH_DEFAULT(SCL_BUILD_STATIC_LIBS "Build static libs" OFF)

OPTION_WITH_DEFAULT(SCL_PYTHON_GENERATOR "Compile fedex_plus_python" ON)
OPTION_WITH_DEFAULT(SCL_CPP_GENERATOR "Compile fedex_plus" ON)

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(SCL_CMAKE_DIR "${SCL_SOURCE_DIR}/cmake")
if(NOT IS_SUBBUILD)
  SET(CMAKE_MODULE_PATH "${SCL_CMAKE_DIR};${CMAKE_MODULE_PATH}")
else(NOT IS_SUBBUILD)
  SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${SCL_CMAKE_DIR}")
endif(NOT IS_SUBBUILD)

INCLUDE(${SCL_CMAKE_DIR}/SCL_Utils.cmake)

# Save the current LC_ALL, LC_MESSAGES, and LANG environment variables and set them
# to "C" so things like date output are as expected
SET(_orig_lc_all      $ENV{LC_ALL})
SET(_orig_lc_messages $ENV{LC_MESSAGES})
SET(_orig_lang        $ENV{LANG})
IF(_orig_lc_all)
  SET(ENV{LC_ALL}      C)
ENDIF(_orig_lc_all)
IF(_orig_lc_messages)
  SET(ENV{LC_MESSAGES} C)
ENDIF(_orig_lc_messages)
IF(_orig_lang)
  SET(ENV{LANG}        C)
ENDIF(_orig_lang)

#---------------------------------------------------------------------
# Testing option
OPTION_WITH_DEFAULT( SCL_ENABLE_TESTING "Enable unittesting framework" ON )
IF(SCL_ENABLE_TESTING)
  if( NOT DEFINED SCL_BUILD_SCHEMAS )
    set( SCL_BUILD_SCHEMAS "ALL" ) #test all schemas, unless otherwise specified
  endif()
  INCLUDE(CTest)
  ENABLE_TESTING()
ENDIF(SCL_ENABLE_TESTING)

#---------------------------------------------------------------------
# Coverage option
OPTION_WITH_DEFAULT( SCL_ENABLE_COVERAGE "Enable code coverage test" OFF )
IF(SCL_ENABLE_COVERAGE)
  SET(SCL_ENABLE_TESTING ON CACHE BOOL "Testing enabled by coverage option" FORCE)
  # build static libs, better coverage report
  SET(SCL_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE )
  SET(SCL_BUILD_STATIC_LIBS ON CACHE BOOL "Build static libs" FORCE )
  SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG "-fprofile-arcs -ftest-coverage" CACHE STRING "Extra linker flags required by code coverage" FORCE)
  SET(SCL_BUILD_TYPE "Debug" CACHE STRING "Build type required by testing framework" FORCE)
ENDIF(SCL_ENABLE_COVERAGE)

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# the RPATH/INSTALL_NAME_DIR to be used when installing
if (NOT APPLE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib:\$ORIGIN/../lib")
endif(NOT APPLE)
# On OSX, we need to set INSTALL_NAME_DIR instead of RPATH
# http://www.cmake.org/cmake/help/cmake-2-8-docs.html#variable:CMAKE_INSTALL_NAME_DIR
set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH which point to
# directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#-----------------------------------------------------------------------------
# Output directories.
IF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)

FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
  IF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
  ENDIF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
  IF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
  ENDIF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
  IF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SCL_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
  ENDIF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
ENDFOREACH()


#-----------------------------------------------------------------------------
# Configure install locations. 

# The location in which to install SCL. Need a good Debug location 
# for Windows.  Only do this if CMAKE_INSTALL_PREFIX hasn't been set
# already, to try and allow parent builds (if any) some control.
if(NOT IS_SUBBUILD)
  IF(NOT WIN32)
    IF (${CMAKE_BUILD_TYPE} MATCHES "Debug")
      SET(SCL_INSTALL_PREFIX "${SCL_SOURCE_DIR}/../scl-install")
    ELSE()
      SET(SCL_INSTALL_PREFIX "/usr/local")
    ENDIF()
  ENDIF(NOT WIN32)
  SET( SCL_INSTALL_PREFIX ${SCL_INSTALL_PREFIX} CACHE
    PATH "Install prefix prepended to target to create install location" )
  SET( CMAKE_INSTALL_PREFIX ${SCL_INSTALL_PREFIX} CACHE INTERNAL "Prefix prepended to install directories if target destination is not absolute, immutable" FORCE )
endif(NOT IS_SUBBUILD)

OPTION(SCL_BUILD_EXPRESS_ONLY "Only build express parser." OFF)
MARK_AS_ADVANCED(SCL_BUILD_EXPRESS_ONLY)

# Take the scl config file template as the starting point for 
# scl_cf.h.in - scripts may need to append to the template, so
# it is read into memory initially.
SET(CONFIG_H_FILE ${SCL_BINARY_DIR}/include/scl_cf.h.in)
set(CMAKE_CURRENT_PROJECT SCL)
define_property(GLOBAL PROPERTY SCL_CONFIG_H_CONTENTS BRIEF_DOCS "config.h.in contents" FULL_DOCS "config.h.in contents for SCL project")
if(NOT COMMAND CONFIG_H_APPEND)
  macro(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endmacro(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)
file(READ ${SCL_SOURCE_DIR}/include/scl_cf_cmake.h.in CONFIG_H_FILE_CONTENTS)
CONFIG_H_APPEND(SCL "${CONFIG_H_FILE_CONTENTS}")

find_package(LEMON)
if(NOT COMMAND LEMON_TARGET)
  include(${SCL_CMAKE_DIR}/LEMON_Util.cmake)
endif(NOT COMMAND LEMON_TARGET)

INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckTypeSize)

CHECK_INCLUDE_FILE(ndir.h HAVE_NDIR_H)
CHECK_INCLUDE_FILE(stdarg.h HAVE_STDARG_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(sys/param.h HAVE_SYS_PARAM_H)
CHECK_INCLUDE_FILE(sysent.h HAVE_SYSENT_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(dirent.h HAVE_DIRENT_H)
CHECK_INCLUDE_FILE(stdbool.h HAVE_STDBOOL_H)
CHECK_INCLUDE_FILE(process.h HAVE_PROCESS_H)
CHECK_INCLUDE_FILE(io.h HAVE_IO_H)

CHECK_FUNCTION_EXISTS(abs HAVE_ABS)
CHECK_FUNCTION_EXISTS(memcpy HAVE_MEMCPY)
CHECK_FUNCTION_EXISTS(memmove HAVE_MEMMOVE)
CHECK_FUNCTION_EXISTS(getopt HAVE_GETOPT)

CHECK_TYPE_SIZE("ssize_t" SSIZE_T)

# Now that all the tests are done, configure the scl_cf.h file:
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY SCL_CONFIG_H_CONTENTS)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
configure_file(${CONFIG_H_FILE} ${SCL_BINARY_DIR}/include/scl_cf.h)

################ create scl_version_string.h, http://stackoverflow.com/questions/3780667
# Using 'ver_string' instead of 'scl_version_string.h' is a trick to force the
# command to always execute when the custom target is built. It works because
# a file by that name never exists.
add_custom_target(version_string ALL
  DEPENDS ver_string )
# creates scl_version_string.h using cmake script
add_custom_command(OUTPUT ver_string ${CMAKE_CURRENT_BINARY_DIR}/include/scl_version_string.h
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${SCL_SOURCE_DIR}
  -DBINARY_DIR=${SCL_BINARY_DIR}
  -P ${SCL_CMAKE_DIR}/scl_version_string.cmake)
# scl_version_string.h is a generated file
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/include/scl_version_string.h
  PROPERTIES GENERATED TRUE
  HEADER_FILE_ONLY TRUE )
################

if(MSVC)
  # add_definitions( -Wall )
  add_definitions( -D__MSVC__ -D__WIN32__ )
  # Disable warning for preferred usage of secure functions (example strcpy should be strcpy_s, ...)
  add_definitions( -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS )
elseif(BORLAND)
  add_definitions( -D__BORLAND__ -D__WIN32__ )
else()
  add_definitions( -pedantic -W -Wall -Wundef -Wfloat-equal -Wshadow -Winline -Wno-long-long )
endif()

include_directories(
  ${SCL_SOURCE_DIR}/include
  ${SCL_BINARY_DIR}/include
  )

#ADD_SUBDIRECTORY(src/base)
ADD_SUBDIRECTORY(src/express)
ADD_SUBDIRECTORY(src/exppp)
ADD_SUBDIRECTORY(src/fedex_plus)
#ADD_SUBDIRECTORY(src/fedex_python)
ADD_SUBDIRECTORY(src/clstepcore)
ADD_SUBDIRECTORY(src/cleditor)
ADD_SUBDIRECTORY(src/cldai)
ADD_SUBDIRECTORY(src/clutils)
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(data)
IF(ENABLE_TESTING)
  ADD_SUBDIRECTORY( test )
ENDIF(ENABLE_TESTING)
ADD_SUBDIRECTORY(doc)

# this is for testing - 'make core' builds everything that isn't generated
add_custom_target( core )
add_dependencies( core stepdai check-express stepeditor fedex_plus )

if(NOT IS_SUBBUILD)
  ###############################################################################
  # SCL Packaging                                                               #
  # $make package                                                               #
  ###############################################################################

  SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "STEP Class Library")
  SET(CPACK_SET_DESTDIR "ON")

  SET(CPACK_PACKAGE_VERSION_MAJOR ${SCL_VERSION_MAJOR})
  SET(CPACK_PACKAGE_VERSION_MINOR ${SCL_VERSION_MINOR})
  SET(CPACK_PACKAGE_NAME SCL )

  SET(CPACK_PACKAGE_CONTACT "SCL Developers <scl-dev@googlegroups.com>")
  INCLUDE(CPack)

  ########################################################################################
  # Uninstall code                                                                       #
  # From http://www.cmake.org/Wiki/CMake_FAQ#Can_I_do_.22make_uninstall.22_with_CMake.3F #
  # ######################################################################################
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

endif(NOT IS_SUBBUILD)
