#
# - Find lemon executable and provides macros to generate custom build rules
# The module defines the following variables
#
#  LEMON_EXECUTABLE - path to the lemon program
#  LEMON_TEMPLATE - location of the lemon template file

#=============================================================================
#                 F I N D L E M O N . C M A K E
#
# Originally based off of FindBISON.cmake from Kitware's CMake distribution
#
# Copyright (c) 2010-2020 United States Government as represented by
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

set(_LEMON_SEARCHES)

# Search LEMON_ROOT first if it is set.
if(LEMON_ROOT)
  set(_LEMON_SEARCH_ROOT PATHS ${LEMON_ROOT} NO_DEFAULT_PATH)
  list(APPEND _LEMON_SEARCHES _LEMON_SEARCH_ROOT)
endif()

# Normal search.
set(_LEMON_x86 "(x86)")
set(_LEMON_SEARCH_NORMAL
    PATHS  "$ENV{ProgramFiles}/lemon"
          "$ENV{ProgramFiles${_LEMON_x86}}/lemon")
unset(_LEMON_x86)
list(APPEND _LEMON_SEARCHES _LEMON_SEARCH_NORMAL)

set(LEMON_NAMES lemon)

# Try each search configuration.
foreach(search ${_LEMON_SEARCHES})
  find_program(LEMON_EXECUTABLE lemon ${${search}} PATH_SUFFIXES bin)
endforeach()
mark_as_advanced(LEMON_EXECUTABLE)

foreach(search ${_LEMON_SEARCHES})
  find_file(LEMON_TEMPLATE lempar.c ${${search}} PATH_SUFFIXES ${DATA_DIR} ${DATA_DIR}/lemon)
endforeach()
mark_as_advanced(LEMON_TEMPLATE)

if (LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)
  # look for the template in share
  if (DATA_DIR AND EXISTS "${DATA_DIR}/lemon/lempar.c")
    set (LEMON_TEMPLATE "${DATA_DIR}/lemon/lempar.c")
  elseif (EXISTS "share/lemon/lempar.c")
    set (LEMON_TEMPLATE "share/lemon/lempar.c")
  elseif (EXISTS "/usr/share/lemon/lempar.c")
    set (LEMON_TEMPLATE "/usr/share/lemon/lempar.c")
  endif (DATA_DIR AND EXISTS "${DATA_DIR}/lemon/lempar.c")
endif (LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)

if (LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)
  # look for the template in bin dir
  get_filename_component(lemon_path ${LEMON_EXECUTABLE} PATH)
  if (lemon_path)
    if (EXISTS ${lemon_path}/lempar.c)
      set (LEMON_TEMPLATE "${lemon_path}/lempar.c")
    endif (EXISTS ${lemon_path}/lempar.c)
    if (EXISTS /usr/share/lemon/lempar.c)
      set (LEMON_TEMPLATE "/usr/share/lemon/lempar.c")
    endif (EXISTS /usr/share/lemon/lempar.c)
  endif (lemon_path)
endif(LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)

if (LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)
  # fallback
  set (LEMON_TEMPLATE "lempar.c")
  if (NOT EXISTS ${LEMON_TEMPLATE})
    message(WARNING "Lemon's lempar.c template file could not be found automatically, set LEMON_TEMPLATE")
  endif (NOT EXISTS ${LEMON_TEMPLATE})
endif (LEMON_EXECUTABLE AND NOT LEMON_TEMPLATE)

mark_as_advanced(LEMON_TEMPLATE)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LEMON DEFAULT_MSG LEMON_EXECUTABLE LEMON_TEMPLATE)

# Define the macro
#  LEMON_TARGET(<Name> <LemonInput> <LemonSource> <LemonHeader>
#		[<ArgString>])
# which will create a custom rule to generate a parser. <LemonInput> is
# the path to a lemon file. <LemonSource> is the desired name for the
# generated source file. <LemonHeader> is the desired name for the
# generated header which contains the token list. Anything in the optional
# <ArgString> parameter is appended to the lemon command line.
#
#  ====================================================================
#  Example:
#
#   find_package(LEMON)
#   LEMON_TARGET(MyParser parser.y parser.c parser.h)
#   add_executable(Foo main.cpp ${LEMON_MyParser_OUTPUTS})
#  ====================================================================

