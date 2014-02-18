#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

# Utility macro checking for a specific arg in un-named arguments to a
# macro.
macro(CHECK_OPT prop result args)
  set(${result} 0)
  foreach(extraarg ${args})
    if(${extraarg} STREQUAL "${prop}")
      set(${result} 1)
    endif(${extraarg} STREQUAL "${prop}")
  endforeach(extraarg ${args})
endmacro(CHECK_OPT)

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

# Take a target definition and find out what its libraries
# are supplying in the way of DLL definitions.
macro(GET_TARGET_DLL_DEFINES targetname target_libs)
  if(CPP_DLL_DEFINES)
    # In case of re-running cmake, make sure the DLL_IMPORTS define
    # for this specific library is removed, since for the actual target
    # we need to export, not import.
    get_property(${targetname}_DLL_DEFINES GLOBAL PROPERTY ${targetname}_DLL_DEFINES)
    if(${targetname}_DLL_DEFINES)
      list(REMOVE_ITEM ${targetname}_DLL_DEFINES ${targetname}_DLL_IMPORTS)
    endif(${targetname}_DLL_DEFINES)
    foreach(libitem ${target_libs})
      list(FIND BRLCAD_LIBS ${libitem} FOUNDIT)
      if(NOT ${FOUNDIT} STREQUAL "-1")
	get_property(${libitem}_DLL_DEFINES GLOBAL PROPERTY ${libitem}_DLL_DEFINES)
	list(APPEND ${targetname}_DLL_DEFINES ${${libitem}_DLL_DEFINES})
	if(${targetname}_DLL_DEFINES)
	  list(REMOVE_DUPLICATES ${targetname}_DLL_DEFINES)
	endif(${targetname}_DLL_DEFINES)
      endif(NOT ${FOUNDIT} STREQUAL "-1")
    endforeach(libitem ${target_libs})
  endif(CPP_DLL_DEFINES)
endmacro(GET_TARGET_DLL_DEFINES)

# For BRL-CAD targets, use CXX as the language if the user requests it
macro(SET_CXX_LANG SRC_FILES)
  if(ENABLE_ALL_CXX_COMPILE)
    foreach(srcfile ${SRC_FILES})
      if(NOT ${CMAKE_CURRENT_SOURCE_DIR}/${srcfile} MATCHES "src/other")
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcfile})
	  set_source_files_properties(${srcfile} PROPERTIES LANGUAGE CXX)
	endif(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${srcfile})
      endif(NOT ${CMAKE_CURRENT_SOURCE_DIR}/${srcfile} MATCHES "src/other")
    endforeach(srcfile ${SRC_FILES})
  endif(ENABLE_ALL_CXX_COMPILE)
endmacro(SET_CXX_LANG SRC_FILES)

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
# one language, we need to apply flags on a per-file basis
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
    if("${file_language}" STREQUAL "C" AND NOT "${C_INLINE}" STREQUAL "inline")
      set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_DEFINITIONS "inline=${C_INLINE}")
    endif("${file_language}" STREQUAL "C" AND NOT "${C_INLINE}" STREQUAL "inline")
  endforeach(srcfile ${srcslist})
endmacro(FLAGS_TO_FILES)

# Handle C++ NO_STRICT settings
macro(CXX_NO_STRICT cxx_srcslist args)
  CHECK_CXX_COMPILER_FLAG(-Wno-error NOERROR_FLAG_CXX)
  if(NOERROR_FLAG_CXX)
    foreach(extraarg ${args})
      if(${extraarg} MATCHES "NO_STRICT_CXX" AND BRLCAD_ENABLE_STRICT)
	foreach(srcfile ${cxx_srcslist})
	  get_filename_component(srcfile_ext ${srcfile} EXT)
	  if(${srcfile_ext} STREQUAL ".cxx" OR ${srcfile_ext} STREQUAL ".cpp" OR ${srcfile_ext} STREQUAL ".cc")
	    get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	    set_source_files_properties(${srcfile} COMPILE_FLAGS "${previous_flags} -Wno-error")
	  endif()
	endforeach(srcfile ${cxx_srcslist})
      endif(${extraarg} MATCHES "NO_STRICT_CXX" AND BRLCAD_ENABLE_STRICT)
    endforeach(extraarg ${args})
  endif(NOERROR_FLAG_CXX)
endmacro(CXX_NO_STRICT cxx_srcslist)

