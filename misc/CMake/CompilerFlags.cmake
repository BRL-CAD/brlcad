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

###
# _brlcad_register_flag_probe(<C|CXX> <flag>)
#
# Register a compiler flag for deferred parallel testing.  The result-variable
# name is computed exactly as CHECK_FLAG does (uppercase flag, non-alphanumeric
# → '_', then appended with _<LANG>_FLAG_FOUND), e.g.:
#   "pipe" + "C"   →  PIPE_C_FLAG_FOUND
#   "Wno-inline" + "CXX" → WNO_INLINE_CXX_FLAG_FOUND
#
# If the variable already exists in the CMake cache the probe is skipped so
# that warm (re-)configures are instant.  Pending probes are stored in the
# BRLCAD_CFLAG_BATCH_<LANG> and BRLCAD_CFLAG_FLAG_<VAR> global properties.
###
macro(_brlcad_register_flag_probe _brfp_lang _brfp_flag)
  string(TOUPPER "${_brfp_flag}" _brfp_upper)
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" _brfp_upper "${_brfp_upper}")
  set(_brfp_var "${_brfp_upper}_${_brfp_lang}_FLAG_FOUND")
  if(NOT DEFINED "${_brfp_var}")
    get_property(_brfp_list GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${_brfp_lang}")
    list(FIND _brfp_list "${_brfp_var}" _brfp_idx)
    if(_brfp_idx EQUAL -1)
      list(APPEND _brfp_list "${_brfp_var}")
      set_property(GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${_brfp_lang}" "${_brfp_list}")
      set_property(GLOBAL PROPERTY "BRLCAD_CFLAG_FLAG_${_brfp_var}" "-${_brfp_flag}")
    endif()
    unset(_brfp_idx)
    unset(_brfp_list)
  endif()
  unset(_brfp_var)
  unset(_brfp_upper)
endmacro()

###
# _brlcad_run_flag_batch(<C|CXX> [WERROR <flag>])
#
# Build all pending compiler-flag probes for LANG in a single parallel
# mini-project, then write the per-flag results into the CMake cache as
# CACHE INTERNAL variables.  Subsequent check_c_compiler_flag /
# check_cxx_compiler_flag calls for the same flags find the result already
# in cache and return in ~3 ms instead of ~70 ms.
#
# The batch builds all targets in parallel with --keep-going so even failing
# flags do not abort the build.  Flag success is then determined by checking
# whether the output binary was produced — avoiding one subprocess call per
# flag and dramatically reducing per-probe overhead.
#
# WERROR <flag>: optional error-on-warning flag (e.g. "-Werror") added
#                globally to the mini-project so that unrecognised -W*
#                options are detected correctly.
###
function(_brlcad_run_flag_batch _brfb_lang)
  cmake_parse_arguments(_brfb "" "WERROR" "" ${ARGN})

  get_property(_brfb_vars GLOBAL PROPERTY "BRLCAD_CFLAG_BATCH_${_brfb_lang}")
  if(NOT _brfb_vars)
    return()
  endif()

  # Collect only the probes that haven't been cached yet.
  set(_brfb_pending)
  foreach(_brfb_var IN LISTS _brfb_vars)
    if(NOT DEFINED "${_brfb_var}")
      list(APPEND _brfb_pending "${_brfb_var}")
    endif()
  endforeach()
  if(NOT _brfb_pending)
    return()
  endif()

  set(_brfb_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/CFLAG_BATCH_${_brfb_lang}_sources")
  set(_brfb_build_dir "${CMAKE_BINARY_DIR}/CMakeTmp/CFLAG_BATCH_${_brfb_lang}_build")
  file(MAKE_DIRECTORY "${_brfb_src_dir}")

  # Shared test source — valid for both C and C++.
  if("${_brfb_lang}" STREQUAL "CXX")
    set(_brfb_src_file "${_brfb_src_dir}/cflag_test.cpp")
    file(WRITE "${_brfb_src_file}" "int main() { return 0; }\n")
    set(_brfb_lang_kw "CXX")
  else()
    set(_brfb_src_file "${_brfb_src_dir}/cflag_test.c")
    file(WRITE "${_brfb_src_file}" "int main(void) { return 0; }\n")
    set(_brfb_lang_kw "C")
  endif()

  # Write the mini CMakeLists.txt.  Pinning CMAKE_RUNTIME_OUTPUT_DIRECTORY to
  # the build root gives a predictable binary location for all generators.
  set(_brfb_cml "${_brfb_src_dir}/CMakeLists.txt")
  file(WRITE "${_brfb_cml}"
    "cmake_minimum_required(VERSION 3.22)\n"
    "project(BRLCADFlagBatch${_brfb_lang} ${_brfb_lang_kw})\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \"\${CMAKE_BINARY_DIR}\")\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG \"\${CMAKE_BINARY_DIR}\")\n"
    "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE \"\${CMAKE_BINARY_DIR}\")\n"
  )
  if(_brfb_WERROR)
    file(APPEND "${_brfb_cml}" "add_compile_options(${_brfb_WERROR})\n")
  endif()
  foreach(_brfb_var IN LISTS _brfb_pending)
    get_property(_brfb_flag GLOBAL PROPERTY "BRLCAD_CFLAG_FLAG_${_brfb_var}")
    file(APPEND "${_brfb_cml}"
      "add_executable(${_brfb_var} \"${_brfb_src_file}\")\n"
      "target_compile_options(${_brfb_var} PRIVATE ${_brfb_flag})\n"
    )
  endforeach()

  # Configure the mini-project (silent).
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
    OUTPUT_VARIABLE _cfg_out
    ERROR_VARIABLE _cfg_err
  )
  file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH CFLAG CONFIGURE LOG: ${_brfb_lang} ===\n${_cfg_out}\n${_cfg_err}\n")

  # Build all pending probes in parallel, continuing past failures so every
  # flag gets a chance regardless of unrelated flags that fail.
  if(_brfb_cfg_result EQUAL 0)
    cmake_host_system_information(RESULT _brfb_ncpus QUERY NUMBER_OF_PHYSICAL_CORES)
    if(NOT _brfb_ncpus OR _brfb_ncpus LESS 1)
      set(_brfb_ncpus 1)
    endif()
    # Pass the native-tool keep-going flag so the build continues past failures.
    if(CMAKE_GENERATOR MATCHES "Ninja")
      set(_brfb_keepgoing "--" "-k" "0")
    elseif(CMAKE_GENERATOR MATCHES "Makefiles")
      set(_brfb_keepgoing "--" "-k")
    elseif(CMAKE_GENERATOR MATCHES "Xcode")
      set(_brfb_keepgoing "--" "-PBXBuildsContinueAfterErrors=YES")
    else()
      set(_brfb_keepgoing "")
    endif()
    set(_brfb_build_cmd
      "${CMAKE_COMMAND}" "--build" "${_brfb_build_dir}"
      "-j" "${_brfb_ncpus}" ${_brfb_keepgoing})
    if(CMAKE_BUILD_TYPE AND NOT _brfb_keepgoing)
      # --config must precede the native-tool separator
      list(APPEND _brfb_build_cmd "--config" "${CMAKE_BUILD_TYPE}")
    endif()
    execute_process(
      COMMAND ${_brfb_build_cmd}
      RESULT_VARIABLE _brfb_ignored
      OUTPUT_VARIABLE _build_out
      ERROR_VARIABLE _build_err
    )
    file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH CFLAG BUILD LOG: ${_brfb_lang} ===\n${_build_out}\n${_build_err}\n")
  endif()

  # Determine success by binary existence — no per-target subprocess needed.
  # CMAKE_RUNTIME_OUTPUT_DIRECTORY is pinned to the build root in the mini-project.
  foreach(_brfb_var IN LISTS _brfb_pending)
    if(NOT DEFINED "${_brfb_var}")
      message(CHECK_START "Performing Test ${_brfb_var} (batch)")
      set(_brfb_success FALSE)
      if(_brfb_cfg_result EQUAL 0)
        if(EXISTS "${_brfb_build_dir}/${_brfb_var}"
           OR EXISTS "${_brfb_build_dir}/${_brfb_var}.exe")
          set(_brfb_success TRUE)
        endif()
      endif()
      if(_brfb_success)
        set(${_brfb_var} 1 CACHE INTERNAL "Test ${_brfb_var}" FORCE)
        message(CHECK_PASS "Success")
      else()
        set(${_brfb_var} "" CACHE INTERNAL "Test ${_brfb_var}" FORCE)
        message(CHECK_FAIL "Failed")
      endif()
    endif()
  endforeach()
endfunction()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
