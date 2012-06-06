#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

# When defining targets, we need to know if we have a no-error flag
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)

# Take a target definition and find out what definitions its libraries
# are using
macro(GET_TARGET_DEFINES targetname target_libs)
# Take care of compile flags and definitions
  foreach(libitem ${target_libs})
    list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
    if(NOT ${FOUNDIT} STREQUAL "-1")
      get_property(${libitem}_DEFINES GLOBAL PROPERTY ${libitem}_DEFINES)
      list(APPEND ${targetname}_DEFINES ${${libitem}_DEFINES})
      if(${targetname}_DEFINES)
	list(REMOVE_DUPLICATES ${targetname}_DEFINES)
      endif(${targetname}_DEFINES)
    endif(NOT ${FOUNDIT} STREQUAL "-1")
  endforeach(libitem ${target_libs})
endmacro(GET_TARGET_DEFINES)

# Take a target definition and find out what compilation flags its libraries
# are using
macro(GET_TARGET_FLAGS targetname target_libs)
  set(FLAG_LANGUAGES C CXX)
  foreach(lang ${FLAG_LANGUAGES})
    get_property(${targetname}_${lang}_FLAGS GLOBAL PROPERTY ${targetname}_${lang}_FLAGS)
    foreach(libitem ${target_libs})
      list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
      if(NOT ${FOUNDIT} STREQUAL "-1")
	get_property(${libitem}_${lang}_FLAGS GLOBAL PROPERTY ${libitem}_${lang}_FLAGS)
	list(APPEND ${targetname}_${lang}_FLAGS ${${libitem}_${lang}_FLAGS})
      endif(NOT ${FOUNDIT} STREQUAL "-1")
    endforeach(libitem ${target_libs})
    if(${targetname}_${lang}_FLAGS)
      list(REMOVE_DUPLICATES ${targetname}_${lang}_FLAGS)
    endif(${targetname}_${lang}_FLAGS)
    set_property(GLOBAL PROPERTY ${targetname}_${lang}_FLAGS "${${targetname}_${lang}_FLAGS}")
    mark_as_advanced(${targetname}_${lang}_FLAGS)
  endforeach(lang ${FLAG_LANGUAGES})
endmacro(GET_TARGET_FLAGS)