# BRL-CAD style checking test
macro(VALIDATE_STYLE srcslist)
if(BRLCAD_STYLE_VALIDATE)
  make_directory(${CMAKE_CURRENT_BINARY_DIR}/validation)
  foreach(srcfile ${srcslist})
    # Generated files won't conform to our style guidelines
    get_property(IS_GENERATED SOURCE ${srcfile} PROPERTY GENERATED)
    if(NOT IS_GENERATED)
      get_filename_component(root_name ${srcfile} NAME_WE)
      string(MD5 path_md5 "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
      set(outfiles_root "${CMAKE_CURRENT_BINARY_DIR}/validation/${root_name}_${path_md5}")
      set(srcfile_tmp "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
      set(stampfile_tmp "${stampfile}")
      configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/astyle.cmake.in ${outfiles_root}.cmake @ONLY)
      add_custom_command(
	OUTPUT ${outfiles_root}.checked
	COMMAND ${CMAKE_COMMAND} -P ${outfiles_root}.cmake
	DEPENDS ${srcfile} ${ASTYLE_EXECUTABLE_TARGET}
	COMMENT "Validating style of ${srcfile}"
	)
      set_source_files_properties(${srcfile} PROPERTIES OBJECT_DEPENDS ${outfiles_root}.checked)
    endif(NOT IS_GENERATED)
  endforeach(srcfile ${srcslist})
endif(BRLCAD_STYLE_VALIDATE)
endmacro(VALIDATE_STYLE)

macro(VALIDATE_TARGET_STYLE targetname)
  if(BRLCAD_STYLE_VALIDATE)
      configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/validate_style.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_validate.cmake @ONLY)
      add_custom_command(
	TARGET ${targetname} PRE_LINK
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_validate.cmake
	COMMENT "Checking validation status of ${targetname} srcs"
	)
  endif(BRLCAD_STYLE_VALIDATE)
endmacro(VALIDATE_TARGET_STYLE targetname)

