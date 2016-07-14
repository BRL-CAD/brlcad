#
# - Find perplex executable and provides macros to generate custom build rules
# The module defines the following variables
#
#  PERPLEX_EXECUTABLE - path to the perplex program
#  PERPLEX_TEMPLATE - location of the perplex template file

#=============================================================================
#                 F I N D P E R P L E X . C M A K E
#
# Originally based off of FindBISON.cmake from Kitware's CMake distribution
#
# Copyright (c) 2010-2016 United States Government as represented by
#                the U.S. Army Research Laboratory.
# Copyright 2009 Kitware, Inc.
# Copyright 2006 Tristan Carel
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * The names of the authors may not be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

find_program(PERPLEX_EXECUTABLE perplex DOC "path to the perplex executable")
mark_as_advanced(PERPLEX_EXECUTABLE)

if(PERPLEX_EXECUTABLE AND NOT PERPLEX_TEMPLATE)
  get_filename_component(perplex_path ${PERPLEX_EXECUTABLE} PATH)
  if(perplex_path)
    set(PERPLEX_TEMPLATE "")
    if(EXISTS ${perplex_path}/../share/perplex/perplex_template.c)
      get_filename_component(perplex_template_path "${perplex_path}/../share/perplex/perplex_template.c" ABSOLUTE)
      set(PERPLEX_TEMPLATE "${perplex_template_path}")
    endif(EXISTS ${perplex_path}/../share/perplex/perplex_template.c)
    if(EXISTS ${perplex_path}/../share/perplex_template.c AND NOT PERPLEX_TEMPLATE)
      get_filename_component(perplex_template_path "${perplex_path}/../share/perplex_template.c" ABSOLUTE)
      set(PERPLEX_TEMPLATE "${perplex_template_path}")
    endif(EXISTS ${perplex_path}/../share/perplex_template.c AND NOT PERPLEX_TEMPLATE)
  endif(perplex_path)
  if(EXISTS /usr/share/perplex/perplex_template.c AND NOT PERPLEX_TEMPLATE)
    set(PERPLEX_TEMPLATE "/usr/share/perplex/perplex_template.c")
  endif(EXISTS /usr/share/perplex/perplex_template.c AND NOT PERPLEX_TEMPLATE)
endif(PERPLEX_EXECUTABLE AND NOT PERPLEX_TEMPLATE)
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(PERPLEX DEFAULT_MSG PERPLEX_EXECUTABLE PERPLEX_TEMPLATE)
mark_as_advanced(PERPLEX_TEMPLATE)

# Defines two macros - PERPLEX_TARGET, which takes perplex inputs and
# runs both perplex and re2c to generate C source code/headers, and
# ADD_PERPLEX_LEMON_DEPENDENCY which is used to set up dependencies between
# scanner and parser targets when necessary.
#
# #====================================================================
#  Example:
#
#   find_package(LEMON)
#   find_package(RE2C)
#   find_package(PERPLEX)
#
#   LEMON_TARGET(MyParser parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp")
#   PERPLEX_TARGET(MyScanner scanner.re  "${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp" "${CMAKE_CURRENT_BINARY_DIR}/scanner_header.hpp")
#   ADD_PERPLEX_LEMON_DEPENDENCY(MyScanner MyParser)
#
#   include_directories("${CMAKE_CURRENT_BINARY_DIR}")
#   add_executable(Foo
#      Foo.cc
#      ${LEMON_MyParser_OUTPUTS}
#      ${PERPLEX_MyScanner_OUTPUTS}
#   )
#  ====================================================================
#
#=============================================================================
#
# Originally based off of FindBISON.cmake from Kitware's CMake distribution
#
# Copyright (c) 2010-2016 United States Government as represented by
#                the U.S. Army Research Laboratory.
# Copyright 2009 Kitware, Inc.
# Copyright 2006 Tristan Carel
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * The names of the authors may not be used to endorse or promote
#   products derived from this software without specific prior written
#   permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#=============================================================================

#============================================================
# PERPLEX_TARGET (public macro)
#============================================================

