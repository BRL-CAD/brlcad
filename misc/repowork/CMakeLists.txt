# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# set CMake project name
project(repowork)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-g)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

set(repowork_srcs
  blob.cpp
  commit.cpp
  misc_cmds.cpp
  repowork.cpp
  reset.cpp
  tag.cpp
  )

add_executable(repowork ${repowork_srcs})

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-O3 O3_COMPILER_FLAG)
if (O3_COMPILER_FLAG)
  target_compile_options(repowork PRIVATE "-O3")
endif (O3_COMPILER_FLAG)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


