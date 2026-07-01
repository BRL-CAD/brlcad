#     B R L C A D _ C H E C K F U N C T I O N S . C M A K E
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
# Automate putting variables from tests into a config.h.in file,
# and otherwise wrap check macros in extra logic as needed

include(CMakeParseArguments)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckIncludeFileCXX)
include(CheckTypeSize)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckCInline)
include(CheckCSourceRuns)
include(CheckCXXSourceRuns)

set(
  standard_header_template
  "
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_SIGNAL_H
# include <signal.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SYSCTL_H
typedef unsigned char u_char;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned short u_short;
# include <sys/sysctl.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
"
)

# Use this function to construct compile defines that uses the CMake
# header tests results to properly support the above header includes
function(standard_header_cppflags header_cppflags)
  set(have_cppflags)
  string(REPLACE "\n" ";" template_line "${standard_header_template}")
  foreach(line ${template_line})
    if("${line}" MATCHES ".* HAVE_.*_H")
      # extract the HAVE_* symbol from the #ifdef lines
      string(REGEX REPLACE "^[ ]*#[ ]*if.*[ ]+" "" have_symbol "${line}")
      if(NOT "${${have_symbol}}" MATCHES "^[\"]*$") # matches empty or ""
        set(have_cppflags "${have_cppflags} -D${have_symbol}=${${have_symbol}}")
      endif(NOT "${${have_symbol}}" MATCHES "^[\"]*$")
    endif("${line}" MATCHES ".* HAVE_.*_H")
  endforeach(line ${template_line})
  set(${header_cppflags} "${have_cppflags}" PARENT_SCOPE)
endfunction(standard_header_cppflags)

# The macros and functions below need C_STANDARD_FLAGS to be set for most
# compilers - be sure it is in those cases
if("${C_STANDARD_FLAGS}" STREQUAL "")
  message(FATAL_ERROR "C_STANDARD_FLAGS is not set - should at least be defining the C standard")
endif("${C_STANDARD_FLAGS}" STREQUAL "")

set(BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES ON CACHE BOOL "Batch compatible configure probes into parallel mini-project builds")
mark_as_advanced(BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)

###
# Register a compile/link probe into a named batch.  The source file must be
# written to ${CMAKE_BINARY_DIR}/CMakeTmp/<BATCH>_sources/<VAR>.c before the
# batch is executed.
###
macro(_brlcad_register_probe)
  cmake_parse_arguments(_BRP "" "BATCH;VAR" "LIBS" ${ARGN})

  get_property(_brp_list GLOBAL PROPERTY "BRLCAD_PROBE_BATCH_${_BRP_BATCH}")
  list(FIND _brp_list "${_BRP_VAR}" _brp_idx)
  if(_brp_idx EQUAL -1)
    list(APPEND _brp_list "${_BRP_VAR}")
    set_property(GLOBAL PROPERTY "BRLCAD_PROBE_BATCH_${_BRP_BATCH}" "${_brp_list}")
    if(_BRP_LIBS)
      set_property(GLOBAL PROPERTY "BRLCAD_PROBE_${_BRP_BATCH}_${_BRP_VAR}_LIBS" "${_BRP_LIBS}")
    endif()
  endif()

  unset(_brp_list)
  unset(_brp_idx)
  unset(_BRP_BATCH)
  unset(_BRP_VAR)
  unset(_BRP_LIBS)
endmacro()

