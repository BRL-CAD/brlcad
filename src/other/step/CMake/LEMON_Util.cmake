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
# Copyright 2010 United States Government as represented by
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
MACRO(LEMON_TARGET Name LemonInput LemonSource LemonHeader)
	IF(NOT ${ARGC} EQUAL 4 AND NOT ${ARGC} EQUAL 5)
		MESSAGE(SEND_ERROR "Usage")
	ELSE()
		GET_FILENAME_COMPONENT(LemonInputFull ${LemonInput}  ABSOLUTE)
		GET_FILENAME_COMPONENT(LemonSourceFull ${LemonSource} ABSOLUTE)
		GET_FILENAME_COMPONENT(LemonHeaderFull ${LemonHeader} ABSOLUTE)
		
		IF(NOT ${LemonInput} STREQUAL ${LemonInputFull})
			SET(LEMON_${Name}_INPUT "${CMAKE_CURRENT_BINARY_DIR}/${LemonInput}")
		ELSE(NOT ${LemonInput} STREQUAL ${LemonInputFull})
			SET(LEMON_${Name}_INPUT "${LemonInput}")
		ENDIF(NOT ${LemonInput} STREQUAL ${LemonInputFull})

		IF(NOT ${LemonSource} STREQUAL ${LemonSourceFull})
			SET(LEMON_${Name}_OUTPUT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${LemonSource}")
		ELSE(NOT ${LemonSource} STREQUAL ${LemonSourceFull})
			SET(LEMON_${Name}_OUTPUT_SOURCE "${LemonSource}")
		ENDIF(NOT ${LemonSource} STREQUAL ${LemonSourceFull})

		IF(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})
			SET(LEMON_${Name}_OUTPUT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${LemonHeader}")
		ELSE(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})
			SET(LEMON_${Name}_OUTPUT_HEADER "${LemonHeader}")
		ENDIF(NOT ${LemonHeader} STREQUAL ${LemonHeaderFull})

		SET(LEMON_${Name}_EXTRA_ARGS    "${ARGV4}")

		# get input name minus path
		GET_FILENAME_COMPONENT(INPUT_NAME "${LemonInput}" NAME)
		SET(LEMON_BIN_INPUT ${CMAKE_CURRENT_BINARY_DIR}/${INPUT_NAME})

		# names of lemon output files will be based on the name of the input file
		STRING(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.c"   LEMON_GEN_SOURCE "${INPUT_NAME}")
		STRING(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.h"   LEMON_GEN_HEADER "${INPUT_NAME}")
		STRING(REGEX REPLACE "^(.*)(\\.[^.]*)$" "\\1.out" LEMON_GEN_OUT    "${INPUT_NAME}")

		# copy input to bin directory and run lemon
		ADD_CUSTOM_COMMAND(
		    OUTPUT ${LEMON_GEN_OUT} ${LEMON_GEN_SOURCE} ${LEMON_GEN_HEADER}
		    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${LemonInput} ${LEMON_BIN_INPUT}
		    COMMAND ${LEMON_EXECUTABLE} ${INPUT_NAME} ${LEMON_${Name}_EXTRA_ARGS}
		    DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET}
		    COMMENT "[LEMON][${Name}] Building parser with ${LEMON_EXECUTABLE}"
		    )

		# rename generated outputs
		IF(NOT "${LemonSource}" STREQUAL "${LEMON_GEN_SOURCE}")
		    ADD_CUSTOM_COMMAND(
			OUTPUT ${LemonSource} 
			COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_SOURCE} ${LemonSource}
			DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_SOURCE} 
			)
		    SET(LEMON_${Name}_OUTPUTS ${LemonSource} ${LEMON_${Name}_OUTPUTS})
		ENDIF(NOT "${LemonSource}" STREQUAL "${LEMON_GEN_SOURCE}")
		IF(NOT "${LemonHeader}" STREQUAL "${LEMON_GEN_HEADER}")
		    ADD_CUSTOM_COMMAND(
			OUTPUT ${LemonHeader}
			COMMAND ${CMAKE_COMMAND} -E copy ${LEMON_GEN_HEADER} ${LemonHeader}
			DEPENDS ${LemonInput} ${LEMON_EXECUTABLE_TARGET} ${LEMON_GEN_HEADER} 
			)
		    SET(LEMON_${Name}_OUTPUTS ${LemonHeader} ${LEMON_${Name}_OUTPUTS})
		ENDIF(NOT "${LemonHeader}" STREQUAL "${LEMON_GEN_HEADER}")

		SET(LEMON_${Name}_OUTPUTS ${LEMON_GEN_OUT} ${LemonSource} ${LemonHeader})

		# macro ran successfully
		SET(LEMON_${Name}_DEFINED TRUE)
	ENDIF(NOT ${ARGC} EQUAL 4 AND NOT ${ARGC} EQUAL 5)
ENDMACRO(LEMON_TARGET)
#
#============================================================
# LEMON_Utils.cmake ends here

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