include(CMakeParseArguments)

if(NOT COMMAND PERPLEX_TARGET)
  macro(PERPLEX_TARGET Name Input)
    get_filename_component(IN_FILE_WE ${Input} NAME_WE)
    set(PVAR_PREFIX ${Name}_${IN_FILE_WE})

    if(${ARGC} GREATER 3)
      CMAKE_PARSE_ARGUMENTS(${PVAR_PREFIX} "" "TEMPLATE;OUT_SRC_FILE;OUT_HDR_FILE;WORKING_DIR" "" ${ARGN})
    endif(${ARGC} GREATER 3)

    # Need a working directory
    if("${${PVAR_PREFIX}_WORKING_DIR}" STREQUAL "")
      set(${PVAR_PREFIX}_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/${PVAR_PREFIX}")
    endif("${${PVAR_PREFIX}_WORKING_DIR}" STREQUAL "")
    file(MAKE_DIRECTORY ${${PVAR_PREFIX}_WORKING_DIR})

    # Set up intermediate and final output names

    # Output source file
    if ("${${PVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")
      set(${PVAR_PREFIX}_OUT_SRC_FILE ${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.c)
    else ("${${PVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")
      get_filename_component(specified_out_dir ${${PVAR_PREFIX}_OUT_SRC_FILE} PATH)
      if(NOT "${specified_out_dir}" STREQUAL "")
	message(FATAL_ERROR "\nFull path specified for OUT_SRC_FILE - should be filename only.\n")
      endif(NOT "${specified_out_dir}" STREQUAL "")
      set(${PVAR_PREFIX}_OUT_SRC_FILE ${${PVAR_PREFIX}_WORKING_DIR}/${${PVAR_PREFIX}_OUT_SRC_FILE})
    endif ("${${PVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")

    # Output header file
    if ("${${PVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")
      set(${PVAR_PREFIX}_OUT_HDR_FILE ${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.h)
    else ("${${PVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")
      get_filename_component(specified_out_dir ${${PVAR_PREFIX}_OUT_HDR_FILE} PATH)
      if(NOT "${specified_out_dir}" STREQUAL "")
	message(FATAL_ERROR "\nFull path specified for OUT_HDR_FILE - should be filename only.\n")
      endif(NOT "${specified_out_dir}" STREQUAL "")
      set(${PVAR_PREFIX}_OUT_HDR_FILE ${${PVAR_PREFIX}_WORKING_DIR}/${${PVAR_PREFIX}_OUT_HDR_FILE})
    endif ("${${PVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")

    # input file
    get_filename_component(in_full ${Input} ABSOLUTE)
    if("${in_full}" STREQUAL "${Input}")
      set(perplex_in_file ${Input})
    else("${in_full}" STREQUAL "${Input}")
      set(perplex_in_file "${CMAKE_CURRENT_SOURCE_DIR}/${Input}")
    endif("${in_full}" STREQUAL "${Input}")

    # Intermediate file
    set(re2c_src "${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.re")

    # Make sure we have a template
    if ("${${PVAR_PREFIX}_TEMPLATE}" STREQUAL "")
      if(PERPLEX_TEMPLATE)
	set(${PVAR_PREFIX}_TEMPLATE ${PERPLEX_TEMPLATE})
      else(PERPLEX_TEMPLATE)
	message(FATAL_ERROR "\nNo Perplex template file specified - please specify the file using the PERPLEX_TEMPLATE variable:\ncmake .. -DPERPLEX_TEMPLATE=/path/to/template_file.c\n")
      endif(PERPLEX_TEMPLATE)
    endif ("${${PVAR_PREFIX}_TEMPLATE}" STREQUAL "")

    get_filename_component(IN_FILE ${Input} NAME)
    add_custom_command(
      OUTPUT ${re2c_src} ${${PVAR_PREFIX}_OUT_HDR_FILE} ${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE}
      COMMAND ${CMAKE_COMMAND} -E copy ${perplex_in_file} ${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE}
      COMMAND ${PERPLEX_EXECUTABLE} -c -o ${re2c_src} -i ${${PVAR_PREFIX}_OUT_HDR_FILE} -t ${${PVAR_PREFIX}_TEMPLATE} ${${PVAR_PREFIX}_WORKING_DIR}/${IN_FILE}
      DEPENDS ${Input} ${${PVAR_PREFIX}_TEMPLATE} ${PERPLEX_EXECUTABLE_TARGET} ${RE2C_EXECUTABLE_TARGET}
      WORKING_DIRECTORY ${${PVAR_PREFIX}_WORKING_DIR}
      COMMENT "[PERPLEX][${Name}] Generating re2c input with ${PERPLEX_EXECUTABLE}"
      )

    if(NOT DEBUGGING_GENERATED_SOURCES)
      add_custom_command(
	OUTPUT ${${PVAR_PREFIX}_OUT_SRC_FILE}
	COMMAND ${RE2C_EXECUTABLE} --no-debug-info --no-generation-date -c -o ${${PVAR_PREFIX}_OUT_SRC_FILE} ${re2c_src}
	DEPENDS ${Input} ${re2c_src} ${${PVAR_PREFIX}_OUT_HDR_FILE} ${PERPLEX_EXECUTABLE_TARGET} ${RE2C_EXECUTABLE_TARGET}
	WORKING_DIRECTORY ${${PVAR_PREFIX}_WORKING_DIR}
	COMMENT "[RE2C][${Name}] Building scanner with ${RE2C_EXECUTABLE}"
	)
    else(NOT DEBUGGING_GENERATED_SOURCES)
      add_custom_command(
	OUTPUT ${${PVAR_PREFIX}_OUT_SRC_FILE}
	COMMAND ${RE2C_EXECUTABLE} --no-generation-date -c -o ${${PVAR_PREFIX}_OUT_SRC_FILE} ${re2c_src}
	DEPENDS ${Input} ${re2c_src} ${${PVAR_PREFIX}_OUT_HDR_FILE} ${PERPLEX_EXECUTABLE_TARGET} ${RE2C_EXECUTABLE_TARGET}
	WORKING_DIRECTORY ${${PVAR_PREFIX}_WORKING_DIR}
	COMMENT "[RE2C][${Name}] Building scanner with ${RE2C_EXECUTABLE}"
	)
    endif(NOT DEBUGGING_GENERATED_SOURCES)

    set(PERPLEX_${Name}_DEFINED TRUE)
    set(PERPLEX_${Name}_SRC ${${PVAR_PREFIX}_OUT_SRC_FILE})
    set(PERPLEX_${Name}_HDR ${${PVAR_PREFIX}_OUT_HDR_FILE})
    set(PERPLEX_${Name}_INCLUDE_DIR ${${PVAR_PREFIX}_WORKING_DIR})
  endmacro(PERPLEX_TARGET)
endif(NOT COMMAND PERPLEX_TARGET)

#============================================================
# ADD_PERPLEX_LEMON_DEPENDENCY (public macro)
#============================================================
if(NOT COMMAND ADD_PERPLEX_LEMON_DEPENDENCY)
  macro(ADD_PERPLEX_LEMON_DEPENDENCY PERPLEXTarget LemonTarget)

    if(NOT PERPLEX_${PERPLEXTarget}_SRC)
      message(SEND_ERROR "PERPLEX target `${PERPLEXTarget}' does not exists.")
    endif()

    if(NOT LEMON_${LemonTarget}_HDR)
      message(SEND_ERROR "Lemon target `${LemonTarget}' does not exists.")
    endif()

    set_source_files_properties(${PERPLEX_${PERPLEXTarget}_SRC}
      PROPERTIES OBJECT_DEPENDS ${LEMON_${LemonTarget}_HDR})
  endmacro(ADD_PERPLEX_LEMON_DEPENDENCY)
endif(NOT COMMAND ADD_PERPLEX_LEMON_DEPENDENCY)

#============================================================
# FindPERPLEX.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
