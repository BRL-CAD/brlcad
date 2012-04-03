#               B R L C A D _ U T I L . C M A K E
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
#-----------------------------------------------------------------------------
# Pretty-printing macro that generates a box around a string and prints the
# resulting message.
macro(BOX_PRINT input_string border_string)
  string(LENGTH ${input_string} MESSAGE_LENGTH)
  string(LENGTH ${border_string} SEPARATOR_STRING_LENGTH)
  while(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
    set(SEPARATOR_STRING "${SEPARATOR_STRING}${border_string}")
    string(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
  endwhile(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
  message("${SEPARATOR_STRING}")
  message("${input_string}")
  message("${SEPARATOR_STRING}")
endmacro()

#-----------------------------------------------------------------------------
include(CheckCCompilerFlag)
CHECK_C_COMPILER_FLAG(-Wno-error NOERROR_FLAG)

macro(CPP_WARNINGS srcslist)
  # We need to specify specific flags for c++ files - their warnings are
  # not usually used to hault the build, althogh BRLCAD_ENABLE_CXX_STRICT
  # can be set to on to achieve that
  if(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
    foreach(srcfile ${${srcslist}})
      if(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
	if(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  if(NOERROR_FLAG)
	    get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	    set_source_files_properties(${srcfile} COMPILE_FLAGS "-Wno-error ${previous_flags}")
	  endif(NOERROR_FLAG)
	else(BRLCAD_ENABLE_COMPILER_WARNINGS)
	  get_property(previous_flags SOURCE ${srcfile} PROPERTY COMPILE_FLAGS)
	  set_source_files_properties(${srcfile} COMPILE_FLAGS "-w ${previous_flags}")
	endif(BRLCAD_ENABLE_COMPILER_WARNINGS)
      endif(${srcfile} MATCHES "cpp$" OR ${srcfile} MATCHES "cc$")
    endforeach(srcfile ${${srcslist}})
  endif(BRLCAD_ENABLE_STRICT AND NOT BRLCAD_ENABLE_CXX_STRICT)
endmacro(CPP_WARNINGS)

#-----------------------------------------------------------------------------
# It is sometimes convenient to be able to supply both a filename and a 
# variable name containing a list of files to a single macro.
# This routine handles both forms of input - a separate variable is specified
# that contains the name of the variable holding the list, and in the case
# where there is no such variable one is prepared.  This way, subsequent
# macro logic need only deal with a variable holding a list, whatever the
# original form of the input.
macro(NORMALIZE_FILE_LIST inlist targetvar)
  set(havevarname 0)
  foreach(maybefilename ${inlist})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
      set(havevarname 1)
    endif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${maybefilename})
  endforeach(maybefilename ${${targetvar}})
  set(fullpath_files)
  if(NOT havevarname)
    CMAKEFILES(${inlist})
    set(fullpath_files)
    foreach(filename ${inlist})
      get_filename_component(file_fullpath "${filename}" ABSOLUTE)
      if(NOT "${filename}" STREQUAL "${file_fullpath}")
        set(fullpath_files ${fullpath_files} "${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
      else(NOT "${filename}" STREQUAL "${file_fullpath}")
        set(fullpath_files ${fullpath_files} "${filename}")
      endif(NOT "${filename}" STREQUAL "${file_fullpath}")
    endforeach(filename ${inlist})
    set(FILELIST "${fullpath_files}")
    set(${targetvar} "FILELIST")
    BRLCAD_TARGET_NAME("${inlist}" targetname)
  else(NOT havevarname)
    CMAKEFILES(${${inlist}})
    set(fullpath_files)
    foreach(filename ${${inlist}})
      get_filename_component(file_fullpath "${filename}" ABSOLUTE)
      if(NOT "${filename}" STREQUAL "${file_fullpath}")
        set(fullpath_files ${fullpath_files} "${CMAKE_CURRENT_SOURCE_DIR}/${filename}")
      else(NOT "${filename}" STREQUAL "${file_fullpath}")
        set(fullpath_files ${fullpath_files} "${filename}")
      endif(NOT "${filename}" STREQUAL "${file_fullpath}")
    endforeach(filename ${${inlist}})
    set(${inlist} "${fullpath_files}")
    set(${targetvar} "${inlist}")
    set(targetname "${inlist}")
  endif(NOT havevarname)
  if(NOT "${ARGV2}" STREQUAL "")
    set(${ARGV2} "${targetname}")
  endif(NOT "${ARGV2}" STREQUAL "")
endmacro(NORMALIZE_FILE_LIST)

#-----------------------------------------------------------------------------
# Some of the more advanced build system features in BRL-CAD's CMake build 
# need to know whether symlink support is present on the current OS - go
# ahead and do this test up front, caching the results.
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


#-----------------------------------------------------------------------------
# For situations like file copying, where we sometimes need to autogenerate
# target names, it is important to make sure we can avoid generating absurdly
# long names.  To do this, we run candidate names through a length filter
# and use their MD5 hash if they are too long.
macro(BRLCAD_TARGET_NAME input_string outputvar)
  string(REGEX REPLACE "/" "_" targetstr ${input_string})
  string(REGEX REPLACE "\\." "_" targetstr ${targetstr})
  string(LENGTH "${targetstr}" STRLEN)
  # If the input string is longer than 30 characters, generate a 
  # shorter string using the md5 hash.  It will be cryptic but 
  # the odds are very good it'll be a unique target name
  # and the string will be short enough, which is what we need.
  if ("${STRLEN}" GREATER 30)
    file(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/MD5CONTENTS "${targetstr}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E md5sum ${CMAKE_BINARY_DIR}/CMakeTmp/MD5CONTENTS OUTPUT_VARIABLE targetname)
    string(REPLACE " ${CMAKE_BINARY_DIR}/CMakeTmp/MD5CONTENTS" "" targetname "${targetname}")
    string(STRIP "${targetname}" targetname)
    file(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/MD5CONTENTS)
    set(${outpvar} ${targetname})
  else ("${STRLEN}" GREATER 30)
    set(${outputvar} "${targetstr}")
  endif ("${STRLEN}" GREATER 30)
endmacro(BRLCAD_TARGET_NAME)


#-----------------------------------------------------------------------------
# It is sometimes necessary for build logic to be aware of all instances
# of a certain category of target that have been defined for a particular
# build directory - for example, the pkgIndex.tcl generation targets need
# to ensure that all data copying targets have done their work before they
# generate their indexes.  To support this, macros are define that allow
# globally available lists to be defined, maintained and accessed.  We use
# this approach instead of directory properties because CMake's documentation
# seems to indicate that directory properties also apply to subdirectories,
# and we want these lists to be associated with one and only one directory.
macro(BRLCAD_ADD_DIR_LIST_ENTRY list_name dir_in list_entry)
  string(REGEX REPLACE "/" "_" currdir_str ${dir_in})
  string(TOUPPER "${currdir_str}" currdir_str)
  get_property(${list_name}_${currdir_str} GLOBAL PROPERTY DATA_TARGETS_${currdir_str})
  if(NOT ${list_name}_${currdir_str})
    define_property(GLOBAL PROPERTY CMAKE_LIBRARY_TARGET_LIST BRIEF_DOCS "${list_name}" FULL_DOCS "${list_name} for directory ${dir_in}")
  endif(NOT ${list_name}_${currdir_str})
  set_property(GLOBAL APPEND PROPERTY ${list_name}_${currdir_str} ${list_entry})
endmacro(BRLCAD_ADD_DIR_LIST_ENTRY)

macro(BRLCAD_GET_DIR_LIST_CONTENTS list_name dir_in outvar)
  string(REGEX REPLACE "/" "_" currdir_str ${dir_in})
  string(TOUPPER "${currdir_str}" currdir_str)
  get_property(${list_name}_${currdir_str} GLOBAL PROPERTY ${list_name}_${currdir_str})
  set(${outvar} "${DATA_TARGETS_${currdir_str}}")
endmacro(BRLCAD_GET_DIR_LIST_CONTENTS)


#-----------------------------------------------------------------------------
# We need a way to tell whether one path is a subpath of another path without
# relying on regular expressions, since file paths may have characters in them
# that will trigger regex matching behavior when we don't want it.  (To test,
# for example, use a build directory name of build++) 
#
# The routine below does the check without using regex matching. 
macro(IS_SUBPATH in_candidate_subpath in_full_path result_var)
  # Convert paths to lists of directories - regex based
  # matching won't work reliably, so instead look at each
  # element compared to its corresponding element in the
  # other path using string comparison.

  # get the CMake form of the path so we have something consistent
  # to work on
  file(TO_CMAKE_PATH "${in_full_path}" full_path)
  file(TO_CMAKE_PATH "${in_candidate_subpath}" candidate_subpath)

  # check the string lengths - if the "subpath" is longer
  # than the full path, there's not point in going further
  string(LENGTH "${full_path}" FULL_LENGTH)
  string(LENGTH "${candidate_subpath}" SUB_LENGTH)
  if("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")
    set(${result_var} 0)
  else("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")
    # OK, maybe it's a subpath - time to actually check 
    string(REPLACE "/" ";" full_path_list "${full_path}")
    string(REPLACE "/" ";" candidate_subpath_list "${candidate_subpath}")
    set(found_difference 0)
    while(NOT found_difference AND candidate_subpath_list)
      list(GET full_path_list 0 full_path_element)
      list(GET candidate_subpath_list 0 subpath_element)
      if("${full_path_element}" STREQUAL "${subpath_element}")
	list(REMOVE_AT full_path_list 0)
	list(REMOVE_AT candidate_subpath_list 0)
      else("${full_path_element}" STREQUAL "${subpath_element}")
	set(found_difference 1)
      endif("${full_path_element}" STREQUAL "${subpath_element}")
    endwhile(NOT found_difference AND candidate_subpath_list)
    # Now we know - report the result
    if(NOT found_difference)
      set(${result_var} 1)
    else(NOT found_difference)
      set(${result_var} 0)
    endif(NOT found_difference)
  endif("${SUB_LENGTH}" GREATER "${FULL_LENGTH}")
endmacro(IS_SUBPATH)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
