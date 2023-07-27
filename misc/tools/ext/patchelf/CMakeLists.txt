# Minimum required version of CMake
cmake_minimum_required(VERSION 3.18)

# Set CMake project name
project(PATCHELF)

if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT MSVC AND NOT APPLE)
  add_executable(patchelf patchelf.cc)
  set_target_properties(patchelf PROPERTIES FOLDER "Compilation Utilities")
  set(PATCHELF_EXECUTABLE "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${BIN_DIR}/patchelf" CACHE STRING "patchelf location" FORCE)
endif (NOT MSVC AND NOT APPLE)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
