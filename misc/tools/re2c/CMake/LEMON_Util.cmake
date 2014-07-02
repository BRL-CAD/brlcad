# Defines the macro
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
#
#=============================================================================
#
# Originally based off of FindBISON.cmake from Kitware's CMake distribution
#
# Copyright (c) 2010-2012 United States Government as represented by
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
# LEMON_TARGET (public macro)
#============================================================
#
macro(LEMON_TARGET Name LemonInput LemonSource LemonHeader)
  if(NOT ${ARGC} EQUAL 4 AND NOT ${ARGC} EQUAL 5)
    message(SEND_ERROR "Usage")
  else()
    get_filename_component(LemonInputFull ${LemonInput}  ABSOLUTE)
    get_filename_component(LemonSourceFull ${LemonSource} ABSOLUTE)
    get_filename_component(LemonHeaderFull ${LemonHeader} ABSOLUTE)
    
    if(NOT ${LemonInput} STREQUAL ${LemonInputFull})
      set(LEMON_${Name}_INPUT "${CMAKE_CURRENT_BINARY_DIR}/${LemonInput}")
    else(NOT ${LemonInput} STREQUAL ${LemonInputFull})
      set(LEMON_${Name}_INPUT "${LemonInput}")
    endif(NOT ${LemonInput} STREQUAL ${LemonInputFull})

    if(NOT ${LemonSource} STREQUAL ${LemonSourceFull})
      set(LEMON_${Name}_OUTPUT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${LemonSource}")
    else(NOT ${LemonSource} STREQUAL ${LemonSourceFull})
      set(LEMON_${Name}_OUTPUT_SOURCE "${LemonSource}")
    endif(NOT ${LemonSource} STREQUAL ${LemonSourceFull})

    if(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})
      set(LEMON_${Name}_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${LemonHeader}")
    else(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})
      set(LEMON_${Name}_OUTPUT_HEADER "${LemonHeader}")
    endif(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})

    set(LEMON_${Name}_EXTRA_ARGS    "${ARGV4}")

    # get input name minus path
    get_filename_component(INPUT_NAME "${LemonInput}" NAME)
    set(LEMON_BIN_INPUT ${CMAKE_CURRENT_BINARY_DIR}/${INPUT_NAME})

    # names of lemon output files will be based on the name of the input file
    string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.c"   LEMON_GEN_SOURCE "${INPUT_NAME}")
    string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.h"   LEMON_GEN_HEADER "${INPUT_NAME}")
    string(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.out" LEMON_GEN_OUT    "${INPUT_NAME}")

    # copy input to bin directory and run lemon
    add_custom_command(
      OUTPUT ${LEMON_GEN_OUT} ${LEMON_GEN_SOURCE} ${LEMON_GEN_HEADER}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${LemonInput} ${LEMON_BIN_INPUT}
      COMMAND ${LEMON_EXECUTABLE} ${INPUT_NAME} ${LEMON_${Name}_EXTRA_ARGS}
      DEPENDS ${LemonInput} ${LEMON_TEMPLATE} ${LEMON_EXECUTABLE_TARGET}
      COMMENT "[LEMON][${Name}] Building parser with ${LEMON_EXECUTABLE}"
      )

    # rename generated outputs
    if(NOT "${LemonSource}" STREQUAL "${LEMON_GEN_SOURCE}")
      add_custom_command(
	OUTPUT ${LemonSource} 
	COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_SOURCE} ${LemonSource}
	DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_SOURCE} 
	)
      set(LEMON_${Name}_OUTPUTS ${LemonSource} ${LEMON_${Name}_OUTPUTS})
    endif(NOT "${LemonSource}" STREQUAL "${LEMON_GEN_SOURCE}")
    if(NOT "${LemonHeader}" STREQUAL "${LEMON_GEN_HEADER}")
      add_custom_command(
	OUTPUT ${LemonHeader}
	COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_HEADER} ${LemonHeader}
	DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_HEADER} 
	)
      set(LEMON_${Name}_OUTPUTS ${LemonHeader} ${LEMON_${Name}_OUTPUTS})
    endif(NOT "${LemonHeader}" STREQUAL "${LEMON_GEN_HEADER}")

    set(LEMON_${Name}_OUTPUTS ${LEMON_GEN_OUT} ${LemonSource} ${LemonHeader})

    # make sure we clean up generated output and copied input
    if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${LEMON_${Name}_OUTPUTS}") 
    else("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
      set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${LEMON_${Name}_OUTPUTS};${LEMON_BIN_INPUT}") 
    endif("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")

    # macro ran successfully
    set(LEMON_${Name}_DEFINED TRUE)
  endif(NOT ${ARGC} EQUAL 4 AND NOT ${ARGC} EQUAL 5)
endmacro(LEMON_TARGET)
#
#============================================================
# LEMON_Utils.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
