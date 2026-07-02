#             C O M P I L E R F L A G S . C M A K E
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

include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckLinkerFlag)
include(CMakeParseArguments)
include(CMakePushCheckState)

set(CMAKE_BUILD_TYPES DEBUG RELEASE)

# Debugging function to print all current flags
function(PRINT_BUILD_FLAGS)
  message("Current Build Flags (${ARGV0}):\n")
  message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
  message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  message("CMAKE_SHARED_LINKER_FLAGS: ${CMAKE_SHARED_LINKER_FLAGS}")
  message("CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  foreach(BTYPE ${CMAKE_BUILD_TYPES})
    message(" ")
    message("CMAKE_C_FLAGS_${BTYPE}: ${CMAKE_C_FLAGS_${BTYPE}}")
    message("CMAKE_CXX_FLAGS_${BTYPE}: ${CMAKE_CXX_FLAGS_${BTYPE}}")
    message("CMAKE_SHARED_LINKER_FLAGS_${BTYPE}: ${CMAKE_SHARED_LINKER_FLAGS_${BTYPE}}")
    message("CMAKE_EXE_LINKER_FLAGS_${BTYPE}: ${CMAKE_EXE_LINKER_FLAGS_${BTYPE}}")
  endforeach(BTYPE ${CMAKE_BUILD_TYPES})
  message(" ")
endfunction(PRINT_BUILD_FLAGS)

# Clear all currently defined CMake compiler and linker flags.
# NOTE - we don't currently manage MSVC build flags directly,
# so in that case we leave the CMake defaults.
macro(CLEAR_BUILD_FLAGS)
  set(BUILD_FLAGS_TO_CLEAR CMAKE_C_FLAGS CMAKE_CXX_FLAGS CMAKE_SHARED_LINKER_FLAGS CMAKE_EXE_LINKER_FLAGS)
  if(NOT MSVC)
    foreach(bflag ${BUILD_FLAGS_TO_CLEAR})
      set(${bflag} "")
      unset(${bflag} CACHE)
      foreach(BTYPE ${CMAKE_BUILD_TYPES})
        set(${bflag}_${BTYPE} "")
        unset(${bflag}_${BTYPE} CACHE)
      endforeach(BTYPE ${CMAKE_BUILD_TYPES})
    endforeach(bflag ${BUILD_FLAGS_TO_CLEAR})

    set(CMAKE_C_FLAGS "$ENV{CFLAGS}")
    set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "$ENV{LDFLAGS}")
  endif(NOT MSVC)
endmacro(CLEAR_BUILD_FLAGS)

# To reduce verbosity in this file, determine up front which
# build configuration type (if any) we are using and stash
# the variables we want to assign flags to into a common
# variable that will be used for all routines.
macro(ADD_NEW_FLAG FLAG_TYPE NEW_FLAG CONFIG_LIST)
  if(${NEW_FLAG})
    if("${CONFIG_LIST}" STREQUAL "ALL")
      set(CMAKE_${FLAG_TYPE}_FLAGS "${CMAKE_${FLAG_TYPE}_FLAGS} ${${NEW_FLAG}}")
    elseif("${CONFIG_LIST}" STREQUAL "Debug" AND NOT CMAKE_BUILD_TYPE)
      set(CMAKE_${FLAG_TYPE}_FLAGS "${CMAKE_${FLAG_TYPE}_FLAGS} ${${NEW_FLAG}}")
    else("${CONFIG_LIST}" STREQUAL "ALL")
      foreach(CFG_TYPE ${CONFIG_LIST})
        set(VALID_CONFIG 1)
        if(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "${CFG_TYPE}")
          set(VALID_CONFIG "-1")
        endif(NOT "${CMAKE_BUILD_TYPE}" STREQUAL "${CFG_TYPE}")
        if(NOT "${VALID_CONFIG}" STREQUAL "-1")
          string(TOUPPER "${CFG_TYPE}" CFG_TYPE)
          if(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
            set(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE} "${CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE}} ${${NEW_FLAG}}")
          else(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
            set(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE} "${${NEW_FLAG}}")
          endif(CMAKE_${FLAG_TYPE}_FLAGS_${CFG_TYPE})
        endif(NOT "${VALID_CONFIG}" STREQUAL "-1")
      endforeach(CFG_TYPE ${CONFIG_LIST})
    endif("${CONFIG_LIST}" STREQUAL "ALL")
  endif(${NEW_FLAG})
