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
# ***                   SC's CMakeLists.txt                      ***
# *******************************************************************

# This file contains the top level CMakeLists.txt logic for the
# SC software package.

PROJECT(SC)

# SC version
SET(SC_VERSION_MAJOR 0)
SET(SC_VERSION_MINOR 7-dev)
SET(SC_VERSION ${SC_VERSION_MAJOR}.${SC_VERSION_MINOR})

# SC ABI version. SC_ABI_SOVERSION should be incremented
# for each release introducing API incompatibilities
SET(SC_ABI_SOVERSION 2)
SET(SC_ABI_VERSION ${SC_ABI_SOVERSION}.0.0)

# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7)
IF(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
  if ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
	  CMAKE_POLICY(SET CMP0026 OLD)
  endif ("${CMAKE_VERSION}" VERSION_GREATER 2.99)
ENDIF(COMMAND CMAKE_POLICY)

IF( NOT DEFINED INCLUDE_INSTALL_DIR )
  SET( INCLUDE_INSTALL_DIR include )
ENDIF( NOT DEFINED INCLUDE_INSTALL_DIR )

IF( NOT DEFINED LIB_INSTALL_DIR )
  SET( LIB_INSTALL_DIR lib )
ENDIF( NOT DEFINED LIB_INSTALL_DIR )

IF( NOT DEFINED BIN_INSTALL_DIR )
  SET( BIN_INSTALL_DIR bin )
ENDIF( NOT DEFINED BIN_INSTALL_DIR )

IF( NOT DEFINED SC_BUILD_TYPE )
  SET( SC_BUILD_TYPE "Debug" CACHE STRING "Build type" ) # By default set debug build
ENDIF( NOT DEFINED SC_BUILD_TYPE )
IF(NOT SC_IS_SUBBUILD)
  SET(CMAKE_BUILD_TYPE ${SC_BUILD_TYPE} CACHE INTERNAL "Build type, immutable" FORCE )
ELSE(NOT SC_IS_SUBBUILD)
  SET(CMAKE_BUILD_TYPE ${SC_BUILD_TYPE} )
ENDIF(NOT SC_IS_SUBBUILD)

# Define helper macro OPTION_WITH_DEFAULT
MACRO( OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT )
  IF( NOT DEFINED ${OPTION_NAME} )
    SET( ${OPTION_NAME} ${OPTION_DEFAULT} )
  ENDIF( NOT DEFINED ${OPTION_NAME} )
  OPTION( ${OPTION_NAME} "${OPTION_STRING}" ${${OPTION_NAME}} )
ENDMACRO( OPTION_WITH_DEFAULT OPTION_NAME OPTION_STRING OPTION_DEFAULT )

# build shared libs by default
OPTION_WITH_DEFAULT(SC_BUILD_SHARED_LIBS "Build shared libs" ON )

# don't build static libs by default
OPTION_WITH_DEFAULT(SC_BUILD_STATIC_LIBS "Build static libs" OFF)

OPTION_WITH_DEFAULT(SC_PYTHON_GENERATOR "Compile exp2python" ON)
OPTION_WITH_DEFAULT(SC_CPP_GENERATOR "Compile exp2cxx" ON)

OPTION_WITH_DEFAULT(SC_MEMMGR_ENABLE_CHECKS "Enable sc_memmgr's memory leak detection" OFF)
OPTION_WITH_DEFAULT(SC_TRACE_FPRINTF "Enable extra comments in generated code so the code's source in exp2cxx may be located" OFF)

if(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS )
  set( SC_SDAI_ADDITIONAL_EXES_SRCS "" CACHE STRING "Source files for additional executables to be linked with SDAI libs" )
endif(NOT DEFINED SC_SDAI_ADDITIONAL_EXES_SRCS )

if( NOT DEFINED SC_BUILD_SCHEMAS )
    message( "** CMake variable SC_BUILD_SCHEMAS not set. Defaults to building ALL schemas. This will take a while; see http://stepcode.org/mw/index.php?title=STEPcode_CMake_variables#SC_BUILD_SCHEMAS" )
    #this makes SC_BUILD_SCHEMAS show up in cmake-gui
    SET(SC_BUILD_SCHEMAS "ALL" CACHE string "Semicolon-separated list of paths to EXPRESS schemas to be built" )
endif( NOT DEFINED SC_BUILD_SCHEMAS )

# CMake derives much of its functionality from modules, typically
# stored in one directory - let CMake know where to find them.
SET(SC_CMAKE_DIR "${SC_SOURCE_DIR}/cmake")
if(NOT IS_SUBBUILD)
  SET(CMAKE_MODULE_PATH "${SC_CMAKE_DIR};${CMAKE_MODULE_PATH}")
else(NOT IS_SUBBUILD)
  SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${SC_CMAKE_DIR}")
endif(NOT IS_SUBBUILD)

INCLUDE(${SC_CMAKE_DIR}/SC_Utils.cmake)

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
# Coverage option
OPTION_WITH_DEFAULT( SC_ENABLE_COVERAGE "Enable code coverage test" OFF )
IF(SC_ENABLE_COVERAGE)
  SET(SC_ENABLE_TESTING ON CACHE BOOL "Testing enabled by coverage option" FORCE)
  # build static libs, better coverage report
  SET(SC_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE )
  SET(SC_BUILD_STATIC_LIBS ON CACHE BOOL "Build static libs" FORCE )
  SET(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  SET(CMAKE_C_FLAGS_DEBUG "-O0 -g -fprofile-arcs -ftest-coverage" CACHE STRING "Extra compile flags required by code coverage" FORCE)
  SET(CMAKE_MODULE_LINKER_FLAGS_DEBUG "-fprofile-arcs -ftest-coverage" CACHE STRING "Extra linker flags required by code coverage" FORCE)
  SET(SC_BUILD_TYPE "Debug" CACHE STRING "Build type required by testing framework" FORCE)
  SET( SC_PYTHON_GENERATOR OFF ) #won't build with static libs
ENDIF(SC_ENABLE_COVERAGE)

#---------------------------------------------------------------------
# Testing option
OPTION_WITH_DEFAULT( SC_ENABLE_TESTING "Enable unittesting framework" OFF )
IF(SC_ENABLE_TESTING)
  if( NOT DEFINED SC_BUILD_SCHEMAS )
    set( SC_BUILD_SCHEMAS "ALL" ) #test all schemas, unless otherwise specified
  endif()
  INCLUDE(CTest)
  ENABLE_TESTING()
ENDIF(SC_ENABLE_TESTING)

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
IF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
ENDIF(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
IF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
ENDIF(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
IF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SC_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
ENDIF(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)

FOREACH(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
  STRING(TOUPPER "${CFG_TYPE}" CFG_TYPE)
  IF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all libraries.")
  ENDIF(NOT "CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_TYPE}")
  IF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/lib CACHE INTERNAL "Single output directory for building all archives.")
  ENDIF(NOT "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_TYPE}")
  IF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
    SET("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}" ${SC_BINARY_DIR}/bin CACHE INTERNAL "Single output directory for building all executables.")
  ENDIF(NOT "CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE}")
ENDFOREACH()


#-----------------------------------------------------------------------------
# Configure install locations.

# The location in which to install SC. Need a good Debug location
# for Windows.  Only do this if CMAKE_INSTALL_PREFIX hasn't been set
# already, to try and allow parent builds (if any) some control.
if(NOT SC_IS_SUBBUILD)
  IF(NOT WIN32)
    IF (${CMAKE_BUILD_TYPE} MATCHES "Debug")
      SET(SC_INSTALL_PREFIX "${SC_SOURCE_DIR}/../sc-install")
    ELSE()
      SET(SC_INSTALL_PREFIX "/usr/local")
    ENDIF()
  ENDIF(NOT WIN32)
  SET( SC_INSTALL_PREFIX ${SC_INSTALL_PREFIX} CACHE
    PATH "Install prefix prepended to target to create install location" )
  SET( CMAKE_INSTALL_PREFIX ${SC_INSTALL_PREFIX} CACHE INTERNAL "Prefix prepended to install directories if target destination is not absolute, immutable" FORCE )
endif(NOT SC_IS_SUBBUILD)

OPTION(SC_BUILD_EXPRESS_ONLY "Only build express parser." OFF)
MARK_AS_ADVANCED(SC_BUILD_EXPRESS_ONLY)

# Take the sc config file template as the starting point for
# sc_cf.h.in - scripts may need to append to the template, so
# it is read into memory initially.
SET(CONFIG_H_FILE ${SC_BINARY_DIR}/include/sc_cf.h.in)
set_source_files_properties(${CONFIG_H_FILE} PROPERTIES GENERATED TRUE)
set(CMAKE_CURRENT_PROJECT SC)
define_property(GLOBAL PROPERTY SC_CONFIG_H_CONTENTS BRIEF_DOCS "config.h.in contents" FULL_DOCS "config.h.in contents for SC project")
if(NOT COMMAND CONFIG_H_APPEND)
  macro(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endmacro(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)
file(READ ${SC_SOURCE_DIR}/include/sc_cf_cmake.h.in CONFIG_H_FILE_CONTENTS)
CONFIG_H_APPEND(SC "${CONFIG_H_FILE_CONTENTS}")

# The Express parser uses the tools Perplex, RE2C and Lemon to generate code
# from higher level inputs.  Depending on available tools and options, the
# SC build can either re-generate code as part of the build, or use cached
# files that are ready for compilation.
#
# SC_GENERATE_LEXER_PARSER is the "high level" control a user sets to determine
# how the SC build will interact (or not) with these tools.  AUTO (the
# default) means it will search for the necessary tools, and use them only if
# everything is found.  If not, it will fall back to the cached versions.  If
# this option is set to ON and the necessary tools are not found, the
# configure step will fail.  If it is set to OFF, SC will not even try to use
# the generators and will instead use the cached sources.
if(NOT SC_GENERATE_LEXER_PARSER)
  set(SC_GENERATE_LEXER_PARSER "AUTO" CACHE STRING "Use Perplex, RE2C and Lemon to generate C source code.")
else(NOT SC_GENERATE_LEXER_PARSER)
  string(TOUPPER "${SC_GENERATE_LEXER_PARSER}" SC_GENERATE_LEXER_PARSER)
endif(NOT SC_GENERATE_LEXER_PARSER)
set_property(CACHE SC_GENERATE_LEXER_PARSER PROPERTY STRINGS AUTO ON OFF)
if (NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "AUTO" AND NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON" AND NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")
	message(WARNING "Unknown value ${SC_GENERATE_LEXER_PARSER} supplied for BRLCAD_WORD_SIZE - defaulting to AUTO")
	message(WARNING "Valid options are AUTO, ON and OFF")
	set(SC_GENERATE_LEXER_PARSER "AUTO" CACHE STRING "Use Perplex, RE2C and Lemon to generate C source code.")
endif (NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "AUTO" AND NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON" AND NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")

# If the generators have not been turned off, we need to check for them
if(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")
	find_package(LEMON)
	find_package(RE2C)
	find_package(PERPLEX)
	if(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND PERPLEX_TEMPLATE AND RE2C_EXECUTABLE)
		# Templates may be anywhere - make sure we have a stable path if a relative
		# path was specified at CMake time
		get_filename_component(lemon_template_fpath "${LEMON_TEMPLATE}" ABSOLUTE)
		if(NOT "${lemon_template_fpath}" STREQUAL "${LEMON_TEMPLATE}")
			get_filename_component(LEMON_TEMPLATE "${CMAKE_BINARY_DIR}/${LEMON_TEMPLATE}" ABSOLUTE)
		endif(NOT "${lemon_template_fpath}" STREQUAL "${LEMON_TEMPLATE}")
		get_filename_component(perplex_template_fpath "${PERPLEX_TEMPLATE}" ABSOLUTE)
		if(NOT "${perplex_template_fpath}" STREQUAL "${PERPLEX_TEMPLATE}")
			get_filename_component(PERPLEX_TEMPLATE "${CMAKE_BINARY_DIR}/${PERPLEX_TEMPLATE}" ABSOLUTE)
		endif(NOT "${perplex_template_fpath}" STREQUAL "${PERPLEX_TEMPLATE}")

		if(NOT COMMAND LEMON_TARGET)
			include(${SC_CMAKE_DIR}/LEMON_Util.cmake)
		endif(NOT COMMAND LEMON_TARGET)
		if(NOT COMMAND PERPLEX_TARGET)
			include(${SC_CMAKE_DIR}/PERPLEX_Util.cmake)
		endif(NOT COMMAND PERPLEX_TARGET)
		set(SC_GENERATE_LP_SOURCES 1)
	else(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND PERPLEX_TEMPLATE AND RE2C_EXECUTABLE)
		if("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
			message(FATAL_ERROR "\nSC_GENERATE_LEXER_PARSER set to ON, but one or more components of the Perplex/RE2C/Lemon toolchain were not found.\n")
		else("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
			set(SC_GENERATE_LP_SOURCES 0)
		endif("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
	endif(LEMON_EXECUTABLE AND LEMON_TEMPLATE AND PERPLEX_EXECUTABLE AND PERPLEX_TEMPLATE AND RE2C_EXECUTABLE)
else(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")
	set(SC_GENERATE_LP_SOURCES 0)
endif(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")

INCLUDE(CheckLibraryExists)
INCLUDE(CheckIncludeFile)
INCLUDE(CheckFunctionExists)
INCLUDE(CheckTypeSize)
INCLUDE(CMakePushCheckState)
INCLUDE(CheckCXXSourceRuns)

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

set( TEST_STD_THREAD "
#include <iostream>
#include <thread>
void do_work() {
        std::cout << \"thread\" << std::endl;
}
int main() {
        std::thread t(do_work);
        t.join();
}
" )
cmake_push_check_state()
  if( UNIX )
    set( CMAKE_REQUIRED_FLAGS "-pthread -std=c++0x" )
  else( UNIX )
    # vars probably need set for MSVC11, embarcadero, etc
  endif( UNIX )
  CHECK_CXX_SOURCE_RUNS( "${TEST_STD_THREAD}" HAVE_STD_THREAD )   #quotes are *required*!
cmake_pop_check_state()

# Now that all the tests are done, configure the sc_cf.h file:
get_property(CONFIG_H_FILE_CONTENTS GLOBAL PROPERTY SC_CONFIG_H_CONTENTS)
file(WRITE ${CONFIG_H_FILE} "${CONFIG_H_FILE_CONTENTS}")
configure_file(${CONFIG_H_FILE} ${SC_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_cf.h)

################ create sc_version_string.h, http://stackoverflow.com/questions/3780667
# Using 'ver_string' instead of 'sc_version_string.h' is a trick to force the
# command to always execute when the custom target is built. It works because
# a file by that name never exists.
configure_file(${SC_CMAKE_DIR}/sc_version_string.cmake ${SC_BINARY_DIR}/sc_version_string.cmake @ONLY)
add_custom_target(version_string ALL DEPENDS ver_string )
# creates sc_version_string.h using cmake script
add_custom_command(OUTPUT ver_string ${CMAKE_CURRENT_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_version_string.h
                   COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${SC_SOURCE_DIR}
                                            -DBINARY_DIR=${SC_BINARY_DIR}
                                            -P ${SC_BINARY_DIR}/sc_version_string.cmake)
# sc_version_string.h is a generated file
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_version_string.h
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
  ${SC_SOURCE_DIR}/include
  ${SC_BINARY_DIR}/include
  )

ADD_SUBDIRECTORY(src/base)
ADD_SUBDIRECTORY(src/express)
ADD_SUBDIRECTORY(src/exppp)
ADD_SUBDIRECTORY(src/exp2cxx)
ADD_SUBDIRECTORY(src/exp2python)
ADD_SUBDIRECTORY(src/clstepcore)
ADD_SUBDIRECTORY(src/cleditor)
ADD_SUBDIRECTORY(src/cldai)
ADD_SUBDIRECTORY(src/clutils)
if( NOT WIN32 ) # don't build cllazyfile on windows until export/import macros are in place
  ADD_SUBDIRECTORY( src/cllazyfile )
endif( NOT WIN32 )
ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(data)
IF(SC_ENABLE_TESTING)
  ADD_SUBDIRECTORY( test )
ENDIF(SC_ENABLE_TESTING)
ADD_SUBDIRECTORY(doc)

# this is for testing - 'make core' builds everything that isn't generated
add_custom_target( core )
add_dependencies( core stepdai check-express stepeditor exp2cxx )

if(NOT SC_IS_SUBBUILD)
  ###############################################################################
  # SC Packaging                                                               #
  # $make package                                                               #
  ###############################################################################

  SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "STEPcode")
  SET(CPACK_SET_DESTDIR "ON")

  SET(CPACK_PACKAGE_VERSION_MAJOR ${SC_VERSION_MAJOR})
  SET(CPACK_PACKAGE_VERSION_MINOR ${SC_VERSION_MINOR})
  SET(CPACK_PACKAGE_NAME SC )

  SET(CPACK_PACKAGE_CONTACT "SC Developers <scl-dev@googlegroups.com>")
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

endif(NOT SC_IS_SUBBUILD)
