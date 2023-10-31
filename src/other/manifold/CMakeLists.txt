# Copyright 2020 The Manifold Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.18)
project(manifold LANGUAGES CXX)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MANIFOLD_PAR "NONE" CACHE STRING "Parallel backend, either \"TBB\" or \"NONE\"")
set(MANIFOLD_FLAGS "" CACHE STRING "Manifold compiler flags")

if (MSVC)
  set(MANIFOLD_FLAGS ${MANIFOLD_FLAGS} /DNOMINMAX)
else()
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WARNING_FLAGS -Werror -Wall -Wno-sign-compare -Wno-unused -Wno-array-bounds -Wno-stringop-overflow)
  else()
    set(WARNING_FLAGS -Werror -Wall -Wno-sign-compare -Wno-unused)
  endif()
  set(MANIFOLD_FLAGS ${MANIFOLD_FLAGS} ${WARNING_FLAGS} -O3)
endif()

if (NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif (NOT DEFINED BIN_DIR)

if (NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

if (NOT DEFINED INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT DEFINED INCLUDE_DIR)


add_subdirectory(src)
add_subdirectory(include)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
