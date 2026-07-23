#       B R L C A D _ E X T E R N A L D E P S . C M A K E
# BRL-CAD
#
# Copyright (c) 2023-2026 United States Government as represented by
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
#
# Logic to set up external 3rd-party dependences, either system
# installed or locally prebuilt for bundling with BRL-CAD.
#
# BRLCAD_EXT_DIR is a successor method for managing increasingly
# large, complex, and numerous 3rd-party dependencies in BRL-CAD.
# Dependencies are in separate "bext" repository:
# https://github.com/BRL-CAD/bext.git
#
# BRL-CAD's build will automatically download, compile, and use the
# "bext" repository by default, but building separately is recommended
# for devs who compile BRL-CAD frequently.  Once installed, specify
# BRLCAD_EXT_DIR to specify the prebuilt "bext" install directory:
#
#  cmake path/to/brlcad -DBRLCAD_EXT_DIR=path/to/bext_output
#
# Alternatively, devs can manually create a CMakeUserPresets.json file
# in BRL-CAD's source dir containing:
#
# {
#   "version": 6,
#   "configurePresets": [
#     {
#       "name": "my_bext",
#       "cacheVariables": {
#         "BRLCAD_EXT_DIR": {
#           "type": "PATH",
#           "value": "path/to/bext_output"
#         }
#       }
#     }
#   ]
# }
#
# Copied into BRL-CAD's source root, cmake can be shortened to:
#
# cmake ../brlcad --preset=my_bext
#
# Multiple presets can be defined to allow selection of different
# build configurations.  For example, we could define a second preset
# that enables Qt (e.g., cmake ../brlcad --preset=bext_qt):
#
# {
#   "version": 6,
#   "configurePresets": [
#     {
#       "name": "my_bext",
#       "cacheVariables": {
#         "BRLCAD_EXT_DIR": {
#           "type": "PATH",
#           "value": "path/to/bext_output"
#         }
#       }
#     },
#     {
#       "name": "bext_qt",
#       "inherits": "my_bext",
#       "cacheVariables": {
#         "BRLCAD_ENABLE_QT": "ON"
#       }
#     }
#   ]
# }
#
###

# FIXME: File defines globals used outside this file.

# When we need to have CMake treat includes as system paths to avoid
# warnings, we add those patterns to the SYS_INCLUDE_PATTERNS list.
#
# TODO - with the modern target-based approach to dependencies, we should
# be able to dispense with this global variable all together...
mark_as_advanced(SYS_INCLUDE_PATTERNS)


#####################################################################
# Utility functions for use when processing ${BRLCAD_EXT_DIR}/install files
#####################################################################

# In multiconfig we need to scrub excluded files out of multiple
# ${BRLCAD_EXT_DIR}/install copies, so wrap logic to do so into a
# function.
function(strip_excluded RDIR EXPATTERNS)
  foreach(ep ${${EXPATTERNS}})
    file(GLOB_RECURSE MATCHING_FILES LIST_DIRECTORIES false RELATIVE "${RDIR}" "${RDIR}/${ep}")
    foreach(rf ${MATCHING_FILES})
      file(REMOVE "${RDIR}/${rf}")
      if(EXISTS ${RDIR}/${rf})
        message(FATAL_ERROR "Removing ${RDIR}/${rf} failed")
      endif(EXISTS ${RDIR}/${rf})
    endforeach(rf ${MATCHING_FILES})
  endforeach(ep ${${EXPATTERNS}})
endfunction(strip_excluded)

# See if a file matches a pattern to skip its processing.
# Sets the variable held in SVAR in the parent scope.
function(skip_processing tf SVAR)
  if(IS_SYMLINK ${tf})
    set(${SVAR} 1 PARENT_SCOPE)
    return()
  endif(IS_SYMLINK ${tf})
  foreach(skp ${NOPROCESS_PATTERNS})
    if("${tf}" MATCHES "${skp}")
      set(${SVAR} 1 PARENT_SCOPE)
      return()
    endif("${tf}" MATCHES "${skp}")
  endforeach(skp ${NOPROCESS_PATTERNS})
  set(${SVAR} 0 PARENT_SCOPE)
endfunction(skip_processing)

function(brlcad_ext_write_file_list outfile)
  file(WRITE "${outfile}" "")
  foreach(_brlcad_ext_file ${ARGN})
    file(APPEND "${outfile}" "${_brlcad_ext_file}\n")
  endforeach(_brlcad_ext_file ${ARGN})
endfunction(brlcad_ext_write_file_list)

function(brlcad_ext_path_forms outvar)
  set(_brlcad_ext_path_forms)
  foreach(_brlcad_ext_path ${ARGN})
    if("${_brlcad_ext_path}" STREQUAL "")
      continue()
    endif("${_brlcad_ext_path}" STREQUAL "")
    list(APPEND _brlcad_ext_path_forms "${_brlcad_ext_path}")
    get_filename_component(_brlcad_ext_abs_path "${_brlcad_ext_path}" ABSOLUTE)
    list(APPEND _brlcad_ext_path_forms "${_brlcad_ext_abs_path}")
    file(REAL_PATH "${_brlcad_ext_path}" _brlcad_ext_real_path)
    list(APPEND _brlcad_ext_path_forms "${_brlcad_ext_real_path}")
  endforeach(_brlcad_ext_path ${ARGN})
  if(_brlcad_ext_path_forms)
    list(REMOVE_DUPLICATES _brlcad_ext_path_forms)
  endif(_brlcad_ext_path_forms)
  set(${outvar} ${_brlcad_ext_path_forms} PARENT_SCOPE)
endfunction(brlcad_ext_path_forms)

function(brlcad_ext_path_clear_targets outvar)
  brlcad_ext_path_forms(_brlcad_ext_path_forms ${ARGN})
  set(_brlcad_ext_path_targets ${_brlcad_ext_path_forms})
  foreach(_brlcad_ext_path ${_brlcad_ext_path_forms})
    list(APPEND _brlcad_ext_path_targets "${_brlcad_ext_path}/")
  endforeach(_brlcad_ext_path ${_brlcad_ext_path_forms})
  if(_brlcad_ext_path_targets)
    list(REMOVE_DUPLICATES _brlcad_ext_path_targets)
  endif(_brlcad_ext_path_targets)
  set(${outvar} ${_brlcad_ext_path_targets} PARENT_SCOPE)
endfunction(brlcad_ext_path_clear_targets)

function(brlcad_ext_count_strclear_updates outvar output)
  string(REGEX MATCHALL "[^\n\r]+: +(cleared|replaced) [0-9]+ instances" _brlcad_ext_strclear_matches "${output}")
  list(LENGTH _brlcad_ext_strclear_matches _brlcad_ext_strclear_count)
  set(${outvar} "${_brlcad_ext_strclear_count}" PARENT_SCOPE)
endfunction(brlcad_ext_count_strclear_updates)

function(brlcad_ext_append_strclear_log logfile label output)
  brlcad_ext_count_strclear_updates(_brlcad_ext_strclear_count "${output}")
  if(_brlcad_ext_strclear_count GREATER 0)
    file(APPEND "${logfile}" "\n==== ${label} ====\n${output}")
    if(NOT "${output}" MATCHES "\n$")
      file(APPEND "${logfile}" "\n")
    endif(NOT "${output}" MATCHES "\n$")
  endif(_brlcad_ext_strclear_count GREATER 0)
endfunction(brlcad_ext_append_strclear_log)

function(brlcad_ext_append_file_list_log logfile label)
  if(ARGN)
    list(LENGTH ARGN _brlcad_ext_file_count)
    file(APPEND "${logfile}" "\n==== ${label} (${_brlcad_ext_file_count} files) ====\n")
    foreach(_brlcad_ext_file ${ARGN})
      file(APPEND "${logfile}" "${_brlcad_ext_file}\n")
    endforeach(_brlcad_ext_file ${ARGN})
  endif(ARGN)
endfunction(brlcad_ext_append_file_list_log)

function(brlcad_ext_rpath_output_to_properties prefix output)
  string(REPLACE ";" "@BRLCAD_EXT_SEMICOLON@" _brlcad_ext_rpath_output "${output}")
  string(REPLACE "\r\n" "\n" _brlcad_ext_rpath_output "${_brlcad_ext_rpath_output}")
  string(REPLACE "\n" ";" _brlcad_ext_rpath_lines "${_brlcad_ext_rpath_output}")
  foreach(_brlcad_ext_rpath_line ${_brlcad_ext_rpath_lines})
    if("${_brlcad_ext_rpath_line}" MATCHES "^([^:]+):(.*)$")
      set(_brlcad_ext_rpath_file "${CMAKE_MATCH_1}")
      set(_brlcad_ext_rpath_value "${CMAKE_MATCH_2}")
      string(REPLACE "@BRLCAD_EXT_SEMICOLON@" ";" _brlcad_ext_rpath_value "${_brlcad_ext_rpath_value}")
      string(SHA256 _brlcad_ext_rpath_key "${_brlcad_ext_rpath_file}")
      set_property(GLOBAL PROPERTY "BRLCAD_EXT_RPATH_${prefix}_${_brlcad_ext_rpath_key}" "${_brlcad_ext_rpath_value}")
    endif("${_brlcad_ext_rpath_line}" MATCHES "^([^:]+):(.*)$")
  endforeach(_brlcad_ext_rpath_line ${_brlcad_ext_rpath_lines})
endfunction(brlcad_ext_rpath_output_to_properties)

function(brlcad_ext_log_plief_rpath_changes outvar logfile label before_prefix after_prefix)
  set(_brlcad_ext_rpath_change_count 0)
  set(_brlcad_ext_rpath_change_log)
  foreach(_brlcad_ext_rpath_file ${ARGN})
    string(SHA256 _brlcad_ext_rpath_key "${_brlcad_ext_rpath_file}")
    get_property(_brlcad_ext_before GLOBAL PROPERTY "BRLCAD_EXT_RPATH_${before_prefix}_${_brlcad_ext_rpath_key}")
    get_property(_brlcad_ext_after GLOBAL PROPERTY "BRLCAD_EXT_RPATH_${after_prefix}_${_brlcad_ext_rpath_key}")
    if("${_brlcad_ext_before}" STREQUAL "")
      set(_brlcad_ext_before "<none>")
    endif("${_brlcad_ext_before}" STREQUAL "")
    if("${_brlcad_ext_after}" STREQUAL "")
      set(_brlcad_ext_after "<none>")
    endif("${_brlcad_ext_after}" STREQUAL "")
    if(NOT "${_brlcad_ext_before}" STREQUAL "${_brlcad_ext_after}")
      math(EXPR _brlcad_ext_rpath_change_count "${_brlcad_ext_rpath_change_count} + 1")
      string(APPEND _brlcad_ext_rpath_change_log "${_brlcad_ext_rpath_file}\n  before: ${_brlcad_ext_before}\n  after:  ${_brlcad_ext_after}\n")
    endif(NOT "${_brlcad_ext_before}" STREQUAL "${_brlcad_ext_after}")
  endforeach(_brlcad_ext_rpath_file ${ARGN})

  if(_brlcad_ext_rpath_change_count GREATER 0)
    file(APPEND "${logfile}" "\n==== ${label} (${_brlcad_ext_rpath_change_count} files) ====\n")
    file(APPEND "${logfile}" "${_brlcad_ext_rpath_change_log}")
  endif(_brlcad_ext_rpath_change_count GREATER 0)
  set(${outvar} "${_brlcad_ext_rpath_change_count}" PARENT_SCOPE)
endfunction(brlcad_ext_log_plief_rpath_changes)

function(brlcad_ext_append_plief_report_log outvar logfile label output)
  set(_brlcad_ext_rpath_change_count 0)
  set(_brlcad_ext_rpath_change_log)
  string(REPLACE ";" "@BRLCAD_EXT_SEMICOLON@" _brlcad_ext_plief_output "${output}")
  string(REPLACE "\r\n" "\n" _brlcad_ext_plief_output "${_brlcad_ext_plief_output}")
  string(REPLACE "\n" ";" _brlcad_ext_plief_lines "${_brlcad_ext_plief_output}")
  foreach(_brlcad_ext_plief_line ${_brlcad_ext_plief_lines})
    if("${_brlcad_ext_plief_line}" MATCHES "^([^\t]*)\t([^\t]*)\t(.*)$")
      set(_brlcad_ext_rpath_file "${CMAKE_MATCH_1}")
      set(_brlcad_ext_before "${CMAKE_MATCH_2}")
      set(_brlcad_ext_after "${CMAKE_MATCH_3}")
      string(REPLACE "@BRLCAD_EXT_SEMICOLON@" ";" _brlcad_ext_before "${_brlcad_ext_before}")
      string(REPLACE "@BRLCAD_EXT_SEMICOLON@" ";" _brlcad_ext_after "${_brlcad_ext_after}")
      if("${_brlcad_ext_before}" STREQUAL "")
        set(_brlcad_ext_before "<none>")
      endif("${_brlcad_ext_before}" STREQUAL "")
      if("${_brlcad_ext_after}" STREQUAL "")
        set(_brlcad_ext_after "<none>")
      endif("${_brlcad_ext_after}" STREQUAL "")
      math(EXPR _brlcad_ext_rpath_change_count "${_brlcad_ext_rpath_change_count} + 1")
      string(APPEND _brlcad_ext_rpath_change_log "${_brlcad_ext_rpath_file}\n  before: ${_brlcad_ext_before}\n  after:  ${_brlcad_ext_after}\n")
    endif("${_brlcad_ext_plief_line}" MATCHES "^([^\t]*)\t([^\t]*)\t(.*)$")
  endforeach(_brlcad_ext_plief_line ${_brlcad_ext_plief_lines})

  if(_brlcad_ext_rpath_change_count GREATER 0)
    file(APPEND "${logfile}" "\n==== ${label} (${_brlcad_ext_rpath_change_count} files) ====\n")
    file(APPEND "${logfile}" "${_brlcad_ext_rpath_change_log}")
  endif(_brlcad_ext_rpath_change_count GREATER 0)
  set(${outvar} "${_brlcad_ext_rpath_change_count}" PARENT_SCOPE)
endfunction(brlcad_ext_append_plief_report_log)


# Look for CMake files used in find_package. pkgconfig (*.pc) files can be used
# by CMake, so they are treated as CMake inputs for the purposes of this test.
set(CMAKE_PATTERNS
  ".*Deps.cmake$"
  ".*Modules.cmake$"
  ".*Version.cmake$"
  ".*argets.cmake$"
  ".*onfig.cmake$"
  ".*.pc$"
  )
function(is_cmake_file tf CVAR)
  foreach(cmk ${CMAKE_PATTERNS})
    if("${tf}" MATCHES "${cmk}")
      set(${CVAR} 1 PARENT_SCOPE)
      return()
    endif("${tf}" MATCHES "${cmk}")
  endforeach(cmk ${CMAKE_PATTERNS})
  set(${CVAR} 0 PARENT_SCOPE)
endfunction(is_cmake_file)

