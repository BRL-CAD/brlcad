# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

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
set_target_properties(astyle PROPERTIES FOLDER "Compilation Utilities")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