include(CMakeParseArguments)

if(NOT COMMAND LEMON_TARGET)
  macro(LEMON_TARGET Name Input)

    get_filename_component(IN_FILE_WE ${Input} NAME_WE)
    set(LVAR_PREFIX ${Name}_${IN_FILE_WE})

    if(${ARGC} GREATER 3)
      CMAKE_PARSE_ARGUMENTS(${LVAR_PREFIX} "" "OUT_SRC_FILE;OUT_HDR_FILE;WORKING_DIR;EXTRA_ARGS" "" ${ARGN})
    endif(${ARGC} GREATER 3)

    if (TARGET perplex_stage)
      set(DEPS_TARGET perplex_stage)
    endif (TARGET perplex_stage)

    # Need a working directory
    if("${${LVAR_PREFIX}_WORKING_DIR}" STREQUAL "")
      set(${LVAR_PREFIX}_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/${LVAR_PREFIX}")
    endif("${${LVAR_PREFIX}_WORKING_DIR}" STREQUAL "")
    file(MAKE_DIRECTORY ${${LVAR_PREFIX}_WORKING_DIR})

    # Output source file
    if ("${${LVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")
      set(${LVAR_PREFIX}_OUT_SRC_FILE ${${LVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.c)
    else ("${${LVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")
      get_filename_component(specified_out_dir ${${LVAR_PREFIX}_OUT_SRC_FILE} PATH)
      if(NOT "${specified_out_dir}" STREQUAL "")
	message(FATAL_ERROR "\nFull path specified for OUT_SRC_FILE - should be filename only.\n")
      endif(NOT "${specified_out_dir}" STREQUAL "")
      set(${LVAR_PREFIX}_OUT_SRC_FILE ${${LVAR_PREFIX}_WORKING_DIR}/${${LVAR_PREFIX}_OUT_SRC_FILE})
    endif ("${${LVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "")

    # Output header file
    if ("${${LVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")
      set(${LVAR_PREFIX}_OUT_HDR_FILE ${${LVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.h)
    else ("${${LVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")
      get_filename_component(specified_out_dir ${${LVAR_PREFIX}_OUT_HDR_FILE} PATH)
      if(NOT "${specified_out_dir}" STREQUAL "")
	message(FATAL_ERROR "\nFull path specified for OUT_HDR_FILE - should be filename only.\n")
      endif(NOT "${specified_out_dir}" STREQUAL "")
      set(${LVAR_PREFIX}_OUT_HDR_FILE ${${LVAR_PREFIX}_WORKING_DIR}/${${LVAR_PREFIX}_OUT_HDR_FILE})
    endif ("${${LVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "")

    # input file
    get_filename_component(in_full ${Input} ABSOLUTE)
    if("${in_full}" STREQUAL "${Input}")
      set(lemon_in_file ${Input})
    else("${in_full}" STREQUAL "${Input}")
      set(lemon_in_file "${CMAKE_CURRENT_SOURCE_DIR}/${Input}")
    endif("${in_full}" STREQUAL "${Input}")


    # names of lemon output files will be based on the name of the input file
    set(LEMON_GEN_SOURCE ${${LVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.c)
    set(LEMON_GEN_HEADER ${${LVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.h)
    set(LEMON_GEN_OUT ${${LVAR_PREFIX}_WORKING_DIR}/${IN_FILE_WE}.out)

    # copy input to bin directory and run lemon
    get_filename_component(INPUT_NAME ${Input} NAME)
    add_custom_command(
      OUTPUT ${LEMON_GEN_OUT} ${LEMON_GEN_SOURCE} ${LEMON_GEN_HEADER}
      COMMAND ${CMAKE_COMMAND} -E copy ${lemon_in_file} ${${LVAR_PREFIX}_WORKING_DIR}/${INPUT_NAME}
      COMMAND ${CMAKE_COMMAND} -E touch ${${LVAR_PREFIX}_WORKING_DIR}/${INPUT_NAME}-tmp_cpy.done
      COMMAND ${CMAKE_COMMAND} -E remove ${${LVAR_PREFIX}_WORKING_DIR}/${INPUT_NAME}-tmp_cpy.done
      COMMAND ${LEMON_EXECUTABLE} -T${LEMON_TEMPLATE} ${${LVAR_PREFIX}_WORKING_DIR}/${INPUT_NAME} ${${LVAR_PREFIX}__EXTRA_ARGS}
      DEPENDS ${Input} ${LEMON_EXECUTABLE_TARGET} ${DEPS_TARGET}
      WORKING_DIRECTORY ${${LVAR_PREFIX}_WORKING_DIR}
      COMMENT "[LEMON][${Name}] Building parser with ${LEMON_EXECUTABLE}"
      )

    # rename generated outputs
    if(NOT "${${LVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "${LEMON_GEN_SOURCE}")
      add_custom_command(
	OUTPUT ${${LVAR_PREFIX}_OUT_SRC_FILE} ${${LVAR_PREFIX}_OUT_SRC_FILE}-src_cpy.done
	COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_SOURCE} ${${LVAR_PREFIX}_OUT_SRC_FILE}
	COMMAND ${CMAKE_COMMAND} -E touch ${${LVAR_PREFIX}_OUT_SRC_FILE}-src_cpy.done
	DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_SOURCE} ${DEPS_TARGET}
	)
      set(LEMON_${Name}_OUTPUTS ${${LVAR_PREFIX}_OUT_SRC_FILE} ${LEMON_${Name}_OUTPUTS} ${${LVAR_PREFIX}_OUT_SRC_FILE}-src_cpy.done)
    endif(NOT "${${LVAR_PREFIX}_OUT_SRC_FILE}" STREQUAL "${LEMON_GEN_SOURCE}")
    if(NOT "${${LVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "${LEMON_GEN_HEADER}")
      add_custom_command(
	OUTPUT ${${LVAR_PREFIX}_OUT_HDR_FILE} ${${LVAR_PREFIX}_OUT_HDR_FILE}-hdr_cpy.done
	COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_HEADER} ${${LVAR_PREFIX}_OUT_HDR_FILE}
	COMMAND ${CMAKE_COMMAND} -E touch ${${LVAR_PREFIX}_OUT_HDR_FILE}-hdr_cpy.done
	DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_HEADER} ${DEPS_TARGET}
	)
      set(LEMON_${Name}_OUTPUTS ${${LVAR_PREFIX}_OUT_HDR_FILE} ${LEMON_${Name}_OUTPUTS} ${${LVAR_PREFIX}_OUT_HDR_FILE}-hdr_cpy.done)
    endif(NOT "${${LVAR_PREFIX}_OUT_HDR_FILE}" STREQUAL "${LEMON_GEN_HEADER}")

    set(LEMON_${Name}_OUTPUTS ${LEMON_${Name}_OUTPUTS} ${LEMON_GEN_OUT})

    # make sure we clean up generated output and copied input
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${LEMON_${Name}_OUTPUTS}")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${${LVAR_PREFIX}_WORKING_DIR}/${INPUT_NAME}")

    # macro ran successfully
    set(LEMON_${Name}_DEFINED TRUE)

    set(LEMON_${Name}_SRC ${${LVAR_PREFIX}_OUT_SRC_FILE})
    set(LEMON_${Name}_HDR ${${LVAR_PREFIX}_OUT_HDR_FILE})
    set(LEMON_${Name}_INCLUDE_DIR ${${LVAR_PREFIX}_WORKING_DIR})

  endmacro(LEMON_TARGET)
endif(NOT COMMAND LEMON_TARGET)

#============================================================
# FindLEMON.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