#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
macro(BRLCAD_ADDEXEC execname srcslist libslist)

  # Check at comple time the standard BRL-CAD style rules
  VALIDATE_STYLE("${srcslist}")

  # Go all C++ if the settings request it
  SET_CXX_LANG("${srcslist}")

  # Call standard CMake commands
  add_executable(${execname} ${srcslist})
  target_link_libraries(${execname} ${libslist})
  VALIDATE_TARGET_STYLE(${execname})

  # In some situations (usually test executables) we want to be able
  # to force the executable to remain in the local compilation
  # directory regardless of the global CMAKE_RUNTIME_OUTPUT_DIRECTORY
  # setting.  The NO_INSTALL flag is used to denote such executables.
  # If an executable isn't to be installed or needs to be installed
  # somewhere other than the default location, the NO_INSTALL argument
  # bypasses the standard install command call.
  CHECK_OPT("NO_INSTALL" NO_EXEC_INSTALL "${ARGN}")
  if(NO_EXEC_INSTALL)
    # Unfortunately, we currently need Windows binaries in the same directories as their DLL libraries
    if(NOT WIN32)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	set_target_properties(${execname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  set_target_properties(${execname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${CFG_TYPE_UPPER} ${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE})
	endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      endif(NOT CMAKE_CONFIGURATION_TYPES)
    endif(NOT WIN32)
  else(NO_EXEC_INSTALL)
    install(TARGETS ${execname} DESTINATION ${BIN_DIR})
  endif(NO_EXEC_INSTALL)

  # Use the list of libraries to be linked into this target to
  # accumulate the necessary definitions and compilation flags.
  GET_TARGET_DEFINES(${execname} "${libslist}")
  # For DLL libraries, we may need additional flags
  GET_TARGET_DLL_DEFINES(${execname} "${libslist}")
  GET_TARGET_FLAGS(${execname} "${libslist}")

  # Find out if we have C, C++, or both
  SRCS_LANG("${srcslist}" exec_type ${execname})

  # Add the definitions
  foreach(lib_define ${${execname}_DEFINES})
    set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
  endforeach(lib_define ${${execname}_DEFINES})
  foreach(lib_define ${${execname}_DLL_DEFINES})
    set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
  endforeach(lib_define ${${execname}_DLL_DEFINES})

  # If we have a mixed language exec, pass on the flags to
  # the source files - otherwise, use the target.
  if(${exec_type} STREQUAL "MIXED")
    FLAGS_TO_FILES("${srcslist}" ${execname})
  else(${exec_type} STREQUAL "MIXED")
    # All one language - we can apply the flags to the target
    foreach(lib_flag ${${execname}_${exec_type}_FLAGS})
      set_property(TARGET ${execname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
    endforeach(lib_flag ${${execname}_${exec_type}_FLAGS})
    if(NOT "${C_INLINE}" STREQUAL "inline" AND "${exec_type}" STREQUAL "C")
      set_property(TARGET ${execname} APPEND PROPERTY COMPILE_DEFINITIONS "inline=${C_INLINE}")
    endif(NOT "${C_INLINE}" STREQUAL "inline" AND "${exec_type}" STREQUAL "C")
  endif(${exec_type} STREQUAL "MIXED")

  # If this target is marked as incompatible with the strict flags, disable them
  foreach(extraarg ${ARGN})
    if(${extraarg} MATCHES "NO_STRICT$" AND BRLCAD_ENABLE_STRICT)
      CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)
      if(NOERROR_FLAG)
	set_property(TARGET ${execname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
      endif(NOERROR_FLAG)
    endif(${extraarg} MATCHES "NO_STRICT$" AND BRLCAD_ENABLE_STRICT)
  endforeach(extraarg ${ARGN})

  # C++ is handled separately (on a per-file basis) if we have mixed sources via the NO_STRICT_CXX flag
  if(${exec_type} STREQUAL "MIXED")
    CXX_NO_STRICT("${srcslist}" "${ARGN}")
  endif(${exec_type} STREQUAL "MIXED")

endmacro(BRLCAD_ADDEXEC execname srcslist libslist)


#-----------------------------------------------------------------------------
# Library macro handles both shared and static libs, so one "BRLCAD_ADDLIB"
# statement will cover both automatically
macro(BRLCAD_ADDLIB libname srcslist libslist)

  # Go all C++ if the settings request it
  SET_CXX_LANG("${srcslist}")

  # Add ${libname} to the list of BRL-CAD libraries
  list(APPEND BRLCAD_LIBS ${libname})
  list(REMOVE_DUPLICATES BRLCAD_LIBS)
  set(BRLCAD_LIBS "${BRLCAD_LIBS}" CACHE STRING "BRL-CAD libraries" FORCE)

  # Collect the definitions and flags needed by this library
  GET_TARGET_DEFINES(${libname} "${libslist}")
  # For DLL libraries, we may need additional flags
  GET_TARGET_DLL_DEFINES(${libname} "${libslist}")
  GET_TARGET_FLAGS(${libname} "${libslist}")

  # Find out if we have C, C++, or both
  SRCS_LANG("${srcslist}" lib_type ${libname})

  # If we have a mixed language library, go ahead and pass on the flags to
  # the source files - we don't need the targets defined since the flags
  # will be managed per-source-file.
  if(${lib_type} STREQUAL "MIXED")
    FLAGS_TO_FILES("${srcslist}" ${libname})
  endif(${lib_type} STREQUAL "MIXED")

  # Check at comple time the standard BRL-CAD style rules
  VALIDATE_STYLE("${srcslist}")

  # Handle "shared" libraries (with MSVC, these would be dynamic libraries)
  if(BUILD_SHARED_LIBS)

    add_library(${libname} SHARED ${srcslist})
    VALIDATE_TARGET_STYLE(${libname})

    # Make sure we don't end up with outputs named liblib...
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname} PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")

    # Generate the upper case abbreviation of the library that is used for
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

    # If a library isn't to be installed or needs to be installed
    # somewhere other than the default location, the NO_INSTALL argument
    # bypasses the standard install command call. Otherwise, call install
    # with standard arguments.
    CHECK_OPT("NO_INSTALL" NO_LIB_INSTALL "${ARGN}")
    if(NOT NO_LIB_INSTALL)
      install(TARGETS ${libname}
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
    endif(NOT NO_LIB_INSTALL)

    # Apply the definitions.
    foreach(lib_define ${${libname}_DEFINES})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${libname}_DEFINES})
    # If we're building a DLL, also apply the DLL definitions
    foreach(lib_define ${${libname}_DLL_DEFINES})
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${lib_define}")
    endforeach(lib_define ${${libname}_DLL_DEFINES})

    # If we haven't already taken care of the flags on a per-file basis,
    # apply them to the target now that we have one.
    if(NOT ${lib_type} STREQUAL "MIXED")
      # All one language - we can apply the flags to the target
      foreach(lib_flag ${${libname}_${lib_type}_FLAGS})
	set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "${lib_flag}")
      endforeach(lib_flag ${${libname}_${lib_type}_FLAGS})
      if(NOT "${C_INLINE}" STREQUAL "inline" AND "${lib_type}" STREQUAL "C")
	set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "inline=${C_INLINE}")
      endif(NOT "${C_INLINE}" STREQUAL "inline" AND "${lib_type}" STREQUAL "C")
    endif(NOT ${lib_type} STREQUAL "MIXED")

    foreach(extraarg ${ARGN})
      if(${extraarg} MATCHES "NO_STRICT" AND BRLCAD_ENABLE_STRICT AND NOT  ${lib_type} STREQUAL "MIXED")
        CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname} APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NO_STRICT" AND BRLCAD_ENABLE_STRICT AND NOT  ${lib_type} STREQUAL "MIXED")
    endforeach(extraarg ${ARGN})
  endif(BUILD_SHARED_LIBS)

  # Handle static libraries (renaming requirements to both allow unique targets and
  # respect standard naming conventions.)
  if(BUILD_STATIC_LIBS)
    add_library(${libname}-static STATIC ${srcslist})
    VALIDATE_TARGET_STYLE(${libname}-static)

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

    # If a library isn't to be installed or needs to be installed
    # somewhere other than the default location, the NO_INSTALL argument
    # bypasses the standard install command call. Otherwise, call install
    # with standard arguments.
    CHECK_OPT("NO_INSTALL" NO_LIB_INSTALL "${ARGN}")
    if(NOT NO_LIB_INSTALL)
      install(TARGETS ${libname}-static
	RUNTIME DESTINATION ${BIN_DIR}
	LIBRARY DESTINATION ${LIB_DIR}
	ARCHIVE DESTINATION ${LIB_DIR})
    endif(NOT NO_LIB_INSTALL)

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
      if(${extraarg} MATCHES "NO_STRICT" AND BRLCAD_ENABLE_STRICT AND NOT  ${lib_type} STREQUAL "MIXED")
        CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)
	if(NOERROR_FLAG)
	  set_property(TARGET ${libname}-static APPEND PROPERTY COMPILE_FLAGS "-Wno-error")
	endif(NOERROR_FLAG)
      endif(${extraarg} MATCHES "NO_STRICT" AND BRLCAD_ENABLE_STRICT AND NOT  ${lib_type} STREQUAL "MIXED")
    endforeach(extraarg ${ARGN})
  endif(BUILD_STATIC_LIBS)

  # C++ STRICTNESS is handled separately (on a per-file basis) if we have mixed
  # sources via the NO_STRICT_CXX flag
  if(${lib_type} STREQUAL "MIXED")
    CXX_NO_STRICT("${srcslist}" "${ARGN}")
  endif(${lib_type} STREQUAL "MIXED")

  # For any CPP_DLL_DEFINES DLL library, users of that library will need the DLL_IMPORTS
  # definition specific to that library.  Add it to ${libname}_DLL_DEFINES
  if(CPP_DLL_DEFINES)
    list(APPEND ${libname}_DLL_DEFINES ${UPPER_CORE}_DLL_IMPORTS)
  endif(CPP_DLL_DEFINES)
  set_property(GLOBAL PROPERTY ${libname}_DLL_DEFINES "${${libname}_DLL_DEFINES}")

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
# Wrapper to properly include directories for a BRL-CAD build.  Handles the
# SYSTEM option to the include_directories command, as well as calling the
# sort macro.
macro(BRLCAD_INCLUDE_DIRS DIR_LIST)

  set(INCLUDE_DIRS ${${DIR_LIST}})
  if(INCLUDE_DIRS)
    list(REMOVE_DUPLICATES INCLUDE_DIRS)
  endif(INCLUDE_DIRS)

  BRLCAD_SORT_INCLUDE_DIRS(INCLUDE_DIRS)

  foreach(inc_dir ${INCLUDE_DIRS})
    get_filename_component(abs_inc_dir ${inc_dir} ABSOLUTE)
    IS_SUBPATH("${BRLCAD_SOURCE_DIR}" "${abs_inc_dir}" IS_LOCAL)
    if (NOT IS_LOCAL)
      IS_SUBPATH("${BRLCAD_BINARY_DIR}" "${abs_inc_dir}" IS_LOCAL)
    endif (NOT IS_LOCAL)
    if("${inc_dir}" MATCHES "other" OR NOT IS_LOCAL)
      # Unfortunately, a bug in the CMake SYSTEM option to
      # include_directories requires that these variables
      # be explicitly set on OSX
      if(APPLE)
	set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
	set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ")
      endif(APPLE)
      include_directories(SYSTEM ${inc_dir})
    else("${inc_dir}" MATCHES "other" OR NOT IS_LOCAL)
      include_directories(${inc_dir})
    endif("${inc_dir}" MATCHES "other" OR NOT IS_LOCAL)
  endforeach(inc_dir ${ALL_INCLUDES})

