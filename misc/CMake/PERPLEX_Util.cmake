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
#   LEMON_TARGET(MyParser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
#   PERPLEX_TARGET(MyScanner scanner.re  ${CMAKE_CURRENT_BIANRY_DIR}/scanner.cpp ${CMAKE_CURRENT_BINARY_DIR}/scanner_header.hpp)
#   ADD_PERPLEX_LEMON_DEPENDENCY(MyScanner MyParser)
#
#   include_directories(${CMAKE_CURRENT_BINARY_DIR})
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
# PERPLEX_TARGET (public macro)
#============================================================
MACRO(PERPLEX_TARGET Name Input OutputSrc OutputHeader)
    IF(${ARGC} GREATER 4)
	SET(Template ${ARGV4})
    ELSE(${ARGC} GREATER 4)
	SET(Template ${BRLCAD_SOURCE_DIR}/src/other/perplex/scanner_template.c)
    ENDIF(${ARGC} GREATER 4)

    get_filename_component(OutputName ${OutputSrc} NAME)
    SET(re2c_src "${CMAKE_CURRENT_BINARY_DIR}/${OutputName}.re")

    ADD_CUSTOM_COMMAND(
	OUTPUT ${re2c_src} ${OutputHeader}
	COMMAND ${PERPLEX_EXECUTABLE} -c -o ${re2c_src} -i ${OutputHeader} -t ${Template} ${Input}
	DEPENDS ${Input} ${Template} ${PERPLEX_EXECUTABLE_TARGET} ${RE2C_EXECUTABLE_TARGET}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "[PERPLEX][${Name}] Generating re2c input with ${PERPLEX_EXECUTABLE}"
	)
    ADD_CUSTOM_COMMAND(
	OUTPUT ${OutputSrc}
	COMMAND ${RE2C_EXECUTABLE} -c -o ${OutputSrc} ${re2c_src}
	DEPENDS ${Input} ${re2c_src} ${OutputHeader} ${PERPLEX_EXECUTABLE_TARGET} ${RE2C_EXECUTABLE_TARGET}
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMENT "[RE2C][${Name}] Building scanner with ${RE2C_EXECUTABLE}"
	)
    SET(PERPLEX_${Name}_DEFINED TRUE)
    SET(PERPLEX_${Name}_OUTPUTS ${OutputSrc})
    SET(PERPLEX_${Name}_INPUT ${Input})
ENDMACRO(PERPLEX_TARGET)

#============================================================
# ADD_PERPLEX_LEMON_DEPENDENCY (public macro)
#============================================================
MACRO(ADD_PERPLEX_LEMON_DEPENDENCY PERPLEXTarget LemonTarget)

    IF(NOT PERPLEX_${PERPLEXTarget}_OUTPUTS)
	MESSAGE(SEND_ERROR "PERPLEX target `${PERPLEXTarget}' does not exists.")
    ENDIF()

    IF(NOT LEMON_${LemonTarget}_OUTPUT_HEADER)
	MESSAGE(SEND_ERROR "Lemon target `${LemonTarget}' does not exists.")
    ENDIF()

    SET_SOURCE_FILES_PROPERTIES(${PERPLEX_${PERPLEXTarget}_OUTPUTS}
	PROPERTIES OBJECT_DEPENDS ${LEMON_${LemonTarget}_OUTPUT_HEADER})
ENDMACRO(ADD_PERPLEX_LEMON_DEPENDENCY)

#============================================================
# PERPLEX_Utils.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
