# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(PATCHELF)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT MSVC AND NOT APPLE)
  add_executable(patchelf patchelf.cc)
  set_target_properties(patchelf PROPERTIES FOLDER "Compilation Utilities")
endif (NOT MSVC AND NOT APPLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
