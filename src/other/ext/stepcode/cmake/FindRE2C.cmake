# The module defines the following variables:
#  RE2C_EXECUTABLE - the path to the re2c executable
#
#=============================================================================

find_program(RE2C_EXECUTABLE re2c DOC "path to the re2c executable")
mark_as_advanced(RE2C_EXECUTABLE)

if(RE2C_EXECUTABLE)

  execute_process(COMMAND ${RE2C_EXECUTABLE} -v
    OUTPUT_VARIABLE RE2C_version_output
    ERROR_VARIABLE  RE2C_version_error
    RESULT_VARIABLE RE2C_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  if(NOT ${RE2C_version_result} EQUAL 0)
    message(SEND_ERROR
      "Command \"${RE2C_EXECUTABLE} -v\" failed with output:\n${RE2C_version_output}\n${RE2C_version_error}")
  else()
    string(REGEX REPLACE "^re2c ([0-9]+[^ ]*)( .*)?$" "\\1" RE2C_VERSION "${RE2C_version_output}")
  endif()

  # Provide a macro to generate custom build rules:

  # RE2C_TARGET(Name RE2CInput RE2COutput [COMPILE_FLAGS <string>])
  # which creates a custom command  to generate the <RE2COutput> file from
  # the <RE2CInput> file.  If COMPILE_FLAGS option is specified, the next
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
  #   LEMON_TARGET(MyParser parser.y "${CMAKE_CURRENT_BINARY_DIR}/parser.cpp")
  #   RE2C_TARGET(MyScanner scanner.re  "${CMAKE_CURRENT_BINARY_DIR}/scanner.cpp")
  #   ADD_RE2C_LEMON_DEPENDENCY(MyScanner MyParser)
  #
  #   include_directories("${CMAKE_CURRENT_BINARY_DIR}")
  #   add_executable(Foo
  #      Foo.cc
  #      ${LEMON_MyParser_OUTPUTS}
  #      ${RE2C_MyScanner_OUTPUTS}
  #   )
  #  ====================================================================
  #
  #=============================================================================
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
  # RE2C_TARGET (public macro)
  #============================================================
  #
  if(NOT COMMAND RE2C_TARGET)
    function(RE2C_TARGET Name RE2CInput RE2COutput)
      set(_re2c_args COMPILE_FLAGS DEFINES_FILE)
      cmake_parse_arguments(RE2C_TARGET_ARG "" "${_re2c_args}" "" ${ARGN})

      if(NOT "${RE2C_TARGET_ARG_UNPARSED_ARGUMENTS}" STREQUAL "")
	message(SEND_ERROR "RE2C_TARGET(<Name> <RE2CInput> <RE2COutput>
	[COMPILE_FLAGS <string>]
	[DEFINES_FILE <string>])")
      else()
	if(NOT "${RE2C_TARGET_ARG_COMPILE_FLAGS}" STREQUAL "")
	  set(RE2C_EXECUTABLE_opts "${RE2C_TARGET_ARG_COMPILE_FLAGS}")
	  separate_arguments(RE2C_EXECUTABLE_opts)
	else()
	  set(RE2C_EXECUTABLE_opts "")
	endif()

	if(NOT "${RE2C_TARGET_ARG_DEFINES_FILE}" STREQUAL "")
	  list(APPEND RE2C_EXECUTABLE_opts -t ${RE2C_TARGET_ARG_DEFINES_FILE})
	endif()

	add_custom_command(
	  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${RE2COutput}
	  COMMAND ${RE2C_EXECUTABLE}
	  ARGS ${RE2C_EXECUTABLE_opts} -o ${RE2COutput} ${CMAKE_CURRENT_SOURCE_DIR}/${RE2CInput}
	  DEPENDS ${RE2CInput}
	  COMMENT "[RE2C][${Name}] Building scanner with ${RE2C_EXECUTABLE}"
	  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	  )

	# TODO: check, for generated sources headers should be included in outputs
	set(RE2C_${Name}_DEFINED TRUE PARENT_SCOPE)
	set(RE2C_${Name}_OUTPUTS ${RE2COutput} PARENT_SCOPE)
	if("${RE2C_TARGET_ARG_DEFINES_FILE}" STREQUAL "")
	  set(RE2C_${Name}_OUTPUT_HEADER "" PARENT_SCOPE)
	else()
	  set(RE2C_${Name}_OUTPUT_HEADER "${RE2C_TARGET_ARG_DEFINES_FILE}" PARENT_SCOPE)
	endif()
	set(RE2C_${Name}_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
      endif()

    endfunction(RE2C_TARGET)
  endif(NOT COMMAND RE2C_TARGET)
  #============================================================

  #============================================================
  # ADD_RE2C_LEMON_DEPENDENCY (public macro)
  #============================================================
  #
  if(NOT COMMAND ADD_RE2C_LEMON_DEPENDENCY)
    function(ADD_RE2C_LEMON_DEPENDENCY RE2CTarget LemonTarget)

      if(NOT RE2C_${RE2CTarget}_OUTPUTS)
	message(SEND_ERROR "RE2C target `${RE2CTarget}' does not exists.")
      endif()

      if(NOT LEMON_${LemonTarget}_OUTPUT_HEADER)
	message(SEND_ERROR "Lemon target `${LemonTarget}' does not exists.")
      endif()

      set_source_files_properties(${RE2C_${RE2CTarget}_OUTPUTS}
	PROPERTIES OBJECT_DEPENDS ${LEMON_${LemonTarget}_OUTPUT_HEADER})
    endfunction(ADD_RE2C_LEMON_DEPENDENCY)
  endif(NOT COMMAND ADD_RE2C_LEMON_DEPENDENCY)
  #============================================================

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RE2C REQUIRED_VARS RE2C_EXECUTABLE
  VERSION_VAR RE2C_VERSION)

# RE2C_Util.cmake ends here

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