# Since the checking process can be long, we want some sort of
# feedback indicating we're progressing.
function(bfile_type_msg ALL_CNT ALL_PROCESSED BINARY_LIST)
  list(LENGTH ${BINARY_LIST} BCNT)
  if(BCNT)
    math(EXPR skip_msg "${BCNT} % 100")
    if(${skip_msg} EQUAL 0)
      message("Found ${BCNT} shared object or executable files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif(${skip_msg} EQUAL 0)
  endif(BCNT)
endfunction(bfile_type_msg)

function(nfile_type_msg ALL_CNT ALL_PROCESSED NOEXEC_LIST)
  list(LENGTH ${NOEXEC_LIST} NCNT)
  if(NCNT)
    math(EXPR skip_msg "${NCNT} % 100")
    if(${skip_msg} EQUAL 0)
      message("Found ${NCNT} binary files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif(${skip_msg} EQUAL 0)
  endif(NCNT)
endfunction(nfile_type_msg)

function(tfile_type_msg ALL_CNT ALL_PROCESSED TEXT_LIST)
  list(LENGTH ${TEXT_LIST} TCNT)
  if(TCNT)
    math(EXPR skip_msg "${TCNT} % 500")
    if(${skip_msg} EQUAL 0)
      message("Found ${TCNT} text files (characterized ${ALL_PROCESSED} of ${ALL_CNT} files.")
      return()
    endif(${skip_msg} EQUAL 0)
  endif(TCNT)
endfunction(tfile_type_msg)

function(brlcad_ext_record_file_type out_prop count_prop label interval all_cnt fname)
  set_property(GLOBAL APPEND PROPERTY ${out_prop} "${fname}")

  get_property(_brlcad_ext_count GLOBAL PROPERTY ${count_prop})
  if(NOT _brlcad_ext_count)
    set(_brlcad_ext_count 0)
  endif(NOT _brlcad_ext_count)
  math(EXPR _brlcad_ext_count "${_brlcad_ext_count} + 1")
  set_property(GLOBAL PROPERTY ${count_prop} "${_brlcad_ext_count}")

  get_property(_brlcad_ext_processed GLOBAL PROPERTY BRLCAD_EXT_PROCESSED_FILE_COUNT)
  if(NOT _brlcad_ext_processed)
    set(_brlcad_ext_processed 0)
  endif(NOT _brlcad_ext_processed)
  math(EXPR _brlcad_ext_processed "${_brlcad_ext_processed} + 1")
  set_property(GLOBAL PROPERTY BRLCAD_EXT_PROCESSED_FILE_COUNT "${_brlcad_ext_processed}")

  math(EXPR _brlcad_ext_skip_msg "${_brlcad_ext_count} % ${interval}")
  if(_brlcad_ext_count AND ${_brlcad_ext_skip_msg} EQUAL 0)
    message("Found ${_brlcad_ext_count} ${label} files (characterized ${_brlcad_ext_processed} of ${all_cnt} files.")
  endif(_brlcad_ext_count AND ${_brlcad_ext_skip_msg} EQUAL 0)
endfunction(brlcad_ext_record_file_type)

function(brlcad_ext_record_file_type_list out_prop count_prop label interval all_cnt)
  foreach(_brlcad_ext_fname ${ARGN})
    brlcad_ext_record_file_type(${out_prop} ${count_prop} "${label}" ${interval} ${all_cnt} "${_brlcad_ext_fname}")
  endforeach(_brlcad_ext_fname ${ARGN})
endfunction(brlcad_ext_record_file_type_list)

# For processing purposes, there are three categories of
# ${BRLCAD_EXT_DIR}/install file:
#
# 1.  exe or shared library files needing RPATH adjustment (which also
#     need to be installed with executable permissions)
# 2.  binary files which are NOT executable files (images, etc.)
# 3.  text files
#
# If we don't want to hard-code information about specific files we
# expect to find in ${BRLCAD_EXT_DIR}/install, we need a way to detect
# "on the fly" what we are dealing with.
function(
  file_type
  fname
  ALL_CNT
)
  if(IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})
    return()
  endif(IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})

  foreach(skp ${NOPROCESS_PATTERNS})
    if("${fname}" MATCHES "${skp}")
      brlcad_ext_record_file_type(BRLCAD_EXT_NTEXT_FILES BRLCAD_EXT_NTEXT_COUNT "text" 500 ${ALL_CNT} "${fname}")
      return()
    endif("${fname}" MATCHES "${skp}")
  endforeach(skp ${NOPROCESS_PATTERNS})
  execute_process(COMMAND ${STRCLEAR_EXECUTABLE} -B ${CMAKE_BINARY_DIR}/${fname} RESULT_VARIABLE TXT_FILE)
  if("${TXT_FILE}" GREATER 0)
    brlcad_ext_record_file_type(BRLCAD_EXT_NTEXT_FILES BRLCAD_EXT_NTEXT_COUNT "text" 500 ${ALL_CNT} "${fname}")
    return()
  endif("${TXT_FILE}" GREATER 0)

  # Some kind of binary file, can we set an RPATH?
  if(P_RPATH_EXECUTABLE)
    execute_process(
      COMMAND ${P_RPATH_EXECUTABLE} ${CMAKE_BINARY_DIR}/${fname}
      RESULT_VARIABLE NOT_BIN_OBJ
      OUTPUT_VARIABLE NB_OUT
      ERROR_VARIABLE NB_ERR
    )
    if(NOT_BIN_OBJ)
      brlcad_ext_record_file_type(BRLCAD_EXT_NNOEXEC_FILES BRLCAD_EXT_NNOEXEC_COUNT "binary" 100 ${ALL_CNT} "${fname}")
      return()
    else(NOT_BIN_OBJ)
      #message("File needing RPATH setting: ${fname}")
      brlcad_ext_record_file_type(BRLCAD_EXT_NBINARY_FILES BRLCAD_EXT_NBINARY_COUNT "shared object or executable" 100 ${ALL_CNT} "${fname}")
      return()
    endif(NOT_BIN_OBJ)
  endif(P_RPATH_EXECUTABLE)
  if(APPLE)
    execute_process(
      COMMAND otool -l ${CMAKE_BINARY_DIR}/${fname}
      RESULT_VARIABLE ORESULT
      OUTPUT_VARIABLE OTOOL_OUT
      ERROR_VARIABLE NB_ERR
    )
    if("${OTOOL_OUT}" MATCHES "Archive")
      brlcad_ext_record_file_type(BRLCAD_EXT_NNOEXEC_FILES BRLCAD_EXT_NNOEXEC_COUNT "binary" 100 ${ALL_CNT} "${fname}")
      return()
    endif("${OTOOL_OUT}" MATCHES "Archive")
    if("${OTOOL_OUT}" MATCHES "not an object")
      brlcad_ext_record_file_type(BRLCAD_EXT_NNOEXEC_FILES BRLCAD_EXT_NNOEXEC_COUNT "binary" 100 ${ALL_CNT} "${fname}")
      return()
    endif("${OTOOL_OUT}" MATCHES "not an object")
    # Not one of the exceptions - binary
    #message("File needing RPATH setting: ${fname}")
    brlcad_ext_record_file_type(BRLCAD_EXT_NBINARY_FILES BRLCAD_EXT_NBINARY_COUNT "shared object or executable" 100 ${ALL_CNT} "${fname}")
    return()
  endif(APPLE)

  # If we haven't figured it out, treat as noexec binary
  brlcad_ext_record_file_type(BRLCAD_EXT_NNOEXEC_FILES BRLCAD_EXT_NNOEXEC_COUNT "binary" 100 ${ALL_CNT} "${fname}")
endfunction(file_type)

function(brlcad_ext_batch_file_type ALL_CNT)
  set(_brlcad_ext_text_files)
  set(_brlcad_ext_binary_probe_files)

  foreach(fname ${ARGN})
    if(IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})
      continue()
    endif(IS_SYMLINK ${CMAKE_BINARY_DIR}/${fname})

    set(_brlcad_ext_skip FALSE)
    foreach(skp ${NOPROCESS_PATTERNS})
      if("${fname}" MATCHES "${skp}")
        set(_brlcad_ext_skip TRUE)
        break()
      endif("${fname}" MATCHES "${skp}")
    endforeach(skp ${NOPROCESS_PATTERNS})

    if(_brlcad_ext_skip)
      list(APPEND _brlcad_ext_text_files "${fname}")
    else(_brlcad_ext_skip)
      list(APPEND _brlcad_ext_binary_probe_files "${fname}")
    endif(_brlcad_ext_skip)
  endforeach(fname ${ARGN})

  set(_brlcad_ext_binary_files)
  if(_brlcad_ext_binary_probe_files)
    set(_brlcad_ext_classify_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_classify_strclear.txt")
    brlcad_ext_write_file_list("${_brlcad_ext_classify_list}" ${_brlcad_ext_binary_probe_files})
    execute_process(
      COMMAND ${STRCLEAR_EXECUTABLE} --classify --files "${_brlcad_ext_classify_list}"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      RESULT_VARIABLE _brlcad_ext_strclear_classify_result
      OUTPUT_VARIABLE _brlcad_ext_strclear_classify_output
      ERROR_VARIABLE _brlcad_ext_strclear_classify_error
    )
    if(NOT _brlcad_ext_strclear_classify_result EQUAL 0)
      message(WARNING "Batch strclear classification failed, falling back to per-file characterization: ${_brlcad_ext_strclear_classify_error}")
      foreach(fname ${ARGN})
        file_type("${fname}" ${ALL_CNT})
      endforeach(fname ${ARGN})
      return()
    endif(NOT _brlcad_ext_strclear_classify_result EQUAL 0)

    string(REPLACE "\n" ";" _brlcad_ext_strclear_records "${_brlcad_ext_strclear_classify_output}")
    foreach(_brlcad_ext_record ${_brlcad_ext_strclear_records})
      if(NOT _brlcad_ext_record)
        continue()
      endif(NOT _brlcad_ext_record)
      string(JSON _brlcad_ext_class_type ERROR_VARIABLE _brlcad_ext_class_type_error GET "${_brlcad_ext_record}" type)
      string(JSON _brlcad_ext_class_path ERROR_VARIABLE _brlcad_ext_class_path_error GET "${_brlcad_ext_record}" path)
      if(NOT _brlcad_ext_class_type_error STREQUAL "NOTFOUND" OR NOT _brlcad_ext_class_path_error STREQUAL "NOTFOUND")
        message(WARNING "Batch strclear classification returned invalid JSON, falling back to per-file characterization.")
        foreach(fname ${ARGN})
          file_type("${fname}" ${ALL_CNT})
        endforeach(fname ${ARGN})
        return()
      endif(NOT _brlcad_ext_class_type_error STREQUAL "NOTFOUND" OR NOT _brlcad_ext_class_path_error STREQUAL "NOTFOUND")
      if("${_brlcad_ext_class_type}" STREQUAL "TEXT")
        list(APPEND _brlcad_ext_text_files "${_brlcad_ext_class_path}")
      elseif("${_brlcad_ext_class_type}" STREQUAL "BINARY")
        list(APPEND _brlcad_ext_binary_files "${_brlcad_ext_class_path}")
      else("${_brlcad_ext_class_type}" STREQUAL "TEXT")
        message(WARNING "Batch strclear classification returned unknown type ${_brlcad_ext_class_type}, falling back to per-file characterization.")
        foreach(fname ${ARGN})
          file_type("${fname}" ${ALL_CNT})
        endforeach(fname ${ARGN})
        return()
      endif("${_brlcad_ext_class_type}" STREQUAL "TEXT")
    endforeach(_brlcad_ext_record ${_brlcad_ext_strclear_records})
  endif(_brlcad_ext_binary_probe_files)

  set(_brlcad_ext_rpath_files)
  set(_brlcad_ext_noexec_files)
  if(_brlcad_ext_binary_files AND NOT P_RPATH_SUPPORTS_CLASSIFY)
    # No RPATH-capable tool (e.g. plief) is available to sub-classify the
    # binaries.  This is the normal case on Windows, where PE files have no
    # RPATH concept - there is nothing to set, so every binary is treated as
    # a non-exec binary (matching the per-file file_type() fallback, which
    # drops through to the noexec bucket when P_RPATH_EXECUTABLE is unset).
    set(_brlcad_ext_noexec_files ${_brlcad_ext_binary_files})
  else()
    set(_brlcad_ext_plief_classify_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_classify_plief.txt")
    brlcad_ext_write_file_list("${_brlcad_ext_plief_classify_list}" ${_brlcad_ext_binary_files})
    execute_process(
      COMMAND ${P_RPATH_EXECUTABLE} --classify --files "${_brlcad_ext_plief_classify_list}"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      RESULT_VARIABLE _brlcad_ext_plief_classify_result
      OUTPUT_VARIABLE _brlcad_ext_plief_classify_output
      ERROR_VARIABLE _brlcad_ext_plief_classify_error
    )
    if(NOT _brlcad_ext_plief_classify_result EQUAL 0)
      message(WARNING "Batch plief classification failed, falling back to per-file characterization: ${_brlcad_ext_plief_classify_error}")
      foreach(fname ${ARGN})
	file_type("${fname}" ${ALL_CNT})
      endforeach(fname ${ARGN})
      return()
    endif(NOT _brlcad_ext_plief_classify_result EQUAL 0)

    string(REPLACE "\n" ";" _brlcad_ext_plief_records "${_brlcad_ext_plief_classify_output}")
    foreach(_brlcad_ext_record ${_brlcad_ext_plief_records})
      if(NOT _brlcad_ext_record)
	continue()
      endif(NOT _brlcad_ext_record)
      string(JSON _brlcad_ext_class_type ERROR_VARIABLE _brlcad_ext_class_type_error GET "${_brlcad_ext_record}" type)
      string(JSON _brlcad_ext_class_path ERROR_VARIABLE _brlcad_ext_class_path_error GET "${_brlcad_ext_record}" path)
      if(NOT _brlcad_ext_class_type_error STREQUAL "NOTFOUND" OR NOT _brlcad_ext_class_path_error STREQUAL "NOTFOUND")
	message(WARNING "Batch plief classification returned invalid JSON, falling back to per-file characterization.")
	foreach(fname ${ARGN})
	  file_type("${fname}" ${ALL_CNT})
	endforeach(fname ${ARGN})
	return()
      endif(NOT _brlcad_ext_class_type_error STREQUAL "NOTFOUND" OR NOT _brlcad_ext_class_path_error STREQUAL "NOTFOUND")
      if("${_brlcad_ext_class_type}" STREQUAL "ELF")
	list(APPEND _brlcad_ext_rpath_files "${_brlcad_ext_class_path}")
      elseif("${_brlcad_ext_class_type}" STREQUAL "OTHER")
	list(APPEND _brlcad_ext_noexec_files "${_brlcad_ext_class_path}")
      else("${_brlcad_ext_class_type}" STREQUAL "ELF")
	message(WARNING "Batch plief classification returned unknown type ${_brlcad_ext_class_type}, falling back to per-file characterization.")
	foreach(fname ${ARGN})
	  file_type("${fname}" ${ALL_CNT})
	endforeach(fname ${ARGN})
	return()
      endif("${_brlcad_ext_class_type}" STREQUAL "ELF")
    endforeach(_brlcad_ext_record ${_brlcad_ext_plief_records})
  endif()

  brlcad_ext_record_file_type_list(BRLCAD_EXT_NTEXT_FILES BRLCAD_EXT_NTEXT_COUNT "text" 500 ${ALL_CNT} ${_brlcad_ext_text_files})
  brlcad_ext_record_file_type_list(BRLCAD_EXT_NNOEXEC_FILES BRLCAD_EXT_NNOEXEC_COUNT "binary" 100 ${ALL_CNT} ${_brlcad_ext_noexec_files})
  brlcad_ext_record_file_type_list(BRLCAD_EXT_NBINARY_FILES BRLCAD_EXT_NBINARY_COUNT "shared object or executable" 100 ${ALL_CNT} ${_brlcad_ext_rpath_files})
endfunction(brlcad_ext_batch_file_type)

# Copy everything in ${BRLCAD_EXT_DIR}/install into the build
# directory
function(initialize_tp_files)
  # Rather than complicate matters trying to pick and choose what to
  # move, just stage everything.  Depending on what the dependencies
  # write into their install directories we may have to be more
  # selective about this in the future, but for now let's try
  # simplicity - the less we can couple this logic to the specific
  # contents of ${BRLCAD_EXT_DIR}/install, the better.
  #
  # Unfortuately, as implemented this is currently quite slow, but
  # cmake's -E copy_directory command follows symlinks rather than
  # duplicating them:
  # https://cmake.org/cmake/help/latest/manual/cmake.1.html
  if("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR}")
  else("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
    set(EXT_DIR_STR "${BRLCAD_EXT_INSTALL_DIR} (via symlink ${BRLCAD_EXT_DIR}/install)")
  endif("${BRLCAD_EXT_DIR}/install" STREQUAL "${BRLCAD_EXT_INSTALL_DIR}")
  message("Staging third party files from ${EXT_DIR_STR} in build directory...")
  file(GLOB SDIRS LIST_DIRECTORIES true RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  foreach(sd ${SDIRS})
    # Bundle up the sub-directory's contents so that the archive will expand with
    # paths relative to the ${BRLCAD_EXT_DIR}/install root
    message("Packing ${sd} subdirectory...")
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_BINARY_DIR}/${sd}.tar "${sd}"
      WORKING_DIRECTORY "${BRLCAD_EXT_INSTALL_DIR}"
    )
    message("Packing ${sd} subdirectory... done.")

    # Make sure build directory has the target directory to write to
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${sd})

    # Whether we're single or multi config, do a top level
    # decompression to give the install targets a uniform source for
    # all configurations.
    message("Expanding ${sd}.tar in build directory...")
    if("${CMAKE_VERSION}" VERSION_LESS "3.24")
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      )
    else("${CMAKE_VERSION}" VERSION_LESS "3.24")
      # If we have it, use --touch instead of the (very slow) per file
      # -E touch update
      # https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-E_tar-touch
      execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf ${CMAKE_BINARY_DIR}/${sd}.tar --touch
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      )
    endif("${CMAKE_VERSION}" VERSION_LESS "3.24")
    message("Expanding ${sd}.tar in build directory... done.")
    # Copying complete - remove the archive file
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/${sd}.tar)
  endforeach(sd ${SDIRS})

  # The above copy is indiscriminate, so we follow behind it and strip
  # out the files we don't wish to include
  message("Removing files indicated by exclude patterns...")
  strip_excluded("${CMAKE_BINARY_DIR}" EXCLUDED_PATTERNS)
  message("Removing files indicated by exclude patterns... done.")

  # In older CMake, unpacking the files didn't come with the option to
  # update their timestamps - see
  # https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-E_tar-touch
  # If we want to be able to check if ${BRLCAD_EXT_DIR}/install has
  # changed, we need to make sure the build dir copies are newer than
  # the ${BRLCAD_EXT_DIR}/install copies for IS_NEWER_THAN testing.
  if("${CMAKE_VERSION}" VERSION_LESS "3.24")
    message("Updating copied file timestamps...")
    foreach(tf ${TP_FILES})
      execute_process(COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_BINARY_DIR}/${tf}")
    endforeach(tf ${TP_FILES})
    message("Updating copied file timestamps... done.")
  endif("${CMAKE_VERSION}" VERSION_LESS "3.24")

  message("Staging third party files from ${EXT_DIR_STR} in build directory... done.")

  # NOTE - we may need to find and redo symlinks, if we get any that
  # are full paths - we expect .so and .so.* style symlinks on some
  # platforms, and there may be others.  If those paths are absolute
  # in BRLCAD_EXT_INSTALL_DIR they will be wrong when copied into the
  # BRL-CAD build - they will work on the build machine since full
  # path links will resolve, but will fail when installed on another
  # machine.  A quick tests suggests we don't have any like that right
  # now, but it's not clear we can count on that...
