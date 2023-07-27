# Copyright (c) 2010-2023 United States Government as represented by
# the U.S. Army Research Laboratory.
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
# ***                   env2c CMakeLists.txt                      ***
# *******************************************************************

# Minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# set CMake project name
project(INACTVHDRS)



include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

# We use the std::filesystem from C++17 in this utility, so only try to build
# it if we know we have a compiler that supports it - BRL-CAD as a whole does
# not yet require C++17, so until we do this target may need to be disabled
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)

if(COMPILER_SUPPORTS_CXX17)
  add_executable(inactvhdrs inactvhdrs.cpp)
  target_compile_features(inactvhdrs PRIVATE cxx_std_17)
  check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)
  if (O3_COMPILER_FLAG)
    # If we have the O3 flag, use it
    target_compile_options(inactvhdrs PRIVATE "-O3")
  endif (O3_COMPILER_FLAG)
endif(COMPILER_SUPPORTS_CXX17)

if(COMMAND CMAKEFILES)
  CMAKEFILES(
    CMakeLists.txt
    inactvhdrs.cpp
    )
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

