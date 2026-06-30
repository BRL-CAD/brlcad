#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2026 United States Government as represented by
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


# global property for our list of binaries
define_property(
  GLOBAL
  PROPERTY BRLCAD_EXEC_FILES
  BRIEF_DOCS "BRL-CAD binaries"
  FULL_DOCS "List of installed BRL-CAD binary programs"
)

# Installed BRL-CAD library targets and their object-library variants.  This
# lets aggregate targets derive their contents from BRLCAD_ADDLIB declarations
# instead of maintaining a separate, stale-prone library list.
define_property(
  GLOBAL
  PROPERTY BRLCAD_LIB_TARGETS
  BRIEF_DOCS "BRL-CAD libraries"
  FULL_DOCS "List of installed BRL-CAD library targets"
)

define_property(
  GLOBAL
  PROPERTY BRLCAD_LIB_OBJECT_TARGETS
  BRIEF_DOCS "BRL-CAD library object targets"
  FULL_DOCS "List of installed BRL-CAD library object targets"
)

define_property(
  GLOBAL
  PROPERTY BRLCAD_LIB_EXTERNAL_LINK_LIBS
  BRIEF_DOCS "BRL-CAD aggregate external link libraries"
  FULL_DOCS "List of non-BRL-CAD link libraries needed by aggregate BRL-CAD targets"
)


# Need sophisticated option parsing
include(CMakeParseArguments)


#-----------------------------------------------------------------------------
# C/C++ language management

function(src_lang srcfile resultvar)
  get_property(file_language SOURCE ${srcfile} PROPERTY LANGUAGE)
  if (file_language)
    set(${resultvar} "${file_language}" PARENT_SCOPE)
    return()
  endif()
  get_filename_component(srcfile_ext ${srcfile} EXT)
  if(${srcfile_ext} MATCHES ".cxx$" OR ${srcfile_ext} MATCHES ".cpp$" OR ${srcfile_ext} MATCHES ".cc$")
    set(file_language CXX)
    set_source_files_properties(${srcfile} PROPERTIES LANGUAGE CXX)
  elseif(${srcfile_ext} STREQUAL ".c")
    set(file_language C)
    set_source_files_properties(${srcfile} PROPERTIES LANGUAGE C)
  endif(${srcfile_ext} MATCHES ".cxx$" OR ${srcfile_ext} MATCHES ".cpp$" OR ${srcfile_ext} MATCHES ".cc$")
  set(${resultvar} "${file_language}" PARENT_SCOPE)
endfunction()