endfunction(initialize_tp_files)

# If we have a pre-existing list of files, we need to determine the
# status of the current directories vs the list.  Sets three lists at
# the parent scope: TP_NEW - files in ${BRLCAD_EXT_DIR}/install that
# are new since the previous list was generated TP_STALE - files in
# the old list that are no longer in ${BRLCAD_EXT_DIR}/install
# TP_CHANGED - files present in both lists but newer in
# ${BRLCAD_EXT_DIR}/install
#
# Note that TP_NEW will be empty if there is no previous state.  The
# main logic uses a different variable - TP_INIT - to notify the
# appropriate processing steps that there are files to work on, since
# we don't want to do the copying step with configure_file when the
# initialize routines have already done the work.
function(tp_compare_state TP_NEW_LIST TP_PREV_LIST)
  # See if any new files have appeared compared to the previous state
  set(LTP_NEW "${${TP_NEW_LIST}}")
  set(LTP_PREVIOUS "${${TP_PREV_LIST}}")
  if(LTP_PREVIOUS)
    list(REMOVE_ITEM LTP_NEW ${LTP_PREVIOUS})
  else(LTP_PREVIOUS)
    # If everything is new, we're initializing
    set(LTP_NEW)
  endif(LTP_PREVIOUS)

  # See if any files previously copied into build dir were removed
  set(LTP_STALE ${LTP_PREVIOUS})
  if(${TP_NEW_LIST})
    list(REMOVE_ITEM LTP_STALE ${${TP_NEW_LIST}})
  endif(${TP_NEW_LIST})

  # We also need to see if any files are new based on timestamps.
  set(LTP_CHANGED)
  set(LTP_EXISTING ${${TP_NEW_LIST}})
  if(LTP_NEW)
    list(REMOVE_ITEM LTP_EXISTING ${LTP_NEW})
  endif(LTP_NEW)
  foreach(ef ${LTP_EXISTING})
    if(${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
      set(LTP_CHANGED ${LTP_CHANGED} ${ef})
    endif(${BRLCAD_EXT_INSTALL_DIR}/${ef} IS_NEWER_THAN ${CMAKE_BINARY_DIR}/${ef})
  endforeach(ef ${LTP_EXISTING})

  set(TP_NEW "${LTP_NEW}" PARENT_SCOPE)
  set(TP_STALE "${LTP_STALE}" PARENT_SCOPE)
  set(TP_CHANGED "${LTP_CHANGED}" PARENT_SCOPE)
endfunction(tp_compare_state)

# The relative RPATH is specific to the location and platform
function(find_relative_rpath fp rp)
  # We don't want the filename to count, so offset our directory count
  # down by 1
  set(dcnt -1)
  set(fp_cpy ${fp})
  while(NOT "${fp_cpy}" STREQUAL "")
    get_filename_component(pdir "${fp_cpy}" DIRECTORY)
    set(fp_cpy ${pdir})
    math(EXPR dcnt "${dcnt} + 1")
  endwhile(NOT "${fp_cpy}" STREQUAL "")
  if(APPLE)
    set(RELATIVE_RPATH "@executable_path")
  else(APPLE)
    set(RELATIVE_RPATH ":\\$ORIGIN")
  endif(APPLE)
  set(acnt 0)
  while(acnt LESS dcnt)
    set(RELATIVE_RPATH "${RELATIVE_RPATH}/..")
    math(EXPR acnt "${acnt} + 1")
  endwhile(acnt LESS dcnt)
  set(RELATIVE_RPATH "${RELATIVE_RPATH}/${LIB_DIR}")
  set(${rp} "${RELATIVE_RPATH}" PARENT_SCOPE)
endfunction(find_relative_rpath)

# Apply the RPATH settings to be used in the build directory.  This is
# a bit different from what is done for the final install - the goal
# here is not to produce relocatable files, but just have things work
# in place in the build locations.
function(rpath_build_dir_process ROOT_DIR lf logfile history_log updated_var)

  if(P_RPATH_EXECUTABLE)
    execute_process(
      COMMAND ${P_RPATH_EXECUTABLE} --set-rpath "${ROOT_DIR}/${LIB_DIR}" ${lf}
      WORKING_DIRECTORY ${ROOT_DIR}
      RESULT_VARIABLE _brlcad_ext_rpath_result
      ERROR_VARIABLE _brlcad_ext_rpath_error
    )
    if(NOT "${_brlcad_ext_rpath_result}" STREQUAL "0")
      message(WARNING "RPATH update failed for ${ROOT_DIR}/${lf} (${_brlcad_ext_rpath_result}): ${_brlcad_ext_rpath_error}")
      #return()
    endif(NOT "${_brlcad_ext_rpath_result}" STREQUAL "0")
  elseif(APPLE)
    execute_process(
      COMMAND install_name_tool -delete_rpath "${BRLCAD_EXT_DIR}/${BRLCAD_EXT_DIR}/install/${LIB_DIR}" ${lf}
      WORKING_DIRECTORY ${ROOT_DIR}
      OUTPUT_VARIABLE OOUT
      RESULT_VARIABLE ORESULT
      ERROR_VARIABLE OERROR
    )
    execute_process(
      COMMAND install_name_tool -add_rpath "${ROOT_DIR}/${LIB_DIR}" ${lf}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
  endif(P_RPATH_EXECUTABLE)

  # RPATH updates are complete - now clear out any other stale paths
  # in the file
  #message("${STRCLEAR_EXECUTABLE} -v -b -c ${ROOT_DIR}/${lf} \"${BRLCAD_EXT_DIR_REAL}/${LIB_DIR}\"
  #    \"${BRLCAD_EXT_DIR_REAL}/${BIN_DIR}\" \"${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR}\" \"${BRLCAD_EXT_DIR_REAL}/\"")

  execute_process(
    COMMAND
      ${STRCLEAR_EXECUTABLE} -v -p -b -c ${ROOT_DIR}/${lf} ${_brlcad_ext_binary_clear_targets}
    OUTPUT_VARIABLE _brlcad_ext_strclear_output
    ECHO_OUTPUT_VARIABLE
  )
  brlcad_ext_count_strclear_updates(_brlcad_ext_updated "${_brlcad_ext_strclear_output}")
  if(logfile)
    brlcad_ext_append_strclear_log("${logfile}" "Residual stale path cleanup by strclear: ${ROOT_DIR}/${lf}" "${_brlcad_ext_strclear_output}")
  endif(logfile)
  if(history_log)
    brlcad_ext_append_strclear_log("${history_log}" "Residual stale path cleanup by strclear: ${ROOT_DIR}/${lf}" "${_brlcad_ext_strclear_output}")
  endif(history_log)
  set(${updated_var} "${_brlcad_ext_updated}" PARENT_SCOPE)

  # Modern Apple security features (particularly on ARM64) complicate
  # our post-processing of these files with strclear.  For more info,
  # see:
  # https://developer.apple.com/documentation/security/updating_mac_software
  # https://developer.apple.com/documentation/xcode/embedding-nonstandard-code-structures-in-a-bundle
  # https://stackoverflow.com/questions/71744856/install-name-tool-errors-on-arm64
  if(APPLE)
    execute_process(
      COMMAND codesign --force -s - ${lf}
      WORKING_DIRECTORY ${ROOT_DIR}
    )
  endif(APPLE)
endfunction(rpath_build_dir_process)

# bext repository management
include(BRLCAD_EXT_Setup)

#####################################################################
# Processing for BRLCAD_EXT_INSTALL_DIR contents. We need to
# keep the build directory copies of ${BRLCAD_EXT_DIR}/install files
# in sync with the BRLCAD_EXT_DIR originals, if they change.
#####################################################################
function(brlcad_bext_process)

  if(BRLCAD_DISABLE_RELOCATION)
    return()
  endif(BRLCAD_DISABLE_RELOCATION)

  # For repeat configure passes, we need to check any existing files
  # copied against the ${BRLCAD_EXT_DIR}/install dir's contents, to
  # detect if the latter has changed and we need to redo the process.
  set(TP_INVENTORY "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty.txt")
  set(TP_INVENTORY_BINARIES "${CMAKE_BINARY_DIR}/CMakeFiles/thirdparty_binaries.txt")
  set(BRLCAD_EXT_BUILD_CACHE "${CMAKE_CURRENT_BINARY_DIR}/bext_build/CMakeCache.txt")

  # If we are using git, do some checks
  bext_sha1_checks()

  set(BEXT_REFRESH_NEEDED FALSE)
  if(
    "${BRLCAD_BUNDLED_LIBS}" STREQUAL "BUNDLED"
    AND EXISTS "${BRLCAD_EXT_BUILD_CACHE}"
  )
    file(STRINGS "${BRLCAD_EXT_BUILD_CACHE}" BEXT_EIGEN_ENABLED REGEX "^ENABLE_EIGEN:BOOL=ON$")
    file(STRINGS "${BRLCAD_EXT_BUILD_CACHE}" BEXT_OPENCV_ENABLED REGEX "^ENABLE_OPENCV:BOOL=ON$")
    file(STRINGS "${BRLCAD_EXT_BUILD_CACHE}" BEXT_ZLIB_ENABLED REGEX "^ENABLE_ZLIB:BOOL=ON$")
    if(NOT BEXT_EIGEN_ENABLED OR NOT BEXT_OPENCV_ENABLED OR NOT BEXT_ZLIB_ENABLED)
      set(BEXT_REFRESH_NEEDED TRUE)
    endif(NOT BEXT_EIGEN_ENABLED OR NOT BEXT_OPENCV_ENABLED OR NOT BEXT_ZLIB_ENABLED)
  endif()

  if(NOT EXISTS "${BRLCAD_EXT_INSTALL_DIR}" OR NOT EXISTS "${BRLCAD_EXT_NOINSTALL_DIR}" OR BEXT_REFRESH_NEEDED)
    message("Attempting to prepare our own version of the bext dependencies\n")
    if(BEXT_REFRESH_NEEDED)
      message("Refreshing bext configuration to enforce bundled Eigen, OpenCV, and Zlib.\n")
      file(REMOVE_RECURSE "${CMAKE_CURRENT_BINARY_DIR}/bext_build" "${CMAKE_CURRENT_BINARY_DIR}/bext_output")
    endif(BEXT_REFRESH_NEEDED)
    brlcad_ext_setup()
  endif()

  # If we have a bext directories in the build directory, we need to clear them for distcheck
  distclean("${CMAKE_BINARY_DIR}/bext")
  distclean("${CMAKE_BINARY_DIR}/bext_build")
  distclean("${CMAKE_BINARY_DIR}/bext_output")

  # See if we have plief available for rpath manipulation.  If it is
  # available, we will be using it to manage the RPATH settings for
  # third party exe/lib files.  If not, see if patchelf is available
  # instead.
  find_program(P_RPATH_EXECUTABLE NAMES plief HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})
  if(NOT P_RPATH_EXECUTABLE)
    find_program(P_RPATH_EXECUTABLE NAMES patchelf HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR})
  endif(NOT P_RPATH_EXECUTABLE)
  set(P_RPATH_SUPPORTS_FILE_LIST FALSE)
  set(P_RPATH_SUPPORTS_CLASSIFY FALSE)
  set(P_RPATH_SUPPORTS_CHANGE_REPORT FALSE)
  set(P_RPATH_SUPPORTS_SET_IF_NEEDED FALSE)
  set(P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND FALSE)
  if(P_RPATH_EXECUTABLE)
    execute_process(
      COMMAND ${P_RPATH_EXECUTABLE} --help
      RESULT_VARIABLE P_RPATH_HELP_RESULT
      OUTPUT_VARIABLE P_RPATH_HELP_OUTPUT
      ERROR_QUIET
    )
    if(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--files")
      set(P_RPATH_SUPPORTS_FILE_LIST TRUE)
    endif(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--files")
    if(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--classify")
      set(P_RPATH_SUPPORTS_CLASSIFY TRUE)
    endif(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--classify")
    if(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--report-rpath-changes")
      set(P_RPATH_SUPPORTS_CHANGE_REPORT TRUE)
    endif(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--report-rpath-changes")
    if(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--set-rpath-if-needed")
      set(P_RPATH_SUPPORTS_SET_IF_NEEDED TRUE)
    endif(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--set-rpath-if-needed")
    if(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--set-rpath-if-needed-prepend")
      set(P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND TRUE)
    endif(P_RPATH_HELP_RESULT EQUAL 0 AND "${P_RPATH_HELP_OUTPUT}" MATCHES "--set-rpath-if-needed-prepend")
  endif(P_RPATH_EXECUTABLE)

  # Find the tool we use to scrub EXT paths from files
  find_program(STRCLEAR_EXECUTABLE strclear HINTS ${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR} REQUIRED)
  execute_process(
    COMMAND ${STRCLEAR_EXECUTABLE} --help
    RESULT_VARIABLE STRCLEAR_HELP_RESULT
    OUTPUT_VARIABLE STRCLEAR_HELP_OUTPUT
    ERROR_QUIET
  )
  set(STRCLEAR_SUPPORTS_FILE_LIST FALSE)
  set(STRCLEAR_SUPPORTS_CLASSIFY FALSE)
  if(STRCLEAR_HELP_RESULT EQUAL 0 AND "${STRCLEAR_HELP_OUTPUT}" MATCHES "--files")
    set(STRCLEAR_SUPPORTS_FILE_LIST TRUE)
  endif(STRCLEAR_HELP_RESULT EQUAL 0 AND "${STRCLEAR_HELP_OUTPUT}" MATCHES "--files")
  if(STRCLEAR_HELP_RESULT EQUAL 0 AND "${STRCLEAR_HELP_OUTPUT}" MATCHES "--classify")
    set(STRCLEAR_SUPPORTS_CLASSIFY TRUE)
  endif(STRCLEAR_HELP_RESULT EQUAL 0 AND "${STRCLEAR_HELP_OUTPUT}" MATCHES "--classify")
  set(_brlcad_ext_strclear_verbose_arg)
  set(_brlcad_ext_strclear_echo_arg)
  if(BRLCAD_VERBOSE)
    set(_brlcad_ext_strclear_verbose_arg "-v")
    set(_brlcad_ext_strclear_echo_arg ECHO_OUTPUT_VARIABLE)
  endif(BRLCAD_VERBOSE)

  set(BRLCAD_EXT_INSTALL_POSTPROCESS_SCRIPT "${CMAKE_BINARY_DIR}/CMakeFiles/BRLCADInstallPostprocess.cmake")
  set(BRLCAD_EXT_INSTALL_POSTPROCESS_STAMP_DIR "${CMAKE_BINARY_DIR}/CMakeFiles/install-postprocess")
  file(MAKE_DIRECTORY "${BRLCAD_EXT_INSTALL_POSTPROCESS_STAMP_DIR}")
  file(WRITE "${BRLCAD_EXT_INSTALL_POSTPROCESS_SCRIPT}" [=[
function(_brlcad_postprocess_stamp_path outvar stamp_dir source file signature)
  file(MAKE_DIRECTORY "${stamp_dir}")
  string(SHA256 _brlcad_stamp_key "${source}|${file}|${signature}")
  set(${outvar} "${stamp_dir}/${_brlcad_stamp_key}.sha256" PARENT_SCOPE)
endfunction()

function(_brlcad_postprocess_needed outvar stamp_dir source file signature)
  if(NOT EXISTS "${source}")
    set(${outvar} FALSE PARENT_SCOPE)
    return()
  endif()

  file(SHA256 "${source}" _brlcad_source_hash)
  _brlcad_postprocess_stamp_path(_brlcad_stamp_file "${stamp_dir}" "${source}" "${file}" "${signature}")
  if(EXISTS "${file}" AND EXISTS "${_brlcad_stamp_file}")
    file(SHA256 "${file}" _brlcad_current_hash)
    file(READ "${_brlcad_stamp_file}" _brlcad_stamp_contents)
    string(REGEX MATCH "^([0-9a-fA-F]+)\n([0-9a-fA-F]+)" _brlcad_stamp_match "${_brlcad_stamp_contents}")
    if(_brlcad_stamp_match)
      set(_brlcad_stamped_source_hash "${CMAKE_MATCH_1}")
      set(_brlcad_stamped_clean_hash "${CMAKE_MATCH_2}")
      if("${_brlcad_source_hash}" STREQUAL "${_brlcad_stamped_source_hash}" AND "${_brlcad_current_hash}" STREQUAL "${_brlcad_stamped_clean_hash}")
        set(${outvar} FALSE PARENT_SCOPE)
        return()
      endif()
    endif()
  endif()

  set(${outvar} TRUE PARENT_SCOPE)
endfunction()

function(_brlcad_postprocess_finish stamp_dir source file signature)
  if(NOT EXISTS "${source}" OR NOT EXISTS "${file}")
    return()
  endif()

  file(SHA256 "${source}" _brlcad_source_hash)
  file(SHA256 "${file}" _brlcad_clean_hash)
  _brlcad_postprocess_stamp_path(_brlcad_stamp_file "${stamp_dir}" "${source}" "${file}" "${signature}")
  file(WRITE "${_brlcad_stamp_file}" "${_brlcad_source_hash}\n${_brlcad_clean_hash}\n")
endfunction()

function(_brlcad_install_copy source file install_type)
  get_filename_component(_brlcad_dest_dir "${file}" DIRECTORY)
  file(MAKE_DIRECTORY "${_brlcad_dest_dir}")
  file(INSTALL DESTINATION "${_brlcad_dest_dir}" TYPE ${install_type} FILES "${source}")
endfunction()

function(_brlcad_path_forms outvar)
  set(_brlcad_forms)
  foreach(_brlcad_path ${ARGN})
    if("${_brlcad_path}" STREQUAL "")
      continue()
    endif()
    list(APPEND _brlcad_forms "${_brlcad_path}")
    get_filename_component(_brlcad_abs_path "${_brlcad_path}" ABSOLUTE)
    list(APPEND _brlcad_forms "${_brlcad_abs_path}")
    file(REAL_PATH "${_brlcad_path}" _brlcad_real_path)
    list(APPEND _brlcad_forms "${_brlcad_real_path}")
  endforeach()
  if(_brlcad_forms)
    list(REMOVE_DUPLICATES _brlcad_forms)
  endif()
  set(${outvar} ${_brlcad_forms} PARENT_SCOPE)
endfunction()

function(brlcad_install_strclear_replace stamp_dir strclear source file install_type from_path to_path verbose)
  set(_brlcad_signature "strclear-replace|${strclear}|${install_type}|${from_path}|${to_path}")
  _brlcad_postprocess_needed(_brlcad_needed "${stamp_dir}" "${source}" "${file}" "${_brlcad_signature}")
  if(NOT _brlcad_needed)
    return()
  endif()

  _brlcad_install_copy("${source}" "${file}" "${install_type}")
  set(_brlcad_strclear_verbose_arg)
  if(verbose)
    set(_brlcad_strclear_verbose_arg "-v")
  endif()
  execute_process(
    COMMAND "${strclear}" ${_brlcad_strclear_verbose_arg} -p -r "${file}" "${from_path}" "${to_path}"
    RESULT_VARIABLE _brlcad_result
  )
  if(_brlcad_result EQUAL 0)
    _brlcad_postprocess_finish("${stamp_dir}" "${source}" "${file}" "${_brlcad_signature}")
  else()
    message(WARNING "Post-install path replacement failed for ${file}")
  endif()
endfunction()

function(brlcad_install_binary_postprocess stamp_dir strclear source file install_type mode rpath_tool install_rpath build_lib_path rel_rpath use_selective_rpath verbose)
  set(_brlcad_signature "binary-postprocess|${install_type}|${mode}|${rpath_tool}|${install_rpath}|${build_lib_path}|${rel_rpath}|${use_selective_rpath}|${strclear}")
  _brlcad_postprocess_needed(_brlcad_needed "${stamp_dir}" "${source}" "${file}" "${_brlcad_signature}")
  if(NOT _brlcad_needed)
    return()
  endif()

  _brlcad_install_copy("${source}" "${file}" "${install_type}")
  set(_brlcad_result 0)
  if("${mode}" STREQUAL "RPATH_TOOL")
    set(_brlcad_selective_rpath_args)
    if(use_selective_rpath)
      set(_brlcad_selective_rpath_args --set-rpath-if-needed --set-rpath-if-needed-prepend --stale-rpath-prefix "${build_lib_path}")
    endif()
    execute_process(
      COMMAND "${rpath_tool}" --set-rpath "${install_rpath}" ${_brlcad_selective_rpath_args} "${file}"
      RESULT_VARIABLE _brlcad_result
    )
  elseif("${mode}" STREQUAL "APPLE")
    execute_process(
      COMMAND install_name_tool -delete_rpath "${build_lib_path}" "${file}"
      RESULT_VARIABLE _brlcad_result
      OUTPUT_VARIABLE _brlcad_output
      ERROR_VARIABLE _brlcad_error
    )
    if(_brlcad_result EQUAL 0)
      execute_process(
        COMMAND install_name_tool -add_rpath "${rel_rpath}" "${file}"
        RESULT_VARIABLE _brlcad_result
      )
    endif()
  endif()

  if(NOT _brlcad_result EQUAL 0)
    message(WARNING "Post-install RPATH update failed for ${file}")
    return()
  endif()

  set(_brlcad_strclear_verbose_arg)
  if(verbose)
    set(_brlcad_strclear_verbose_arg "-v")
  endif()
  _brlcad_path_forms(_brlcad_build_lib_paths "${build_lib_path}")
  set(_brlcad_binary_clear_paths ${_brlcad_build_lib_paths})
  foreach(_brlcad_build_lib_path ${_brlcad_build_lib_paths})
    list(APPEND _brlcad_binary_clear_paths "${_brlcad_build_lib_path}/")
  endforeach()
  if(_brlcad_binary_clear_paths)
    list(REMOVE_DUPLICATES _brlcad_binary_clear_paths)
  endif()
  execute_process(
    COMMAND "${strclear}" ${_brlcad_strclear_verbose_arg} -p -b -c "${file}" ${_brlcad_binary_clear_paths}
    RESULT_VARIABLE _brlcad_result
  )
  if(NOT _brlcad_result EQUAL 0)
    message(WARNING "Post-install binary path cleanup failed for ${file}")
    return()
  endif()

  if("${mode}" STREQUAL "APPLE")
    execute_process(
      COMMAND codesign --force -s - "${file}"
      RESULT_VARIABLE _brlcad_result
    )
    if(NOT _brlcad_result EQUAL 0)
      message(WARNING "Post-install codesign failed for ${file}")
      return()
    endif()
  endif()

  _brlcad_postprocess_finish("${stamp_dir}" "${source}" "${file}" "${_brlcad_signature}")
endfunction()
]=])

  # If we got to ${BRLCAD_EXT_DIR}/install through a symlink, we need to
  # expand it so we can spot the path that would have been used in
  # ${BRLCAD_EXT_DIR}/install files
  # TODO - once we can require CMake 3.21 minimum, add EXPAND_TILDE to
  # the arguments list
  file(REAL_PATH "${BRLCAD_EXT_INSTALL_DIR}" BRLCAD_EXT_DIR_REAL)
  set(_brlcad_ext_install_scrub_path "${BRLCAD_EXT_INSTALL_DIR}")
  if(DEFINED BRLCAD_EXT_DIR AND EXISTS "${BRLCAD_EXT_DIR}/install")
    set(_brlcad_ext_install_scrub_path "${BRLCAD_EXT_DIR}/install")
  endif(DEFINED BRLCAD_EXT_DIR AND EXISTS "${BRLCAD_EXT_DIR}/install")
  brlcad_ext_path_forms(_brlcad_ext_install_path_forms "${_brlcad_ext_install_scrub_path}" "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_DIR_REAL}")
  brlcad_ext_path_clear_targets(_brlcad_ext_install_clear_targets "${_brlcad_ext_install_scrub_path}" "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_DIR_REAL}")
  brlcad_ext_path_clear_targets(
    _brlcad_ext_binary_clear_targets
    "${_brlcad_ext_install_scrub_path}/${LIB_DIR}"
    "${BRLCAD_EXT_INSTALL_DIR}/${LIB_DIR}"
    "${BRLCAD_EXT_DIR_REAL}/${LIB_DIR}"
    "${_brlcad_ext_install_scrub_path}/${BIN_DIR}"
    "${BRLCAD_EXT_INSTALL_DIR}/${BIN_DIR}"
    "${BRLCAD_EXT_DIR_REAL}/${BIN_DIR}"
    "${_brlcad_ext_install_scrub_path}/${INCLUDE_DIR}"
    "${BRLCAD_EXT_INSTALL_DIR}/${INCLUDE_DIR}"
    "${BRLCAD_EXT_DIR_REAL}/${INCLUDE_DIR}"
    "${_brlcad_ext_install_scrub_path}"
    "${BRLCAD_EXT_INSTALL_DIR}"
    "${BRLCAD_EXT_DIR_REAL}"
  )

  # These patterns are used to identify sets of files where we are
  # assuming we don't need to do post-processing to correct file paths
  # from the external install.
  set(NOPROCESS_PATTERNS ".*/encodings/.*" ".*/include/.*" ".*/man/.*" ".*/msgs/.*")

  # These patterns are to be excluded from ${BRLCAD_EXT_DIR}/install
  # bundling - i.e., even if present in the specified
  # ${BRLCAD_EXT_DIR}/install directory, BRL-CAD will not incorporate
  # them.  Generally speaking this is used to avoid files needed for
  # external building but counterproductive in the BRL-CAD install.
  set(
    EXCLUDED_PATTERNS
    ${LIB_DIR}/itcl4.2.3/itclConfig.sh
    ${LIB_DIR}/tclConfig.sh
    ${LIB_DIR}/tdbc1.1.5/tdbcConfig.sh
    ${LIB_DIR}/tkConfig.sh
    )


  # Ascertain the current state of ${BRLCAD_EXT_DIR}/install
  file(GLOB_RECURSE TP_FILES LIST_DIRECTORIES false RELATIVE "${BRLCAD_EXT_INSTALL_DIR}" "${BRLCAD_EXT_INSTALL_DIR}/*")
  # Filter out the files removed via STRIP_EXCLUDED
  foreach(ep ${EXCLUDED_PATTERNS})
    list(FILTER TP_FILES EXCLUDE REGEX ${ep})
  endforeach(ep ${EXCLUDED_PATTERNS})

  # For the very first pass we bulk copy the contents of the
  # BRLCAD_EXT_INSTALL_DIR tree into our own directory.  For some of the
  # external dependencies (like Tcl) library elements must be in sane relative
  # locations to binaries being executed, and leaving them in
  # BRLCAD_EXT_INSTALL_DIR won't work.  On Windows, the dlls for all the
  # dependencies will need to be located correctly relative to the bin build
  # directory.
  set(TP_INIT)
  if(NOT EXISTS "${TP_INVENTORY}")

    initialize_tp_files()

    # Special variable for when we need to know about first time
    # initialization
    set(TP_INIT "${TP_FILES}")

    # With a clean copy, there aren't any previous files to check
    set(TP_PREVIOUS)

  else(NOT EXISTS "${TP_INVENTORY}")

    # If we are repeating a configure process, we need to see what (if
    # anything) has changed.  Read in the previous list.
    file(READ "${TP_INVENTORY}" TP_P)
    string(REPLACE "\n" ";" TP_PREVIOUS "${TP_P}")

  endif(NOT EXISTS "${TP_INVENTORY}")

  # Write the current third party file list
  string(REPLACE ";" "\n" TP_W "${TP_FILES}")
  file(WRITE "${TP_INVENTORY}" "${TP_W}")

  # Make sure both lists are sorted
  list(SORT TP_FILES)
  list(SORT TP_PREVIOUS)

  # See what the delta looks like between the previous
  # ${BRLCAD_EXT_DIR}/install state (if any) and the current
  message("Comparing previous and current states...")
  tp_compare_state(TP_FILES TP_PREVIOUS)
  message("Comparing previous and current states... done.")

  # If we do have changes in a repeat configure process, we're going
  # to have to redo the find_package tests.  However, we don't want to
  # repeat them if we don't have to, so key the reset process on what
  # we find.
  set(RESET_TP FALSE CACHE BOOL "resetting flag")

  if(BRLCAD_TP_FULL_RESET)
    if(TP_NEW OR TP_CHANGED OR TP_STALE)
      # If the user has requested it, if anything has changed we do a
      # full flush and re-copy of the ${BRLCAD_EXT_DIR}/install
      # contents.  This is useful if one is changing the
      # ${BRLCAD_EXT_DIR}/install directory to a completely different
      # directory rather than incrementally updating the same
      # directory.  In the former case, timestamps aren't a reliable
      # indicator of what to update in the build tree.
      #
      # The tradeoff is a full re-initialization is usually slower,
      # since it is doing more work.  On platforms where configure is
      # slow, this can be significant.  Hence the user setting to
      # control behavior.

      # Clear old files
      foreach(ef ${TP_PREVIOUS})
        file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      endforeach(ef ${TP_PREVIOUS})

      # Redo full copy
      initialize_tp_files()

      # Reset all the find_package results
      set(RESET_TP TRUE CACHE BOOL "resetting flag")
    endif(TP_NEW OR TP_CHANGED OR TP_STALE)
  else(BRLCAD_TP_FULL_RESET)
    # Clear copies of anything found to be stale
    if(TP_STALE)
      list(LENGTH TP_STALE _brlcad_ext_stale_count)
      message("Removing stale 3rd party files in build directory...")
      foreach(ef ${TP_STALE})
        file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
      endforeach(ef ${TP_STALE})
      message("Removing stale 3rd party files in build directory... done (${_brlcad_ext_stale_count} files).")
    endif(TP_STALE)

    # Stage new files - we don't have the bulk tar mechanism going
    # after the first configure pass, so we have to do the copies
    # needed explicitly.  TP_COMPARE_STATE shouldn't populate TP_NEW
    # unless we have a previous state to compare to.
    #
    # configure_file follows symlinks rather than copying them, so we
    # can't use that for this application and have to fall back on
    # file(COPY):
    # https://gitlab.kitware.com/cmake/cmake/-/issues/14609
    if(TP_NEW)
      list(LENGTH TP_NEW _brlcad_ext_new_count)
      message("Staging new 3rd party files from ${BRLCAD_EXT_DIR}/install...")
      foreach(ef ${TP_NEW})
        file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
        get_filename_component(EF_DIR ${ef} DIRECTORY)
        get_filename_component(EF_NAME ${ef} NAME)
        file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR}/${EF_DIR})
      endforeach(ef ${TP_NEW})
      message("Staging new 3rd party files from ${BRLCAD_EXT_DIR}/install... done (${_brlcad_ext_new_count} files).")
    endif(TP_NEW)

    # Stage changed files
    if(TP_CHANGED)
      list(LENGTH TP_CHANGED _brlcad_ext_changed_count)
      message("Staging changed 3rd party files from ${BRLCAD_EXT_DIR}/install...")
      foreach(ef ${TP_CHANGED})
        file(REMOVE ${CMAKE_BINARY_DIR}/${ef})
        get_filename_component(EF_DIR ${ef} DIRECTORY)
        get_filename_component(EF_NAME ${ef} NAME)
        file(COPY ${BRLCAD_EXT_INSTALL_DIR}/${ef} DESTINATION ${CMAKE_BINARY_DIR}/${EF_DIR})
        execute_process(COMMAND ${CMAKE_COMMAND} -E touch_nocreate "${CMAKE_BINARY_DIR}/${ef}")
      endforeach(ef ${TP_CHANGED})
      message("Staging changed 3rd party files from ${BRLCAD_EXT_DIR}/install... done (${_brlcad_ext_changed_count} files).")
    endif(TP_CHANGED)

    # If the directory file lists differ, we have to reset find package
    if(TP_NEW OR TP_STALE OR TP_CHANGED)
      set(RESET_TP TRUE CACHE BOOL "resetting flag")
    endif(TP_NEW OR TP_STALE OR TP_CHANGED)
  endif(BRLCAD_TP_FULL_RESET)

  # We'll either have TP_INIT (on the first pass) or (possibly) one or
  # both of the others.  Regardless, the processing from here on out
  # is the same.
  set(TP_PROCESS ${TP_CHANGED} ${TP_NEW} ${TP_INIT})

  # We're only going to characterize new files, but even on repeat
  # configures we need to know about ALL binary files, old and new,
  # for defining the install rules.  Read the cached list, if any, and
  # scrub out any entries that are in one of the processing or
  # clean-up lists
  set(BINARY_FILES)
  set(TEXT_FILES)
  set(NOEXEC_FILES)
  if(EXISTS ${TP_INVENTORY_BINARIES})
    file(READ "${TP_INVENTORY_BINARIES}" TP_B)
    string(REPLACE "\n" ";" BINARY_FILES "${TP_B}")
    if(TP_STALE)
      list(REMOVE_ITEM BINARY_FILES ${TP_STALE})
    endif(TP_STALE)
    if(TP_NEW)
      list(REMOVE_ITEM BINARY_FILES ${TP_NEW})
    endif(TP_NEW)
    if(TP_CHANGED)
      list(REMOVE_ITEM BINARY_FILES ${TP_CHANGED})
    endif(TP_CHANGED)
  endif(EXISTS ${TP_INVENTORY_BINARIES})

  # Use various tools to sort out which files are exec/lib files,
  # targeting only the files we've determined need processing (for an
  # initialization this is everything, but for subsequent passes there
  # is likely to be much less work to do.)
  message("Characterizing new or changed bundled third party files...")
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NBINARY_FILES "")
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NTEXT_FILES "")
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NNOEXEC_FILES "")
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NBINARY_COUNT 0)
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NTEXT_COUNT 0)
  set_property(GLOBAL PROPERTY BRLCAD_EXT_NNOEXEC_COUNT 0)
  set_property(GLOBAL PROPERTY BRLCAD_EXT_PROCESSED_FILE_COUNT 0)
  list(LENGTH TP_PROCESS ALL_PCNT)
  # Batch classification only requires strclear's --classify support to split
  # text vs. binary.  The RPATH sub-classification (plief) is optional:
  #
  #   * If an RPATH tool can batch-classify (P_RPATH_SUPPORTS_CLASSIFY), the
  #     binaries are further split inside brlcad_ext_batch_file_type.
  #   * If no RPATH tool exists at all (NOT P_RPATH_EXECUTABLE, the normal
  #     Windows case - PE files have no RPATH), there is nothing to
  #     sub-classify and every binary is a non-exec binary.
  #
  # Only when an RPATH tool is present but cannot batch-classify (e.g. a
  # patchelf without --classify) do we still need the per-file path, so that
  # the tool is invoked per binary to set RPATH correctly.  This lets Windows
  # avoid the per-file execute_process() storm, which is the dominant
  # configure cost, without regressing that edge case.
  if(STRCLEAR_SUPPORTS_CLASSIFY AND NOT APPLE AND (P_RPATH_SUPPORTS_CLASSIFY OR NOT P_RPATH_EXECUTABLE))
    brlcad_ext_batch_file_type(${ALL_PCNT} ${TP_PROCESS})
  else()
    foreach(lf ${TP_PROCESS})
      file_type("${lf}" ${ALL_PCNT})
    endforeach(lf ${TP_PROCESS})
  endif()
  get_property(NBINARY_FILES GLOBAL PROPERTY BRLCAD_EXT_NBINARY_FILES)
  get_property(NTEXT_FILES GLOBAL PROPERTY BRLCAD_EXT_NTEXT_FILES)
  get_property(NNOEXEC_FILES GLOBAL PROPERTY BRLCAD_EXT_NNOEXEC_FILES)
  message("Characterizing new or changed bundled third party files... done.")

  # Combine the previous lists and the new determinations, writing the
  # final lists back out to files
  set(ALL_BINARY_FILES ${BINARY_FILES} ${NBINARY_FILES})
  string(REPLACE ";" "\n" TP_B "${ALL_BINARY_FILES}")
  file(WRITE "${TP_INVENTORY_BINARIES}" "${TP_B}")

  set(_brlcad_ext_strclear_log "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_strclear_updates.log")
  set(_brlcad_ext_strclear_history_log "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_strclear_updates_history.log")
  if(NBINARY_FILES OR NNOEXEC_FILES OR NTEXT_FILES)
    string(TIMESTAMP _brlcad_ext_strclear_log_time "%Y-%m-%d %H:%M:%S %z")
    file(WRITE "${_brlcad_ext_strclear_log}" "BRL-CAD bundled third party path update log for latest configure pass\n")
    file(APPEND "${_brlcad_ext_strclear_log}" "Timestamp: ${_brlcad_ext_strclear_log_time}\n")
    file(APPEND "${_brlcad_ext_strclear_log}" "Build directory: ${CMAKE_BINARY_DIR}\n")
    file(APPEND "${_brlcad_ext_strclear_log}" "External install directory: ${BRLCAD_EXT_DIR_REAL}\n")

    file(APPEND "${_brlcad_ext_strclear_history_log}" "\n########################################################################\n")
    file(APPEND "${_brlcad_ext_strclear_history_log}" "BRL-CAD bundled third party path update log entry\n")
    file(APPEND "${_brlcad_ext_strclear_history_log}" "Timestamp: ${_brlcad_ext_strclear_log_time}\n")
    file(APPEND "${_brlcad_ext_strclear_history_log}" "Build directory: ${CMAKE_BINARY_DIR}\n")
    file(APPEND "${_brlcad_ext_strclear_history_log}" "External install directory: ${BRLCAD_EXT_DIR_REAL}\n")
  endif(NBINARY_FILES OR NNOEXEC_FILES OR NTEXT_FILES)

  if(NBINARY_FILES)
    set(_brlcad_ext_plief_updated 0)
    set(_brlcad_ext_binary_updated 0)
    list(LENGTH NBINARY_FILES _brlcad_ext_binary_processed)
    message("Setting rpath on new 3rd party lib and exe files...")
    if(P_RPATH_SUPPORTS_FILE_LIST AND STRCLEAR_SUPPORTS_FILE_LIST AND NOT APPLE)
      set(_brlcad_ext_binary_files)
      foreach(lf ${NBINARY_FILES})
        list(APPEND _brlcad_ext_binary_files "${CMAKE_BINARY_DIR}/${lf}")
      endforeach(lf ${NBINARY_FILES})
      if(_brlcad_ext_binary_files)
        set(_brlcad_ext_binary_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_binary_postprocess.txt")
        brlcad_ext_write_file_list("${_brlcad_ext_binary_list}" ${_brlcad_ext_binary_files})
        brlcad_ext_append_file_list_log("${_brlcad_ext_strclear_log}" "RPATH tool input files" ${_brlcad_ext_binary_files})
        brlcad_ext_append_file_list_log("${_brlcad_ext_strclear_history_log}" "RPATH tool input files" ${_brlcad_ext_binary_files})
        if(P_RPATH_SUPPORTS_CHANGE_REPORT)
          set(_brlcad_ext_rpath_set_if_needed_args)
          if(P_RPATH_SUPPORTS_SET_IF_NEEDED AND P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND)
            set(_brlcad_ext_rpath_set_if_needed_args --set-rpath-if-needed --set-rpath-if-needed-prepend --stale-rpath-prefix "${_brlcad_ext_install_scrub_path}")
          endif(P_RPATH_SUPPORTS_SET_IF_NEEDED AND P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND)
          execute_process(
            COMMAND ${P_RPATH_EXECUTABLE} --set-rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" ${_brlcad_ext_rpath_set_if_needed_args} --report-rpath-changes --files "${_brlcad_ext_binary_list}"
            RESULT_VARIABLE _brlcad_ext_set_rpath_result
            OUTPUT_VARIABLE _brlcad_ext_plief_report
            ERROR_VARIABLE _brlcad_ext_set_rpath_error
          )
          if(_brlcad_ext_set_rpath_result EQUAL 0)
            brlcad_ext_append_plief_report_log(
              _brlcad_ext_plief_updated
              "${_brlcad_ext_strclear_log}"
              "RPATH changes made by plief"
              "${_brlcad_ext_plief_report}"
            )
            brlcad_ext_append_plief_report_log(
              _brlcad_ext_plief_updated
              "${_brlcad_ext_strclear_history_log}"
              "RPATH changes made by plief"
              "${_brlcad_ext_plief_report}"
            )
          else(_brlcad_ext_set_rpath_result EQUAL 0)
            message(WARNING "Batch RPATH update failed: ${_brlcad_ext_set_rpath_error}")
          endif(_brlcad_ext_set_rpath_result EQUAL 0)
        else(P_RPATH_SUPPORTS_CHANGE_REPORT)
          string(SHA256 _brlcad_ext_rpath_log_key "${CMAKE_BINARY_DIR}|${_brlcad_ext_strclear_log_time}|${_brlcad_ext_binary_list}")
          set(_brlcad_ext_rpath_before_prefix "PLIEF_BEFORE_${_brlcad_ext_rpath_log_key}")
          set(_brlcad_ext_rpath_after_prefix "PLIEF_AFTER_${_brlcad_ext_rpath_log_key}")
          execute_process(
            COMMAND ${P_RPATH_EXECUTABLE} --print-rpath --files "${_brlcad_ext_binary_list}"
            RESULT_VARIABLE _brlcad_ext_rpath_before_result
            OUTPUT_VARIABLE _brlcad_ext_rpath_before_output
            ERROR_VARIABLE _brlcad_ext_rpath_before_error
          )
          if(_brlcad_ext_rpath_before_result EQUAL 0)
            brlcad_ext_rpath_output_to_properties("${_brlcad_ext_rpath_before_prefix}" "${_brlcad_ext_rpath_before_output}")
          else(_brlcad_ext_rpath_before_result EQUAL 0)
            message(WARNING "Unable to inspect pre-update RPATH values: ${_brlcad_ext_rpath_before_error}")
          endif(_brlcad_ext_rpath_before_result EQUAL 0)

          execute_process(
            COMMAND ${P_RPATH_EXECUTABLE} --set-rpath "${CMAKE_BINARY_DIR}/${LIB_DIR}" --files "${_brlcad_ext_binary_list}"
            RESULT_VARIABLE _brlcad_ext_set_rpath_result
            ERROR_VARIABLE _brlcad_ext_set_rpath_error
          )
          if(NOT _brlcad_ext_set_rpath_result EQUAL 0)
            message(WARNING "Batch RPATH update failed: ${_brlcad_ext_set_rpath_error}")
          endif(NOT _brlcad_ext_set_rpath_result EQUAL 0)
          execute_process(
            COMMAND ${P_RPATH_EXECUTABLE} --print-rpath --files "${_brlcad_ext_binary_list}"
            RESULT_VARIABLE _brlcad_ext_rpath_after_result
            OUTPUT_VARIABLE _brlcad_ext_rpath_after_output
            ERROR_VARIABLE _brlcad_ext_rpath_after_error
          )
          if(_brlcad_ext_rpath_after_result EQUAL 0)
            brlcad_ext_rpath_output_to_properties("${_brlcad_ext_rpath_after_prefix}" "${_brlcad_ext_rpath_after_output}")
            if(_brlcad_ext_rpath_before_result EQUAL 0)
              brlcad_ext_log_plief_rpath_changes(
                _brlcad_ext_plief_updated
                "${_brlcad_ext_strclear_log}"
                "RPATH changes made by plief"
                "${_brlcad_ext_rpath_before_prefix}"
                "${_brlcad_ext_rpath_after_prefix}"
                ${_brlcad_ext_binary_files}
              )
              brlcad_ext_log_plief_rpath_changes(
                _brlcad_ext_plief_updated
                "${_brlcad_ext_strclear_history_log}"
                "RPATH changes made by plief"
                "${_brlcad_ext_rpath_before_prefix}"
                "${_brlcad_ext_rpath_after_prefix}"
                ${_brlcad_ext_binary_files}
              )
            endif(_brlcad_ext_rpath_before_result EQUAL 0)
          else(_brlcad_ext_rpath_after_result EQUAL 0)
            message(WARNING "Unable to inspect post-update RPATH values: ${_brlcad_ext_rpath_after_error}")
          endif(_brlcad_ext_rpath_after_result EQUAL 0)
        endif(P_RPATH_SUPPORTS_CHANGE_REPORT)
        message("Clearing residual stale path strings from 3rd party binaries with strclear...")
        execute_process(
          COMMAND
            ${STRCLEAR_EXECUTABLE} -v -p -b --files "${_brlcad_ext_binary_list}" ${_brlcad_ext_binary_clear_targets}
          OUTPUT_VARIABLE _brlcad_ext_binary_strclear_output
          ECHO_OUTPUT_VARIABLE
        )
        brlcad_ext_count_strclear_updates(_brlcad_ext_binary_count "${_brlcad_ext_binary_strclear_output}")
        math(EXPR _brlcad_ext_binary_updated "${_brlcad_ext_binary_updated} + ${_brlcad_ext_binary_count}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Residual stale path cleanup by strclear for shared objects and executables" "${_brlcad_ext_binary_strclear_output}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Residual stale path cleanup by strclear for shared objects and executables" "${_brlcad_ext_binary_strclear_output}")
      endif(_brlcad_ext_binary_files)
      list(LENGTH _brlcad_ext_binary_files _brlcad_ext_binary_processed)
    else(P_RPATH_SUPPORTS_FILE_LIST AND STRCLEAR_SUPPORTS_FILE_LIST AND NOT APPLE)
      # Set local RPATH so the files will work during build
      message("Clearing residual stale path strings from 3rd party binaries with strclear...")
      foreach(lf ${NBINARY_FILES})
        brlcad_ext_append_file_list_log("${_brlcad_ext_strclear_log}" "RPATH tool input files" "${CMAKE_BINARY_DIR}/${lf}")
        brlcad_ext_append_file_list_log("${_brlcad_ext_strclear_history_log}" "RPATH tool input files" "${CMAKE_BINARY_DIR}/${lf}")
        rpath_build_dir_process("${CMAKE_BINARY_DIR}" "${lf}" "${_brlcad_ext_strclear_log}" "${_brlcad_ext_strclear_history_log}" _brlcad_ext_binary_count)
        math(EXPR _brlcad_ext_binary_updated "${_brlcad_ext_binary_updated} + ${_brlcad_ext_binary_count}")
      endforeach(lf ${NBINARY_FILES})
    endif(P_RPATH_SUPPORTS_FILE_LIST AND STRCLEAR_SUPPORTS_FILE_LIST AND NOT APPLE)
    message("Setting rpath on new 3rd party lib and exe files... done (${_brlcad_ext_binary_processed} files checked, ${_brlcad_ext_plief_updated} files updated by plief, ${_brlcad_ext_binary_updated} files updated by strclear).")
  endif(NBINARY_FILES)

  if(NNOEXEC_FILES)
    set(_brlcad_ext_noexec_updated 0)
    message("Scrubbing paths from new 3rd party data files...")
    if(STRCLEAR_SUPPORTS_FILE_LIST)
      set(_brlcad_ext_noexec_files)
      foreach(tf ${NNOEXEC_FILES})
        skip_processing(${tf} SKIP_FILE)
        if(SKIP_FILE)
          continue()
        endif(SKIP_FILE)
        list(APPEND _brlcad_ext_noexec_files "${CMAKE_BINARY_DIR}/${tf}")
      endforeach(tf ${NNOEXEC_FILES})
      if(_brlcad_ext_noexec_files)
        set(_brlcad_ext_noexec_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_noexec_strclear.txt")
        brlcad_ext_write_file_list("${_brlcad_ext_noexec_list}" ${_brlcad_ext_noexec_files})
        execute_process(
          COMMAND ${STRCLEAR_EXECUTABLE} -v -p --binary-only --files "${_brlcad_ext_noexec_list}" ${_brlcad_ext_install_clear_targets}
          OUTPUT_VARIABLE _brlcad_ext_noexec_strclear_output
          ${_brlcad_ext_strclear_echo_arg}
        )
        brlcad_ext_count_strclear_updates(_brlcad_ext_noexec_count "${_brlcad_ext_noexec_strclear_output}")
        math(EXPR _brlcad_ext_noexec_updated "${_brlcad_ext_noexec_updated} + ${_brlcad_ext_noexec_count}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Data file path scrubbing" "${_brlcad_ext_noexec_strclear_output}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Data file path scrubbing" "${_brlcad_ext_noexec_strclear_output}")
      endif(_brlcad_ext_noexec_files)
    else(STRCLEAR_SUPPORTS_FILE_LIST)
      foreach(tf ${NNOEXEC_FILES})
        skip_processing(${tf} SKIP_FILE)
        if(SKIP_FILE)
          continue()
        endif(SKIP_FILE)

        # Replace any stale paths in the files
        #message("${STRCLEAR_EXECUTABLE} -v -b -c ${CMAKE_BINARY_DIR}/${tf} ${BRLCAD_EXT_DIR_REAL}")
        execute_process(
          COMMAND ${STRCLEAR_EXECUTABLE} -v -p -b -c "${CMAKE_BINARY_DIR}/${tf}" ${_brlcad_ext_install_clear_targets}
          OUTPUT_VARIABLE _brlcad_ext_noexec_strclear_output
          ${_brlcad_ext_strclear_echo_arg}
        )
        brlcad_ext_count_strclear_updates(_brlcad_ext_noexec_count "${_brlcad_ext_noexec_strclear_output}")
        math(EXPR _brlcad_ext_noexec_updated "${_brlcad_ext_noexec_updated} + ${_brlcad_ext_noexec_count}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Data file path scrubbing: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_noexec_strclear_output}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Data file path scrubbing: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_noexec_strclear_output}")
      endforeach(tf ${NNOEXEC_FILES})
    endif(STRCLEAR_SUPPORTS_FILE_LIST)
    message("Scrubbing paths from new 3rd party data files... done (${_brlcad_ext_noexec_updated} files updated).")
  endif(NNOEXEC_FILES)

  if(NTEXT_FILES)
    set(_brlcad_ext_text_updated 0)
    message("Replacing paths in new 3rd party text files...")
    if(STRCLEAR_SUPPORTS_FILE_LIST)
      set(_brlcad_ext_cmake_text_files)
      set(_brlcad_ext_install_text_files)
      foreach(tf ${NTEXT_FILES})
        skip_processing(${tf} SKIP_FILE)
        if(SKIP_FILE)
          continue()
        endif(SKIP_FILE)

        # Test if this is a CMake file used for find_package.  If it is, we need
        # the paths in these files to reflect the build directory hierarchy
        # during build, and the final install location after installed.
        # (Otherwise, find_package will fail when trying to use the modern
        # Config.cmake approach to package setup.)  Accordingly, they need both
        # processing for their build dir copy and an install rule to finalize
        # their paths once installed.
        #
        # As normally structured, it appears the standard Config.cmake files will
        # avoid using absolute paths.  However, it is possible for projects to
        # customize these files, so we can't guarantee they WON'T use them... and
        # the pkgconfig .pc files do typically use full paths.
        is_cmake_file(${tf} CMAKE_FILE)
        if(CMAKE_FILE)
          list(APPEND _brlcad_ext_cmake_text_files "${CMAKE_BINARY_DIR}/${tf}")
        else(CMAKE_FILE)
          list(APPEND _brlcad_ext_install_text_files "${CMAKE_BINARY_DIR}/${tf}")
        endif(CMAKE_FILE)
      endforeach(tf ${NTEXT_FILES})

      if(_brlcad_ext_cmake_text_files)
        set(_brlcad_ext_cmake_text_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_cmake_text_strclear.txt")
        brlcad_ext_write_file_list("${_brlcad_ext_cmake_text_list}" ${_brlcad_ext_cmake_text_files})
        execute_process(
          COMMAND ${STRCLEAR_EXECUTABLE} -v -p --files "${_brlcad_ext_cmake_text_list}" "${_brlcad_ext_install_scrub_path}" "${CMAKE_BINARY_DIR}"
          OUTPUT_VARIABLE _brlcad_ext_text_strclear_output
          ${_brlcad_ext_strclear_echo_arg}
        )
        brlcad_ext_count_strclear_updates(_brlcad_ext_text_count "${_brlcad_ext_text_strclear_output}")
        math(EXPR _brlcad_ext_text_updated "${_brlcad_ext_text_updated} + ${_brlcad_ext_text_count}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "CMake/package text path replacement for build-tree paths" "${_brlcad_ext_text_strclear_output}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "CMake/package text path replacement for build-tree paths" "${_brlcad_ext_text_strclear_output}")
      endif(_brlcad_ext_cmake_text_files)

      if(_brlcad_ext_install_text_files)
        set(_brlcad_ext_install_text_list "${CMAKE_BINARY_DIR}/CMakeFiles/brlcad_ext_install_text_strclear.txt")
        brlcad_ext_write_file_list("${_brlcad_ext_install_text_list}" ${_brlcad_ext_install_text_files})
        execute_process(
          COMMAND ${STRCLEAR_EXECUTABLE} -v -p --files "${_brlcad_ext_install_text_list}" "${_brlcad_ext_install_scrub_path}" "${CMAKE_INSTALL_PREFIX}"
          OUTPUT_VARIABLE _brlcad_ext_text_strclear_output
          ${_brlcad_ext_strclear_echo_arg}
        )
        brlcad_ext_count_strclear_updates(_brlcad_ext_text_count "${_brlcad_ext_text_strclear_output}")
        math(EXPR _brlcad_ext_text_updated "${_brlcad_ext_text_updated} + ${_brlcad_ext_text_count}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Text path replacement for install-prefix paths" "${_brlcad_ext_text_strclear_output}")
        brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Text path replacement for install-prefix paths" "${_brlcad_ext_text_strclear_output}")
      endif(_brlcad_ext_install_text_files)
    else(STRCLEAR_SUPPORTS_FILE_LIST)
      foreach(tf ${NTEXT_FILES})
        skip_processing(${tf} SKIP_FILE)
        if(SKIP_FILE)
          continue()
        endif(SKIP_FILE)

        # Test if this is a CMake file used for find_package.  If it is, we need
        # the paths in these files to reflect the build directory hierarchy
        # during build, and the final install location after installed.
        # (Otherwise, find_package will fail when trying to use the modern
        # Config.cmake approach to package setup.)  Accordingly, they need both
        # processing for their build dir copy and an install rule to finalize
        # their paths once installed.
        #
        # As normally structured, it appears the standard Config.cmake files will
        # avoid using absolute paths.  However, it is possible for projects to
        # customize these files, so we can't guarantee they WON'T use them... and
        # the pkgconfig .pc files do typically use full paths.
        is_cmake_file(${tf} CMAKE_FILE)
        if(CMAKE_FILE)
	  #message("${STRCLEAR_EXECUTABLE} -v -r \"${CMAKE_BINARY_DIR}/${tf}\" \"${BRLCAD_EXT_DIR_REAL}\" \"${CMAKE_BINARY_DIR}\"")
	  execute_process(
	    COMMAND
	    ${STRCLEAR_EXECUTABLE} -v -p -r "${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_install_scrub_path}" "${CMAKE_BINARY_DIR}"
            OUTPUT_VARIABLE _brlcad_ext_text_strclear_output
            ${_brlcad_ext_strclear_echo_arg}
	    )
          brlcad_ext_count_strclear_updates(_brlcad_ext_text_count "${_brlcad_ext_text_strclear_output}")
          math(EXPR _brlcad_ext_text_updated "${_brlcad_ext_text_updated} + ${_brlcad_ext_text_count}")
          brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Text path replacement for build-tree paths: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_text_strclear_output}")
          brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Text path replacement for build-tree paths: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_text_strclear_output}")
        else(CMAKE_FILE)
	  #message("${STRCLEAR_EXECUTABLE} -v -r \"${CMAKE_BINARY_DIR}/${tf}\" \"${BRLCAD_EXT_DIR_REAL}\" \"${CMAKE_INSTALL_PREFIX}\"")
	  execute_process(
	    COMMAND
	    ${STRCLEAR_EXECUTABLE} -v -p -r "${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_install_scrub_path}" "${CMAKE_INSTALL_PREFIX}"
            OUTPUT_VARIABLE _brlcad_ext_text_strclear_output
            ${_brlcad_ext_strclear_echo_arg}
	    )
          brlcad_ext_count_strclear_updates(_brlcad_ext_text_count "${_brlcad_ext_text_strclear_output}")
          math(EXPR _brlcad_ext_text_updated "${_brlcad_ext_text_updated} + ${_brlcad_ext_text_count}")
          brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_log}" "Text path replacement for install-prefix paths: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_text_strclear_output}")
          brlcad_ext_append_strclear_log("${_brlcad_ext_strclear_history_log}" "Text path replacement for install-prefix paths: ${CMAKE_BINARY_DIR}/${tf}" "${_brlcad_ext_text_strclear_output}")
        endif(CMAKE_FILE)
      endforeach(tf ${NTEXT_FILES})
    endif(STRCLEAR_SUPPORTS_FILE_LIST)
    message("Replacing paths in new 3rd party text files... done (${_brlcad_ext_text_updated} files updated).")
  endif(NTEXT_FILES)

  if(NBINARY_FILES OR NNOEXEC_FILES OR NTEXT_FILES)
    message("Detailed 3rd party path update log for this configure pass: ${_brlcad_ext_strclear_log}")
    message("Cumulative 3rd party path update history log: ${_brlcad_ext_strclear_history_log}")
  endif(NBINARY_FILES OR NNOEXEC_FILES OR NTEXT_FILES)

  # Tell the build cleanup about all the copied-in files - otherwise
  # it won't the distcheck cleaning logic won't know to scrub them.
  distclean("${TP_FILES}")

  # Everything until now has been setting the stage in the build
  # directory. Now we set up the install rules.  It is for these
  # stages that we need complete knowledge of the third party files,
  # since configure re-defines all of these rules on every pass.
  set(_brlcad_ext_cmake_install_rules 0)
  foreach(tf ${TP_FILES})
    # Rather than doing the PROGRAMS install for all binary files, we
    # target just those in the bin directory - those are the ones we
    # would expect to want CMake's *_EXECUTE permissions during
    # install.
    get_filename_component(dir "${tf}" DIRECTORY)
    if(NOT dir)
      message("Error - unexpected toplevel ext file: ${tf} ")
      continue()
    endif(NOT dir)

    # If we know it's a binary file, install it through the guarded
    # postprocess helper.  The helper owns the copy step because the
    # postprocessed install result intentionally differs from the raw
    # build-tree source.
    if("${tf}" IN_LIST ALL_BINARY_FILES)
      set(REL_RPATH)
      find_relative_rpath("${tf}" REL_RPATH)
      set(_brlcad_install_postprocess_mode "STRCLEAR_ONLY")
      set(_brlcad_install_postprocess_tool "")
      set(_brlcad_install_postprocess_rpath "")
      if(P_RPATH_EXECUTABLE)
        set(_brlcad_install_postprocess_mode "RPATH_TOOL")
        set(_brlcad_install_postprocess_tool "${P_RPATH_EXECUTABLE}")
        set(_brlcad_install_postprocess_rpath "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${REL_RPATH}")
      elseif(APPLE)
        set(_brlcad_install_postprocess_mode "APPLE")
      endif(P_RPATH_EXECUTABLE)
      set(_brlcad_install_use_selective_rpath FALSE)
      if(P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND)
        set(_brlcad_install_use_selective_rpath TRUE)
      endif(P_RPATH_SUPPORTS_SET_IF_NEEDED_PREPEND)
      install(
        CODE
          "include(\"${BRLCAD_EXT_INSTALL_POSTPROCESS_SCRIPT}\")\nbrlcad_install_binary_postprocess(\"${BRLCAD_EXT_INSTALL_POSTPROCESS_STAMP_DIR}\" \"${STRCLEAR_EXECUTABLE}\" \"${CMAKE_BINARY_DIR}/${tf}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${tf}\" \"PROGRAM\" \"${_brlcad_install_postprocess_mode}\" \"${_brlcad_install_postprocess_tool}\" \"${_brlcad_install_postprocess_rpath}\" \"${CMAKE_BINARY_DIR}/${LIB_DIR}\" \"${REL_RPATH}\" \"${_brlcad_install_use_selective_rpath}\" \"${BRLCAD_VERBOSE}\")"
      )
      continue()
    endif("${tf}" IN_LIST ALL_BINARY_FILES)

    # BIN_DIR may contain scripts that aren't explicitly binary files
    # - catch those based on path
    if(${dir} MATCHES "${BIN_DIR}$")
      install(PROGRAMS "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
    else(${dir} MATCHES "${BIN_DIR}$")
      is_cmake_file(${tf} CMAKE_FILE)
      if(CMAKE_FILE)
        math(EXPR _brlcad_ext_cmake_install_rules "${_brlcad_ext_cmake_install_rules} + 1")
        install(
          CODE
          "include(\"${BRLCAD_EXT_INSTALL_POSTPROCESS_SCRIPT}\")\nbrlcad_install_strclear_replace(\"${BRLCAD_EXT_INSTALL_POSTPROCESS_STAMP_DIR}\" \"${STRCLEAR_EXECUTABLE}\" \"${CMAKE_BINARY_DIR}/${tf}\" \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${tf}\" \"FILE\" \"${CMAKE_BINARY_DIR}\" \"\${CMAKE_INSTALL_PREFIX}\" \"${BRLCAD_VERBOSE}\")"
          )
      else(CMAKE_FILE)
        install(FILES "${CMAKE_BINARY_DIR}/${tf}" DESTINATION "${dir}")
      endif(CMAKE_FILE)
    endif(${dir} MATCHES "${BIN_DIR}$")
  endforeach(tf ${TP_FILES})
  if(_brlcad_ext_cmake_install_rules)
    message("Adding install rules for ${_brlcad_ext_cmake_install_rules} CMake find_package files.")
  endif(_brlcad_ext_cmake_install_rules)

  # Because ${BRLCAD_EXT_DIR}/install is handled at configure time
  # (and indeed MUST be handled at configure time so find_package
  # results will be correct) we make the CMake process depend on the
  # ${BRLCAD_EXT_DIR}/install files
  foreach(ef ${TP_FILES})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BRLCAD_EXT_INSTALL_DIR}/${ef})
  endforeach(ef ${TP_FILES})

  # Add a extnoinstall touched file to also trigger CMake as above, to
  # help ensure a reconfigure whenever the brlcad_externals repository
  # is built.  There should be a build-stamp file there that should be
  # updated after each build run in brlcad_externals, regardless of
  # what happens with other files.
  file(
    GLOB_RECURSE TP_NOINST_FILES
    LIST_DIRECTORIES false
    RELATIVE "${BRLCAD_EXT_NOINSTALL_DIR}"
    "${BRLCAD_EXT_NOINSTALL_DIR}/*"
  )
  # For consistency, ignore files that would fall into the
  # STRIP_EXCLUDED set
  foreach(ep ${EXCLUDED_PATTERNS})
    list(FILTER TP_NOINST_FILES EXCLUDE REGEX ${ep})
  endforeach(ep ${EXCLUDED_PATTERNS})
  foreach(ef ${TP_NOINST_FILES})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${BRLCAD_EXT_NOINSTALL_DIR}/${ef})
  endforeach(ef ${TP_NOINST_FILES})
