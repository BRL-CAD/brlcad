# BRLCAD_Path_Privacy.cmake
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

function(brlcad_producer_paths result)
  set(paths "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" "${BRLCAD_EXT_DIR}")
  if(EXISTS "${BRLCAD_EXT_DIR}/CMakeCache.txt")
    file(STRINGS "${BRLCAD_EXT_DIR}/CMakeCache.txt" source_line
      REGEX "^CMAKE_HOME_DIRECTORY:INTERNAL=")
    string(REGEX REPLACE "^[^=]*=" "" producer_source "${source_line}")
    list(APPEND paths "${producer_source}")
  endif()
  set(forms)
  foreach(path IN LISTS paths)
    if(NOT IS_ABSOLUTE "${path}")
      continue()
    endif()
    cmake_path(NORMAL_PATH path)
    list(APPEND forms "${path}")
    if(EXISTS "${path}")
      file(REAL_PATH "${path}" real_path)
      list(APPEND forms "${real_path}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES forms)
  set(${result} "${forms}" PARENT_SCOPE)
endfunction()

function(brlcad_configure_path_privacy)
  brlcad_producer_paths(producer_paths)
  check_c_compiler_flag("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." BRLCAD_C_FILE_PREFIX_MAP)
  check_cxx_compiler_flag("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=." BRLCAD_CXX_FILE_PREFIX_MAP)
  set(coverage_profile_maps)
  foreach(path IN LISTS producer_paths)
    foreach(language C CXX)
      if(BRLCAD_${language}_FILE_PREFIX_MAP)
        add_compile_options("$<$<COMPILE_LANGUAGE:${language}>:-ffile-prefix-map=${path}=.>")
        if(BRLCAD_ENABLE_COVERAGE AND CMAKE_${language}_COMPILER_ID STREQUAL "GNU")
          list(APPEND coverage_profile_maps
            "$<$<COMPILE_LANGUAGE:${language}>:-fprofile-prefix-map=${path}=${path}>")
        endif()
      endif()
    endforeach()
  endforeach()

  # Coverage readers need to distinguish source and build roots.  Override
  # only profiling maps after the file maps so debug and macro paths stay
  # private while gcov can locate the original sources and copied headers.
  add_compile_options(${coverage_profile_maps})

  if(NOT UNIX OR NOT CMAKE_C_COMPILER_ID MATCHES "Clang" OR
      NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    return()
  endif()

  # LLVM preserves the input argument in bitcode's source_filename even when
  # debug and macro prefix maps are active.  Stable relative aliases cover
  # that field without changing the source tree or archive representation.
  set(alias_root "${CMAKE_BINARY_DIR}/CMakeFiles/privacy")
  file(MAKE_DIRECTORY "${alias_root}")
  file(REAL_PATH "${alias_root}" alias_root)
  set(alias_sources)
  foreach(path IN LISTS producer_paths)
    if(IS_DIRECTORY "${path}")
      file(REAL_PATH "${path}" path)
      list(APPEND alias_sources "${path}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES alias_sources)
  set(index 0)
  foreach(path IN LISTS alias_sources)
    set(alias "${alias_root}/r${index}")
    if(IS_SYMLINK "${alias}")
      file(READ_SYMLINK "${alias}" previous_target)
      if(NOT previous_target STREQUAL path)
        file(REMOVE "${alias}")
      endif()
    endif()
    if(NOT EXISTS "${alias}")
      file(CREATE_LINK "${path}" "${alias}" SYMBOLIC RESULT result)
      if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Unable to create a relative compiler input alias: ${result}")
      endif()
    endif()
    math(EXPR index "${index} + 1")
  endforeach()
  set(launcher_template [=[
cmake_minimum_required(VERSION 3.22)
set(roots "@alias_sources@")
set(alias_root "@alias_root@")
set(command)
set(started FALSE)
set(source_next FALSE)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" working_directory)
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(index RANGE 0 ${last})
  set(argument "${CMAKE_ARGV${index}}")
  if(NOT started)
    if(argument STREQUAL "--")
      set(started TRUE)
    endif()
    continue()
  endif()
  if(source_next)
    get_filename_component(input "${argument}" ABSOLUTE)
    file(REAL_PATH "${input}" input)
    set(best_length -1)
    set(root_index 0)
    foreach(root IN LISTS roots)
      cmake_path(IS_PREFIX root "${input}" NORMALIZE matches)
      string(LENGTH "${root}" length)
      if(matches AND length GREATER best_length)
        set(best_length ${length})
        set(best_root "${root}")
        set(best_index ${root_index})
      endif()
      math(EXPR root_index "${root_index} + 1")
    endforeach()
    if(best_length LESS 0)
      message(FATAL_ERROR "Compiler input is outside the registered producer directories: ${input}")
    endif()
    file(RELATIVE_PATH suffix "${best_root}" "${input}")
    file(RELATIVE_PATH argument "${working_directory}"
      "${alias_root}/r${best_index}/${suffix}")
    set(source_next FALSE)
  elseif(argument STREQUAL "-c" OR argument STREQUAL "/c")
    set(source_next TRUE)
  endif()
  string(REPLACE ";" "\\;" argument "${argument}")
  list(APPEND command "${argument}")
endforeach()
if(NOT started OR NOT command)
  message(FATAL_ERROR "Missing compiler command")
endif()
execute_process(COMMAND ${command} RESULT_VARIABLE result)
if(NOT result STREQUAL "0")
  message(FATAL_ERROR "Compiler failed with status ${result}")
endif()
]=])
  string(CONFIGURE "${launcher_template}" launcher_content @ONLY)
  set(launcher "${alias_root}/compile.cmake")
  file(WRITE "${launcher}" "${launcher_content}")
  string(SHA256 signature "${launcher_content}")
  foreach(language C CXX)
    set(command "${CMAKE_COMMAND};-DPRIVACY_SIGNATURE=${signature};-P;${launcher};--")
    if(CMAKE_${language}_COMPILER_LAUNCHER)
      list(APPEND command ${CMAKE_${language}_COMPILER_LAUNCHER})
    endif()
    set(CMAKE_${language}_COMPILER_LAUNCHER "${command}" PARENT_SCOPE)
  endforeach()
endfunction()

function(brlcad_install_path_privacy)
  if(NOT STRCLEAR_EXECUTABLE)
    return()
  endif()
  brlcad_producer_paths(producer_paths)
  set(helper "${BRLCAD_EXT_NOINSTALL_DIR}/share/bext/RefreshSpdx.cmake")
  if(NOT EXISTS "${helper}")
    message(FATAL_ERROR "The prebuilt dependency bundle lacks the SPDX integrity helper")
  endif()
  configure_file("${helper}" "${CMAKE_BINARY_DIR}/CMakeFiles/RefreshSpdx.cmake" COPYONLY)
  configure_file("${BRLCAD_CMAKE_DIR}/InstallPathPrivacy.cmake.in"
    "${CMAKE_BINARY_DIR}/CMakeFiles/InstallPathPrivacy.cmake" @ONLY)
  install(SCRIPT "${CMAKE_BINARY_DIR}/CMakeFiles/InstallPathPrivacy.cmake")
endfunction()