endmacro(BRLCAD_INCLUDE_DIRS DIR_LIST)

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
  BRLCAD_INCLUDE_DIRS(ALL_INCLUDES)
endmacro(BRLCAD_LIB_INCLUDE_DIRS)


#-----------------------------------------------------------------------------
# Files needed by BRL-CAD need to be both installed and copied into the
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
# Because BRLCAD_MANAGE_FILES defines custom commands and specifies the files it is
# to copy/symlink as dependencies, there is a potential for file names to
# conflict with build target names. (This has actually been observed with
# MSVC - our file INSTALL in the toplevel source directory conflicts with the
# MSVC target named INSTALL.) To avoid conflicts and make the dependencies
# of the custom commands robust we supply full file paths as dependencies to
# the file copying custom commands.

macro(BRLCAD_MANAGE_FILES inputdata targetdir)
  # Handle both a list of one or more files and variable holding a list of files -
  # find out what we've got.
  NORMALIZE_FILE_LIST("${inputdata}" datalist fullpath_datalist targetname)

  #-----------------------------------------------------------------------------
  # Some of the more advanced build system features in BRL-CAD's CMake
  # build need to know whether symlink support is present on the
  # current OS - go ahead and do this test up front, caching the
  # results.
  if(NOT DEFINED HAVE_SYMLINK)
    message("--- Checking operating system support for file symlinking")
    file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src "testing for symlink ability")
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest)
    if(EXISTS ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest)
      message("--- Checking operating system support for file symlinking - Supported")
      set(HAVE_SYMLINK 1 CACHE BOOL "Platform supports creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest)
    else(EXISTS ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest)
      message("--- Checking operating system support for file symlinking - Unsupported")
      set(HAVE_SYMLINK 0 CACHE BOOL "Platform does not support creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src)
    endif(EXISTS ${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest)
  endif(NOT DEFINED HAVE_SYMLINK)

  # Now that the input data and target names are in order, define the custom
  # commands needed for build directory data copying on this platform (per
  # symlink test results.)

  if(HAVE_SYMLINK)

    # Make sure the target directory exists (symlinks need the target directory already in place)
    if(NOT CMAKE_CONFIGURATION_TYPES)
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${targetdir})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)

    # Using symlinks - in this case, the custom command doesn't actually have to
    # do the work every time the source file changes - once established, the symlink
    # will behave correctly.  That being the case, we just go ahead and establish the
    # symlinks in the configure stage.
    foreach(filename ${fullpath_datalist})
      get_filename_component(ITEM_NAME ${filename} NAME)
      if(NOT CMAKE_CONFIGURATION_TYPES)
	execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} ${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME})
      else(NOT CMAKE_CONFIGURATION_TYPES)
	foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	  string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}/${ITEM_NAME})
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
      set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${targetdir}\")\n")
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	set(${targetname}_cmake_contents "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}\")\n")
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
      DISTCLEAN(${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME})
    else(NOT CMAKE_CONFIGURATION_TYPES)
      foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
	DISTCLEAN(${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${targetdir}/${ITEM_NAME})
      endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    endif(NOT CMAKE_CONFIGURATION_TYPES)
  endforeach(filename ${fullpath_datalist})

  # The installation rule relates only to the original source directory copy, and so doesn't
  # need to explicitly concern itself with configurations.
  CHECK_OPT("EXEC" EXEC_INSTALL "${ARGN}")
  if(EXEC_INSTALL)
    install(PROGRAMS ${datalist} DESTINATION ${targetdir})
  else(EXEC_INSTALL)
    install(FILES ${datalist} DESTINATION ${targetdir})
  endif(EXEC_INSTALL)

endmacro(BRLCAD_MANAGE_FILES)

#-----------------------------------------------------------------------------
# Specific uses of the BRLCAD_MANAGED_FILES functionality - these cover most
# of the common cases in BRL-CAD.

macro(BRLCAD_ADDDATA datalist targetdir)
  BRLCAD_MANAGE_FILES(${datalist} ${DATA_DIR}/${targetdir})
endmacro(BRLCAD_ADDDATA)

macro(ADD_DOC doclist targetdir)
  set(doc_target_dir ${DOC_DIR}/${targetdir})
  BRLCAD_MANAGE_FILES(${doclist} ${doc_target_dir})
endmacro(ADD_DOC)

macro(ADD_MAN_PAGES num inmanlist)
  set(man_target_dir ${MAN_DIR}/man${num})
  BRLCAD_MANAGE_FILES(${inmanlist} ${man_target_dir})
endmacro(ADD_MAN_PAGES)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
