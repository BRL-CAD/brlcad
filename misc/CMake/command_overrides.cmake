# By default (as of version 2.8.2) CMake does not provide access to
# global lists of executable and library targets.  This is useful
# in a number of situations related to formulating custom rules and
# target dependency management.  To avoid the necessity of replacing
# add_library and add_executable calls with custom macros, override
# the function names and call the _add_* functions to access the CMake
# functionality previously available under the add_* functions. See
# http://www.cmake.org/pipermail/cmake/2010-September/039388.html

# We also need to provide bookkeeping logic here for the distribution
# verification or "distcheck" routines that will validate the state
# of the source tree against that expected and accounted for in the
# build files.  The global coverage needed for the purpose results in
# the add_library/add_executable command override mechanism having
# to serve two purposes at once; since we only override these functions
# once the logic for both jobs is intertwined below.

# Functions in CMake have local variable scope,
# hence the use of properties to allow access to directory-specific
# and global information scopes.
define_property(GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST BRIEF_DOCS "libtarget list" FULL_DOCS "Library target list")
define_property(GLOBAL PROPERTY CMAKE_EXEC_TARGET_LIST BRIEF_DOCS "exec target list" FULL_DOCS "Executable target list")
define_property(GLOBAL PROPERTY CMAKE_CUSTOM_TARGET_LIST BRIEF_DOCS "custom target list" FULL_DOCS "Custom target list")
define_property(GLOBAL PROPERTY CMAKE_EXTERNAL_TARGET_LIST BRIEF_DOCS "external target list" FULL_DOCS "External target list")
mark_as_advanced(CMAKE_LIBRARY_TARGET_LIST)
mark_as_advanced(CMAKE_EXEC_TARGET_LIST)
mark_as_advanced(CMAKE_CUSTOM_TARGET_LIST)
mark_as_advanced(CMAKE_EXTERNAL_TARGET_LIST)

# Override and wrap add_library.  While we're at it, avoid doubling up
# on the lib prefix for libraries if the target name is lib<target>
function(add_library name)
  _add_library(${name} ${ARGN})
  CMAKEFILES(${ARGN})
  set(add_lib_to_list 1)
  foreach(libarg ${ARGN})
    if("${libarg}" STREQUAL "INTERFACE")
      set(add_lib_to_list 0)
    endif("${libarg}" STREQUAL "INTERFACE")
  endforeach(libarg ${ARGN})
  if (add_lib_to_list)
    set_property(GLOBAL APPEND PROPERTY CMAKE_LIBRARY_TARGET_LIST ${name})
  endif (add_lib_to_list)
endfunction(add_library)

# Override and wrap add_executable
function(add_executable name)
  _add_executable(${name} ${ARGN})
  CMAKEFILES(${ARGN})
  set_property(GLOBAL APPEND PROPERTY CMAKE_EXEC_TARGET_LIST ${name})
endfunction(add_executable)

# Override and wrap add_custom_target
function(add_custom_target name)
  _add_custom_target(${name} ${ARGN})
  set_property(GLOBAL APPEND PROPERTY CMAKE_CUSTOM_TARGET_LIST ${name})
endfunction(add_custom_target)

# Note that at the moment we do not need to override CMake's external
# project mechanisms because CMake does not use them, but if that changes
# in the future an override will need to be added here - probably of the
# ExternalProject_Add functionality.

# Override and wrap configure_file.  In the case of configure_file, we'll
# check that the file is part of the source tree and not itself a
# generated file, but not reject full-path entries since there are quite a
# few of them. This means that, unlike CMAKEFILES's reliance on full vs.
# relative path comparisons, generated files supplied to configure_file
# need to have the GENERATED property set in order to reliably tell which
# files should be added to the build system's lists.  Not
# so critical with not-in-src-dir builds, but makes a big difference
# spotting files to avoid when all generated files have source directory
# prefixes.
function(configure_file file targetfile)
  _configure_file(${file} ${targetfile} ${ARGN})
  # Tag output from configure with the GENERATED tag
  set_source_files_properties(${targetfile} PROPERTIES GENERATED TRUE)
  # If it's a generated file, don't register it
  get_property(IS_GENERATED SOURCE ${file} PROPERTY GENERATED)
  if(NOT IS_GENERATED)
    get_filename_component(item_absolute ${file} ABSOLUTE)
    # If we're not in the source dir, we can do some extra checking.
    if(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
      IS_SUBPATH("${CMAKE_BINARY_DIR}" "${item_absolute}" SUBPATH_TEST)
    else(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
      set(SUBPATH_TEST "0")
    endif(NOT "${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    if("${SUBPATH_TEST}" STREQUAL "0")
      IS_SUBPATH("${CMAKE_SOURCE_DIR}" "${item_absolute}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
	set_property(GLOBAL APPEND PROPERTY CMAKE_IGNORE_FILES "${item_absolute}")
      endif("${SUBPATH_TEST}" STREQUAL "1")
    else("${SUBPATH_TEST}" STREQUAL "0")
      message(WARNING "The generated file ${file} is passed to configure_file but does not have the GENERATED source file property set in CMake.  It is HIGHLY recommended that the generated property be set for this file in \"${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt\" using a command with the following form:\nset_source_files_properties(<file> PROPERTIES GENERATED TRUE)\n(or in \"${CMAKE_SOURCE_DIR}/src/other/CMakeLists.txt\" for third party components with their own build system.)\n")
    endif("${SUBPATH_TEST}" STREQUAL "0")
  endif(NOT IS_GENERATED)
  if(NOT ${targetfile} MATCHES "distclean")
    DISTCLEAN(${targetfile})
  endif(NOT ${targetfile} MATCHES "distclean")
endfunction(configure_file)

# Override and wrap add_subdirectory.
function(add_subdirectory name)
  _add_subdirectory(${name} ${ARGN})
  set_property(GLOBAL APPEND PROPERTY CMAKE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${name}")
  set_property(GLOBAL APPEND PROPERTY CMAKE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${name}/CMakeLists.txt")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/CMakeFiles")
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/cmake_install.cmake")
  foreach(clearpattern ${DISTCLEAN_OUTFILES})
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/${clearpattern}")
  endforeach(clearpattern ${DISTCLEAN_OUTFILES})
endfunction(add_subdirectory)


function(add_test test_name test_prog)
  _add_test(${test_name} ${test_prog} ${ARGN})
  if (NOT "${test_name}" STREQUAL "NAME")
    if (NOT "${test_name}" MATCHES ^regress- AND NOT "${test_prog}" MATCHES ^regress- AND NOT "${test_name}" STREQUAL "benchmark")
      add_dependencies(unit ${test_prog})
      add_dependencies(check ${test_prog})
    endif (NOT "${test_name}" MATCHES ^regress- AND NOT "${test_prog}" MATCHES ^regress- AND NOT "${test_name}" STREQUAL "benchmark")
  else (NOT "${test_name}" STREQUAL "NAME")
    if (NOT "${ARGV1}" MATCHES ^regress- AND NOT "${ARGV3}" MATCHES ^regress- AND NOT "${ARGV1}" MATCHES "benchmark")
      add_dependencies(unit ${ARGV3})
      add_dependencies(check ${ARGV3})
    endif (NOT "${ARGV1}" MATCHES ^regress- AND NOT "${ARGV3}" MATCHES ^regress- AND NOT "${ARGV1}" MATCHES "benchmark")
  endif (NOT "${test_name}" STREQUAL "NAME")
endfunction(add_test)




# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