endfunction(brlcad_bext_process)

#####################################################################
# We want find_package calls that re-run every time configure is run,
# which means we need to unset cache variables.  Most of the packages
# use the brlcad_find_package wrapper for this, but in a few cases
# it's more complicated.
#####################################################################

# Not all packages will define all of these, but it shouldn't matter -
# an unset of an unused variable shouldn't be harmful
function(find_package_reset pname trigger_var)
  unset(${pname}_DIR CACHE)
  unset(${pname}_CONFIG CACHE)
  unset(${pname}_DEFINITIONS CACHE)
  unset(${pname}_FOUND CACHE)
  unset(${pname}_INCLUDE_DIR CACHE)
  unset(${pname}_INCLUDE_DIRS CACHE)
  unset(${pname}_LIBRARIES CACHE)
  unset(${pname}_LIBRARY CACHE)
  unset(${pname}_LIBRARY_DEBUG CACHE)
  unset(${pname}_LIBRARY_RELEASE CACHE)
  unset(${pname}_VERSION_STRING CACHE)
  unset(${pname}_PREFIX_STR CACHE)
endfunction(find_package_reset pname trigger_var)

# OpenGL can get complicated.  Define a macro to centralize the "right" way to
# hunt for this.
#
# Will set an OPENGL_TARGETS variable, which is what should be used linking
# OpenGL in target_link_libraries lines.
function(find_package_opengl)
  cmake_parse_arguments(O "REQUIRED" "" "" ${ARGN})

  # Initialize to empty
  set(OPENGL_TARGETS "" PARENT_SCOPE)

  # If we're X11, we don't want the OSX framework
  set(_TMP_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK})

  # Core component is always required
  set(OPENGL_COMPONENTS OpenGL)

  # Check if we are intentionally targeting X11/GLX
  set(USING_X11 FALSE)
  if(APPLE AND NOT BRLCAD_ENABLE_AQUA)
    set(USING_X11 TRUE)
  elseif(WIN32 AND BRLCAD_ENABLE_X11)
    set(USING_X11 TRUE)
  elseif(UNIX AND NOT APPLE)
    # Linux/BSD default to X11 unless wayland-only (handled by GLX component)
    set(USING_X11 TRUE)
  endif()

  if(USING_X11)
    # If we're trying X11 on Mac, it currently (07/2026) doesn't work reliably
    # - most Xquartz packages aren't built for M chips and will now crash at
    # runtime due to Rosetta being removed from OSX.  Rather than set us up
    # for crashing, don't try the X11+OpenGL detection there.
    if (APPLE)
      return()
    endif()

    # Append GLX for X11 environments
    list(APPEND OPENGL_COMPONENTS GLX)

    # Windows might need some help.
    if(WIN32)
      # MSYS2/Cygwin X11 paths if building in those environments
      list(APPEND CMAKE_PREFIX_PATH "/usr/X11R6" "/usr/include/X11")
    endif()
  endif()

  # OpenGL find_package execution
  if(O_REQUIRED)
    find_package(OpenGL REQUIRED COMPONENTS ${OPENGL_COMPONENTS})
  else()
    find_package(OpenGL COMPONENTS ${OPENGL_COMPONENTS})
  endif()

  # find_package done - restoring framework setting
  set(CMAKE_FIND_FRAMEWORK ${_TMP_FIND_FRAMEWORK})

  # Use the portable legacy-GL abstraction target when it exists.  Our
  # FindOpenGL.cmake maps this to either libGL directly or the GLVND
  # OpenGL+GLX pair, which is what the desktop OpenGL code wants.
  set(OPENGL_TARGETS)
  if(TARGET OpenGL::GL)
    list(APPEND OPENGL_TARGETS OpenGL::GL)
  elseif(TARGET OpenGL::OpenGL)
    list(APPEND OPENGL_TARGETS OpenGL::OpenGL)
  endif()
  if(USING_X11 AND TARGET OpenGL::GLX AND NOT TARGET OpenGL::GL)
    list(APPEND OPENGL_TARGETS OpenGL::GLX)
  endif()

  # All done - result to parent scope
  set(OPENGL_TARGETS "${OPENGL_TARGETS}" PARENT_SCOPE)

  if(OPENGL_TARGETS)

    brlcad_deferred_define("BRLCAD_OPENGL 1")

    # Check for headers
    include(CheckIncludeFiles)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${C_STANDARD_FLAGS}")
    check_include_files(GL/gl.h HAVE_GL_GL_H)
    if (HAVE_GL_GL_H)
      brlcad_deferred_define("HAVE_GL_GL_H 1")
    endif()
    check_include_files(gl/device.h HAVE_GL_DEVICE_H)
    if (HAVE_GL_DEVICE_H)
      brlcad_deferred_define("HAVE_GL_DEVICE_H 1")
    endif()
    check_include_files(gl/glext.h HAVE_GL_GLEXT_H)
    if (HAVE_GL_GLEXT_H)
      brlcad_deferred_define("HAVE_GL_GLEXT_H 1")
    endif()
    check_include_files(gl/wglext.h HAVE_GL_WGLEXT_H)
    if (HAVE_GL_WGLEXT_H)
      brlcad_deferred_define("HAVE_GL_WGLEXT_H 1")
    endif()
    cmake_pop_check_state()

    if(USING_X11 AND TARGET OpenGL::GLX)
      brlcad_deferred_define("HAVE_GL_GLX_H 1")
    endif()

    if(OPENGL_USING_FRAMEWORK)
      brlcad_deferred_define("HAVE_OPENGL_GL_H 1")
    endif()

  endif()