function(set_srcs_lang SRC_FILES cxx_enable)
  foreach(srcfile ${SRC_FILES})
    set(sfile ${srcfile})
    if(NOT EXISTS "${srcfile}")
      set(sfile "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
    endif()
    if (EXISTS "${sfile}")
      # Make sure each file is identified as C or C++, if it is supposed to be
      # one of those types
      src_lang("${srcfile}" f_lang)
      # If the user is asking to use CXX as the language for C files, do so in
      # all viable cases
      if(${cxx_enable})
	get_property(_no_cxx SOURCE ${srcfile} PROPERTY NO_CXX_COMPILE)
	if(NOT _no_cxx AND "${f_lang}" STREQUAL "C")
	  set_source_files_properties(${srcfile} PROPERTIES LANGUAGE CXX)
	  set_property(SOURCE ${srcfile} APPEND PROPERTY COMPILE_DEFINITIONS NO_CXX_COMPILE register=)
	endif()
      endif()
    endif()
  endforeach()
endfunction()


#-----------------------------------------------------------------------------
# BRL-CAD style checking with AStyle
function(VALIDATE_STYLE targetname srcslist)

  # See if we've got astyle
  find_program(ASTYLE_EXECUTABLE astyle HINTS "${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR}")

  if(BRLCAD_STYLE_VALIDATE AND ASTYLE_EXECUTABLE AND NOT "${srcslist}" STREQUAL "")
    # Find out of any of the files need to be ignored
    set(fullpath_srcslist)
    foreach(srcfile ${srcslist})
      if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
        get_property(IGNORE_FILE SOURCE ${srcfile} PROPERTY EXTERNAL)
        if(IGNORE_FILE)
          message("Note: skipping style validation on external file ${srcfile}")
        else(IGNORE_FILE)
          set(fullpath_srcslist ${fullpath_srcslist} "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
        endif(IGNORE_FILE)
      endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
    endforeach(srcfile ${srcslist})

    # If we have a list that isn't empty, use it
    if(fullpath_srcslist)
      add_custom_command(
        TARGET ${targetname}
        PRE_LINK
        COMMAND "${ASTYLE_EXECUTABLE}" --report --options=${BRLCAD_SOURCE_DIR}/misc/astyle.opt ${fullpath_srcslist}
        COMMENT "Checking formatting of ${targetname} srcs"
      )
      if(TARGET astyle)
        add_dependencies(${targetname} astyle)
      endif(TARGET astyle)
    endif(fullpath_srcslist)
  endif(BRLCAD_STYLE_VALIDATE AND ASTYLE_EXECUTABLE AND NOT "${srcslist}" STREQUAL "")
endfunction(VALIDATE_STYLE)


#---------------------------------------------------------------------
# When a local 3rd-party library (e.g., zlib) is used instead of a
# system version, ensure local headers are included before system
# headers. This is usually handled by explicitly specifying local
# include paths, but non-standard installations (e.g., macports) can
# introduce conflicts by including alternative versions.
#
# To avoid such conflicts, BRL-CAD sorts include dirs based on gcc's
# left-to-right search order semantic
# (http://gcc.gnu.org/onlinedocs/cpp/Search-Path.html):
#
# 1. CMAKE_CURRENT_BINARY_DIR and CMAKE_CURRENT_SOURCE_DIR come first.
#
# 2. BRLCAD_BINARY_DIR/include & BRLCAD_SOURCE_DIR/include are second.
#
# 3. Paths rooted in BRLCAD_SOURCE_DIR or BRLCAD_BINARY_DIR are next.
#
# 4. All other paths are appended last.
#
###
function(BRLCAD_SORT_INCLUDE_DIRS DIR_LIST)
  if(${DIR_LIST})
    set(
      ORDERED_ELEMENTS
      "${CMAKE_CURRENT_BINARY_DIR}"
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${BRLCAD_BINARY_DIR}/include"
      "${BRLCAD_SOURCE_DIR}/include"
    )
    set(LAST_ELEMENTS "/usr/local/include" "/usr/include")

    set(NEW_DIR_LIST "")
    set(LAST_DIR_LIST "")

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
      is_subpath("${BRLCAD_BINARY_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
        set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
        list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # paths in BRL-CAD source dir
    foreach(inc_path ${${DIR_LIST}})
      is_subpath("${BRLCAD_SOURCE_DIR}" "${inc_path}" SUBPATH_TEST)
      if("${SUBPATH_TEST}" STREQUAL "1")
        set(NEW_DIR_LIST ${NEW_DIR_LIST} ${inc_path})
        list(REMOVE_ITEM ${DIR_LIST} ${inc_path})
      endif("${SUBPATH_TEST}" STREQUAL "1")
    endforeach(inc_path ${${DIR_LIST}})

    # Pull out include paths that are definitely system paths (and
    # hence need to come after ours
    foreach(element ${LAST_ELEMENTS})
      set(DEF_EXISTS "-1")
      list(FIND ${DIR_LIST} ${element} DEF_EXISTS)
      if(NOT "${DEF_EXISTS}" STREQUAL "-1")
        set(LAST_DIR_LIST ${LAST_DIR_LIST} ${element})
        list(REMOVE_ITEM ${DIR_LIST} ${element})
      endif(NOT "${DEF_EXISTS}" STREQUAL "-1")
    endforeach(element ${LAST_ELEMENTS})

    # add anything that might be left
    set(NEW_DIR_LIST ${NEW_DIR_LIST} ${${DIR_LIST}} ${LAST_DIR_LIST})

    # remove any duplicates
    list(REMOVE_DUPLICATES NEW_DIR_LIST)

    # put the results into DIR_LIST
    set(${DIR_LIST} ${NEW_DIR_LIST} PARENT_SCOPE)
  endif(${DIR_LIST})
endfunction(BRLCAD_SORT_INCLUDE_DIRS)


function(BRLCAD_CLASSIFY_INCLUDE_DIR inc_dir local_var syspath_var)
  get_filename_component(_abs_inc_dir "${inc_dir}" ABSOLUTE)

  is_subpath("${BRLCAD_SOURCE_DIR}" "${_abs_inc_dir}" _is_local)
  if(NOT _is_local)
    is_subpath("${BRLCAD_BINARY_DIR}" "${_abs_inc_dir}" _is_local)
  endif()

  set(_is_syspath 0)
  foreach(_sp ${SYS_INCLUDE_PATTERNS})
    if("${inc_dir}" MATCHES "${_sp}")
      set(_is_syspath 1)
      break()
    endif()
  endforeach()

  set(${local_var} "${_is_local}" PARENT_SCOPE)
  set(${syspath_var} "${_is_syspath}" PARENT_SCOPE)
endfunction()


###
# wrapper for specifying include dirs.
#
# automatically applies SYSTEM for dirs not in the source or build
# dir.  automatically skips duplicates.
function(BRLCAD_INCLUDE_DIRS targetname dirlist itype)
  set(INCLUDE_DIRS)
  foreach(idr ${${dirlist}})
    if(NOT "${idr}" MATCHES ".*NOTFOUND")
      list(APPEND INCLUDE_DIRS ${idr})
    endif(NOT "${idr}" MATCHES ".*NOTFOUND")
  endforeach(idr ${INCLUDE_DIRS})
  if(NOT INCLUDE_DIRS)
    return()
  endif(NOT INCLUDE_DIRS)

  list(REMOVE_DUPLICATES INCLUDE_DIRS)
  brlcad_sort_include_dirs(INCLUDE_DIRS)

  set(_local_include_dirs)
  set(_system_include_dirs)
  set(_system_after_include_dirs)
  foreach(inc_dir ${INCLUDE_DIRS})
    brlcad_classify_include_dir("${inc_dir}" IS_LOCAL IS_SYSPATH)
    if(IS_SYSPATH OR NOT IS_LOCAL)
      if(IS_SYSPATH)
        list(APPEND _system_include_dirs "${inc_dir}")
      else(IS_SYSPATH)
        list(APPEND _system_after_include_dirs "${inc_dir}")
      endif(IS_SYSPATH)
    else(IS_SYSPATH OR NOT IS_LOCAL)
      list(APPEND _local_include_dirs "${inc_dir}")
    endif(IS_SYSPATH OR NOT IS_LOCAL)
  endforeach(inc_dir ${INCLUDE_DIRS})

  if(_local_include_dirs)
    target_include_directories(${targetname} BEFORE ${itype} ${_local_include_dirs})
  endif()
  if(_system_include_dirs)
    target_include_directories(${targetname} SYSTEM ${itype} ${_system_include_dirs})
  endif()
  if(_system_after_include_dirs)
    target_include_directories(${targetname} SYSTEM AFTER ${itype} ${_system_after_include_dirs})
  endif()
endfunction(BRLCAD_INCLUDE_DIRS)


#--------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
function(BRLCAD_ADDEXEC execname srcslist libslist)
  cmake_parse_arguments(E "TEST;TEST_USESDATA;NO_INSTALL;NO_MAN;NO_STRICT;ALL_CXX;NO_CXX;NO_STRICT_CXX;GUI" "FOLDER;UNITY_BUILD_CODE_BEFORE_INCLUDE" "UNITY_BUILD_SKIP" ${ARGN})

  # Let CMAKEFILES know what's going on
  cmakefiles(${srcslist})

  # See if we have a testing executable.  If testing is off, skip
  if(NOT BUILD_TESTING)
    if(E_TEST OR E_TEST_USESDATA)
      return()
    endif(E_TEST OR E_TEST_USESDATA)
  endif(NOT BUILD_TESTING)

  # Go all C++ if the settings request it
  set(all_cxx OFF)
  if (ENABLE_ALL_CXX_COMPILE AND NOT E_NO_CXX)
    set(all_cxx ON)
  endif()
  if (E_ALL_CXX)
    set(all_cxx ON)
  endif()
  set_srcs_lang("${srcslist}" "${all_cxx}")

  # Add the executable.  If the caller indicates this is a GUI type
  # executable, add the correct flag for Visual Studio building (where
  # it matters)
  if(E_GUI)
    add_executable(${execname} WIN32 ${srcslist})
  else(E_GUI)
    add_executable(${execname} ${srcslist})
  endif(E_GUI)

  # Set the standard build definitions for all BRL-CAD targets
  target_compile_definitions(${execname} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

  # If this is an installed program, note that
  if(NOT E_NO_INSTALL AND NOT E_TEST)
    set_property(GLOBAL APPEND PROPERTY BRLCAD_EXEC_FILES "${execname}")
  endif(NOT E_NO_INSTALL AND NOT E_TEST)

  # Check at compile time the standard BRL-CAD style rules
  validate_style("${execname}" "${srcslist}")

  # If we have libraries to link, link them.
  if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
    target_link_libraries(${execname} ${libslist})
  endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")

  # NO_INSTALL flag forces binaries to remain in the local compilation
  # directory, bypassing the global CMAKE_RUNTIME_OUTPUT_DIRECTORY
  # setting.  This is useful for test executables or when an
  # executable is not to be installed in the default location.
  if(E_NO_INSTALL OR E_TEST)
    # Windows binaries need to go into the same dir as DLLs
    if(NOT WIN32 AND NOT E_TEST_USESDATA)
      set_target_properties(${execname} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
    endif(NOT WIN32 AND NOT E_TEST_USESDATA)
  else(E_NO_INSTALL OR E_TEST)
    if(NOT E_TEST_USESDATA)
      install(TARGETS ${execname} DESTINATION ${BIN_DIR})
    endif(NOT E_TEST_USESDATA)
  endif(E_NO_INSTALL OR E_TEST)

  # Set the folder property (used in programs such as Visual Studio to
  # organize build targets.
  if(E_NO_INSTALL AND NOT E_FOLDER)
    set(SUBFOLDER "/Build Only")
  endif(E_NO_INSTALL AND NOT E_FOLDER)
  if(E_TEST AND NOT E_FOLDER AND NOT SUBFOLDER)
    set(SUBFOLDER "/Test Programs")
  endif(E_TEST AND NOT E_FOLDER AND NOT SUBFOLDER)
  if(E_TEST_USESDATA AND NOT E_FOLDER AND NOT SUBFOLDER)
    set(SUBFOLDER "/Test Programs")
  endif(E_TEST_USESDATA AND NOT E_FOLDER AND NOT SUBFOLDER)
  if(E_FOLDER)
    set(SUBFOLDER "/${E_FOLDER}")
  endif(E_FOLDER)
  set_target_properties(${execname} PROPERTIES FOLDER "BRL-CAD Executables${SUBFOLDER}")

  if(BRLCAD_EXTRADOCS)
    if(NOT E_TEST AND NOT E_TEST_USESDATA AND NOT E_NO_INSTALL AND NOT E_NO_MAN)
      if(EXISTS ${CMAKE_SOURCE_DIR}/doc/asciidoc/system/man1/${execname}.adoc)
	add_asciidoc("MAN1" ${CMAKE_SOURCE_DIR}/doc/asciidoc/system/man1/${execname}.adoc man1 "")
      else()
        message("No man page defined for ${execname}")
      endif()
    endif()
  endif(BRLCAD_EXTRADOCS)

  # Apply unity (jumbo) build batching when the global option is enabled.
  # UNITY_BUILD_SKIP lists source files to compile individually (e.g. when
  # they contain file-scope symbols that clash with other TUs).
  # UNITY_BUILD_CODE_BEFORE_INCLUDE injects a code snippet before each
  # batched file's #include directive (e.g. to #undef per-file macros).
  # Targets with fewer than 8 source files are skipped: they are too small
  # to benefit meaningfully from batching, and the isolation cost outweighs
  # the compile-time gain.
  if(BRLCAD_ENABLE_UNITY_BUILD)
    list(LENGTH srcslist _srcs_count)
    if(_srcs_count GREATER_EQUAL 8)
      set_target_properties(${execname} PROPERTIES UNITY_BUILD ON)
      if(E_UNITY_BUILD_SKIP)
        set_source_files_properties(${E_UNITY_BUILD_SKIP} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
      endif(E_UNITY_BUILD_SKIP)
      if(E_UNITY_BUILD_CODE_BEFORE_INCLUDE)
        set_target_properties(${execname} PROPERTIES
          UNITY_BUILD_CODE_BEFORE_INCLUDE "${E_UNITY_BUILD_CODE_BEFORE_INCLUDE}"
        )
      endif(E_UNITY_BUILD_CODE_BEFORE_INCLUDE)
    endif(_srcs_count GREATER_EQUAL 8)
  endif(BRLCAD_ENABLE_UNITY_BUILD)
endfunction(BRLCAD_ADDEXEC execname srcslist libslist)


# Resolve a dependency token to a static-variant target when available.
function(BRLCAD_STATIC_VARIANT out_var dep)
  set(_dep "${dep}")
  if(TARGET "${dep}")
    get_target_property(_svt "${dep}" BRLCAD_STATIC_VARIANT_TARGET)
    if(_svt AND TARGET "${_svt}")
      set(_dep "${_svt}")
    elseif(TARGET "${dep}-static")
      set(_dep "${dep}-static")
    elseif(TARGET "${dep}_static")
      set(_dep "${dep}_static")
    endif()
  endif(TARGET "${dep}")
  set(${out_var} "${_dep}" PARENT_SCOPE)
endfunction(BRLCAD_STATIC_VARIANT)


# Given a list of dependency tokens, resolve static variants when requested.
function(BRLCAD_RESOLVE_LIBDEPS out_var link_mode)
  set(_resolved)
  foreach(_dep ${ARGN})
    if("${_dep}" STREQUAL "" OR "${_dep}" STREQUAL "NONE")
      continue()
    endif()
    if("${link_mode}" STREQUAL "STATIC")
      brlcad_static_variant(_resolved_dep "${_dep}")
    else()
      set(_resolved_dep "${_dep}")
    endif()
    list(APPEND _resolved "${_resolved_dep}")
  endforeach()
  set(${out_var} ${_resolved} PARENT_SCOPE)
endfunction(BRLCAD_RESOLVE_LIBDEPS)


# Check if the CXX compiler supports static linking of its standard libraries.
# This is necessary to work around linker errors with std::filesystem on
# some Red Hat GCC Toolset versions.
include(CheckLinkerFlag)
check_linker_flag(CXX "-static-libstdc++" BRLCAD_HAVE_STATIC_LIBSTDCXX)
check_linker_flag(CXX "-static-libgcc" BRLCAD_HAVE_STATIC_LIBGCC)

# Add a link-only executable that forces all object files in a static archive
# onto the link line.  This catches missing PRIVATE/PUBLIC dependency
# declarations that ordinary archive linking can hide until a downstream
# consumer references the affected object.
#   libstatic - target name of the static library archive to validate
#   ARGN      - additional link dependencies needed by the static archive
function(BRLCAD_ADD_STATIC_LINK_TEST libstatic)
  if(NOT BRLCAD_VALIDATE_STATIC_LINKS)
    return()
  endif(NOT BRLCAD_VALIDATE_STATIC_LINKS)
  # Require both raw link items as an inseparable pair: one enables whole
  # archive resolution before libstatic, and the other disables it before
  # following dependencies.  If either item is missing, the link test would
  # over- or under-apply whole-archive behavior and is skipped.
  if(NOT BRLCAD_LINKER_WHOLE_ARCHIVE_LINK_ITEM OR NOT BRLCAD_LINKER_NO_WHOLE_ARCHIVE_LINK_ITEM)
    return()
  endif()
  if(NOT TARGET ${libstatic})
    return()
  endif(NOT TARGET ${libstatic})

  set(_link_test_src "${BRLCAD_CMAKE_DIR}/static_link_test_main.cpp")

  set(_link_test "${libstatic}-static-link-test")
  if(TARGET ${_link_test})
    return()
  endif(TARGET ${_link_test})

  add_executable(${_link_test} EXCLUDE_FROM_ALL "${_link_test_src}")
  target_compile_definitions(${_link_test} PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(
    ${_link_test}
    PRIVATE
    ${BRLCAD_LINKER_WHOLE_ARCHIVE_LINK_ITEM}
    ${libstatic}
    ${BRLCAD_LINKER_NO_WHOLE_ARCHIVE_LINK_ITEM}
    ${ARGN}
  )
  set_target_properties(${_link_test} PROPERTIES FOLDER "BRL-CAD Static Link Tests")

  # Conditionally add flags to work around GCC toolset linker issues with std::filesystem.
  # This forces the linker to use the complete static C++ runtime from the toolset,
  # which contains the necessary symbols.
  if(BRLCAD_HAVE_STATIC_LIBSTDCXX)
    target_link_options(${_link_test} PRIVATE -static-libstdc++)
  endif()
  if(BRLCAD_HAVE_STATIC_LIBGCC)
    target_link_options(${_link_test} PRIVATE -static-libgcc)
  endif()

  # With newer GCC, our bitv structure triggers a compiler
  # warning about writing 1 byte into a region of size 0.
  if(${BRLCAD_OPTIMIZED} MATCHES "ON")
    check_c_compiler_flag(-Wno-stringop-overflow HAVE_NO_STRINGOP_OVERFLOW)
    if(HAVE_NO_STRINGOP_OVERFLOW)
      target_link_options(${_link_test} PRIVATE -Wno-stringop-overflow)
    endif(HAVE_NO_STRINGOP_OVERFLOW)
  endif(${BRLCAD_OPTIMIZED} MATCHES "ON")

  if(TARGET check)
    add_dependencies(check ${_link_test})
  endif(TARGET check)
endfunction(BRLCAD_ADD_STATIC_LINK_TEST)


# Register installed BRL-CAD library targets for aggregate outputs.
function(BRLCAD_REGISTER_LIBRARY_TARGET libname)
  if(NOT TARGET ${libname})
    return()
  endif()
  set_property(GLOBAL APPEND PROPERTY BRLCAD_LIB_TARGETS "${libname}")
  if(TARGET ${libname}-brlcad-obj)
    set_property(GLOBAL APPEND PROPERTY BRLCAD_LIB_OBJECT_TARGETS "${libname}-brlcad-obj")
  elseif(TARGET ${libname}-obj)
    set_property(GLOBAL APPEND PROPERTY BRLCAD_LIB_OBJECT_TARGETS "${libname}-obj")
  elseif(BRLCAD_ENABLE_BRLCAD_LIBRARY)
    message(FATAL_ERROR "${libname} was registered for the aggregate brlcad library, but no object-library target exists for it")
  endif()
endfunction(BRLCAD_REGISTER_LIBRARY_TARGET)


# Filter BRL-CAD targets out of a dependency list.  Aggregate object targets
# compile all BRL-CAD libraries into one DLL, so they must not inherit internal
# *_DLL_IMPORTS definitions from normal BRL-CAD target usage requirements.
function(BRLCAD_FILTER_AGGREGATE_DEPS out_var)
  set(_filtered)
  foreach(_dep ${ARGN})
    if("${_dep}" STREQUAL "" OR "${_dep}" STREQUAL "NONE")
      continue()
    endif()
    set(_candidate "${_dep}")
    if("${_candidate}" MATCHES "^\$<LINK_ONLY:([^>]+)>$")
      set(_candidate "${CMAKE_MATCH_1}")
    endif()
    if(TARGET "${_candidate}")
      get_target_property(_candidate_obj "${_candidate}" BRLCAD_LIBRARY_OBJECT_TARGET)
      if(_candidate_obj)
        continue()
      endif()
    endif()
    list(APPEND _filtered "${_dep}")
  endforeach()
  set(${out_var} ${_filtered} PARENT_SCOPE)
endfunction(BRLCAD_FILTER_AGGREGATE_DEPS)


# Preserve non-BRL-CAD usage requirements from internal dependencies that are
# filtered out of aggregate object targets.  For example, librt normally sees
# OPENNURBS_IMPORTS through libbrep's interface, but the aggregate librt object
# target cannot link to libbrep because libbrep's own object code is linked
# directly into brlcad.dll.
function(BRLCAD_AGGREGATE_DEP_COMPILE_DEFINITIONS out_var)
  set(_defs)
  foreach(_dep ${ARGN})
    set(_candidate "${_dep}")
    if("${_candidate}" MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
      set(_candidate "${CMAKE_MATCH_1}")
    endif()
    if(NOT TARGET "${_candidate}")
      continue()
    endif()
    get_target_property(_candidate_obj "${_candidate}" BRLCAD_LIBRARY_OBJECT_TARGET)
    if(NOT _candidate_obj)
      continue()
    endif()

    get_target_property(_iface_defs "${_candidate}" INTERFACE_COMPILE_DEFINITIONS)
    if(NOT _iface_defs)
      continue()
    endif()
    get_target_property(_brlcad_import_def "${_candidate}" BRLCAD_LIBRARY_IMPORT_DEFINITION)
    foreach(_def ${_iface_defs})
      if(_brlcad_import_def AND "${_def}" STREQUAL "${_brlcad_import_def}")
	continue()
      endif()
      list(APPEND _defs "${_def}")
    endforeach()
  endforeach()
  if(_defs)
    list(REMOVE_DUPLICATES _defs)
  endif()
  set(${out_var} ${_defs} PARENT_SCOPE)
endfunction(BRLCAD_AGGREGATE_DEP_COMPILE_DEFINITIONS)


# Resolve external link items while package/imported targets are still visible
# in the directory that declared the library.  Some find_package targets are not
# global, so resolving them later from src/CMakeLists.txt can fail.
function(BRLCAD_RESOLVE_AGGREGATE_LINK_ITEM out_var link_item)
  set(_resolved "${link_item}")
  if(TARGET "${link_item}")
    get_target_property(_imported "${link_item}" IMPORTED)
    if(_imported)
      set(_locs)
      # On Windows, the linker requires the .lib import library, not the .dll.
      # IMPORTED_LOCATION for shared targets points at the DLL; IMPORTED_IMPLIB
      # points at the corresponding .lib.  Prefer per-config then generic
      # IMPORTED_IMPLIB before falling back to the location-based lookup.
      if(WIN32)
	get_target_property(_configs "${link_item}" IMPORTED_CONFIGURATIONS)
	if(_configs)
	  foreach(_cfg IN LISTS _configs)
	    string(TOUPPER "${_cfg}" _cfgu)
	    get_target_property(_implib "${link_item}" "IMPORTED_IMPLIB_${_cfgu}")
	    if(_implib)
	      list(APPEND _locs "${_implib}")
	    endif()
	  endforeach()
	endif()
	if(NOT _locs)
	  get_target_property(_implib "${link_item}" IMPORTED_IMPLIB)
	  if(_implib)
	    list(APPEND _locs "${_implib}")
	  endif()
	endif()
      endif()
      if(NOT _locs)
	if(COMMAND get_tgt_locs)
	  get_tgt_locs("${link_item}" _locs)
	endif()
	if(NOT _locs)
	  get_target_property(_loc "${link_item}" IMPORTED_LOCATION)
	  if(_loc)
	    list(APPEND _locs "${_loc}")
	  endif()
        endif()
      endif()
      if(_locs)
        set(_resolved ${_locs})
      endif()
    endif()
  endif()
  set(${out_var} ${_resolved} PARENT_SCOPE)
endfunction(BRLCAD_RESOLVE_AGGREGATE_LINK_ITEM)


# Record non-BRL-CAD link dependencies for aggregate outputs.  BRL-CAD object
# code is added directly to aggregate libraries, so internal BRL-CAD target
# links must be filtered out to avoid duplicate definitions and incorrect
# Windows *_DLL_IMPORTS definitions.
function(BRLCAD_REGISTER_AGGREGATE_LINK_LIBS)
  set(_aggregate_libs)
  foreach(_link ${ARGN})
    set(_candidate "${_link}")
    if("${_candidate}" MATCHES "^\$<LINK_ONLY:([^>]+)>$")
      set(_candidate "${CMAKE_MATCH_1}")
    endif()
    if(TARGET "${_candidate}")
      get_target_property(_candidate_obj "${_candidate}" BRLCAD_LIBRARY_OBJECT_TARGET)
      if(_candidate_obj)
        continue()
      endif()
    endif()
    brlcad_resolve_aggregate_link_item(_resolved_link "${_link}")
    list(APPEND _aggregate_libs ${_resolved_link})
  endforeach()
  if(_aggregate_libs)
    set_property(GLOBAL APPEND PROPERTY BRLCAD_LIB_EXTERNAL_LINK_LIBS ${_aggregate_libs})
  endif()
endfunction(BRLCAD_REGISTER_AGGREGATE_LINK_LIBS)


# Return the external link dependencies recorded when each BRL-CAD library was
# declared.
function(BRLCAD_AGGREGATE_LINK_LIBS out_var)
  get_property(_aggregate_libs GLOBAL PROPERTY BRLCAD_LIB_EXTERNAL_LINK_LIBS)
  if(_aggregate_libs)
    list(REMOVE_DUPLICATES _aggregate_libs)
  endif()
  set(${out_var} ${_aggregate_libs} PARENT_SCOPE)
endfunction(BRLCAD_AGGREGATE_LINK_LIBS)

# Define the optional monolithic BRL-CAD library from the object libraries
# produced by BRLCAD_ADDLIB.  This intentionally depends on USE_OBJECT_LIBS:
# object targets preserve the source lists, compile definitions, and include
# setup from the normal library declarations without platform-specific archive
# extraction tricks.
function(BRLCAD_ADD_AGGREGATE_LIBRARY)
  if(NOT BRLCAD_ENABLE_BRLCAD_LIBRARY)
    return()
  endif()
  if(NOT USE_OBJECT_LIBS)
    message(FATAL_ERROR "BRLCAD_ENABLE_BRLCAD_LIBRARY requires USE_OBJECT_LIBS=ON")
  endif()

  get_property(_brlcad_obj_targets GLOBAL PROPERTY BRLCAD_LIB_OBJECT_TARGETS)
  if(NOT _brlcad_obj_targets)
    message(FATAL_ERROR "BRLCAD_ENABLE_BRLCAD_LIBRARY is ON, but no BRL-CAD object library targets were registered")
  endif()

  set(_brlcad_objs)
  foreach(_obj_target ${_brlcad_obj_targets})
    if(TARGET ${_obj_target})
      list(APPEND _brlcad_objs $<TARGET_OBJECTS:${_obj_target}>)
    endif()
  endforeach()

  add_library(libbrlcad SHARED ${_brlcad_objs})
  set_target_properties(libbrlcad PROPERTIES
    EXPORT_NAME brlcad
    FOLDER "BRL-CAD Shared Libraries"
    PREFIX ""
    OUTPUT_NAME brlcad
  )
  target_compile_definitions(libbrlcad PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  if(BRLCAD_LINKER_NO_UNDEFINED_FLAG)
    target_link_options(libbrlcad PRIVATE ${BRLCAD_LINKER_NO_UNDEFINED_FLAG})
  endif()
  include(CheckLinkerFlag)
  check_linker_flag("CXX" "-Wno-error=maybe-uninitialized" BRLCAD_AGGREGATE_LINKER_WNO_MAYBE_UNINITIALIZED)
  if(BRLCAD_AGGREGATE_LINKER_WNO_MAYBE_UNINITIALIZED)
    target_link_options(libbrlcad PRIVATE -Wno-maybe-uninitialized)
  endif()

  foreach(_obj_target ${_brlcad_obj_targets})
    if(TARGET ${_obj_target})
      add_dependencies(libbrlcad ${_obj_target})
    endif()
  endforeach()

  brlcad_aggregate_link_libs(_aggregate_libs)
  if(_aggregate_libs)
    target_link_libraries(libbrlcad PRIVATE ${_aggregate_libs})
  endif()

  install(
    TARGETS libbrlcad
    EXPORT BRLCADTargets
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR}
  )
endfunction(BRLCAD_ADD_AGGREGATE_LIBRARY)


#---------------------------------------------------------------------
# Library function handles both shared and static libs, so one
# "BRLCAD_ADDLIB" statement will cover both automatically
function(
    BRLCAD_ADDLIB
    libname
    srcslist
    include_dirs
    local_include_dirs
  )
  cmake_parse_arguments(L "SHARED;STATIC;NO_INSTALL;NO_STRICT;ALL_CXX;NO_CXX;NO_STRICT_CXX;NO_UNITY;NO_STATIC_LINK_TEST" "FOLDER" "SHARED_SRCS;STATIC_SRCS;UNITY_BUILD_SKIP;PUBLIC_LIBS;PRIVATE_LIBS;INTERFACE_LIBS" ${ARGN})

  # Let CMAKEFILES know what's going on
  cmakefiles(${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})

  # The naming convention used for variables is the upper case of the
  # library name, without the lib prefix.
  string(REPLACE "lib" "" LOWERCORE "${libname}")
  string(TOUPPER ${LOWERCORE} UPPER_CORE)

  # Go all C++ if the settings request it
  set(all_cxx OFF)
  if (ENABLE_ALL_CXX_COMPILE AND NOT L_NO_CXX)
    set(all_cxx ON)
  endif()
  if (L_ALL_CXX)
    set(all_cxx ON)
  endif()
  set_srcs_lang("${srcslist};${L_SHARED_SRCS};${L_STATIC_SRCS}" "${all_cxx}")

  # Local copy of srcslist in case manipulation is needed
  set(lsrcslist ${srcslist})

  # If we're going to have a specified subfolder, prepare the
  # appropriate string:
  if(L_FOLDER)
    set(SUBFOLDER "/${L_FOLDER}")
  endif(L_FOLDER)

  # Determine library dependency visibility from explicit lists.
  set(_public_libs ${L_PUBLIC_LIBS})
  set(_private_libs ${L_PRIVATE_LIBS})
  set(_interface_libs ${L_INTERFACE_LIBS})

  # Resolve dependencies for shared and static variants.
  brlcad_resolve_libdeps(SHARED_PUBLIC_LIBS SHARED ${_public_libs})
  brlcad_resolve_libdeps(SHARED_PRIVATE_LIBS SHARED ${_private_libs})
  brlcad_resolve_libdeps(SHARED_INTERFACE_LIBS SHARED ${_interface_libs})
  brlcad_resolve_libdeps(STATIC_PUBLIC_LIBS STATIC ${_public_libs})
  brlcad_resolve_libdeps(STATIC_PRIVATE_LIBS STATIC ${_private_libs})
  brlcad_resolve_libdeps(STATIC_INTERFACE_LIBS STATIC ${_interface_libs})

  # Set up includes
  set(PUBLIC_HDRS ${include_dirs})
  set(PRIVATE_HDRS ${local_include_dirs})

  # If we need it, set up the OBJECT library build used by the normal
  # per-library shared/static targets.  Link the object target to its deps so
  # Windows builds inherit required *_DLL_IMPORTS usage requirements.
  if(USE_OBJECT_LIBS)
    add_library(${libname}-obj OBJECT ${lsrcslist})
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname}-obj PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")
    set(lsrcslist $<TARGET_OBJECTS:${libname}-obj>)
    set_target_properties(${libname}-obj PROPERTIES FOLDER "BRL-CAD OBJECT Libraries${SUBFOLDER}")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libname}-obj PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    # Set includes on obj target
    brlcad_include_dirs(${libname}-obj PUBLIC_HDRS PUBLIC)
    brlcad_include_dirs(${libname}-obj PRIVATE_HDRS PRIVATE)

    if(HIDE_INTERNAL_SYMBOLS)
      set_property(TARGET ${libname}-obj APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)

    set(_obj_deps ${SHARED_PUBLIC_LIBS} ${SHARED_PRIVATE_LIBS} ${SHARED_INTERFACE_LIBS})
    if(NOT L_NO_INSTALL)
      brlcad_register_aggregate_link_libs(${_obj_deps})
    endif()
    if(_obj_deps)
      target_link_libraries(${libname}-obj PRIVATE ${_obj_deps})
      foreach(ll ${_obj_deps})
        if(TARGET ${ll})
          add_dependencies(${libname}-obj ${ll})
        endif(TARGET ${ll})
      endforeach(ll ${_obj_deps})
    endif(_obj_deps)

    # Aggregate objects are compiled separately from the normal object target.
    # On Windows the normal object target must import symbols from dependent
    # BRL-CAD DLLs, while the aggregate target must not import those symbols
    # because they are linked into the same brlcad DLL.
    if(BRLCAD_ENABLE_BRLCAD_LIBRARY AND NOT L_NO_INSTALL)
      add_library(${libname}-brlcad-obj OBJECT ${srcslist} ${L_SHARED_SRCS})
      if(${libname} MATCHES "^lib*")
        set_target_properties(${libname}-brlcad-obj PROPERTIES PREFIX "")
      endif(${libname} MATCHES "^lib*")
      set_target_properties(${libname}-brlcad-obj PROPERTIES FOLDER "BRL-CAD OBJECT Libraries/Aggregate${SUBFOLDER}")
      target_compile_definitions(${libname}-brlcad-obj PRIVATE BRLCADBUILD HAVE_CONFIG_H)
      brlcad_include_dirs(${libname}-brlcad-obj PUBLIC_HDRS PUBLIC)
      brlcad_include_dirs(${libname}-brlcad-obj PRIVATE_HDRS PRIVATE)
      if(HIDE_INTERNAL_SYMBOLS)
        set_property(TARGET ${libname}-brlcad-obj APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
      endif(HIDE_INTERNAL_SYMBOLS)
      brlcad_filter_aggregate_deps(_aggregate_obj_deps ${_obj_deps})
      brlcad_aggregate_dep_compile_definitions(_aggregate_dep_defs ${_obj_deps})
      if(_aggregate_dep_defs)
        target_compile_definitions(${libname}-brlcad-obj PRIVATE ${_aggregate_dep_defs})
      endif(_aggregate_dep_defs)
      if(_aggregate_obj_deps)
        target_link_libraries(${libname}-brlcad-obj PRIVATE ${_aggregate_obj_deps})
      endif(_aggregate_obj_deps)
    endif()

    # Apply unity (jumbo) build batching to the object library when the global
    # option is enabled.  Any source files listed in UNITY_BUILD_SKIP are
    # compiled individually so that file-scope symbol conflicts do not arise.
    # Targets with fewer than 8 source files are skipped: the overhead of
    # unity batching is not worth it for small source sets.
    if(BRLCAD_ENABLE_UNITY_BUILD AND NOT L_NO_UNITY)
      set(_all_srcs ${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})
      list(LENGTH _all_srcs _srcs_count)
      if(_srcs_count GREATER_EQUAL 8)
        set_target_properties(${libname}-obj PROPERTIES UNITY_BUILD ON)
        if(L_UNITY_BUILD_SKIP)
          set_source_files_properties(${L_UNITY_BUILD_SKIP} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
        endif(L_UNITY_BUILD_SKIP)
      endif(_srcs_count GREATER_EQUAL 8)
    endif()
  endif(USE_OBJECT_LIBS)

  # Handle the shared library
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
    add_library(${libname} SHARED ${lsrcslist} ${L_SHARED_SRCS})
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname} PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")

    # Set the EXPORT_NAME so installed targets appear as BRLCAD::<short>
    # (e.g. BRLCAD::bu rather than BRLCAD::libbu).
    set_target_properties(${libname} PROPERTIES EXPORT_NAME ${LOWERCORE})

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libname} PRIVATE BRLCADBUILD HAVE_CONFIG_H)
    if(BRLCAD_LINKER_NO_UNDEFINED_FLAG)
      target_link_options(${libname} PRIVATE ${BRLCAD_LINKER_NO_UNDEFINED_FLAG})
    endif(BRLCAD_LINKER_NO_UNDEFINED_FLAG)

    # Set includes on shared target.
    # brlcad_include_dirs() adds paths with correct ordering and SYSTEM flags;
    # this populates both INCLUDE_DIRECTORIES (for building the lib itself) and
    # INTERFACE_INCLUDE_DIRECTORIES (for consumers).  The raw absolute paths it
    # puts in INTERFACE_INCLUDE_DIRECTORIES must be replaced with generator
    # expressions so that the exported BRLCADTargets.cmake is relocatable.
    brlcad_include_dirs(${libname} PUBLIC_HDRS PUBLIC)
    brlcad_include_dirs(${libname} PRIVATE_HDRS PRIVATE)

    # Replace raw absolute paths in INTERFACE_INCLUDE_DIRECTORIES with
    # BUILD_INTERFACE-guarded versions and add the INSTALL_INTERFACE entry.
    # INCLUDE_DIRECTORIES (used to compile this target) is left untouched.
    # Paths that are already generator expressions (propagated from deps)
    # are dropped here — they remain accessible transitively through
    # INTERFACE_LINK_LIBRARIES and re-wrapping causes nested genexprs.
    get_target_property(_raw_iface_dirs ${libname} INTERFACE_INCLUDE_DIRECTORIES)
    set_property(TARGET ${libname} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    if(_raw_iface_dirs)
      foreach(_dir ${_raw_iface_dirs})
        if(NOT _dir MATCHES "^\\$<")
          target_include_directories(${libname} INTERFACE $<BUILD_INTERFACE:${_dir}>)
        endif()
      endforeach()
    endif()
    target_include_directories(${libname} INTERFACE
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/brlcad>)

    # INTERFACE_SYSTEM_INCLUDE_DIRECTORIES can accumulate raw build-tree paths
    # from transitive deps whose find-modules set INTERFACE_INCLUDE_DIRECTORIES
    # to absolute paths (e.g. Geogram::geogram, OPENNURBS::OPENNURBS).  Wrap
    # every raw path in $<BUILD_INTERFACE:...> so it is only visible when
    # consuming the build tree; install-tree consumers re-create those targets
    # via find_dependency() in BRLCADConfig.cmake and therefore get the correct
    # include dirs from those re-found targets instead.
    get_target_property(_raw_sys_dirs ${libname} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    if(_raw_sys_dirs)
      set_property(TARGET ${libname} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
      foreach(_sdir ${_raw_sys_dirs})
        if(NOT _sdir MATCHES "^\\$<")
          set_property(TARGET ${libname} APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${_sdir}>)
        else()
          set_property(TARGET ${libname} APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${_sdir})
        endif()
      endforeach()
    endif()

    # Strip raw build-tree paths from INTERFACE_LINK_LIBRARIES on the
    # shared-library target.  For a shared library the ELF DT_NEEDED
    # entries already encode all runtime dependencies; a raw absolute
    # path to a bundled library in the build tree is therefore both
    # redundant and harmful in the installed BRLCADTargets.cmake (it
    # references a path that does not exist on consumers' machines).
    # Named targets (BRLCAD::*, ZLIB::*, etc.) and generator expressions
    # are preserved; only bare paths are removed.
    get_target_property(_raw_iface_libs ${libname} INTERFACE_LINK_LIBRARIES)
    if(_raw_iface_libs)
      set_property(TARGET ${libname} PROPERTY INTERFACE_LINK_LIBRARIES)
      foreach(_lib ${_raw_iface_libs})
        if(_lib MATCHES "^/" OR _lib MATCHES "^[A-Za-z]:\\\\")
          # Raw absolute path — guard it so it is only used from the build tree.
          set_property(TARGET ${libname} APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES $<BUILD_INTERFACE:${_lib}>)
        else()
          set_property(TARGET ${libname} APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ${_lib})
        endif()
      endforeach()
    endif()

    if(HIDE_INTERNAL_SYMBOLS)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
      set_target_properties(${libname} PROPERTIES BRLCAD_LIBRARY_IMPORT_DEFINITION "${UPPER_CORE}_DLL_IMPORTS")
      # Use target_compile_definitions so the INTERFACE entry is exported via
      # install(EXPORT) and consumers automatically get the right import macro.
      target_compile_definitions(${libname} INTERFACE "${UPPER_CORE}_DLL_IMPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)

    # Enable unity build on the shared library target.  When USE_OBJECT_LIBS is
    # set the main object library (-obj) already covers the srcslist; the
    # shared target is also enabled so that any sources added later via
    # target_sources() (e.g. plugin objects) are also batched.
    # Targets with fewer than 8 source files are skipped.
    if(BRLCAD_ENABLE_UNITY_BUILD AND NOT L_NO_UNITY)
      set(_all_srcs ${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})
      list(LENGTH _all_srcs _srcs_count)
      if(_srcs_count GREATER_EQUAL 8)
        set_target_properties(${libname} PROPERTIES UNITY_BUILD ON)
        if(L_UNITY_BUILD_SKIP)
          set_source_files_properties(${L_UNITY_BUILD_SKIP} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
        endif(L_UNITY_BUILD_SKIP)
      endif(_srcs_count GREATER_EQUAL 8)
    endif()

  endif(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))

  if(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))
    if(L_STATIC)
      set(libstatic ${libname})
    else(L_STATIC)
      set(libstatic ${libname}-static)
    endif(L_STATIC)
    add_library(${libstatic} STATIC ${lsrcslist} ${L_STATIC_SRCS})
    if(${libstatic} MATCHES "^lib*")
      set_target_properties(${libstatic} PROPERTIES PREFIX "")
    endif(${libstatic} MATCHES "^lib*")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libstatic} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    # Set includes on static target
    brlcad_include_dirs(${libstatic} PUBLIC_HDRS PUBLIC)
    brlcad_include_dirs(${libstatic} PRIVATE_HDRS PRIVATE)

    if(NOT MSVC)
      set_target_properties(${libstatic} PROPERTIES OUTPUT_NAME "${libname}")
    endif(NOT MSVC)

    # Enable unity build on the static library target when not going through
    # an object library.
    # Targets with fewer than 8 source files are skipped.
    if(BRLCAD_ENABLE_UNITY_BUILD AND NOT L_NO_UNITY)
      set(_all_srcs ${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})
      list(LENGTH _all_srcs _srcs_count)
      if(_srcs_count GREATER_EQUAL 8)
        set_target_properties(${libstatic} PROPERTIES UNITY_BUILD ON)
        if(L_UNITY_BUILD_SKIP)
          set_source_files_properties(${L_UNITY_BUILD_SKIP} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
        endif(L_UNITY_BUILD_SKIP)
      endif(_srcs_count GREATER_EQUAL 8)
    endif()

  endif(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))

  # Make sure we don't end up with outputs named liblib...
  set(possible_targets ${libname} ${libname}-static)
  foreach(pt ${possible_targets})
    if(TARGET ${pt} AND ${pt} MATCHES "^lib*")
      set_target_properties(${pt} PROPERTIES PREFIX "")
    endif(TARGET ${pt} AND ${pt} MATCHES "^lib*")
  endforeach(pt ${possible_targets})

  # Extra static lib specific work
  if(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))
    # Make sure the target depends on any targets in the dependency lists
    foreach(ll ${STATIC_PUBLIC_LIBS} ${STATIC_PRIVATE_LIBS} ${STATIC_INTERFACE_LIBS})
      if(TARGET ${ll})
        add_dependencies(${libstatic} ${ll})
      endif(TARGET ${ll})
    endforeach(ll ${STATIC_PUBLIC_LIBS} ${STATIC_PRIVATE_LIBS} ${STATIC_INTERFACE_LIBS})

    if(STATIC_PUBLIC_LIBS)
      target_link_libraries(${libstatic} PUBLIC ${STATIC_PUBLIC_LIBS})
    endif(STATIC_PUBLIC_LIBS)
    if(STATIC_PRIVATE_LIBS)
      target_link_libraries(${libstatic} PRIVATE ${STATIC_PRIVATE_LIBS})
    endif(STATIC_PRIVATE_LIBS)
    if(STATIC_INTERFACE_LIBS)
      target_link_libraries(${libstatic} INTERFACE ${STATIC_INTERFACE_LIBS})
    endif(STATIC_INTERFACE_LIBS)

    set_target_properties(${libstatic} PROPERTIES FOLDER "BRL-CAD Static Libraries${SUBFOLDER}")
    validate_style("${libstatic}" "${srcslist};${L_STATIC_SRCS}")
    if (NOT L_NO_STATIC_LINK_TEST)
      brlcad_add_static_link_test(${libstatic} ${STATIC_PUBLIC_LIBS} ${STATIC_PRIVATE_LIBS} ${STATIC_INTERFACE_LIBS})
    endif (NOT L_NO_STATIC_LINK_TEST)

    # ----------------------------------------------------------------
    # Export setup for static targets
    #
    # L_STATIC means this library is built static-only (no shared twin);
    # it belongs in the primary BRLCADTargets export under the plain
    # short name (e.g. BRLCAD::bu).
    #
    # When both shared and static are built (BUILD_STATIC_LIBS), the
    # static variant gets a "-static" suffix (e.g. BRLCAD::bu-static)
    # and goes into the separate BRLCADStaticTargets export so consumers
    # can opt in via BRLCAD_USE_STATIC_LIBS=ON.
    if(L_STATIC)
      set_target_properties(${libstatic} PROPERTIES EXPORT_NAME ${LOWERCORE})
      set(_static_export_set  BRLCADTargets)
    else()
      set_target_properties(${libstatic} PROPERTIES EXPORT_NAME ${LOWERCORE}-static)
      set(_static_export_set  BRLCADStaticTargets)
    endif()

    # Fix INTERFACE_INCLUDE_DIRECTORIES for export: replace the raw
    # absolute paths set by brlcad_include_dirs() with BUILD_INTERFACE-
    # guarded genexprs and add the INSTALL_INTERFACE entry.
    # Paths that are already generator expressions (propagated from dep
    # targets) are dropped here — they are served transitively through
    # INTERFACE_LINK_LIBRARIES and re-adding them causes nested genexprs.
    get_target_property(_raw_siface_dirs ${libstatic} INTERFACE_INCLUDE_DIRECTORIES)
    set_property(TARGET ${libstatic} PROPERTY INTERFACE_INCLUDE_DIRECTORIES)
    if(_raw_siface_dirs)
      foreach(_dir ${_raw_siface_dirs})
        if(NOT _dir MATCHES "^\\$<")
          target_include_directories(${libstatic} INTERFACE $<BUILD_INTERFACE:${_dir}>)
        endif()
      endforeach()
    endif()
    target_include_directories(${libstatic} INTERFACE
      $<INSTALL_INTERFACE:include>
      $<INSTALL_INTERFACE:include/brlcad>)

    # Same fix for INTERFACE_SYSTEM_INCLUDE_DIRECTORIES on the static target.
    get_target_property(_raw_ssys_dirs ${libstatic} INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
    if(_raw_ssys_dirs)
      set_property(TARGET ${libstatic} PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES)
      foreach(_sdir ${_raw_ssys_dirs})
        if(NOT _sdir MATCHES "^\\$<")
          set_property(TARGET ${libstatic} APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<BUILD_INTERFACE:${_sdir}>)
        else()
          set_property(TARGET ${libstatic} APPEND PROPERTY
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${_sdir})
        endif()
      endforeach()
    endif()

    # Strip raw build-tree paths from INTERFACE_LINK_LIBRARIES on the
    # static target.  Guard them with $<BUILD_INTERFACE:...> so they are
    # only visible when building in-tree.
    get_target_property(_raw_siface_libs ${libstatic} INTERFACE_LINK_LIBRARIES)
    if(_raw_siface_libs)
      set_property(TARGET ${libstatic} PROPERTY INTERFACE_LINK_LIBRARIES)
      foreach(_lib ${_raw_siface_libs})
        if(_lib MATCHES "^/" OR _lib MATCHES "^[A-Za-z]:\\\\")
          set_property(TARGET ${libstatic} APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES $<BUILD_INTERFACE:${_lib}>)
        else()
          set_property(TARGET ${libstatic} APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ${_lib})
        endif()
      endforeach()
    endif()

    if(NOT L_NO_INSTALL)
      install(
        TARGETS ${libstatic}
        EXPORT ${_static_export_set}
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR}
      )
    endif(NOT L_NO_INSTALL)
  endif(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))

  if(NOT L_NO_INSTALL)
    set_target_properties(${libname} PROPERTIES BRLCAD_LIBRARY_OBJECT_TARGET "${libname}-obj")
    brlcad_register_library_target(${libname})
  endif()

  # Extra shared lib specific work
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
    set_target_properties(${libname} PROPERTIES FOLDER "BRL-CAD Shared Libraries${SUBFOLDER}")
    validate_style("${libname}" "${srcslist};${L_SHARED_SRCS}")
    if(SHARED_PUBLIC_LIBS)
      target_link_libraries(${libname} PUBLIC ${SHARED_PUBLIC_LIBS})
    endif(SHARED_PUBLIC_LIBS)
    if(SHARED_PRIVATE_LIBS)
      target_link_libraries(${libname} PRIVATE ${SHARED_PRIVATE_LIBS})
    endif(SHARED_PRIVATE_LIBS)
    if(SHARED_INTERFACE_LIBS)
      target_link_libraries(${libname} INTERFACE ${SHARED_INTERFACE_LIBS})
    endif(SHARED_INTERFACE_LIBS)
    if(NOT L_NO_INSTALL)
      install(
        TARGETS ${libname}
        EXPORT BRLCADTargets
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR}
      )
    endif(NOT L_NO_INSTALL)
  endif(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
endfunction(BRLCAD_ADDLIB)


#-----------------------------------------------------------------------------
# We need a way to tell whether one path is a subpath of another path without
# relying on regular expressions, since file paths may have characters in them
# that will trigger regex matching behavior when we don't want it.  (To test,
# for example, use a build directory name of build++)
#
# Sets ${result_var} to 1 if the candidate subpath is actually a subpath of
# the supplied "full" path, otherwise sets it to 0.
#
# The routine below does the check without using regex matching, in order to
# handle path names that contain characters that would be interpreted as active
# in a regex string.
if(NOT COMMAND IS_SUBPATH)
  function(IS_SUBPATH candidate_subpath full_path result_var)
    string(SHA1 _subpath_cache_key "${candidate_subpath}|${full_path}")
    get_property(_subpath_cache_set GLOBAL PROPERTY "BRLCAD_IS_SUBPATH_${_subpath_cache_key}" SET)
    if(_subpath_cache_set)
      get_property(_subpath_cache_result GLOBAL PROPERTY "BRLCAD_IS_SUBPATH_${_subpath_cache_key}")
      set(${result_var} ${_subpath_cache_result} PARENT_SCOPE)
      return()
    endif()

    # get the CMake form of the path so we have something consistent to work on
    file(TO_CMAKE_PATH "${full_path}" c_full_path)
    file(TO_CMAKE_PATH "${candidate_subpath}" c_candidate_subpath)

    # Just assume it isn't until we prove it is
    set(_subpath_result 0)

    # check the string lengths - if the "subpath" is longer than the full path,
    # there's not point in going further
    string(LENGTH "${c_full_path}" FULL_LENGTH)
    string(LENGTH "${c_candidate_subpath}" SUB_LENGTH)
    if("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")
      set_property(GLOBAL PROPERTY "BRLCAD_IS_SUBPATH_${_subpath_cache_key}" "${_subpath_result}")
      set(${result_var} ${_subpath_result} PARENT_SCOPE)
      return()
    endif("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")

    # OK, maybe it's a subpath - time to actually check
    string(SUBSTRING "${c_full_path}" 0 ${SUB_LENGTH} c_full_subpath)
    if(NOT "${c_full_subpath}" STREQUAL "${c_candidate_subpath}")
      set_property(GLOBAL PROPERTY "BRLCAD_IS_SUBPATH_${_subpath_cache_key}" "${_subpath_result}")
      set(${result_var} ${_subpath_result} PARENT_SCOPE)
      return()
    endif(NOT "${c_full_subpath}" STREQUAL "${c_candidate_subpath}")

    # If we get here, it's a subpath
    set(_subpath_result 1)
    set_property(GLOBAL PROPERTY "BRLCAD_IS_SUBPATH_${_subpath_cache_key}" "${_subpath_result}")
    set(${result_var} ${_subpath_result} PARENT_SCOPE)
  endfunction(IS_SUBPATH)
endif(NOT COMMAND IS_SUBPATH)


#---------------------------------------------------------------------
# Files needed by BRL-CAD must be installed and copied (or symlinked,
# if supported) to the build dirs for executables to run during
# the build. Errors during execution should reference source tree
# files for easier editing.
#
# BRLCAD_MANAGE_FILES defines custom commands and uses full file paths
# as dependencies to prevent conflicts with build target names (e.g.,
# "INSTALL" file conflicts with "INSTALL" target on MSVC). This
# ensures robust file management and triggers updates when source
# files change (e.g., regenerating tclIndex and pkgIndex.tcl).
###
function(BRLCAD_MANAGE_FILES inputdata targetdir)
  cmake_parse_arguments(M "" "" "REQUIRED;TRIGGER" ${ARGN})

  # Handle both a list of one or more files and variable holding a
  # list of files - find out what we've got.
  normalize_file_list("${inputdata}" RLIST datalist FPLIST fullpath_datalist TARGET targetname)

  # Identify the source files for CMake
  cmakefiles(${datalist})

  if(M_REQUIRED)
    foreach(mt ${M_REQUIRED})
      if(NOT TARGET ${mt})
        return()
      endif(NOT TARGET ${mt})
    endforeach(mt ${M_REQUIRED})
  endif(M_REQUIRED)

  # Unlike REQUIRED, a TRIGGER option indicates we DON'T need this target
  # unless at least one of the listed targets is defined
  if(M_TRIGGER)
    set(IS_ENABLED 0)
    foreach(mt ${M_TRIGGER})
      if(TARGET ${mt})
        set(IS_ENABLED 1)
      endif(TARGET ${mt})
    endforeach(mt ${M_TRIGGER})
    if(NOT IS_ENABLED)
      return()
    endif(NOT IS_ENABLED)
  endif(M_TRIGGER)

  if(NOT TARGET managed_files)
    add_custom_target(managed_files ALL)
    set_target_properties(managed_files PROPERTIES FOLDER "BRL-CAD File Copying")
  endif(NOT TARGET managed_files)

  string(RANDOM LENGTH 10 ALPHABET 0123456789 VAR_PREFIX)
  if(${ARGC} GREATER 2)
    cmake_parse_arguments(${VAR_PREFIX} "EXEC" "FOLDER" "" ${ARGN})
  endif(${ARGC} GREATER 2)

  #-------------------------------------------------------------------
  # Some of the more advanced build system features in BRL-CAD's CMake
  # build need to know whether symlink support is present on the
  # current OS - go ahead and do this test up front, caching the
  # results.
  if(NOT DEFINED HAVE_SYMLINK)
    message("--- Checking operating system support for file symlinking")
    file(WRITE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "testing for symlink ability")
    file(
      CREATE_LINK "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src"
      "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest"
      RESULT _brlcad_symlink_test_result
      SYMBOLIC
    )
    if("${_brlcad_symlink_test_result}" STREQUAL "0" AND EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Supported")
      set(HAVE_SYMLINK 1 CACHE BOOL "Platform supports creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
    else("${_brlcad_symlink_test_result}" STREQUAL "0" AND EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Unsupported")
      set(HAVE_SYMLINK 0 CACHE BOOL "Platform does not support creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src")
    endif("${_brlcad_symlink_test_result}" STREQUAL "0" AND EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
  endif(NOT DEFINED HAVE_SYMLINK)

  # Now that the input data and target names are in order, define the
  # custom commands needed for build directory data copying on this
  # platform (per symlink test results.)

  if(HAVE_SYMLINK)
    # Make sure the target directory exists (symlinks need the target
    # directory already in place)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${targetdir}")

    # Using symlinks - in this case, the custom command doesn't
    # actually have to do the work every time the source file changes
    # - once established, the symlink will behave correctly.  That
    # being the case, we just go ahead and establish the symlinks in
    # the configure stage.
    foreach(filename ${fullpath_datalist})
      get_filename_component(ITEM_NAME ${filename} NAME)
      set(_brlcad_link_path "${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME}")
      set(_brlcad_create_link 1)
      if(IS_SYMLINK "${_brlcad_link_path}")
        file(READ_SYMLINK "${_brlcad_link_path}" _brlcad_link_target)
        if("${_brlcad_link_target}" STREQUAL "${filename}")
          set(_brlcad_create_link 0)
        else()
          file(REMOVE "${_brlcad_link_path}")
        endif()
      elseif(EXISTS "${_brlcad_link_path}")
        set(_brlcad_create_link 0)
      endif()
      if(_brlcad_create_link)
        file(CREATE_LINK "${filename}" "${_brlcad_link_path}" SYMBOLIC)
      endif()
    endforeach(filename ${fullpath_datalist})

    # Check for and remove dead symbolic links from a previous run.  This scan
    # is per target directory, not per managed-files call.
    string(SHA1 _brlcad_link_scan_key "${CMAKE_BINARY_DIR}/${targetdir}")
    get_property(_brlcad_link_scan_done GLOBAL PROPERTY "BRLCAD_MANAGED_LINK_SCAN_${_brlcad_link_scan_key}" SET)
    if(NOT _brlcad_link_scan_done)
      set_property(GLOBAL PROPERTY "BRLCAD_MANAGED_LINK_SCAN_${_brlcad_link_scan_key}" 1)
      file(GLOB listing LIST_DIRECTORIES false "${CMAKE_BINARY_DIR}/${targetdir}/*")
      foreach(filename ${listing})
        if(IS_SYMLINK "${filename}" AND NOT EXISTS "${filename}")
          message("Removing stale symbolic link ${filename}")
          file(REMOVE "${filename}")
        endif()
      endforeach(filename ${listing})
    endif()

    # The custom command is still necessary - since it depends on the
    # original source files, this will be the trigger that tells other
    # commands depending on this data that they need to re-run when
    # one of the source files is changed.
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      DEPENDS ${fullpath_datalist}
    )
  else(HAVE_SYMLINK)
    # Write out script for copying from source dir to build dir
    set(BRLCAD_COPY_FILES "${fullpath_datalist}")
    set(BRLCAD_COPY_DESTINATION "${CMAKE_BINARY_DIR}/${targetdir}")
    configure_file(
      "${BRLCAD_CMAKE_DIR}/BRLCAD_Copy_Files.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake"
      @ONLY
    )

    # Define custom command for copying from src to bin.
    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake"
      COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel"
      DEPENDS ${fullpath_datalist}
    )
  endif(HAVE_SYMLINK)

  # Define the target and add it to this directories list of data
  # targets
  add_custom_target(${targetname}_cp ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.sentinel")
  set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Copying")
  brlcad_add_dir_list_entry(DATA_TARGETS "${CMAKE_CURRENT_BINARY_DIR}" ${targetname}_cp)

  # Because the target name for managed files is likely cryptic, we
  # add a dependency to the managed_files target so this poriton of
  # the logic can be referenced
  add_dependencies(managed_files ${targetname}_cp)

  # Set the FOLDER property.  If the target has supplied a folder, use
  # that as a subfolder
  if("${${VAR_PREFIX}_FOLDER}" STREQUAL "")
    set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Setup")
  else("${${VAR_PREFIX}_FOLDER}" STREQUAL "")
    set_target_properties(${targetname}_cp PROPERTIES FOLDER "BRL-CAD File Setup/${${VAR_PREFIX}_FOLDER}")
  endif("${${VAR_PREFIX}_FOLDER}" STREQUAL "")

  # Add outputs to the distclean rules - this is consistent regardless
  # of what type the output file is, symlink or copy.  Just need to
  # handle the single and multiconfig cases.
  foreach(filename ${fullpath_datalist})
    get_filename_component(ITEM_NAME "${filename}" NAME)
    distclean("${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME}")
  endforeach(filename ${fullpath_datalist})

  # The installation rule relates only to the original source
  # directory copy, and so doesn't need to explicitly concern itself
  # with configurations.
  if(${VAR_PREFIX}_EXEC)
    install(PROGRAMS ${datalist} DESTINATION ${targetdir})
  else(${VAR_PREFIX}_EXEC)
    install(FILES ${datalist} DESTINATION ${targetdir})
  endif(${VAR_PREFIX}_EXEC)
endfunction(BRLCAD_MANAGE_FILES)


#-----------------------------------------------------------------------------
# Specific uses of the BRLCAD_MANAGE_FILES functionality - these
# cover most of the common cases in BRL-CAD.

function(BRLCAD_ADDDATA datalist targetdir)
  brlcad_manage_files(${datalist} ${DATA_DIR}/${targetdir})
endfunction(BRLCAD_ADDDATA)


function(ADD_DOC doclist targetdir)
  cmake_parse_arguments(A "" "" "REQUIRED" ${ARGN})

  # Identify the source files for CMake
  cmakefiles(${${doclist}})

  if(A_REQUIRED)
    foreach(rdep ${A_REQUIRED})
      if(NOT TARGET ${rdep})
        return()
      endif(NOT TARGET ${rdep})
    endforeach(rdep ${A_REQUIRED})
  endif(A_REQUIRED)

  if(BRLCAD_INSTALL_DOCS)
    set(doc_target_dir ${DOC_DIR}/${targetdir})
    brlcad_manage_files(${doclist} ${doc_target_dir})
  endif(BRLCAD_INSTALL_DOCS)
endfunction(ADD_DOC)


function(ADD_MAN_PAGES num inmanlist)
  if(BRLCAD_INSTALL_DOCS)
    set(man_target_dir ${MAN_DIR}/man${num})
    brlcad_manage_files(${inmanlist} ${man_target_dir})
  endif(BRLCAD_INSTALL_DOCS)
endfunction(ADD_MAN_PAGES)


#--------------------------------------------------------------------
# Regression tests are designed to be executed by a parent CMake
# script, which captures the I/O and stores it in a log file named
# after the test.  By default, tests use ${testname}.cmake.in in the
# source directory unless a TEST_SCRIPT is explicitly provided.
#
# For configuration-dependent builds, specify the executable's target
# name via the EXEC option, using $<TARGET_FILE:${${testname}_EXEC}>
# to ensure the correct program version is run. Scripts must handle
# unquoting paths with special characters.
#
# The TEST_DEFINED option allows skipping add_test and custom command
# setup, leaving test definition to the caller. BRLCAD_REGRESSION_TEST
# will only define the build target and wire the test into
# higher-level commands.
#
# Standard regression test setup:
#
# 1. Defines custom build target "regress-${testname}" for each test.
#
# 2. Adds a label denoting inclusion in "check" and "regress" targets.
#
# 3. Adds ${depends_list} as build requirements, ensuring deps are
#    built before tests run.
#
# 4. STAND_ALONE keyword defines ${testname} without connecting it to
#    the agglomeration targets (i.e., "regress").
#
# 5. TIMEOUT arg sets a test timeout; otherwise a default is used.
#
###
function(BRLCAD_REGRESSION_TEST testname depends_list)
  cmake_parse_arguments(${testname} "TEST_DEFINED;STAND_ALONE" "TEST_SCRIPT;TIMEOUT;EXEC;VEXEC" "" ${ARGN})

  if(NOT BUILD_TESTING)
    return()
  endif(NOT BUILD_TESTING)

  if(depends_list)
    foreach(dep ${depends_list})
      if(NOT TARGET ${dep})
        return()
      endif(NOT TARGET ${dep})
    endforeach(dep ${depends_list})
  endif(depends_list)

  if(NOT ${testname}_TEST_DEFINED)
    # Test isn't yet defined - do the add_test setup
    if(${testname}_TEST_SCRIPT)
      configure_file("${${testname}_TEST_SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake" @ONLY)
    else(${testname}_TEST_SCRIPT)
      configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/${testname}.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake"
        @ONLY
      )
    endif(${testname}_TEST_SCRIPT)
    distclean("${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake")

    if(TARGET ${${testname}_EXEC})
      if(TARGET ${${testname}_VEXEC})
        add_test(
          NAME ${testname}
          COMMAND
          "${CMAKE_COMMAND}" -DEXEC=$<TARGET_FILE:${${testname}_EXEC}> -DVEXEC=$<TARGET_FILE:${${testname}_VEXEC}> -P
          "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake"
	)
      else(TARGET ${${testname}_VEXEC})
        add_test(
          NAME ${testname}
          COMMAND
          "${CMAKE_COMMAND}" -DEXEC=$<TARGET_FILE:${${testname}_EXEC}> -P
          "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake"
	)
      endif(TARGET ${${testname}_VEXEC})
    else(TARGET ${${testname}_EXEC})
      add_test(NAME ${testname} COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/${testname}.cmake")
    endif(TARGET ${${testname}_EXEC})
  endif(NOT ${testname}_TEST_DEFINED)

  # Every regression test gets a build target
  add_custom_target(${testname} COMMAND ${CMAKE_CTEST_COMMAND} -R ^${testname}$ --output-on-failure VERBATIM)
  if(depends_list)
    add_dependencies(${testname} ${depends_list})
  endif(depends_list)

  # Make sure we at least get this into the regression test folder - local
  # subdirectories may override this if they have more specific locations they
  # want to use. We set the default folder based on the relative file path
  get_filename_component(TDIR "${CMAKE_CURRENT_SOURCE_DIR}" NAME_WE)
  set_target_properties(${testname} PROPERTIES FOLDER "BRL-CAD Regression Tests/${TDIR}")

  # In Visual Studio, none of the regress build targets are added to
  # the default build.
  set_target_properties(${testname} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

  # Group any test not excluded by the STAND_ALONE flag with the other
  # regression tests by assigning a standard label
  if(NOT ${testname}_STAND_ALONE)
    set_tests_properties(${testname} PROPERTIES LABELS "Regression")
  else(NOT ${testname}_STAND_ALONE)
    set_tests_properties(${testname} PROPERTIES LABELS "STAND_ALONE")
  endif(NOT ${testname}_STAND_ALONE)

  # Set up dependencies for both regress and check
  if(NOT "${depends_list}" STREQUAL "")
    add_dependencies(regress ${depends_list})
    add_dependencies(check ${depends_list})
  endif(NOT "${depends_list}" STREQUAL "")

  # Set either the standard timeout or a specified custom timeout
  if(${testname}_TIMEOUT)
    set_tests_properties(${testname} PROPERTIES TIMEOUT ${${testname}_TIMEOUT})
  else(${testname}_TIMEOUT)
    set_tests_properties(${testname} PROPERTIES TIMEOUT 300) # FIXME: want <60
  endif(${testname}_TIMEOUT)
endfunction(BRLCAD_REGRESSION_TEST)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