# When a build target source file list contains files that are NOT all 
# one lanaguage, we need to apply flags on a per-file basis
macro(FLAGS_TO_FILES srcslist targetname)
  foreach(srcfile ${srcslist})
    get_property(file_language SOURCE ${srcfile} PROPERTY LANGUAGE)
    if(NOT file_language)
      get_filename_component(srcfile_ext ${srcfile} EXT)
      if(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc")
	set(file_language CXX)
      endif(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc")
      if(${srcfile_ext} STREQUAL ".c")
	set(file_language C)
      endif(${srcfile_ext} STREQUAL ".c")
    endif(NOT file_language)
    if(file_language)
      foreach(lib_flag ${${targetname}_${file_language}_FLAGS})
	set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
      endforeach(lib_flag ${${targetname}_${file_language}_FLAGS})
    endif(file_language)
  endforeach(srcfile ${srcslist})
endmacro(FLAGS_TO_FILES)

# Handle C++ NOSTRICT settings
macro(CXX_NOSTRICT cxx_srcslist args)
  if(NOERROR_FLAG)
    foreach(extraarg ${args})
      if(${extraarg} MATCHES "NOSTRICTCXX" AND BRLCAD_ENABLE_STRICT)
	foreach(srcfile ${cxx_srcslist})
	  get_filename_component(srcfile_ext ${srcfile} EXT)
	  if(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc")
	    get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	    set_source_files_properties(${srcfile} COMPILE_FLAGS "-Wno-error ${previous_flags}")
	  endif()
	endforeach(srcfile ${cxx_srcslist})
      endif(${extraarg} MATCHES "NOSTRICTCXX" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${args})
  endif(NOERROR_FLAG)
endmacro(CXX_NOSTRICT cxx_srcslist)


#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
macro(BRLCAD_ADDEXEC execname srcslist libslist)

  # Call standard CMake commands
  add_executable(${execname} ${srcslist})
  target_link_libraries(${execname} ${libslist})

  # Make sure we don't have a non-installed exec target.  If it is to
  # be installed, call the install function
  set(EXEC_INSTALL 1)
  foreach(extraarg ${ARGN})
    if(${extraarg} MATCHES "NO_INSTALL" AND BRLCAD_ENABLE_STRICT)
      set(EXEC_INSTALL 0)
    endif(${extraarg} MATCHES "NO_INSTALL" AND BRLCAD_ENABLE_STRICT)
  endforeach(extraarg ${ARGN})
  if(EXEC_INSTALL)
    install(TARGETS ${execname} DESTINATION ${BIN_DIR})
  endif(EXEC_INSTALL)

  # Use the list of libraries to be linked into this target to 
  # accumulate the necessary definitions and compilation flags.
  GET_TARGET_DEFINES(${execname} "${libslist}")
  GET_TARGET_FLAGS(${execname} "${libslist}")

  # Find out if we have C, C++, or both
  SRCS_LANG("${srcslist}" exec_type ${execname})

  # Add the definitions
  foreach(lib_define ${${execname}_DEFINES})
    set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
  endforeach(lib_define ${${execname}_DEFINES})

  # If we have a mixed language exec, pass on the flags to
  # the source files - otherwise, use the target.
  if(${exec_type} STREQUAL "MIXED")
    FLAGS_TO_FILES("${srcslist}" ${execname})
  else(${exec_type} STREQUAL "MIXED")
    # All one language - we can apply the flags to the target
    foreach(lib_flag ${${execname}_${exec_type}_FLAGS})
      set_property(TARGET ${execname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
    endforeach(lib_flag ${${execname}_${exec_type}_FLAGS})
  endif(${exec_type} STREQUAL "MIXED")
  
  # If this target is marked as incompatible with the strict flags, disable them
  foreach(extraarg ${ARGN})
    if(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
      if(NOERROR_FLAG)
	set_property(TARGET ${execname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
      endif(NOERROR_FLAG)
    endif(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
  endforeach(extraarg ${ARGN})

  # C++ is handled separately (on a per-file basis) if we have mixed sources via the NOSTRICTCXX flag
  if(${exec_type} STREQUAL "MIXED")
    CXX_NOSTRICT("${srcslist}" "${ARGN}")
  endif(${exec_type} STREQUAL "MIXED")

endmacro(BRLCAD_ADDEXEC execname srcslist libslist)


#-----------------------------------------------------------------------------
# Library macro handles both shared and static libs, so one "BRLCAD_ADDLIB"
# statement will cover both automatically
macro(BRLCAD_ADDLIB libname srcslist libslist)

  # Add ${libname} to the list of BRL-CAD libraries	
  list(APPEND BRLCAD_LIBS ${libname})
  list(REMOVE_DUPLICATES BRLCAD_LIBS)
  set(BRLCAD_LIBS "${BRLCAD_LIBS}" CACHE STRING "BRL-CAD libraries" FORCE)

  # Collect the definitions and flags needed by this library
  # In case of re-running cmake, make sure the DLL_IMPORTS define
  # for this specific library is removed, since for the actual target 
  # we need to export, not import.
  get_property(${libname}_DEFINES GLOBAL PROPERTY ${libname}_DEFINES)
  if(${libname}_DEFINES)
    list(REMOVE_ITEM ${libname}_DEFINES ${libname}_DLL_IMPORTS)
  endif(${libname}_DEFINES)
  GET_TARGET_DEFINES(${libname} "${libslist}")
  GET_TARGET_FLAGS(${libname} "${libslist}")

  # Find out if we have C, C++, or both
  SRCS_LANG("${srcslist}" lib_type ${libname})

  # If we have a mixed language library, go ahead and pass on the flags to
  # the source files - we don't need the targets defined since the flags
  # will be managed per-source-file.
  if(${lib_type} STREQUAL "MIXED")
    FLAGS_TO_FILES("${srcslist}" ${libname})
  endif(${lib_type} STREQUAL "MIXED")

  # Handle "shared" libraries (with MSVC, these would be dynamic libraries)
  if(BUILD_SHARED_LIBS)
    add_library(${libname} SHARED ${srcslist})

    # Make sure we don't end up with outputs named liblib...
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname} PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")
  
    # Generate the upper case abbrevation of the library that is used for
    # DLL definitions.  If We are using DLLs, we need to define export
    # for this library.
    if(CPP_DLL_DEFINES)
      string(REPLACE "lib" "" LOWERCORE "${libname}")
      string(TOUPPER ${LOWERCORE} UPPER_CORE)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
    endif(CPP_DLL_DEFINES)

    # If we have libraries to link, link them.
    if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      target_link_libraries(${libname} ${libslist})
    endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")

    # Call install to setup the installation logic.
    install(TARGETS ${libname} 
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR})

    # Apply the definitions.
    foreach(lib_define ${${libname}_DEFINES})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${libname}_DEFINES})

    # If we haven't already taken care of the flags on a per-file basis,
    # apply them to the target now that we have one.
    if(NOT ${lib_type} STREQUAL "MIXED")
      # All one language - we can apply the flags to the target
      foreach(lib_flag ${${libname}_${lib_type}_FLAGS})
	set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
      endforeach(lib_flag ${${libname}_${lib_type}_FLAGS})
    endif(NOT ${lib_type} STREQUAL "MIXED")

    foreach(extraarg ${ARGN})
      if(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${ARGN})
  endif(BUILD_SHARED_LIBS)

  # Handle static libraries (renaming requirements to both allow unique targets and
  # respect standard naming conventions.)
  if(BUILD_STATIC_LIBS)
    add_library(${libname}-static STATIC ${srcslist})

    # Make sure we don't end up with outputs named liblib...
    if(${libname}-static MATCHES "^lib*")
      set_target_properties(${libname}-static PROPERTIES PREFIX "")
    endif(${libname}-static MATCHES "^lib*")

    if(NOT MSVC)
      if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
	target_link_libraries(${libname}-static ${libslist})
      endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      set_target_properties(${libname}-static PROPERTIES OUTPUT_NAME "${libname}")
    endif(NOT MSVC)

    install(TARGETS ${libname}-static
      RUNTIME DESTINATION ${BIN_DIR}
      LIBRARY DESTINATION ${LIB_DIR}
      ARCHIVE DESTINATION ${LIB_DIR})

    foreach(lib_define ${${libname}_DEFINES})
      set_property(TARGET ${libname}-static APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${libname}_DEFINES})

    # If we haven't already taken care of the flags on a per-file basis,
    # apply them to the target now that we have one.
    if(NOT ${lib_type} STREQUAL "MIXED")
      # All one language - we can apply the flags to the target
      foreach(lib_flag ${${libname}_${lib_type}_FLAGS})
	set_property(TARGET ${libname}-static APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
      endforeach(lib_flag ${${libname}_${lib_type}_FLAGS})
    endif(NOT ${lib_type} STREQUAL "MIXED")

    # If we can't build this library strict, add the -Wno-error flag
    foreach(extraarg ${ARGN})
      if(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname}-static APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NOSTRICT$" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${ARGN})
  endif(BUILD_STATIC_LIBS)

  # C++ STRICTNESS is handled separately (on a per-file basis) if we have mixed 
  # sources via the NOSTRICTCXX flag
  if(${lib_type} STREQUAL "MIXED")
    CXX_NOSTRICT("${srcslist}" "${ARGN}")
  endif(${lib_type} STREQUAL "MIXED")

  # For any CPP_DLL_DEFINES library, users of that library will need the DLL_IMPORTS
  # definition specific to that library.  Add it to ${libname}_DEFINES
  if(CPP_DLL_DEFINES)
    list(APPEND ${libname}_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
  endif(CPP_DLL_DEFINES)
  set_property(GLOBAL PROPERTY ${libname}_DEFINES "${${libname}_DEFINES}")

  mark_as_advanced(BRLCAD_LIBS)

endmacro(BRLCAD_ADDLIB libname srcslist libslist)

#-----------------------------------------------------------------------------
# For situations when a local 3rd party library (say, zlib) has been chosen in 
# preference to a system version of that library, it is important to ensure 
# that the local header(s) get included before the system headers.  Normally 
# this is handled by explicitly specifying the local include paths (which, 
# being explicitly specified and non-standard, are checked prior to default 
# system locations) but there are some situations (macports being a classic 
# example) where *other* "non-standard" installed copies of libraries may 
# exist and be found if those directories are included ahead of the desired 
# local copy.  An observed case:
#
# 1.  macports is installed on OSX
# 2.  X11 is found in macports, X11 directories are set to /usr/macports based paths
# 3.  These paths are mixed into the general include path lists for some BRL-CAD libs.
# 4.  Because these paths are a) non-standard and b) contain zlib.h they result
#     in "system" versions of zlib present in macports being found first even when
#     the local zlib is enabled, if the macports paths happen to appear in the
#     include directory list before the local zlib include paths.
#
# To mitigate this problem, BRL-CAD library include directories are sorted
# according to the following hierarchy (using gcc's left-to-right search
# order as a basis: http://gcc.gnu.org/onlinedocs/cpp/Search-Path.html):
#
# 1.  If CMAKE_CURRENT_BINARY_DIR or CMAKE_CURRENT_SOURCE_DIR are in the
#     include list, they come first.
# 2.  If BRLCAD_BINARY_DIR/include or BRLCAD_SOURCE_DIR/include are present,
#     they come second.
# 3.  For remaining paths, if the "root" path matches the BRLCAD_SOURCE_DIR
#     or BRLCAD_BINARY_DIR paths, they are appended.
# 4.  Any remaining paths are appended.
macro(BRLCAD_SORT_INCLUDE_DIRS DIR_LIST)
  if(${DIR_LIST})
    set(ORDERED_ELEMENTS ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} ${BRLCAD_BINARY_DIR}/include ${BRLCAD_SOURCE_DIR}/include)
    set(NEW_DIR_LIST "")
    foreach(element ${ORDERED_ELEMENTS})
      set(DEF_EXISTS "-1")
      list(FIND ${DIR_LIST} ${element} DEF_EXISTS)
      if(NOT "${DEF_EXISTS}" STREQUAL "-1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${element})
	list(REMOVE_ITEM ${DIR_LIST} ${element})
      endif(NOT "${DEF_EXISTS}" STREQUAL "-1")
    endforeach(element ${ORDERED_ELEMENTS})

    # paths in BRL-CAD build dir
    foreach(inc_path ${${DIR_LIST}})
      IS_SUBPATH("${BRLCAD_BINARY_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # paths in BRL-CAD source dir
    foreach(inc_path ${${DIR_LIST}})
      IS_SUBPATH("${BRLCAD_SOURCE_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
	set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
	list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # add anything that might be left
    set(NEW_DIR_LIST ${NEW_DIR_LIST} ${${DIR_LIST}})

    # remove any duplicates	
    list(REMOVE_DUPLICATES NEW_DIR_LIST)

    # put the results into DIR_LIST
    set(${DIR_LIST} ${NEW_DIR_LIST})
  endif(${DIR_LIST})
endmacro(BRLCAD_SORT_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# Wrapper to handle include directories specific to libraries.  Removes
# duplicates and makes sure the <LIB>_INCLUDE_DIRS list is in the cache
# immediately, so it can be used by other libraries.  These lists are not
# intended as toplevel user settable options so mark as advanced.
macro(BRLCAD_LIB_INCLUDE_DIRS libname DIR_LIST LOCAL_DIR_LIST)
  string(TOUPPER ${libname} LIB_UPPER)

  list(REMOVE_DUPLICATES ${DIR_LIST})
  set(${LIB_UPPER}_INCLUDE_DIRS ${${DIR_LIST}} CACHE STRING "Include directories for lib${libname}" FORCE)
  mark_as_advanced(${DIR_LIST})

  set(ALL_INCLUDES ${${DIR_LIST}} ${${LOCAL_DIR_LIST}})
  BRLCAD_SORT_INCLUDE_DIRS(ALL_INCLUDES)	
  include_directories(${ALL_INCLUDES})
endmacro(BRLCAD_LIB_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# Data files needed by BRL-CAD need to be both installed and copied into the 
# correct locations in the build directories to allow executables to run
# at build time.  Ideally, we would like any error messages returned when 
# running from the build directory to direct back to the copies of files in
# the source tree, since those are the ones that should be edited.
# On platforms that support symlinks, this is possible - build directory
# "copies" of files are symlinks to the source tree version.  On platforms
# without symlink support, we are forced to copy the files into place in
# the build directories.  In both cases we have a build target that is
# triggered if source files are edited in order to allow the build system 
# to take further actions (for example, re-generating tclIndex and pkgIndex.tcl
# files when the source .tcl files change).
#
# Because BRLCAD_ADDDATA defines custom commands and specifies the files it is
# to copy/symlink as dependencies, there is a potential for file names to 
# conflict with build target names. (This has actually been observed with 
# MSVC - our file INSTALL in the toplevel source directory conflicts with the
# MSVC target named INSTALL.)  To avoid conflicts and make the dependencies 
# of the custom commands robust we supply full file paths as dependencies to
# the data copying custom commands.  

macro(BRLCAD_ADDDATA inputdata targetdir)
  # Handle both a list of one or more files and variable holding a list of files - 
  # find out what we've got.
  NORMALIZE_FILE_LIST("${inputdata}" datalist fullpath_datalist targetname)

  # Now that the input data and target names are in order, define the custom
  # commands needed for build directory data copying on this platform (per
  # symlink test results.)

  if(HAVE_SYMLINK)

    # Make sure the target directory exists (symlinks need the target directory already in place)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)

    # Using symlinks - in this case, the custom command doesn't actually have to
    # do the work every time the source file changes - once established, the symlink
    # will behave correctly.  That being the case, we just go ahead and establish the
    # symlinks in the configure stage.
    foreach(filename ${fullpath_datalist})
      get_filename_component(ITEM_NAME ${filename} NAME)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif(NOT CMAKE_CONFIGURATION_TYPES)
    endforeach(filename ${fullpath_datalist})

    # The custom command is still necessary - since it depends on the original source files, 
    # this will be the trigger that tells other commands depending on this data that 
    # they need to re-run one one of the source files is changed.
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      DEPENDS ${fullpath_datalist}
      )

  else(HAVE_SYMLINK)

    # Write out script for copying from source dir to build dir
    set(${targetname}_cmake_contents "set(FILES_TO_COPY \"${fullpath_datalist}\")\n")
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}foreach(filename \${FILES_TO_COPY})\n")
    if(NOT CMAKE_CONFIGURATION_TYPES)
      set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}\")\n")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}\")\n")
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}endforeach(filename \${CURRENT_FILE_LIST})\n")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake "${${targetname}_cmake_contents}")

    # Define custom command for copying from src to bin.
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake
      COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel
      DEPENDS ${fullpath_datalist}
      )
  endif(HAVE_SYMLINK)

  # Define the target and add it to this directories list of data targets
  add_custom_target(${targetname}_cp ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel)
  BRLCAD_ADD_DIR_LIST_ENTRY(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" ${targetname}_cp)

  # Add outputs to the distclean rules - this is consistent regardless of what type the output
  # file is, symlink or copy.  Just need to handle the single and multiconfig cases.
  foreach(filename ${fullpath_datalist})
    get_filename_component(ITEM_NAME "${filename}" NAME)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      DISTCLEAN(${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DATA_DIR}/${targetdir}/${ITEM_NAME})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
  endforeach(filename ${fullpath_datalist})

  # The installation rule relates only to the original source directory copy, and so doesn't
  # need to explicitly concern itself with configurations.
  install(FILES ${datalist} DESTINATION ${DATA_DIR}/${targetdir})

endmacro(BRLCAD_ADDDATA datalist targetdir)

#-----------------------------------------------------------------------------
macro(ADD_MAN_PAGES num inmanlist)
  string(REPLACE "${DATA_DIR}/" "" R_MAN_DIR "${MAN_DIR}")
  set(man_target_dir ${R_MAN_DIR}/man${num})
  BRLCAD_ADDDATA(${inmanlist} ${man_target_dir})
endmacro(ADD_MAN_PAGES num fullpath_manlist)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
