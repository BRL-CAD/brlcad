# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

# Set CMake project name
project(ASTYLE)

set(ASTYLE_SRCS
  astyle_main.cpp
  ASBeautifier.cpp
  ASFormatter.cpp
  ASEnhancer.cpp
  ASLocalizer.cpp
  ASResource.cpp
  )

add_executable(astyle ${ASTYLE_SRCS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
