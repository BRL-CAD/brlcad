# Minimum required version of CMake
cmake_minimum_required(VERSION 2.8)
if(COMMAND CMAKE_POLICY)
  CMAKE_POLICY(SET CMP0003 NEW)
endif(COMMAND CMAKE_POLICY)

# Set CMake project name
project(GDIAM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(NOT DEFINED BIN_DIR)
  set(BIN_DIR bin)
endif(NOT DEFINED BIN_DIR)

if(NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif(NOT DEFINED LIB_DIR)

add_library(libgdiam SHARED gdiam.cpp)
set_target_properties(libgdiam PROPERTIES PREFIX "")
install(TARGETS libgdiam
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if(ENABLE_GDIAM_TESTING)
  # Enable ctest
  ENABLE_TESTING()

  add_executable(gdiam_test gdiam_test.cpp)
  target_link_libraries(gdiam_test libgdiam)

  add_test(unit_cube_test gdiam_test)
endif(ENABLE_GDIAM_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
