project(CVTEST)
cmake_minimum_required(VERSION 3.18)

# Set a non-standard, custom binary output path
set(BIN_DIR deep/dir/bin)

set(LBIN_DIR "${BIN_DIR}")
while (NOT "${LBIN_DIR}" STREQUAL "")
  get_filename_component(LBDIR "${LBIN_DIR}" DIRECTORY)
  set(LBIN_DIR "${LBDIR}")
  if ("${RBIN_DIR}" STREQUAL "")
    set(RBIN_DIR "..")
  else ("${RBIN_DIR}" STREQUAL "")
    set(RBIN_DIR "../${RBIN_DIR}")
  endif ("${RBIN_DIR}" STREQUAL "")
endwhile (NOT "${LBIN_DIR}" STREQUAL "")

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
# not change at runtime.  The script will print some messages and generate
# a file called "test_file.txt" in a build configuration specific directory
configure_file(test.cmake.in test.cmake @ONLY)

# Use the generator expression from dir_info to pass the runtime binary
# directory information to the script.  The script will then use the BIN_DIR
# variable holding the subdirectory, defined by configure_file, to deconstruct
# the runtime binary path and find the runtime root path.
#
# When generating output from the script into the configuration specific
# directory, we can't directly specify the file in OUTPUT.  (See
# https://gitlab.kitware.com/cmake/cmake/-/issues/12877) Instead we generate a
# stamp file, which the associated custom target depends on.  As defined below,
# "make dinfo" will generate test_file.txt in the desired directory but the
# build system will have problems when it comes to installing it.
add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test-${CMAKE_CFG_INTDIR}-stamp"
  COMMAND ${CMAKE_COMMAND} -DEXEC_DIR=$<TARGET_FILE_DIR:dir_info> -P test.cmake
  COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/test-${CMAKE_CFG_INTDIR}-stamp"
  )
add_custom_target(dinfo DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/test-${CMAKE_CFG_INTDIR}-stamp")

# Testing thus far indicates that an install rule may point to a file generated
# by a command such as the one above, but since it cannot be listed as an
# OUTPUT from the build system's perspective due to issue 12877 the generation
# of the file is a side effect of which the build system has no knowledge.
# Consequently, an install rule referencing such a file will not trigger the
# custom target and the install will fail unless the custom target is first
# independently built.
#
# However, since the build system doesn't appear to validate that the file it
# is trying to install is either a) present at configure time or b) a build
# output we may still define an install rule for such a generated file - the
# problem is we have no way to tell the "make install step to trigger the
# script execution if it is missing the file.

# To avoid this we can define a custom target that specifies ALL in its
# properties that depends on the target responsible for generating the targeted
# install file.  The ALL target will be built before install and will thus
# trigger the script execution needed for output generation.  (This is a
# work-around rather than a proper solution with target level awareness, but
# appears to be the best we can currently do.  However, the same ALL target
# should be usable for all custom targets for which we need such a workaround.)
add_custom_target(do_custom ALL DEPENDS dinfo)
install(FILES "$<TARGET_FILE_DIR:dir_info>/${RBIN_DIR}/doc/test_file.txt" DESTINATION doc)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