endmacro(ADD_NEW_FLAG)

# This macro tests for a specified C or C++ compiler flag, setting the
# result in the specified variable.
if(NOT COMMAND CHECK_COMPILER_FLAG)
  macro(CHECK_COMPILER_FLAG FLAG_LANG NEW_FLAG RESULTVAR)
    if("${FLAG_LANG}" STREQUAL "C")
      check_c_compiler_flag(${NEW_FLAG} ${RESULTVAR})
    endif("${FLAG_LANG}" STREQUAL "C")
    if("${FLAG_LANG}" STREQUAL "CXX")
      check_cxx_compiler_flag(${NEW_FLAG} ${RESULTVAR})
    endif("${FLAG_LANG}" STREQUAL "CXX")
  endmacro(CHECK_COMPILER_FLAG LANG NEW_FLAG RESULTVAR)
endif(NOT COMMAND CHECK_COMPILER_FLAG)

# Purpose: test one linker flag using CMake's check_linker_flag helper and
# return the supported/unsupported result to the caller.
#   FLAG_LANG - C or CXX
#   NEW_FLAG  - CMake LINK_OPTIONS spelling, for example LINKER:--no-undefined
#   RESULTVAR - parent-scope variable receiving the boolean test result
function(BRLCAD_CHECK_LINKER_FLAG FLAG_LANG NEW_FLAG RESULTVAR)
  if("${FLAG_LANG}" STREQUAL "C")
    check_linker_flag(C ${NEW_FLAG} ${RESULTVAR})
  elseif("${FLAG_LANG}" STREQUAL "CXX")
    check_linker_flag(CXX ${NEW_FLAG} ${RESULTVAR})
  else()
    message(FATAL_ERROR "Unsupported linker flag test language: ${FLAG_LANG}")
  endif()
  set(${RESULTVAR} ${${RESULTVAR}} PARENT_SCOPE)
endfunction(BRLCAD_CHECK_LINKER_FLAG)

# Test linker option groups that must be used together.
#   FLAG_LANG - C or CXX
#   RESULTVAR - parent-scope variable receiving the boolean test result
#   ARGN      - linker options to test as a group, one option per argument
function(BRLCAD_CHECK_LINKER_FLAGS FLAG_LANG RESULTVAR)
  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_LINK_OPTIONS ${ARGN})
  if("${FLAG_LANG}" STREQUAL "C")
    check_c_source_compiles("int main(void) { return 0; }" ${RESULTVAR})
  elseif("${FLAG_LANG}" STREQUAL "CXX")
    check_cxx_source_compiles("int main() { return 0; }" ${RESULTVAR})
  else()
    message(FATAL_ERROR "Unsupported linker flag group test language: ${FLAG_LANG}")
  endif()
  cmake_pop_check_state()
  set(${RESULTVAR} ${${RESULTVAR}} PARENT_SCOPE)
endfunction(BRLCAD_CHECK_LINKER_FLAGS)

