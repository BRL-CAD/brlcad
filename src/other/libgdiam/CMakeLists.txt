# Minimum required version of CMake
cmake_minimum_required(VERSION 3.1.3)

# Set CMake project name
project(GDIAM)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if (NOT BIN_DIR)
  set(BIN_DIR bin)
endif (NOT BIN_DIR)

if (NOT INCLUDE_DIR)
  set(INCLUDE_DIR include)
endif (NOT INCLUDE_DIR)

if (NOT DEFINED LIB_DIR)
  set(LIB_DIR lib)
endif (NOT DEFINED LIB_DIR)

add_definitions(-DGDIAM_QUIET)

add_library(gdiam SHARED gdiam.cpp)
install(TARGETS gdiam
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

if (MSVC)
   set_property(TARGET gdiam APPEND PROPERTY COMPILE_DEFINITIONS "GDIAM_DLL_EXPORTS")
endif (MSVC)

if (BUILD_STATIC_LIBS)
  add_library(gdiam-static STATIC gdiam.cpp)
  if (CMAKE_CL_64)
    set_target_properties(gdiam-static PROPERTIES STATIC_LIBRARY_FLAGS "/machine:x64")
  endif (CMAKE_CL_64)
  if (MSVC)
    # msvc does not append 'lib' - do it here to have consistent name
    set_target_properties(gdiam-static PROPERTIES PREFIX "lib")
  else (MSVC)
    set_target_properties(gdiam-static PROPERTIES OUTPUT_NAME "gdiam")
  endif (MSVC)
  install(TARGETS gdiam-static
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif (BUILD_STATIC_LIBS)

if (NOT SKIP_INSTALL_HEADERS)
install(FILES gdiam.hpp DESTINATION ${INCLUDE_DIR})
endif (NOT SKIP_INSTALL_HEADERS)

if (ENABLE_GDIAM_TESTING)
  # Enable ctest
  ENABLE_TESTING()

  add_executable(gdiam_test gdiam_test.cpp)
  target_link_libraries(gdiam_test gdiam)

  add_test(unit_cube_test gdiam_test)
endif (ENABLE_GDIAM_TESTING)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
