# TODO:  3.7 has a new property that may help eliminate the need to
# do the wrappers below (wrapping CMake functions in this manner
# is officially discouraged, it causes problems..)
#
# https://cmake.org/cmake/help/latest/prop_dir/BUILDSYSTEM_TARGETS.html#prop_dir:BUILDSYSTEM_TARGETS
#
# Between that and a couple other notes below, we may be able to
# eliminate most of the wrappers now.  configure_file we may need
# to turn into our own function, but if we're managing external
# build systems with ExternalProject_Add now that becomes more
# practical.  Worth doing to get us grounded on officially
# supported CMake features.


#---------------------------------------------------------------------
# By default (as of version 2.8.2) CMake does not provide access to
# global lists of executable and library targets.  This is useful
# in a number of situations related to formulating custom rules and
# target dependency management.  To avoid the necessity of replacing
# add_library and add_executable calls with custom macros, override
# the function names and call the _add_* functions to access the CMake
# functionality previously available under the add_* functions. See
# http://www.cmake.org/pipermail/cmake/2010-September/039388.html

# To allow a hypothetical parent build to disable this mechanism and
# replace it, we wrap the whole show in an IF conditional.  To avoid
# the distcheck setup, the parent file should define the variable
# BRLCAD_IS_SUBBUILD to ON.  Note that this also disables the
# liblib prefix check in add_library, making that the responsibility
# of the parent build as well, and disables the mechanism for ensuring
# that the timing code runs at the correct points during the build.

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
    if(${name} MATCHES "^lib*")
      set_target_properties(${name} PROPERTIES PREFIX "")
    endif(${name} MATCHES "^lib*")

    # TODO - the mechanism below should eventually be replaced by a proper
    # feature in CMake, possibly using BUILDSYSTEM_TARGETS
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

    # TODO - the mechanism below should eventually be replaced by a proper
    # feature in CMake, possibly using BUILDSYSTEM_TARGETS
    set_property(GLOBAL APPEND PROPERTY CMAKE_EXEC_TARGET_LIST ${name})
  endfunction(add_executable)

  # Override and wrap add_custom_target
  function(add_custom_target name)
    _add_custom_target(${name} ${ARGN})

    # TODO - the mechanism below should eventually be replaced by a proper
    # feature in CMake, possibly using BUILDSYSTEM_TARGETS
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
    if (NOT SUPPRESS_GENERATED_TAG)
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
	  message(WARNING "The generated file ${file} is passed to configure_file but does not have the GENERATED source file property set in CMake.  It is HIGHLY recommended that the GENERATED property be set for this file in \"${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt\" using a command with the following form:\nset_source_files_properties(<file> PROPERTIES GENERATED TRUE)\n(or in \"${CMAKE_SOURCE_DIR}/src/other/CMakeLists.txt\" for third party components with their own build system.)\n")
	endif("${SUBPATH_TEST}" STREQUAL "0")
      endif(NOT IS_GENERATED)
      if(NOT ${targetfile} MATCHES "distclean")
	DISTCLEAN(${targetfile})
      endif(NOT ${targetfile} MATCHES "distclean")
    endif (NOT SUPPRESS_GENERATED_TAG)
  endfunction(configure_file)

  # Override and wrap add_subdirectory.
  function(add_subdirectory name)
    _add_subdirectory(${name} ${ARGN})

    # TODO - try to come up with some good way to do this bookkeeping without
    # having to override add_subdirectory...
    #
    # see https://cmake.org/pipermail/cmake-developers/2015-July/025730.html
    # for some work related to this...  the SOURCE_DIR and SOURCES properties
    # went in in CMake 3.4.  Once we can require that as a minimum version, we
    # can probably revisit at least part of this by getting a list of source
    # directories for all targets at the end of the build and setting
    # properties then.
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/CMakeFiles")
    DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/cmake_install.cmake")
    foreach(clearpattern ${DISTCLEAN_OUTFILES})
      DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/${name}/${clearpattern}")
    endforeach(clearpattern ${DISTCLEAN_OUTFILES})
  endfunction(add_subdirectory)


