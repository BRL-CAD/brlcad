#              B R L C A D _ T A R G E T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2025 United States Government as represented by
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

# Need sophisticated option parsing
include(CMakeParseArguments)

# When defining targets, we need to know if we have a no-error flag
include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
check_c_compiler_flag(-Wno-error NOERROR_FLAG_C)
check_cxx_compiler_flag(-Wno-error NOERROR_FLAG_CXX)

# For BRL-CAD targets, use CXX as the language if the user requests it
function(SET_LANG_CXX SRC_FILES)
  if(ENABLE_ALL_CXX_COMPILE)
    foreach(srcfile ${SRC_FILES})
      if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
        set_source_files_properties(${srcfile} PROPERTIES LANGUAGE CXX)
      endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${srcfile}")
    endforeach(srcfile ${SRC_FILES})
  endif(ENABLE_ALL_CXX_COMPILE)
endfunction(SET_LANG_CXX SRC_FILES)

# See if we've got astyle
find_program(ASTYLE_EXECUTABLE astyle HINTS "${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR}")

# BRL-CAD style checking with AStyle
function(VALIDATE_STYLE targetname srcslist)
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

define_property(
  GLOBAL
  PROPERTY BRLCAD_EXEC_FILES
  BRIEF_DOCS "BRL-CAD binaries"
  FULL_DOCS "List of installed BRL-CAD binary programs"
)

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

  foreach(inc_dir ${INCLUDE_DIRS})
    get_filename_component(abs_inc_dir ${inc_dir} ABSOLUTE)
    is_subpath("${BRLCAD_SOURCE_DIR}" "${abs_inc_dir}" IS_LOCAL)
    if(NOT IS_LOCAL)
      is_subpath("${BRLCAD_BINARY_DIR}" "${abs_inc_dir}" IS_LOCAL)
    endif(NOT IS_LOCAL)
    set(IS_SYSPATH 0)
    foreach(sp ${SYS_INCLUDE_PATTERNS})
      if("${inc_dir}" MATCHES "${sp}")
        set(IS_SYSPATH 1)
      endif("${inc_dir}" MATCHES "${sp}")
    endforeach(sp ${SYS_INCLUDE_PATTERNS})
    if(IS_SYSPATH OR NOT IS_LOCAL)
      if(IS_SYSPATH)
        target_include_directories(${targetname} SYSTEM ${itype} ${inc_dir})
      else(IS_SYSPATH)
        target_include_directories(${targetname} SYSTEM AFTER ${itype} ${inc_dir})
      endif(IS_SYSPATH)
    else(IS_SYSPATH OR NOT IS_LOCAL)
      target_include_directories(${targetname} BEFORE ${itype} ${inc_dir})
    endif(IS_SYSPATH OR NOT IS_LOCAL)
  endforeach(inc_dir ${INCLUDE_DIRS})
endfunction(BRLCAD_INCLUDE_DIRS)

#-----------------------------------------------------------------------------
# Core routines for adding executables and libraries to the build and
# install lists of CMake
function(BRLCAD_ADDEXEC execname srcslist libslist)
  cmake_parse_arguments(E "TEST;TEST_USESDATA;NO_INSTALL;NO_MAN;NO_STRICT;NO_STRICT_CXX;GUI" "FOLDER" "" ${ARGN})

  # Let CMAKEFILES know what's going on
  cmakefiles(${srcslist})

  # See if we have a testing executable.  If testing is off, skip
  if(NOT BUILD_TESTING)
    if(E_TEST OR E_TEST_USESDATA)
      return()
    endif(E_TEST OR E_TEST_USESDATA)
  endif(NOT BUILD_TESTING)

  # Go all C++ if the settings request it
  set_lang_cxx("${srcslist}")

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

  # Handle include directories from targets
  if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
    set(dep_includes)
    foreach(ll ${libslist})
      if(TARGET ${ll})
        get_target_property(IDIRS ${ll} INTERFACE_INCLUDE_DIRECTORIES)
        if(IDIRS)
          list(APPEND dep_includes ${IDIRS})
        endif(IDIRS)
      endif(TARGET ${ll})
    endforeach(ll ${libslist})
    brlcad_include_dirs(${execname} dep_includes PRIVATE)
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
      if(EXISTS ${CMAKE_SOURCE_DIR}/doc/docbook/system/man1/${execname}.xml)
        add_docbook("HTML;PHP;MAN1;PDF" ${CMAKE_SOURCE_DIR}/doc/docbook/system/man1/${execname}.xml man1 "")
      else(EXISTS ${CMAKE_SOURCE_DIR}/doc/docbook/system/man1/${execname}.xml)
        message("No man page defined for ${execname}")
      endif(EXISTS ${CMAKE_SOURCE_DIR}/doc/docbook/system/man1/${execname}.xml)
    endif(NOT E_TEST AND NOT E_TEST_USESDATA AND NOT E_NO_INSTALL AND NOT E_NO_MAN)
  endif(BRLCAD_EXTRADOCS)