endfunction()

# zlib compression/decompression library
# https://zlib.net
#
# Note - our copy is modified from Vanilla upstream to support
# specifying a custom prefix - until a similar feature is available in
# upstream zlib, we need this to reliably avoid conflicts between
# bundled and system zlib.
macro(find_package_zlib)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(ZLIB RESET_TP)
  unset(Z_PREFIX CACHE)
  unset(Z_PREFIX_STR CACHE)
  unset(ZLIB_STATUS CACHE)
  set(ZLIB_ROOT "${CMAKE_BINARY_DIR}")
  if(NOT BRLCAD_COMPONENTS OR F_REQUIRED)
    find_package(ZLIB REQUIRED)
  else()
    find_package(ZLIB)
  endif()
  if(ZLIB_FOUND AND ZLIB_LIBRARIES)
    list(GET ZLIB_LIBRARIES 0 ZLIB_FILE)
    is_subpath("${CMAKE_BINARY_DIR}" "${ZLIB_FILE}" ZLIB_LOCAL_TEST)
    if(ZLIB_LOCAL_TEST)
      set(Z_PREFIX_STR "brl_" CACHE STRING "Using local zlib" FORCE)
      set(ZLIB_STATUS "Bundled" CACHE STRING "Zlib bundled status" FORCE)
    else()
      set(ZLIB_STATUS "System" CACHE STRING "Zlib bundled status" FORCE)
    endif(ZLIB_LOCAL_TEST)
  else()
    set(ZLIB_STATUS "NotFound" CACHE STRING "Zlib bundled status" FORCE)
  endif()