###
# Execute all probes registered in a named batch as one generated C project,
# building its targets in parallel.  Successful probes are cached as "1";
# failed probes are cached as "".
###
function(_brlcad_run_probe_batch BATCH_NAME)
  if(NOT BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    return()
  endif()

  cmake_parse_arguments(_BRPB "" "C_FLAGS" "" ${ARGN})

  get_property(_batch_vars GLOBAL PROPERTY "BRLCAD_PROBE_BATCH_${BATCH_NAME}")
  if(NOT _batch_vars)
    return()
  endif()

  set(_pending_vars)
  foreach(_var IN LISTS _batch_vars)
    if(NOT DEFINED ${_var})
      list(APPEND _pending_vars "${_var}")
    endif()
  endforeach()

  set(_cfg_result 1)
  set(_build_dir "${CMAKE_BINARY_DIR}/CMakeTmp/${BATCH_NAME}_build")

  if(_pending_vars)
    set(_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/${BATCH_NAME}_sources")
    file(MAKE_DIRECTORY "${_src_dir}")

    set(_cml "${_src_dir}/CMakeLists.txt")
    file(WRITE "${_cml}"
      "cmake_minimum_required(VERSION 3.22)\n"
      "project(BRLCAD${BATCH_NAME}Batch C)\n"
      "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY \"\${CMAKE_BINARY_DIR}\")\n"
      "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG \"\${CMAKE_BINARY_DIR}\")\n"
      "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE \"\${CMAKE_BINARY_DIR}\")\n"
    )
    foreach(_var IN LISTS _pending_vars)
      file(APPEND "${_cml}" "add_executable(${_var} \"${_src_dir}/${_var}.c\")\n")
      get_property(_libs GLOBAL PROPERTY "BRLCAD_PROBE_${BATCH_NAME}_${_var}_LIBS")
      if(_libs)
        string(REPLACE ";" " " _libs_str "${_libs}")
        file(APPEND "${_cml}" "target_link_libraries(${_var} PRIVATE ${_libs_str})\n")
      endif()
    endforeach()

    set(_cfg_cmd
      "${CMAKE_COMMAND}"
      "-S" "${_src_dir}"
      "-B" "${_build_dir}"
      "-G" "${CMAKE_GENERATOR}"
      "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
    )
    if(CMAKE_GENERATOR_PLATFORM)
      list(APPEND _cfg_cmd "-A" "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
      list(APPEND _cfg_cmd "-T" "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    if(CMAKE_MAKE_PROGRAM)
      list(APPEND _cfg_cmd "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}")
    endif()
    if(CMAKE_BUILD_TYPE)
      list(APPEND _cfg_cmd "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")
    endif()
    if(_BRPB_C_FLAGS)
      list(APPEND _cfg_cmd "-DCMAKE_C_FLAGS=${_BRPB_C_FLAGS}")
    endif()

    execute_process(
      COMMAND ${_cfg_cmd}
      RESULT_VARIABLE _cfg_result
      OUTPUT_VARIABLE _cfg_out
      ERROR_VARIABLE _cfg_err
    )
    file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH PROBE CONFIGURE LOG: ${BATCH_NAME} ===\n${_cfg_out}\n${_cfg_err}\n")

    if(_cfg_result EQUAL 0)
      cmake_host_system_information(RESULT _ncpus QUERY NUMBER_OF_PHYSICAL_CORES)
      if(NOT _ncpus OR _ncpus LESS 1)
        set(_ncpus 1)
      endif()

      if(CMAKE_GENERATOR MATCHES "Ninja")
        set(_keepgoing "--" "-k" "0")
      elseif(CMAKE_GENERATOR MATCHES "Makefiles")
        set(_keepgoing "--" "-k")
      elseif(CMAKE_GENERATOR MATCHES "Xcode")
        set(_keepgoing "--" "-PBXBuildsContinueAfterErrors=YES")
      else()
        set(_keepgoing "")
      endif()

      set(_build_all "${CMAKE_COMMAND}" "--build" "${_build_dir}" "-j" "${_ncpus}" ${_keepgoing})
      if(CMAKE_BUILD_TYPE AND NOT _keepgoing)
        list(APPEND _build_all "--config" "${CMAKE_BUILD_TYPE}")
      endif()
      execute_process(
        COMMAND ${_build_all}
        RESULT_VARIABLE _ignored_result
        OUTPUT_VARIABLE _build_out
        ERROR_VARIABLE _build_err
      )
      file(APPEND "${CMAKE_BINARY_DIR}/CMakeFiles/CMakeError.log" "=== BATCH PROBE BUILD LOG: ${BATCH_NAME} ===\n${_build_out}\n${_build_err}\n")
    endif()
  endif()

  set(_batch_report)
  foreach(_var IN LISTS _batch_vars)
    if(NOT DEFINED ${_var})
      set(_success FALSE)
      if(_cfg_result EQUAL 0)
        if(EXISTS "${_build_dir}/${_var}" OR EXISTS "${_build_dir}/${_var}.exe")
          set(_success TRUE)
        endif()
      endif()
      if(_success)
        set(${_var} 1 CACHE INTERNAL "Test ${_var}" FORCE)
        set(_batch_result_line "Performing Test ${_var} - Success")
      else()
        set(${_var} "" CACHE INTERNAL "Test ${_var}" FORCE)
        set(_batch_result_line "Performing Test ${_var} - Failed")
      endif()
      if(_batch_report)
        string(APPEND _batch_report "\n-- ${_batch_result_line}")
      else()
        string(APPEND _batch_report "${_batch_result_line}")
      endif()
    endif()

    if(${_var} AND CONFIG_H_FILE)
      brlcad_deferred_define("${_var} 1")
    endif()
  endforeach()
  if(_batch_report)
    message(STATUS "${_batch_report}")
  endif()
endfunction()

macro(_brlcad_include_probe HEADER VAR)
  if(NOT BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    brlcad_include_file(${HEADER} ${VAR})
  else()
    set(_bip_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/INCLUDE_PROBE_sources")
    file(MAKE_DIRECTORY "${_bip_src_dir}")
    if(NOT DEFINED ${VAR})
      file(WRITE "${_bip_src_dir}/${VAR}.c" "#include <${HEADER}>\nint main(void) { return 0; }\n")
    endif()
    _brlcad_register_probe(BATCH INCLUDE_PROBE VAR ${VAR})
    unset(_bip_src_dir)
  endif()
endmacro()

macro(_brlcad_func_probe)
  cmake_parse_arguments(_BFP "" "FUNC;VAR" "LIBS" ${ARGN})

  if(NOT BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    if(_BFP_LIBS)
      brlcad_function_exists(${_BFP_FUNC} REQUIRED_LIBS ${_BFP_LIBS})
    else()
      brlcad_function_exists(${_BFP_FUNC})
    endif()
  else()
    set(_bfp_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/FUNC_EXISTS_sources")
    file(MAKE_DIRECTORY "${_bfp_src_dir}")
    if(NOT DEFINED ${_BFP_VAR})
      file(WRITE "${_bfp_src_dir}/${_BFP_VAR}.c"
        "#ifdef __cplusplus\nextern \"C\"\n#endif\nchar ${_BFP_FUNC}();\nint main(void) { return ${_BFP_FUNC}(); }\n"
      )
    endif()
    if(_BFP_LIBS)
      _brlcad_register_probe(BATCH FUNC_EXISTS VAR ${_BFP_VAR} LIBS ${_BFP_LIBS})
    else()
      _brlcad_register_probe(BATCH FUNC_EXISTS VAR ${_BFP_VAR})
    endif()
    unset(_bfp_src_dir)
  endif()

  unset(_BFP_FUNC)
  unset(_BFP_VAR)
  unset(_BFP_LIBS)
endmacro()

macro(_brlcad_struct_probe)
  cmake_parse_arguments(_BSP "" "STRUCT;MEMBER;HEADER;VAR" "" ${ARGN})

  if(NOT BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES)
    brlcad_struct_member("${_BSP_STRUCT}" ${_BSP_MEMBER} ${_BSP_HEADER} ${_BSP_VAR})
  else()
    set(_bsp_src_dir "${CMAKE_BINARY_DIR}/CMakeTmp/STRUCT_PROBE_sources")
    file(MAKE_DIRECTORY "${_bsp_src_dir}")
    if(NOT DEFINED HAVE_${_BSP_VAR})
      file(WRITE "${_bsp_src_dir}/HAVE_${_BSP_VAR}.c"
        "#include <${_BSP_HEADER}>\nint main(void) { ${_BSP_STRUCT} _s; (void)_s.${_BSP_MEMBER}; return 0; }\n"
      )
    endif()
    _brlcad_register_probe(BATCH STRUCT_PROBE VAR HAVE_${_BSP_VAR})
    unset(_bsp_src_dir)
  endif()

  unset(_BSP_STRUCT)
  unset(_BSP_MEMBER)
  unset(_BSP_HEADER)
  unset(_BSP_VAR)
endmacro()

###
# Check if a function exists (i.e., compiles to a valid symbol).  Adds
# HAVE_* define to config header, and HAVE_DECL_* and HAVE_WORKING_* if
# those tests are performed and successful.
###
macro(BRLCAD_FUNCTION_EXISTS function)
  # Use the upper case form of the function for variable names
  string(TOUPPER "${function}" var)

  # Only do the testing once per configure run
  if(NOT DEFINED HAVE_${var})
    # For this first test, be permissive.  For a number of cases, if
    # the function exists AT ALL we want to know it, so don't let the
    # flags get in the way
    set(CMAKE_C_FLAGS_TMP "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "")

    # Clear our testing variables, unless something specifically requested
    # is supplied as a command line argument
    cmake_push_check_state(RESET)
    if(${ARGC} GREATER 2)
      # Parse extra arguments
      cmake_parse_arguments(
        ${var}
        ""
        "DECL_TEST_SRCS"
        "WORKING_TEST_SRCS;REQUIRED_LIBS;REQUIRED_DEFS;REQUIRED_FLAGS;REQUIRED_DIRS"
        ${ARGN}
      )
      set(CMAKE_REQUIRED_LIBRARIES ${${var}_REQUIRED_LIBS})
      set(CMAKE_REQUIRED_FLAGS ${${var}_REQUIRED_FLAGS})
      set(CMAKE_REQUIRED_INCLUDES ${${var}_REQUIRED_DIRS})
      set(CMAKE_REQUIRED_DEFINITIONS ${${var}_REQUIRED_DEFS})
    endif(${ARGC} GREATER 2)

    # Set the compiler definitions for the standard headers
    if("${have_header_cppflags}" STREQUAL "")
      # get the cppflags once, the first time we test
      standard_header_cppflags(have_header_cppflags)
    endif("${have_header_cppflags}" STREQUAL "")
    set(CMAKE_REQUIRED_DEFINITIONS "${have_header_cppflags} ${CMAKE_REQUIRED_DEFINITIONS}")

    # First (permissive) test
    check_function_exists(${function} HAVE_${var})

    # Now, restore the C flags - any subsequent tests will be done using the
    # parent C_FLAGS environment.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_TMP}")

    if(HAVE_${var})
      # Test if the function is declared.This will set the HAVE_DECL_${var}
      # flag.  Unlike the HAVE_${var} results, this will not be automatically
      # written to the CONFIG_H_FILE - the caller must do so if they want to
      # capture the results.
      if(NOT DEFINED HAVE_DECL_${var})
        if(NOT MSVC)
          if(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
            if(ENABLE_ALL_CXX_COMPILE)
              check_cxx_source_compiles("${${${var}_DECL_TEST_SRCS}}" HAVE_DECL_${var})
            else(ENABLE_ALL_CXX_COMPILE)
              check_c_source_compiles("${${${var}_DECL_TEST_SRCS}}" HAVE_DECL_${var})
            endif(ENABLE_ALL_CXX_COMPILE)
          else(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
            if(ENABLE_ALL_CXX_COMPILE)
              check_cxx_source_compiles(
                "${standard_header_template}\nint main(void) {(void)${function}; return 0;}"
                HAVE_DECL_${var}
              )
            else(ENABLE_ALL_CXX_COMPILE)
              check_c_source_compiles(
                "${standard_header_template}\nint main(void) {(void)${function}; return 0;}"
                HAVE_DECL_${var}
              )
            endif(ENABLE_ALL_CXX_COMPILE)
          endif(NOT "${${var}_DECL_TEST_SRCS}" STREQUAL "")
        else()
          # Note - config_win.h (and probably other issues) make the decl tests
          # a no-go with Visual Studio - punt until the larger issues are
          # sorted out.
          set(HAVE_DECL_${var} 1)
        endif(NOT MSVC)
        set(HAVE_DECL_${var} ${HAVE_DECL_${var}} CACHE BOOL "Cache decl test result")
        mark_as_advanced(HAVE_DECL_${var})
      endif(NOT DEFINED HAVE_DECL_${var})

      # If we have sources supplied for the purpose, test if the function is working.
      if(NOT "${${var}_COMPILE_TEST_SRCS}" STREQUAL "")
        if(NOT DEFINED HAVE_WORKING_${var})
          set(HAVE_WORKING_${var} 1)
          foreach(test_src ${${var}_COMPILE_TEST_SRCS})
            if(ENABLE_ALL_CXX_COMPILE)
              check_cxx_source_compiles("${${test_src}}" ${var}_${test_src}_COMPILE)
            else(ENABLE_ALL_CXX_COMPILE)
              check_c_source_compiles("${${test_src}}" ${var}_${test_src}_COMPILE)
            endif(ENABLE_ALL_CXX_COMPILE)
            if(NOT ${var}_${test_src}_COMPILE)
              set(HAVE_WORKING_${var} 0)
            endif(NOT ${var}_${test_src}_COMPILE)
          endforeach(test_src ${${var}_COMPILE_TEST_SRCS})
          set(HAVE_WORKING_${var} ${HAVE_DECL_${var}} CACHE BOOL "Cache working test result")
          mark_as_advanced(HAVE_WORKING_${var})
        endif(NOT DEFINED HAVE_WORKING_${var})
      endif(NOT "${${var}_COMPILE_TEST_SRCS}" STREQUAL "")
    endif(HAVE_${var})

    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_${var})

  # The config file is regenerated every time CMake is run, so we
  # always need this bit even if the testing is already complete.
  if(CONFIG_H_FILE AND HAVE_${var})
    brlcad_deferred_define("HAVE_${var} 1")
  endif(CONFIG_H_FILE AND HAVE_${var})
  if(CONFIG_H_FILE AND HAVE_DECL_${var})
    brlcad_deferred_define("HAVE_DECL_${var} 1")
  endif(CONFIG_H_FILE AND HAVE_DECL_${var})
  if(CONFIG_H_FILE AND HAVE_WORKING_${var})
    brlcad_deferred_define("HAVE_WORKING_${var} 1")
  endif(CONFIG_H_FILE AND HAVE_WORKING_${var})
endmacro(BRLCAD_FUNCTION_EXISTS)

###
# Check if a header exists.  Headers requiring other headers being
# included first can be prepended in the filelist separated by
# semicolons.  Add HAVE_*_H define to config header.
###
macro(BRLCAD_CHECK_INCLUDE filelist var)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
  if(NOT "${ARGV2}" STREQUAL "")
    set(CMAKE_REQUIRED_INCLUDES ${ARGV2} ${CMAKE_REQUIRED_INCLUDES})
  endif(NOT "${ARGV2}" STREQUAL "")
  check_include_files("${filelist}" "${var}")
  cmake_pop_check_state()
endmacro(BRLCAD_CHECK_INCLUDE filelist var)

# Do the above check and add HAVE_*_H define to config header.
macro(BRLCAD_INCLUDE_FILE filelist var)
  brlcad_check_include(${filelist} ${var})
  if(CONFIG_H_FILE AND ${var})
    brlcad_deferred_define("${var} 1")
  endif(CONFIG_H_FILE AND ${var})
endmacro(BRLCAD_INCLUDE_FILE)

###
# Check if a C++ header exists.  Adds HAVE_* define to config header.
###
macro(BRLCAD_INCLUDE_FILE_CXX filename var)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CMAKE_CXX_STD_FLAG}")
  check_include_file_cxx(${filename} ${var})
  cmake_pop_check_state()

  if(CONFIG_H_FILE AND ${var})
    brlcad_deferred_define("${var} 1")
  endif(CONFIG_H_FILE AND ${var})
endmacro(BRLCAD_INCLUDE_FILE_CXX)

###
# Check the size of a given typename, setting the size in the provided
# variable.  If type has header prerequisites, a semicolon-separated header
# list may be specified.  Adds HAVE_ and SIZEOF_ defines to config header.
###
macro(BRLCAD_TYPE_SIZE typename headers)
  # Generate a variable name from the type - need to make sure we end up with a
  # valid variable string.
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" var ${typename})
  string(TOUPPER "${var}" var)

  # To make sure checks are re-run when re-testing the same type with different
  # headers, create a test variable incorporating both the typename and the
  # headers string
  string(REGEX REPLACE "[^a-zA-Z0-9]" "_" testvar "HAVE_${typename}${headers}")
  string(TOUPPER "${testvar}" testvar)

  # Proceed with type check.
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
  set(CMAKE_EXTRA_INCLUDE_FILES "${headers}")
  check_type_size(${typename} ${testvar})
  cmake_pop_check_state()

  # Produce config.h lines as appropriate
  if(CONFIG_H_FILE AND ${testvar})
    brlcad_deferred_define("HAVE_${var} 1")
    brlcad_deferred_define("SIZEOF_${var} ${${testvar}}")
  endif(CONFIG_H_FILE AND ${testvar})
endmacro(BRLCAD_TYPE_SIZE)

###
# Check if a given structure has a specific member element.  Structure
# should be defined within the semicolon-separated header file list.
# Adds HAVE_* to config header and sets VAR.
###
macro(BRLCAD_STRUCT_MEMBER structname member headers var)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")

  check_struct_has_member(${structname} ${member} "${headers}" HAVE_${var})

  cmake_pop_check_state()

  if(CONFIG_H_FILE AND HAVE_${var})
    brlcad_deferred_define("HAVE_${var} 1")
  endif(CONFIG_H_FILE AND HAVE_${var})
endmacro(BRLCAD_STRUCT_MEMBER)

###
# Check if a given function exists in the specified library.
###
macro(BRLCAD_CHECK_LIBRARY targetname lname func)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")

  # Don't error out on the builtin conflict case
  check_c_compiler_flag(-fno-builtin HAVE_NO_BUILTIN)
  if(HAVE_NO_BUILTIN)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -fno-builtin")
  endif(HAVE_NO_BUILTIN)

  if(NOT ${targetname}_LIBRARY)
    check_library_exists(${lname} ${func} "" HAVE_${targetname}_${lname})
  else(NOT ${targetname}_LIBRARY)
    set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${${targetname}_LIBRARY})
    check_function_exists(${func} HAVE_${targetname}_${lname})
  endif(NOT ${targetname}_LIBRARY)
  cmake_pop_check_state()

  if(HAVE_${targetname}_${lname})
    set(${targetname}_LIBRARY "${lname}")
  endif(HAVE_${targetname}_${lname})
endmacro(BRLCAD_CHECK_LIBRARY lname func)

# Special purpose functions

###
# Undocumented.
###
function(BRLCAD_CHECK_BASENAME)
  set(
    BASENAME_SRC
    "
#include <libgen.h>
int main(int argc, char *argv[]) {
if(!argc) return 1;
(void)basename(argv[0]);
return 0;
}"
  )
  if(NOT DEFINED HAVE_BASENAME)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${BASENAME_SRC}" HAVE_BASENAME)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${BASENAME_SRC}" HAVE_BASENAME)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_BASENAME)
  if(HAVE_BASENAME)
    brlcad_deferred_define("HAVE_BASENAME 1")
  endif(HAVE_BASENAME)
endfunction(BRLCAD_CHECK_BASENAME var)

###
# Undocumented.
###
function(BRLCAD_CHECK_DIRNAME)
  set(
    DIRNAME_SRC
    "
#include <libgen.h>
int main(int argc, char *argv[]) {
if(!argc) return 1;
(void)dirname(argv[0]);
return 0;
}"
  )
  if(NOT DEFINED HAVE_DIRNAME)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${DIRNAME_SRC}" HAVE_DIRNAME)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${DIRNAME_SRC}" HAVE_DIRNAME)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_DIRNAME)
  if(HAVE_DIRNAME)
    brlcad_deferred_define("HAVE_DIRNAME 1")
  endif(HAVE_DIRNAME)
endfunction(BRLCAD_CHECK_DIRNAME var)

###
# Undocumented.
# Based on AC_HEADER_SYS_WAIT
###
function(BRLCAD_HEADER_SYS_WAIT)
  set(
    SYS_WAIT_SRC
    "
#include <sys/types.h>
#include <sys/wait.h>
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned int) (stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
int main(void) {
  int s;
  wait(&s);
  s = WIFEXITED(s) ? WEXITSTATUS(s) : 1;
  return 0;
}"
  )
  if(NOT DEFINED WORKING_SYS_WAIT)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${SYS_WAIT_SRC}" WORKING_SYS_WAIT)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${SYS_WAIT_SRC}" WORKING_SYS_WAIT)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED WORKING_SYS_WAIT)
  if(WORKING_SYS_WAIT)
    brlcad_deferred_define("HAVE_SYS_WAIT_H 1")
  endif(WORKING_SYS_WAIT)
endfunction(BRLCAD_HEADER_SYS_WAIT)

###
function(brlcad_alloca)

  include(CheckCSourceRuns)
  include(CheckCXXSourceRuns)

  # Set up required flags
  if(ENABLE_ALL_CXX_COMPILE)
    # Use C++ flags for C++ checks
    set(ALLOCA_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${CXX_STANDARD_FLAGS}")
  else()
    # Use C flags for C checks
    set(ALLOCA_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
  endif()

  set(ALLOCA_TEST_TEMPLATE "
#include <stdlib.h>
@ALLOCA_IMPL@
int main(void)
{
    size_t i;
    unsigned char *p = (unsigned char *)BRLCAD_ALLOCA(4096);
    if (!p)
        return 1;
    for (i = 0; i < 4096; i++)
        p[i] = (unsigned char)(i & 0xff);
    for (i = 0; i < 4096; i++)
        if (p[i] != (unsigned char)(i & 0xff))
            return 1;
    return 0;
}
")

  if(MSVC)
    set(ALLOCA_IMPL "
#include <malloc.h>
#define BRLCAD_ALLOCA(size) _alloca(size)
")
    string(CONFIGURE "${ALLOCA_TEST_TEMPLATE}" TEST_SRC @ONLY)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${ALLOCA_REQUIRED_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${TEST_SRC}" HAVE_WORKING_ALLOCA)
    else()
      check_c_source_runs("${TEST_SRC}" HAVE_WORKING_ALLOCA)
    endif()
    cmake_pop_check_state()
    if(HAVE_WORKING_ALLOCA)
      config_h_append(BRLCAD "
#define HAVE_ALLOCA 1
#include <malloc.h>
#define BRLCAD_ALLOCA(size) _alloca(size)
")
      return()
    endif()
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(ALLOCA_IMPL "
#define BRLCAD_ALLOCA(size) __builtin_alloca(size)
")
    string(CONFIGURE "${ALLOCA_TEST_TEMPLATE}" TEST_SRC @ONLY)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${ALLOCA_REQUIRED_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${TEST_SRC}" HAVE_WORKING_BUILTIN_ALLOCA)
    else()
      check_c_source_runs("${TEST_SRC}" HAVE_WORKING_BUILTIN_ALLOCA)
    endif()
    cmake_pop_check_state()
    if(HAVE_WORKING_BUILTIN_ALLOCA)
      config_h_append(BRLCAD "
#define HAVE_ALLOCA 1
#define BRLCAD_ALLOCA(size) __builtin_alloca(size)
")
      return()
    endif()
    set(ALLOCA_IMPL "
    #include <alloca.h>
    #define BRLCAD_ALLOCA(size) __builtin_alloca(size)
    ")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${ALLOCA_REQUIRED_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${TEST_SRC}" HAVE_WORKING_BUILTIN_ALLOCA_W_HDR)
    else()
      check_c_source_runs("${TEST_SRC}" HAVE_WORKING_BUILTIN_ALLOCA_W_HDR)
    endif()
    cmake_pop_check_state()
    message("HAVE_WORKING_BUILTIN_ALLOCA_W_HDR: ${HAVE_WORKING_BUILTIN_ALLOCA_W_HDR}")
    if(HAVE_WORKING_BUILTIN_ALLOCA_W_HDR)
      config_h_append(BRLCAD "
#define HAVE_ALLOCA 1
#define BRLCAD_ALLOCA(size) __builtin_alloca(size)
")
      return()
    endif()
  endif()

  # Fallback: alloca.h
  set(ALLOCA_IMPL "
#include <alloca.h>
#define BRLCAD_ALLOCA(size) alloca(size)
")
  string(CONFIGURE "${ALLOCA_TEST_TEMPLATE}" TEST_SRC @ONLY)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS "${ALLOCA_REQUIRED_FLAGS}")
  if(ENABLE_ALL_CXX_COMPILE)
    check_cxx_source_runs("${TEST_SRC}" HAVE_WORKING_ALLOCA_H)
  else()
    check_c_source_runs("${TEST_SRC}" HAVE_WORKING_ALLOCA_H)
  endif()
  cmake_pop_check_state()
  if(HAVE_WORKING_ALLOCA_H)
    config_h_append(BRLCAD "
#define HAVE_ALLOCA 1
#define HAVE_ALLOCA_H 1
#include <alloca.h>
#define BRLCAD_ALLOCA(size) alloca(size)
")
    return()
  endif()
endfunction()

###
# See if the compiler supports the C99 %z print specifier for size_t.
# Sets -DHAVE_STDINT_H=1 as global preprocessor flag if found.
###
function(BRLCAD_CHECK_PERCENT_Z)
  check_include_file(stdint.h HAVE_STDINT_H)

  set(
    CHECK_PERCENT_Z_SRC
    "
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif
#include <stdio.h>
int main(int ac, char *av[])
{
  char buf[64] = {0};
  if (sprintf(buf, \"%zu\", (size_t)1) != 1 || buf[0] != '1' || buf[1] != 0)
    return 1;
  return (ac < 0) ? (int)av[0][0] : 0;
}
"
  )

  if(NOT DEFINED HAVE_PERCENT_Z)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(HAVE_STDINT_H)
      set(CMAKE_REQUIRED_DEFINITIONS "-DHAVE_STDINT_H=1")
    endif(HAVE_STDINT_H)
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${CHECK_PERCENT_Z_SRC}" HAVE_PERCENT_Z)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${CHECK_PERCENT_Z_SRC}" HAVE_PERCENT_Z)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_PERCENT_Z)

  if(HAVE_PERCENT_Z)
    brlcad_deferred_define("HAVE_PERCENT_Z 1")
  endif(HAVE_PERCENT_Z)
endfunction(BRLCAD_CHECK_PERCENT_Z)

function(BRLCAD_CHECK_STATIC_ARRAYS)
  set(
    CHECK_STATIC_ARRAYS_SRC
    "
#include <stdio.h>
#include <string.h>

int foobar(char arg[static 100])
{
  return (int)arg[0];
}

int main(int ac, char *av[])
{
  char hello[100];

  if (ac > 0 && av)
    foobar(hello);
  return 0;
}
"
  )
  if(NOT DEFINED HAVE_STATIC_ARRAYS)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    if(ENABLE_ALL_CXX_COMPILE)
      check_cxx_source_runs("${CHECK_STATIC_ARRAYS_SRC}" HAVE_STATIC_ARRAYS)
    else(ENABLE_ALL_CXX_COMPILE)
      check_c_source_runs("${CHECK_STATIC_ARRAYS_SRC}" HAVE_STATIC_ARRAYS)
    endif(ENABLE_ALL_CXX_COMPILE)
    cmake_pop_check_state()
  endif(NOT DEFINED HAVE_STATIC_ARRAYS)
  if(HAVE_STATIC_ARRAYS)
    brlcad_deferred_define("HAVE_STATIC_ARRAYS 1")
  endif(HAVE_STATIC_ARRAYS)
endfunction(BRLCAD_CHECK_STATIC_ARRAYS)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