endfunction(BRLCAD_ADDEXEC execname srcslist libslist)

#---------------------------------------------------------------------
# Library function handles both shared and static libs, so one
# "BRLCAD_ADDLIB" statement will cover both automatically
function(
  BRLCAD_ADDLIB
  libname
  srcslist
  libslist
  include_dirs
  local_include_dirs
)
  cmake_parse_arguments(L "SHARED;STATIC;NO_INSTALL;NO_STRICT;NO_STRICT_CXX" "FOLDER" "SHARED_SRCS;STATIC_SRCS" ${ARGN})

  # Let CMAKEFILES know what's going on
  cmakefiles(${srcslist} ${L_SHARED_SRCS} ${L_STATIC_SRCS})

  # The naming convention used for variables is the upper case of the
  # library name, without the lib prefix.
  string(REPLACE "lib" "" LOWERCORE "${libname}")
  string(TOUPPER ${LOWERCORE} UPPER_CORE)

  # Go all C++ if the settings request it
  set_lang_cxx("${srcslist};${L_SHARED_SRCS};${L_STATIC_SRCS}")

  # Local copy of srcslist in case manipulation is needed
  set(lsrcslist ${srcslist})

  # If we're going to have a specified subfolder, prepare the
  # appropriate string:
  if(L_FOLDER)
    set(SUBFOLDER "/${L_FOLDER}")
  endif(L_FOLDER)

  # Set up includes
  set(PUBLIC_HDRS ${include_dirs})
  foreach(ll ${libslist})
    if(TARGET ${ll})
      get_target_property(IDIRS ${ll} INTERFACE_INCLUDE_DIRECTORIES)
      if(IDIRS)
        list(APPEND PUBLIC_HDRS ${IDIRS})
      endif(IDIRS)
    endif(TARGET ${ll})
  endforeach(ll ${libslist})
  set(PRIVATE_HDRS ${local_include_dirs})

  # If we need it, set up the OBJECT library build
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

    # Ensure object targets depend on other build targets, not just
    # system libs, if compilation relies on actions performed by those
    # targets (e.g., staging headers).  Failing to set those deps can
    # cause very hard-to-debut intermittent build failures, especially
    # with parallel builds, as build order is not guaranteed without
    # explicit deps.
    if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      foreach(ll ${libslist})
        if(TARGET ${ll})
          add_dependencies(${libname}-obj ${ll})
        endif(TARGET ${ll})
      endforeach(ll ${libslist})
    endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
  endif(USE_OBJECT_LIBS)

  # Handle the shared library
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
    add_library(${libname} SHARED ${lsrcslist} ${L_SHARED_SRCS})
    if(${libname} MATCHES "^lib*")
      set_target_properties(${libname} PROPERTIES PREFIX "")
    endif(${libname} MATCHES "^lib*")

    # Set the standard build definitions for all BRL-CAD targets
    target_compile_definitions(${libname} PRIVATE BRLCADBUILD HAVE_CONFIG_H)

    # Set includes on shared target
    brlcad_include_dirs(${libname} PUBLIC_HDRS PUBLIC)
    brlcad_include_dirs(${libname} PRIVATE_HDRS PRIVATE)

    if(HIDE_INTERNAL_SYMBOLS)
      set_property(TARGET ${libname} APPEND PROPERTY COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_EXPORTS")
      set_property(TARGET ${libname} APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "${UPPER_CORE}_DLL_IMPORTS")
    endif(HIDE_INTERNAL_SYMBOLS)
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
    # Make sure the target depends on any targets in the libslist
    foreach(ll ${libslist})
      if(TARGET ${ll})
        add_dependencies(${libstatic} ${ll})
      endif(TARGET ${ll})
    endforeach(ll ${libslist})
    set_target_properties(${libstatic} PROPERTIES FOLDER "BRL-CAD Static Libraries${SUBFOLDER}")
    validate_style("${libstatic}" "${srcslist};${L_STATIC_SRCS}")

    if(NOT L_NO_INSTALL)
      install(
        TARGETS ${libstatic}
        RUNTIME DESTINATION ${BIN_DIR}
        LIBRARY DESTINATION ${LIB_DIR}
        ARCHIVE DESTINATION ${LIB_DIR}
      )
    endif(NOT L_NO_INSTALL)
  endif(L_STATIC OR (BUILD_STATIC_LIBS AND NOT L_SHARED))

  # Extra shared lib specific work
  if(L_SHARED OR (BUILD_SHARED_LIBS AND NOT L_STATIC))
    set_target_properties(${libname} PROPERTIES FOLDER "BRL-CAD Shared Libraries${SUBFOLDER}")
    validate_style("${libname}" "${srcslist};${L_SHARED_SRCS}")
    # If we have libraries to link for a shared library, link them.
    if(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
      target_link_libraries(${libname} ${libslist})
    endif(NOT "${libslist}" STREQUAL "" AND NOT "${libslist}" STREQUAL "NONE")
    if(NOT L_NO_INSTALL)
      install(
        TARGETS ${libname}
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
    # Just assume it isn't until we prove it is
    set(${result_var} 0 PARENT_SCOPE)

    # get the CMake form of the path so we have something consistent to work on
    file(TO_CMAKE_PATH "${full_path}" c_full_path)
    file(TO_CMAKE_PATH "${candidate_subpath}" c_candidate_subpath)

    # check the string lengths - if the "subpath" is longer than the full path,
    # there's not point in going further
    string(LENGTH "${c_full_path}" FULL_LENGTH)
    string(LENGTH "${c_candidate_subpath}" SUB_LENGTH)
    if("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")
      return()
    endif("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")

    # OK, maybe it's a subpath - time to actually check
    string(SUBSTRING "${c_full_path}" 0 ${SUB_LENGTH} c_full_subpath)
    if(NOT "${c_full_subpath}" STREQUAL "${c_candidate_subpath}")
      return()
    endif(NOT "${c_full_subpath}" STREQUAL "${c_candidate_subpath}")

    # If we get here, it's a subpath
    set(${result_var} 1 PARENT_SCOPE)
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
    execute_process(
      COMMAND
        ${CMAKE_COMMAND} -E create_symlink "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src"
        "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest"
      OUTPUT_QUIET # Errors are expected on some platforms (Windows) so
      ERROR_QUIET # by default quiet the output to avoid confusion
    )
    if(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Supported")
      set(HAVE_SYMLINK 1 CACHE BOOL "Platform supports creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src" "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
    else(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
      message("--- Checking operating system support for file symlinking - Unsupported")
      set(HAVE_SYMLINK 0 CACHE BOOL "Platform does not support creation of symlinks" FORCE)
      mark_as_advanced(HAVE_SYMLINK)
      file(REMOVE "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_src")
    endif(EXISTS "${CMAKE_BINARY_DIR}/CMakeTmp/link_test_dest")
  endif(NOT DEFINED HAVE_SYMLINK)

  # Now that the input data and target names are in order, define the
  # custom commands needed for build directory data copying on this
  # platform (per symlink test results.)

  if(HAVE_SYMLINK)
    # Make sure the target directory exists (symlinks need the target
    # directory already in place)
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${targetdir}")

    # Using symlinks - in this case, the custom command doesn't
    # actually have to do the work every time the source file changes
    # - once established, the symlink will behave correctly.  That
    # being the case, we just go ahead and establish the symlinks in
    # the configure stage.
    foreach(filename ${fullpath_datalist})
      get_filename_component(ITEM_NAME ${filename} NAME)
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ${filename} "${CMAKE_BINARY_DIR}/${targetdir}/${ITEM_NAME}"
      )
    endforeach(filename ${fullpath_datalist})

    # check for and remove any dead symbolic links from a previous run
    file(GLOB listing LIST_DIRECTORIES false "${CMAKE_BINARY_DIR}/${targetdir}/*")
    foreach(filename ${listing})
      if(NOT EXISTS ${filename})
        message("Removing stale symbolic link ${filename}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${filename})
      endif(NOT EXISTS ${filename})
    endforeach(filename ${listing})

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
    set(${targetname}_cmake_contents "set(FILES_TO_COPY \"${fullpath_datalist}\")\n")
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}foreach(filename \${FILES_TO_COPY})\n")
    set(
      ${targetname}_cmake_contents
      "${${targetname}_cmake_contents}  file(COPY \${FILES_TO_COPY} DESTINATION \"${CMAKE_BINARY_DIR}/${targetdir}\")\n"
    )
    set(${targetname}_cmake_contents "${${targetname}_cmake_contents}endforeach(filename \${CURRENT_FILE_LIST})\n")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake" "${${targetname}_cmake_contents}")

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