endmacro(find_package_zlib)

# Eigen - linear algebra library
macro(find_package_eigen)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(Eigen3 RESET_TP)
  unset(EIGEN3_STATUS CACHE)
  set(Eigen3_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}")
  if(F_REQUIRED)
    find_package(Eigen3 NO_MODULE REQUIRED)
  else()
    find_package(Eigen3 NO_MODULE)
  endif()
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen)
  list(REMOVE_DUPLICATES SYS_INCLUDE_PATTERNS)
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} Eigen CACHE STRING "Bundled system include dirs" FORCE)

  set(_EIGEN3_INCLUDE_DIR "")
  if(TARGET Eigen3::Eigen)
    get_target_property(_EIGEN3_INCLUDE_DIRS Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES)
    if(_EIGEN3_INCLUDE_DIRS)
      list(GET _EIGEN3_INCLUDE_DIRS 0 _EIGEN3_INCLUDE_DIR)
    endif()
  endif()
  if(NOT _EIGEN3_INCLUDE_DIR AND EIGEN3_INCLUDE_DIR)
    set(_EIGEN3_INCLUDE_DIR "${EIGEN3_INCLUDE_DIR}")
  endif()
  if(NOT _EIGEN3_INCLUDE_DIR AND EIGEN3_INCLUDE_DIRS)
    list(GET EIGEN3_INCLUDE_DIRS 0 _EIGEN3_INCLUDE_DIR)
  endif()
  if(NOT _EIGEN3_INCLUDE_DIR AND Eigen3_INCLUDE_DIRS)
    list(GET Eigen3_INCLUDE_DIRS 0 _EIGEN3_INCLUDE_DIR)
  endif()

  # Let the cache know for downstream consumers and BRLCAD_Summary.cmake
  set(EIGEN3_INCLUDE_DIR "${_EIGEN3_INCLUDE_DIR}" CACHE PATH "Eigen include directory" FORCE)
  if(NOT Eigen3_FOUND)
    set(EIGEN3_STATUS "NotFound" CACHE STRING "Eigen bundled status" FORCE)
  else()
    is_subpath("${CMAKE_BINARY_DIR}" "${EIGEN3_INCLUDE_DIR}" EIGEN_LOCAL_TEST)
    is_subpath("${BRLCAD_EXT_NOINSTALL_DIR}" "${EIGEN3_INCLUDE_DIR}" EIGEN_EXT_TEST)
    if(EIGEN_LOCAL_TEST OR EIGEN_EXT_TEST)
      set(EIGEN3_STATUS "Bundled" CACHE STRING "Eigen bundled status" FORCE)
    else()
      set(EIGEN3_STATUS "System" CACHE STRING "Eigen bundled status" FORCE)
    endif()
  endif()
