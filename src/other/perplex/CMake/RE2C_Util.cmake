#                  F I N D R E 2 C . C M A K E
#
# Provides a macro to generate custom build rules:

# RE2C_TARGET(Name RE2CInput RE2COutput [COMPILE_FLAGS <string>])
# which creates a custom command  to generate the <RE2COutput> file from
# the <RE2CInput> file.  If  COMPILE_FLAGS option is specified, the next
# parameter is added to the re2c  command line. Name is an alias used to
# get  details of  this custom  command.  

# This module also defines a macro:
#  ADD_RE2C_LEMON_DEPENDENCY(RE2CTarget LemonTarget)
# which  adds the  required dependency  between a  scanner and  a parser
# where  <RE2CTarget>  and <LemonTarget>  are  the  first parameters  of
# respectively RE2C_TARGET and LEMON_TARGET macros.
#
#  ====================================================================
#  Example:
#
#   find_package(LEMON)
#   find_package(RE2C)
#
#   LEMON_TARGET(MyParser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
#   RE2C_TARGET(MyScanner scanner.re  ${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp)
#   ADD_RE2C_LEMON_DEPENDENCY(MyScanner MyParser)
#
#   include_directories(${CMAKE_CURRENT_BINARY_DIR})
#   add_executable(Foo
#      Foo.cc
#      ${LEMON_MyParser_OUTPUTS}
#      ${RE2C_MyScanner_OUTPUTS}
#   )
#  ====================================================================
#
#=============================================================================
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
# RE2C_TARGET (public macro)
#============================================================
#
macro(RE2C_TARGET Name Input Output)
  set(RE2C_TARGET_usage "RE2C_TARGET(<Name> <Input> <Output> [COMPILE_FLAGS <string>]")
  if(${ARGC} GREATER 3)
    if(${ARGC} EQUAL 5)
      if("${ARGV3}" STREQUAL "COMPILE_FLAGS")
	set(RE2C_EXECUTABLE_opts  "${ARGV4}")
	SEPARATE_ARGUMENTS(RE2C_EXECUTABLE_opts)
      else()
	message(SEND_ERROR ${RE2C_TARGET_usage})
      endif()
    else()
      message(SEND_ERROR ${RE2C_TARGET_usage})
    endif()
  endif()

  add_custom_command(OUTPUT ${Output}
    COMMAND ${RE2C_EXECUTABLE}
    ARGS ${RE2C_EXECUTABLE_opts} -o${Output} ${Input}
    DEPENDS ${Input} ${RE2C_EXECUTABLE_TARGET}
    COMMENT "[RE2C][${Name}] Building scanner with ${RE2C_EXECUTABLE}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  set(RE2C_${Name}_DEFINED TRUE)
  set(RE2C_${Name}_OUTPUTS ${Output})
  set(RE2C_${Name}_INPUT ${Input})
  set(RE2C_${Name}_COMPILE_FLAGS ${RE2C_EXECUTABLE_opts})
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${Output}")
endmacro(RE2C_TARGET)
#============================================================

#============================================================
# ADD_RE2C_LEMON_DEPENDENCY (public macro)
#============================================================
#
macro(ADD_RE2C_LEMON_DEPENDENCY RE2CTarget LemonTarget)

  if(NOT RE2C_${RE2CTarget}_OUTPUTS)
    message(SEND_ERROR "RE2C target `${RE2CTarget}' does not exists.")
  endif()

  if(NOT LEMON_${LemonTarget}_OUTPUT_HEADER)
    message(SEND_ERROR "Lemon target `${LemonTarget}' does not exists.")
  endif()

  set_source_files_properties(${RE2C_${RE2CTarget}_OUTPUTS}
    PROPERTIES OBJECT_DEPENDS ${LEMON_${LemonTarget}_OUTPUT_HEADER})
endmacro(ADD_RE2C_LEMON_DEPENDENCY)
#============================================================

# RE2C_Util.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
