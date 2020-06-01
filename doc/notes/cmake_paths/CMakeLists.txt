project(CVTEST)
cmake_minimum_required(VERSION 3.17)

# Set a non-standard, custom binary output path
set(BIN_DIR deep/dir/bin)

# Use the custom binary output directory to set the standard
# CMake variables controlling binary output locations
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CVTEST_BINARY_DIR}/${BIN_DIR})
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    if(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
      set(CFG_ROOT ${CVTEST_BINARY_DIR}/${CFG_TYPE})
    else(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
      set(CFG_ROOT ${CVTEST_BINARY_DIR})
    endif(NOT "${CMAKE_CFG_INTDIR}" STREQUAL ".")
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER}" ${CFG_ROOT}/${BIN_DIR})
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif(NOT CMAKE_CONFIGURATION_TYPES)

# Define a target to provide generator properties
add_executable(dir_info dir_info.c)

# Use configure_file to set up a script and define variables that will
# not change at runtime.
configure_file(test.cmake.in test.cmake @ONLY)

# Use the generator expression from dir_info to pass the runtime binary
# directory information to the script.  The script will then use the BIN_DIR
# variable holding the subdirectory, defined by configure_file, to deconstruct
# the runtime binary path and find the runtime root path.
add_custom_target(dinfo
  COMMAND ${CMAKE_COMMAND} -DEXEC_DIR=$<TARGET_FILE_DIR:dir_info> -P test.cmake
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