endmacro(find_package_eigen)

# GeometricTools - geometry library
macro(find_package_gte)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(GTE RESET_TP)
  set(GTE_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}")
  if(F_REQUIRED)
    find_package(GTE REQUIRED)
  else()
    find_package(GTE)
  endif()
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} GTE)
  list(REMOVE_DUPLICATES SYS_INCLUDE_PATTERNS)
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} GTE CACHE STRING "Bundled system include dirs" FORCE)

  # Let the cache know for BRLCAD_Summary.cmake
  set(GTE_INCLUDE_DIR "${GTE_INCLUDE_DIR}" CACHE PATH "GeometricTools include directory" FORCE)
endmacro(find_package_gte)

# SPSR - PoissonRecon surface reconstruction library
macro(find_package_spsr)

  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(SPSR RESET_TP)
  set(SPSR_ROOT "${BRLCAD_EXT_NOINSTALL_DIR}")
  if (F_REQUIRED)
    find_package(SPSR REQUIRED)
  else ()
    find_package(SPSR)
  endif ()
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} SPSR)
  list(REMOVE_DUPLICATES SYS_INCLUDE_PATTERNS)
  set(SYS_INCLUDE_PATTERNS ${SYS_INCLUDE_PATTERNS} SPSR CACHE STRING "Bundled system include dirs" FORCE)

  # Let the cache know for BRLCAD_Summary.cmake
  set(SPSR_INCLUDE_DIR "${SPSR_INCLUDE_DIR}" CACHE PATH "SPSR include directory" FORCE)

endmacro(find_package_spsr)

