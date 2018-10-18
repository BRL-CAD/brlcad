# - Check which parts of the C++11 standard the compiler supports
#
# Copyright 2011,2012 Rolf Eike Beer <eike@sf-mail.de>
# Copyright 2012 Andreas Weis
# Copyright 2014 Ben Morgan <bmorgan.warwick@gmail.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the copyright holders nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#
# Each feature may have up to 3 checks, every one of them in it's own file
# FEATURE.cpp              - example that must build and return 0 when run
# FEATURE_fail.cpp         - example that must build, but may not return 0 when run
# FEATURE_fail_compile.cpp - example that must fail compilation
#
# The first one is mandatory, the latter 2 are optional and do not depend on
# each other (i.e. only one may be present).
#

if(NOT CMAKE_CXX_COMPILER_LOADED)
  message(FATAL_ERROR "CheckCXX11Features modules only works if language CXX is enabled")
endif()

#-----------------------------------------------------------------------
# function cxx11_check_feature(<name> <result>)
#
function(cxx11_check_feature FEATURE_NAME RESULT_VAR)
  if(NOT DEFINED ${RESULT_VAR})
    set(_bindir "${CMAKE_CURRENT_BINARY_DIR}/CheckCXX11Features/cxx11_${FEATURE_NAME}")

    set(_SRCFILE_BASE ${CMAKE_SOURCE_DIR}/misc/CMake/CheckCXX11Features/cxx11-test-${FEATURE_NAME})
    set(_LOG_NAME "\"${FEATURE_NAME}\"")
    message(STATUS "Checking C++11 support for ${_LOG_NAME}")

    set(_SRCFILE "${_SRCFILE_BASE}.cpp")
    set(_SRCFILE_FAIL "${_SRCFILE_BASE}_fail.cpp")
    set(_SRCFILE_FAIL_COMPILE "${_SRCFILE_BASE}_fail_compile.cpp")

    if(CMAKE_CROSSCOMPILING)
      try_compile(${RESULT_VAR} "${_bindir}" "${_SRCFILE}"
        COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
      if(${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
        try_compile(${RESULT_VAR} "${_bindir}_fail" "${_SRCFILE_FAIL}"
          COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
      endif()
    else()
      try_run(_RUN_RESULT_VAR _COMPILE_RESULT_VAR
        "${_bindir}" "${_SRCFILE}"
        #CMAKE_FLAGS "-DCMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY='libc++'"
        COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
      if(_COMPILE_RESULT_VAR AND NOT _RUN_RESULT_VAR)
        set(${RESULT_VAR} TRUE)
      else()
        set(${RESULT_VAR} FALSE)
      endif()

      if(${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL})
        try_run(_RUN_RESULT_VAR _COMPILE_RESULT_VAR
          "${_bindir}_fail" "${_SRCFILE_FAIL}"
          #CMAKE_FLAGS "-DCMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY='libc++'"
          COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
        if(_COMPILE_RESULT_VAR AND _RUN_RESULT_VAR)
          set(${RESULT_VAR} TRUE)
        else()
          set(${RESULT_VAR} FALSE)
        endif()
      endif()
    endif()

    if(${RESULT_VAR} AND EXISTS ${_SRCFILE_FAIL_COMPILE})
      try_compile(_TMP_RESULT "${_bindir}_fail_compile" "${_SRCFILE_FAIL_COMPILE}"
        #CMAKE_FLAGS "-DCMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY='libc++'"
        COMPILE_DEFINITIONS "${CXX11_COMPILER_FLAGS}")
      if(_TMP_RESULT)
        set(${RESULT_VAR} FALSE)
      else()
        set(${RESULT_VAR} TRUE)
      endif()
    endif()

    if(${RESULT_VAR})
      message(STATUS "Checking C++11 support for ${_LOG_NAME}: works")
    else()
      message(FATAL_ERROR "\nChecking C++11 support for ${_LOG_NAME}: not supported\n\n")
    endif()

    set(${RESULT_VAR} ${${RESULT_VAR}} CACHE INTERNAL "C++11 support for ${_LOG_NAME}")
  endif()
endfunction()

