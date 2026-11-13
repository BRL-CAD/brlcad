cmake_minimum_required(VERSION 3.22)
project(brlcad_rpath_validation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
include("${RPATH_MODULE}")
std_build_rpath()

# Both versions deliberately share a SONAME, as two installations would.
foreach(version IN ITEMS legacy current)
  add_library(${version} SHARED version.cpp)
  set_target_properties(${version} PROPERTIES OUTPUT_NAME rpath_fixture
    SOVERSION 1 LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${version}")
endforeach()
target_compile_definitions(legacy PRIVATE FIXTURE_VERSION=1)
target_compile_definitions(current PRIVATE FIXTURE_VERSION=2)
brlcad_install_rpath(current "${LIB_DIR}")
install(TARGETS current LIBRARY DESTINATION "${LIB_DIR}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/legacy.cmake"
  CONTENT "set(legacy_library [==[$<TARGET_FILE:legacy>]==])
set(legacy_soname [==[$<TARGET_SONAME_FILE_NAME:legacy>]==])
")

foreach(name IN ITEMS driver control)
  add_executable(${name} driver.cpp)
  target_link_libraries(${name} PRIVATE current)
  install(TARGETS ${name} RUNTIME DESTINATION "${BIN_DIR}")
endforeach()

# The host must not load current itself: that would mask a module RPATH bug.
add_executable(host host.cpp)
target_link_libraries(host PRIVATE ${CMAKE_DL_LIBS})
install(TARGETS host RUNTIME DESTINATION "${BIN_DIR}")
foreach(name IN ITEMS module module_control)
  add_library(${name} MODULE module.cpp)
  target_link_libraries(${name} PRIVATE current)
  brlcad_install_rpath(${name} "${LIBEXEC_DIR}/ged")
  install(TARGETS ${name} LIBRARY DESTINATION "${LIBEXEC_DIR}/ged")
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${name}.cmake"
    CONTENT "set(${name}_file [==[$<TARGET_FILE_NAME:${name}>]==])
")
endforeach()

# These controls must select the conflicting version; otherwise the test did
# not establish a collision and cannot prove that relative lookup fixed it.
set_target_properties(control PROPERTIES
  INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR};${CMAKE_INSTALL_RPATH}")
relative_rpath(module_rpath SUFFIX "${LIBEXEC_DIR}/ged")
set_target_properties(module_control PROPERTIES
  INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR};${module_rpath}")