# OpenCV - Open Source Computer Vision Library
# http://opencv.org
macro(find_package_opencv)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(OpenCV RESET_TP)

  # First, see if we have a bundled version
  set(OpenCV_DIR_TMP "${OpenCV_DIR}")
  set(OpenCV_DIR "${CMAKE_BINARY_DIR}/${LIB_DIR}/cmake/opencv4")
  set(OpenCV_ROOT ${CMAKE_BINARY_DIR})
  find_package(OpenCV COMPONENTS core features2d imgproc highgui)
  unset(OpenCV_ROOT)

  # If no bundled copy, see what the system has
  if(NOT OpenCV_FOUND)
    set(OpenCV_DIR "${OpenCV_DIR_TMP}")
    if(F_REQUIRED)
      find_package(OpenCV REQUIRED)
    else()
      find_package(OpenCV)
    endif()
    if(OpenCV_FOUND)
      set(OPENCV_STATUS "System" CACHE STRING "OpenCV is bundled" FORCE)
    else(OpenCV_FOUND)
      set(OPENCV_STATUS "NotFound" CACHE STRING "OpenCV is bundled" FORCE)
    endif(OpenCV_FOUND)
  else(NOT OpenCV_FOUND)
    set(OPENCV_STATUS "Bundled" CACHE STRING "OpenCV is bundled" FORCE)
  endif(NOT OpenCV_FOUND)
endmacro(find_package_opencv)

# Helper: ensure a shared library has a SONAME embedded.  Without a
# SONAME, CMake passes the full build-tree path to the linker which
# records a relative DT_NEEDED entry; at runtime the dynamic loader
# tries to resolve that relative path from CWD rather than via RUNPATH,
# causing load failures.
#
# Prefer plief (P_RPATH_EXECUTABLE) since it is already used for RPATH
# management and now supports --print-soname / --set-soname.  Fall back
# to patchelf if plief is not available.
function(_brlcad_ensure_soname lib_path)
  if(NOT lib_path OR NOT EXISTS "${lib_path}")
    return()
  endif()
  # Only shared libraries (.so / .dylib) need a SONAME.
  if(NOT lib_path MATCHES "\\.so(\\.[0-9]+)*$" AND NOT lib_path MATCHES "\\.dylib$")
    return()
  endif()
  # Determine the expected SONAME: just the bare filename.
  get_filename_component(_soname "${lib_path}" NAME)
  # Prefer the already-located P_RPATH_EXECUTABLE (plief); fall back to patchelf.
  set(_soname_tool "${P_RPATH_EXECUTABLE}")
  if(NOT _soname_tool)
    find_program(_soname_tool NAMES patchelf
      HINTS "${BRLCAD_EXT_NOINSTALL_DIR}/${BIN_DIR}" /usr/bin /usr/local/bin)
  endif()
  if(NOT _soname_tool)
    return()
  endif()
  # Read the existing SONAME (empty output = no SONAME).
  execute_process(
    COMMAND "${_soname_tool}" --print-soname "${lib_path}"
    RESULT_VARIABLE _soname_print_result
    OUTPUT_VARIABLE _existing_soname
    ERROR_VARIABLE _soname_print_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(_soname_print_result)
    string(STRIP "${_soname_print_error}" _soname_print_error)
    message(WARNING "${_soname_tool} --print-soname failed for ${lib_path}: ${_soname_print_error}")
    return()
  endif()
  if(_existing_soname STREQUAL "")
    message(STATUS "Setting SONAME ${_soname} on ${lib_path}")
    # Staged third party libraries may preserve read-only install
    # permissions.  Make the build-tree copy writable before asking
    # LIEF/patchelf to rewrite it.
    file(
      CHMOD "${lib_path}"
      FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
    )
    execute_process(
      COMMAND "${_soname_tool}" --set-soname "${_soname}" "${lib_path}"
      RESULT_VARIABLE _soname_result
      OUTPUT_VARIABLE _soname_output
      ERROR_VARIABLE _soname_error
    )
    if(_soname_result)
      string(STRIP "${_soname_output}" _soname_output)
      string(STRIP "${_soname_error}" _soname_error)
      message(WARNING "${_soname_tool} --set-soname failed for ${lib_path}: ${_soname_error}${_soname_output}")
      return()
    endif()
    execute_process(
      COMMAND "${_soname_tool}" --print-soname "${lib_path}"
      RESULT_VARIABLE _soname_verify_result
      OUTPUT_VARIABLE _updated_soname
      ERROR_VARIABLE _soname_verify_error
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(_soname_verify_result)
      string(STRIP "${_soname_verify_error}" _soname_verify_error)
      message(WARNING "${_soname_tool} --print-soname verification failed for ${lib_path}: ${_soname_verify_error}")
      return()
    endif()
    if(NOT "${_updated_soname}" STREQUAL "${_soname}")
      message(WARNING "${_soname_tool} did not set SONAME on ${lib_path}: expected \"${_soname}\", got \"${_updated_soname}\"")
    endif()
  endif()
endfunction()

# TCL - scripting language.  For Tcl/Tk builds we want static lib
# building on so we get the stub libraries.
macro(find_package_tcl)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(TCL RESET_TP)
  find_package_reset(TK RESET_TP)
  if(RESET_TP)
    unset(TCL_INCLUDE_PATH CACHE)
    unset(TCL_STUB_LIBRARY CACHE)
    unset(TCL_TCLSH CACHE)
    unset(TK_INCLUDE_PATH CACHE)
    unset(TK_STUB_LIBRARY CACHE)
    unset(TK_WISH CACHE)
    unset(TK_X11_GRAPHICS CACHE)
    unset(TTK_STUB_LIBRARY CACHE)
  endif(RESET_TP)

  set(TCL_ROOT "${CMAKE_BINARY_DIR}")
  if(F_REQUIRED)
    find_package(TCL REQUIRED)
  else()
    find_package(TCL)
  endif()

  # Ensure the bundled Tcl/Tk shared libraries have a SONAME.  Without
  # one the linker records a relative DT_NEEDED path, which the dynamic
  # loader cannot resolve at runtime via RUNPATH.
  if(TCL_FOUND AND TCL_LIBRARY)
    _brlcad_ensure_soname("${TCL_LIBRARY}")
  endif()
  if(TK_LIBRARY)
    _brlcad_ensure_soname("${TK_LIBRARY}")
  endif()
endmacro(find_package_tcl)

# Qt - cross-platform user interface/application development toolkit
# https://download.qt.io/archive/qt
macro(find_package_qt)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(Qt5 RESET_TP)
  find_package_reset(Qt6 RESET_TP)

  set(
    QtComponents
    Core
    Widgets
    Gui
    Svg
    Network
  )
  if(BRLCAD_ENABLE_OPENGL)
    set(QtComponents ${QtComponents} OpenGL OpenGLWidgets)
  endif(BRLCAD_ENABLE_OPENGL)

  if(RESET_TP)
    foreach(qc ${QtComponents})
      unset(Qt6${qc}_DIR CACHE)
      unset(Qt6${qc}_FOUND CACHE)
      unset(Qt5${qc}_DIR CACHE)
      unset(Qt5${qc}_FOUND CACHE)
    endforeach(qc ${QtComponents})
  endif(RESET_TP)

  # First, see if we have a bundled version
  set(Qt6_DIR_TMP "${Qt6_DIR}")
  set(Qt6_DIR "${CMAKE_BINARY_DIR}/${LIB_DIR}/cmake/Qt6")
  set(Qt6_ROOT ${CMAKE_BINARY_DIR})
  find_package(Qt6 COMPONENTS ${QtComponents} QUIET)
  unset(Qt6_ROOT)

  if(NOT Qt6Widgets_FOUND)
    set(Qt6_DIR "${Qt6_DIR_TMP}")
    find_package(Qt6 COMPONENTS ${QtComponents} QUIET)
  endif(NOT Qt6Widgets_FOUND)
  if(NOT Qt6Widgets_FOUND)
    # We didn't find 6, try 5.  For non-standard install locations,
    # you may need to set the following CMake variables:
    #
    # Qt5_DIR=<install_dir>/lib/cmake/Qt5
    # QT_QMAKE_EXECUTABLE=<install_dir>/bin/qmake
    # AUTORCC_EXECUTABLE=<install_dir>/bin/rcc
    list(REMOVE_ITEM QtComponents OpenGLWidgets)
    if(F_REQUIRED)
      # This is our last attempt - if we're requiring Qt and this
      # fails, it's fatal
      find_package(Qt5 COMPONENTS ${QtComponents} REQUIRED)
    else()
      find_package(Qt5 COMPONENTS ${QtComponents} QUIET)
    endif()
  endif(NOT Qt6Widgets_FOUND)
  if(NOT Qt6Widgets_FOUND AND NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
    message("Qt requested, but Qt installation not found - disabling")
    set(BRLCAD_ENABLE_QT OFF)
  endif(NOT Qt6Widgets_FOUND AND NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
  if(Qt6Widgets_FOUND)
    find_package(Qt6 COMPONENTS Test)
    if(Qt6Test_FOUND)
      config_h_append(BRLCAD "#define USE_QTTEST 1\n")
    endif(Qt6Test_FOUND)
  endif(Qt6Widgets_FOUND)
  mark_as_advanced(Qt6Widgets_DIR)
  mark_as_advanced(Qt6Core_DIR)
  mark_as_advanced(Qt6Gui_DIR)
  mark_as_advanced(Qt5Widgets_DIR)
  mark_as_advanced(Qt5Core_DIR)
  mark_as_advanced(Qt5Gui_DIR)
endmacro(find_package_qt)

macro(_check_bullet_double RESULT_VAR INCDIRS LIBS)
  set(CMAKE_REQUIRED_INCLUDES ${INCDIRS})
  set(CMAKE_REQUIRED_LIBRARIES ${LIBS})
  set(CMAKE_REQUIRED_DEFINITIONS "-DBT_USE_DOUBLE_PRECISION")
  check_cxx_source_compiles("${_bullet_check_src}" ${RESULT_VAR})
endmacro()

# Bullet - physics library
macro(find_package_bullet)
  cmake_parse_arguments(F "REQUIRED" "" "" ${ARGN})

  find_package_reset(Bullet RESET_TP)
  find_package_reset(BULLET RESET_TP)
  unset(BULLET_DYNAMICS_LIBRARY CACHE)
  unset(BULLET_DYNAMICS_LIBRARY_DEBUG CACHE)
  unset(BULLET_COLLISION_LIBRARY CACHE)
  unset(BULLET_COLLISION_LIBRARY_DEBUG CACHE)
  unset(BULLET_MATH_LIBRARY CACHE)
  unset(BULLET_MATH_LIBRARY_DEBUG CACHE)
  unset(BULLET_SOFTBODY_LIBRARY CACHE)
  unset(BULLET_SOFTBODY_LIBRARY_DEBUG CACHE)
  unset(BULLET_STATUS CACHE)

  # Bullet is staged from bext's install tree into the build directory,
  # so prefer that location rather than noinstall.
  set(Bullet_ROOT "${CMAKE_BINARY_DIR}")
  if(F_REQUIRED)
    find_package(Bullet REQUIRED)
  else()
    find_package(Bullet)
  endif()

  if(BULLET_LIBRARIES OR Bullet_FOUND)
    list(GET BULLET_LIBRARIES 0 _bullet_lib0)

    is_subpath("${CMAKE_BINARY_DIR}" "${_bullet_lib0}" BULLET_LOCAL_TEST)
    is_subpath("${BRLCAD_EXT_INSTALL_DIR}" "${_bullet_lib0}" BULLET_EXT_INSTALL_TEST)
    is_subpath("${BRLCAD_EXT_NOINSTALL_DIR}" "${_bullet_lib0}" BULLET_EXT_TEST)
    if(BULLET_LOCAL_TEST OR BULLET_EXT_INSTALL_TEST OR BULLET_EXT_TEST)
      set(BULLET_STATUS "Bundled" CACHE STRING "Bullet bundled status" FORCE)
    else()
      set(BULLET_STATUS "System" CACHE STRING "Bullet bundled status" FORCE)
    endif()

    if(BULLET_STATUS STREQUAL "System")
      include(CheckCXXSourceCompiles)

      set(_bullet_check_src "
#include <btBulletDynamicsCommon.h>
int main() {
    btRigidBody::btRigidBodyConstructionInfo info(1.0, 0, 0);
    btRigidBody body(info);
    btVector3 inertia(0,0,0);
    body.setMassProps(1.0, inertia);
    return 0;
}
      ")

      _check_bullet_double(BULLET_IS_DOUBLE "${BULLET_INCLUDE_DIRS}" "${BULLET_LIBRARIES}")

      if(NOT BULLET_IS_DOUBLE)
        message(STATUS "Found system Bullet, but it is not double-precision. BRL-CAD requires double-precision Bullet.")

        # Try harder: pkg_search_module for bullet-float64 or bullet-dp
        find_package(PkgConfig)
        if(PKG_CONFIG_FOUND)
          pkg_search_module(PC_BULLET bullet-float64 bullet-dp)
          if(PC_BULLET_FOUND)
            _check_bullet_double(BULLET_ALT_IS_DOUBLE "${PC_BULLET_INCLUDE_DIRS}" "${PC_BULLET_LINK_LIBRARIES}")
            if(BULLET_ALT_IS_DOUBLE)
              set(BULLET_INCLUDE_DIRS ${PC_BULLET_INCLUDE_DIRS})
              set(BULLET_LIBRARIES ${PC_BULLET_LINK_LIBRARIES})
              set(BULLET_IS_DOUBLE ON)
            endif()
          endif()
        endif()

        # Try harder: Homebrew double-precision directory on macOS
        if(NOT BULLET_IS_DOUBLE AND APPLE)
          find_library(BULLET_HOMEBREW_DYNAMICS_LIBRARY NAMES BulletDynamics HINTS /opt/homebrew/opt/bullet/lib/bullet/double /usr/local/opt/bullet/lib/bullet/double)
          find_library(BULLET_HOMEBREW_COLLISION_LIBRARY NAMES BulletCollision HINTS /opt/homebrew/opt/bullet/lib/bullet/double /usr/local/opt/bullet/lib/bullet/double)
          find_library(BULLET_HOMEBREW_MATH_LIBRARY NAMES LinearMath HINTS /opt/homebrew/opt/bullet/lib/bullet/double /usr/local/opt/bullet/lib/bullet/double)
          if(BULLET_HOMEBREW_DYNAMICS_LIBRARY AND BULLET_HOMEBREW_COLLISION_LIBRARY AND BULLET_HOMEBREW_MATH_LIBRARY)
            set(_hb_libs ${BULLET_HOMEBREW_DYNAMICS_LIBRARY} ${BULLET_HOMEBREW_COLLISION_LIBRARY} ${BULLET_HOMEBREW_MATH_LIBRARY})
            _check_bullet_double(BULLET_HB_IS_DOUBLE "${BULLET_INCLUDE_DIRS}" "${_hb_libs}")
            if(BULLET_HB_IS_DOUBLE)
              set(BULLET_LIBRARIES ${_hb_libs})
              set(BULLET_IS_DOUBLE ON)
            endif()
          endif()
        endif()

        if(NOT BULLET_IS_DOUBLE)
          message(STATUS "Could not find a double-precision system Bullet. Falling back to bundled Bullet if possible.")
          find_package_reset(Bullet RESET_TP)
          find_package_reset(BULLET RESET_TP)
          unset(BULLET_LIBRARIES CACHE)
          unset(Bullet_FOUND CACHE)
          set(BULLET_STATUS "NotFound" CACHE STRING "Bullet bundled status" FORCE)
        endif()
      endif()

      unset(_bullet_check_src)
    endif()
  else()
    set(BULLET_STATUS "NotFound" CACHE STRING "Bullet bundled status" FORCE)
  endif()
endmacro(find_package_bullet)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