# Synopsis:  CHECK_FLAG(LANG flag [BUILD_TYPES type1 type2 ...] [GROUPS group1 group2 ...] [VARS var1 var2 ...] )
#
# CHECK_FLAG is BRL-CAD's core macro for C/C++ flag testing.
# The first value is the language to test (C or C++ currently).  The
# second entry is the flag (without preliminary dash).
#
# If the first two mandatory options are the only ones provided, a
# successful test of the flag will result in its being assigned to
# *all* compilations using the appropriate global C/C++ CMake
# variable.  If optional parameters are included, they tell the macro
# what to do with the test results instead of doing the default global
# assignment.  Options include assigning the flag to one or more of
# the variable lists associated with build types (e.g. Debug or
# Release), appending the variable to a string that contains a group
# of variables, or assigning the flag to a variable if that variable
# does not already hold a value.  The assignments are not mutually
# exclusive - any or all of them may be used in a given command.
#
# For example, to test a flag and add it to the C Debug configuration
# flags:
#
# CHECK_FLAG(C ggdb3 BUILD_TYPES Debug)
#
# To assign a C flag to a unique variable:
#
# CHECK_FLAG(C c99 VARS C99_FLAG)
#
# To do all assignments at once, for multiple configs and vars:
#
# CHECK_FLAG(C ggdb3
#                   BUILD_TYPES Debug Release
#                   GROUPS DEBUG_FLAGS
#                   VARS DEBUG1 DEBUG2)
macro(CHECK_FLAG)
  # Set up some variables and names
  set(FLAG_LANG ${ARGV0})
  set(flag ${ARGV1})
  string(TOUPPER ${flag} UPPER_FLAG)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" UPPER_FLAG ${UPPER_FLAG})
  set(NEW_FLAG "-${flag}")

  # Start processing arguments
  if(${ARGC} LESS 3)
    # Handle default (global) case
    check_compiler_flag(${FLAG_LANG} ${NEW_FLAG} ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      add_new_flag(${FLAG_LANG} NEW_FLAG ALL)
    endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
  else(${ARGC} LESS 3)
    # Parse extra arguments
    cmake_parse_arguments(FLAG "" "" "BUILD_TYPES;GROUPS;VARS" ${ARGN})

    # Iterate over listed Build types and append the flag to them if successfully tested.
    foreach(build_type ${FLAG_BUILD_TYPES})
      check_compiler_flag(${FLAG_LANG} "${NEW_FLAG}" ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
        add_new_flag(${FLAG_LANG} NEW_FLAG "${build_type}")
      endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    endforeach(build_type ${FLAG_BUILD_TYPES})

    # Append flag to a group of flags (this apparently needs to be
    # a string build, not a CMake list build.  Do this for all supplied
    # group variables.
    foreach(flag_group ${FLAG_GROUPS})
      check_compiler_flag(${FLAG_LANG} "${NEW_FLAG}" ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
      if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
        if(${flag_group})
          set(${flag_group} "${${flag_group}} ${NEW_FLAG}")
        else(${flag_group})
          set(${flag_group} "${NEW_FLAG}")
        endif(${flag_group})
      endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
    endforeach(flag_group ${FLAG_GROUPS})

    # If a variable does not have a value, check the flag and if valid assign
    # the flag as the variable's value.  Do this for all supplied variables.
    foreach(flag_var ${FLAG_VARS})
      if(NOT ${flag_var})
        check_compiler_flag(${FLAG_LANG} "${NEW_FLAG}" ${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND)
        if(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND AND NOT "${${flag_var}}")
          set(${flag_var} "${NEW_FLAG}")
        endif(${UPPER_FLAG}_${FLAG_LANG}_FLAG_FOUND AND NOT "${${flag_var}}")
      endif(NOT ${flag_var})
    endforeach(flag_var ${FLAG_VARS})
  endif(${ARGC} LESS 3)
endmacro(CHECK_FLAG)

# This macro checks whether a specified C flag is available.  See
# CHECK_FLAG() for arguments.
macro(CHECK_C_FLAG)
  check_flag(C ${ARGN})
endmacro(CHECK_C_FLAG)

# This macro checks whether a specified C++ flag is available.  See
# CHECK_FLAG() for arguments.
macro(CHECK_CXX_FLAG)
  check_flag(CXX ${ARGN})
endmacro(CHECK_CXX_FLAG)

###
# Register a compiler flag for deferred parallel testing.  The cache variable
# name is computed the same way CHECK_FLAG computes it, so normal CHECK_FLAG
# calls will use the batched result without changing their application logic.
###
macro(_BRLCAD_REGISTER_FLAG_PROBE FLAG_LANG FLAG)
  if(BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    string(TOUPPER "${FLAG}" _brfp_upper)
    string(REGEX REPLACE "[^a-zA-Z0-9]" "_" _brfp_upper "${_brfp_upper}")
    set(_brfp_var "${_brfp_upper}_${FLAG_LANG}_FLAG_FOUND")

    if(NOT DEFINED ${_brfp_var})
      get_property(_brfp_list GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${FLAG_LANG}")
      list(FIND _brfp_list "${_brfp_var}" _brfp_idx)
      if(_brfp_idx EQUAL -1)
        list(APPEND _brfp_list "${_brfp_var}")
        set_property(GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${FLAG_LANG}" "${_brfp_list}")
        set_property(GLOBAL PROPERTY "BRLCAD_CFLAG_${_brfp_var}_FLAG" "-${FLAG}")
      endif()
    endif()

    unset(_brfp_idx)
    unset(_brfp_list)
    unset(_brfp_var)
    unset(_brfp_upper)
  endif()
endmacro()

###
# Execute all pending compiler flag probes for a language as one generated
# project.  Each flag is tested on its own target and the native build is asked
# to keep going, allowing independent probes to finish in parallel even when
# some flags fail.
###
function(_BRLCAD_RUN_FLAG_BATCH FLAG_LANG)
  if(NOT BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    return()
  endif()

  cmake_parse_arguments(_BRFB "" "WERROR" "" ${ARGN})

  get_property(_brfb_vars GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${FLAG_LANG}")
  if(NOT _brfb_vars)
    return()
  endif()

  set(_brfb_pending)
  foreach(_brfb_var IN LISTS _brfb_vars)
    if(NOT DEFINED ${_brfb_var})
      list(APPEND _brfb_pending "${_brfb_var}")
    endif()
  endforeach()
  if(NOT _brfb_pending)
    return()
  endif()

  if("${FLAG_LANG}" STREQUAL "CXX")
    set(_brfb_lang_kw "CXX")
    set(_brfb_src_file "cflag_test.cpp")
    set(_brfb_src_text "int main() { return 0; }\n")
  elseif("${FLAG_LANG}" STREQUAL "C")
    set(_brfb_lang_kw "C")
    set(_brfb_src_file "cflag_test.c")
    set(_brfb_src_text "int main(void) { return 0; }\n")
  else()
    message(FATAL_ERROR "Unsupported compiler flag test language: ${FLAG_LANG}")
  endif()

  set(_brfb_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/CFLAG_${FLAG_LANG}_sources")
  set(_brfb_build_dir "${CMAKE_BINARY_DIR}/CMakeTmp/CFLAG_${FLAG_LANG}_build")
  file(MAKE_DIRECTORY "${_brfb_src_dir}")
  file(REMOVE_RECURSE "${_brfb_build_dir}")
  file(WRITE "${_brfb_src_dir}/${_brfb_src_file}" "${_brfb_src_text}")

  set(_brfb_cml "${_brfb_src_dir}/CMakeLists.txt")
  file(WRITE "${_brfb_cml}"
    "cmake_minimum_required(VERSION 3.22)\n"
    "project(BRLCAD${FLAG_LANG}FlagBatch ${_brfb_lang_kw})\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \"\${CMAKE_BINARY_DIR}\")\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG \"\${CMAKE_BINARY_DIR}\")\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE \"\${CMAKE_BINARY_DIR}\")\n"
  )
  if(_BRFB_WERROR)
    file(APPEND "${_brfb_cml}" "add_compile_options(${_BRFB_WERROR})\n")
  endif()
  foreach(_brfb_var IN LISTS _brfb_pending)
    get_property(_brfb_flag GLOBAL PROPERTY "BRLCAD_CFLAG_${_brfb_var}_FLAG")
    file(APPEND "${_brfb_cml}"
      "add_executable(${_brfb_var} \"${_brfb_src_dir}/${_brfb_src_file}\")\n"
      "target_compile_options(${_brfb_var} PRIVATE ${_brfb_flag})\n"
    )
  endforeach()

  set(_brfb_cfg_cmd
    "${CMAKE_COMMAND}"
    "-S" "${_brfb_src_dir}"
    "-B" "${_brfb_build_dir}"
    "-G" "${CMAKE_GENERATOR}"
    "-DCMAKE_${_brfb_lang_kw}_COMPILER=${CMAKE_${_brfb_lang_kw}_COMPILER}"
  )
  if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND _brfb_cfg_cmd "-A" "${CMAKE_GENERATOR_PLATFORM}")
  endif()
  if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND _brfb_cfg_cmd "-T" "${CMAKE_GENERATOR_TOOLSET}")
  endif()
  if(CMAKE_MAKE_PROGRAM)
    list(APPEND _brfb_cfg_cmd "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
  endif()
  if(CMAKE_BUILD_TYPE)
    list(APPEND _brfb_cfg_cmd "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
  endif()
  if(CMAKE_${_brfb_lang_kw}_FLAGS)
    list(APPEND _brfb_cfg_cmd "-DCMAKE_${_brfb_lang_kw}_FLAGS=${CMAKE_${_brfb_lang_kw}_FLAGS}")
  endif()

  execute_process(
    COMMAND ${_brfb_cfg_cmd}
    RESULT_VARIABLE _brfb_cfg_result
    OUTPUT_VARIABLE _brfb_cfg_out
    ERROR_VARIABLE _brfb_cfg_err
  )
  file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH COMPILER FLAG CONFIGURE LOG: ${FLAG_LANG} ===\n${_brfb_cfg_out}\n${_brfb_cfg_err}\n")

  if(_brfb_cfg_result EQUAL 0)
    cmake_host_system_information(RESULT _brfb_ncpus QUERY NUMBER_OF_PHYSICAL_CORES)
    if(NOT _brfb_ncpus OR _brfb_ncpus LESS 1)
      set(_brfb_ncpus 1)
    endif()

    if(CMAKE_GENERATOR MATCHES "Ninja")
      set(_brfb_keepgoing "--" "-k" "0")
    elseif(CMAKE_GENERATOR MATCHES "Makefiles")
      set(_brfb_keepgoing "--" "-k")
    elseif(CMAKE_GENERATOR MATCHES "Xcode")
      set(_brfb_keepgoing "--" "-PBXBuildsContinueAfterErrors=YES")
    else()
      set(_brfb_keepgoing "")
    endif()

    set(_brfb_build_cmd "${CMAKE_COMMAND}" "--build" "${_brfb_build_dir}" "-j" "${_brfb_ncpus}" ${_brfb_keepgoing})
    if(CMAKE_BUILD_TYPE AND NOT _brfb_keepgoing)
      list(APPEND _brfb_build_cmd "--config" "${CMAKE_BUILD_TYPE}")
    endif()
    execute_process(
      COMMAND ${_brfb_build_cmd}
      RESULT_VARIABLE _brfb_ignored_result
      OUTPUT_VARIABLE _brfb_build_out
      ERROR_VARIABLE _brfb_build_err
    )
    file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH COMPILER FLAG BUILD LOG: ${FLAG_LANG} ===\n${_brfb_build_out}\n${_brfb_build_err}\n")
  endif()

  # Determine pass/fail exactly the way CMake's check_<lang>_compiler_flag does:
  # a probe passes only if it compiled AND the build output contains no
  # diagnostic indicating the flag was unrecognized or ignored.  This is
  # essential on MSVC, where an unknown flag still compiles successfully
  # (exit 0, artifact produced) but emits command-line warning D9002 - so
  # artifact existence alone would mark bogus flags as supported and add them
  # to the real build's flags.  We reuse CMake's own fail-regex set so the
  # verdict matches the reference try_compile path (verifiable with
  # misc/CMake/probe_validation/).
  include(CMakeCheckCompilerFlagCommonPatterns)
  check_compiler_flag_common_patterns(_brfb_common_patterns)
  set(_brfb_fail_regex)
  set(_brfb_take_regex FALSE)
  foreach(_brfb_tok IN LISTS _brfb_common_patterns)
    if("${_brfb_tok}" STREQUAL "FAIL_REGEX")
      set(_brfb_take_regex TRUE)
    elseif(_brfb_take_regex)
      list(APPEND _brfb_fail_regex "${_brfb_tok}")
      set(_brfb_take_regex FALSE)
    endif()
  endforeach()
  # Language-specific "valid for X but not for Y" patterns, matching CMake's
  # Internal/CheckFlagCommonConfig.cmake.
  if("${FLAG_LANG}" STREQUAL "CXX")
    list(APPEND _brfb_fail_regex "command[ -]line option .* is valid for .* but not for C\\+\\+")
  else()
    list(APPEND _brfb_fail_regex "command[ -]line option .* is valid for .* but not for C")
  endif()

  # Split the combined build output into lines for per-probe attribution.
  # Protect embedded semicolons so foreach() does not split on them; the tokens
  # we attribute by (the target name and the flag text) contain none.
  set(_brfb_log "${_brfb_build_out}\n${_brfb_build_err}")
  string(REPLACE ";" "@@SEMI@@" _brfb_log "${_brfb_log}")
  string(REPLACE "\n" ";" _brfb_lines "${_brfb_log}")

  set(_brfb_report)
  foreach(_brfb_var IN LISTS _brfb_pending)
    if(NOT DEFINED ${_brfb_var})
      # Did the probe compile at all?
      set(_brfb_built FALSE)
      if(_brfb_cfg_result EQUAL 0)
        if(EXISTS "${_brfb_build_dir}/${_brfb_var}" OR EXISTS "${_brfb_build_dir}/${_brfb_var}.exe")
          set(_brfb_built TRUE)
        endif()
      endif()

      # A compiled probe still fails if its flag was reported unrecognized.
      # Attribute a diagnostic line to this probe by the MSBuild project tag
      # (<target>.vcxproj) or by the quoted flag the compiler echoes back.
      set(_brfb_success ${_brfb_built})
      if(_brfb_built)
        get_property(_brfb_flag GLOBAL PROPERTY "BRLCAD_CFLAG_${_brfb_var}_FLAG")
        foreach(_brfb_line IN LISTS _brfb_lines)
          string(FIND "${_brfb_line}" "${_brfb_var}.vcxproj" _brfb_tag_pos)
          string(FIND "${_brfb_line}" "'${_brfb_flag}'" _brfb_flag_pos)
          if(NOT _brfb_tag_pos EQUAL -1 OR NOT _brfb_flag_pos EQUAL -1)
            foreach(_brfb_re IN LISTS _brfb_fail_regex)
              if("${_brfb_line}" MATCHES "${_brfb_re}")
                set(_brfb_success FALSE)
              endif()
            endforeach()
          endif()
        endforeach()
      endif()

      if(_brfb_success)
        set(${_brfb_var} 1 CACHE INTERNAL "Test ${_brfb_var}" FORCE)
        set(_brfb_result_line "Performing Test ${_brfb_var} - Success")
      else()
        set(${_brfb_var} "" CACHE INTERNAL "Test ${_brfb_var}" FORCE)
        set(_brfb_result_line "Performing Test ${_brfb_var} - Failed")
      endif()
      if(_brfb_report)
        string(APPEND _brfb_report "\n-- ${_brfb_result_line}")
      else()
        string(APPEND _brfb_report "${_brfb_result_line}")
      endif()
    endif()
  endforeach()
  if(_brfb_report)
    message(STATUS "${_brfb_report}")
  endif()
endfunction()

# Disable any compilation warning flags currently set
macro(DISABLE_WARNINGS)
  # borland-style
  if(NOT NOWARN_CFLAG)
    check_c_flag("w-" VARS NOWARN_CFLAG)
  endif(NOT NOWARN_CFLAG)
  if(NOT NOWARN_CXXFLAG)
    check_cxx_flag("w-" VARS NOWARN_CXXFLAG)
  endif(NOT NOWARN_CXXFLAG)

  # msvc-style (must test before gcc's -w test)
  if(NOT NOWARN_CFLAG)
    check_c_flag("W0" VARS NOWARN_CFLAG)

    if(NOWARN_CFLAG)
      # replace msvc-style warning level C flags, disable with W0
      string(REGEX REPLACE "[/-][wW][1-4]" "/W0" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
      foreach(BTYPE ${CMAKE_BUILD_TYPES})
        string(REGEX REPLACE "[/-][wW][1-4]" "/W0" CMAKE_C_FLAGS_${BTYPE} "${CMAKE_C_FLAGS_${BTYPE}}")
      endforeach(BTYPE ${CMAKE_BUILD_TYPES})
    endif(NOWARN_CFLAG)
  endif(NOT NOWARN_CFLAG)

  if(NOT NOWARN_CXXFLAG)
    check_cxx_flag("W0" VARS NOWARN_CXXFLAG)

    if(NOWARN_CXXFLAG)
      # replace msvc-style warning level C++ flags, disable with W0
      string(REGEX REPLACE "[/-][wW][1-4]" "/W0" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
      foreach(BTYPE ${CMAKE_BUILD_TYPES})
        string(REGEX REPLACE "[/-][wW][1-4]" "/W0" CMAKE_CXX_FLAGS_${BTYPE} "${CMAKE_CXX_FLAGS_${BTYPE}}")
      endforeach(BTYPE ${CMAKE_BUILD_TYPES})
    endif(NOWARN_CXXFLAG)
  endif(NOT NOWARN_CXXFLAG)

  # gcc/llvm-style
  if(NOT NOWARN_CFLAG)
    check_c_flag("w" VARS NOWARN_CFLAG)
  endif(NOT NOWARN_CFLAG)
  if(NOT NOWARN_CXXFLAG)
    check_cxx_flag("w" VARS NOWARN_CXXFLAG)
  endif(NOT NOWARN_CXXFLAG)

  # turn off warnings in the current scope
  if(NOWARN_CFLAG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${NOWARN_CFLAG}")
  endif(NOWARN_CFLAG)
  if(NOWARN_CXXFLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NOWARN_CXXFLAG}")
  endif(NOWARN_CXXFLAG)
endmacro(DISABLE_WARNINGS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
