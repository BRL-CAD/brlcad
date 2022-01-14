# Copyright (c) 2010-2022 United States Government as represented by
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
cmake_minimum_required(VERSION 3.12)

# set CMake project name
project(ENV2C)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

find_package(Threads)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)
if(BRLCAD_ENABLE_ADDRESS_SANITIZER AND ${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)
  set(CMAKE_REQUIRED_LINK_OPTIONS "${CMAKE_REQUIRED_LINK_OPTIONS};-lasan")
  check_cxx_compiler_flag(-fsanitize=address SANITIZE_ADDRESS_FLAG)
endif(BRLCAD_ENABLE_ADDRESS_SANITIZER AND ${BRLCAD_OPTIMIZED_BUILD} MATCHES "OFF" AND BRLCAD_FLAGS_DEBUG)

add_executable(env2c env2c.cxx)
target_link_libraries(env2c Threads::Threads)

if (O3_COMPILER_FLAG)
  # If we have the O3 flag, use it
  target_compile_options(env2c PRIVATE "-O3")
endif (O3_COMPILER_FLAG)
if (BRLCAD_ENABLE_ADDRESS_SANITIZER AND SANITIZE_ADDRESS_FLAG)
  # With clang, if we are using the address sanitizer it changes
  # the results of the -lpthread check - using the parent build's
  # find package results, we need to also match its use of this
  # compiler flag or the threading linkage will fail
  target_compile_options(env2c PRIVATE "-fsanitize=address")
  target_link_libraries(env2c -lasan)
endif (BRLCAD_ENABLE_ADDRESS_SANITIZER AND SANITIZE_ADDRESS_FLAG)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

