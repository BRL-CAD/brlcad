#                  T R A C K F I L E S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2023 United States Government as represented by
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
# Unlike most CMake based projects, BRL-CAD's current build system works like
# its AutoTools-based precursor and tracks all files present in the source
# tree.  Additionally, it also defines a "distclean" target that is intended to
# "scrub" the tree down to either a vanilla source tree (when doing an
# in-src-dir build) or an empty directory (when doing the more traditional
# compile-in-build-directory process.  (CMake's traditional 'clean' targets do
# not remove all outputs of any sort - custom command outputs and CMake's own
# files are left behind using those targets alone.)
#
# In order to support this process, we define functions that allow the build to
# identify both files that are part of the expected BRL-CAD build tree, and
# expected build outputs that need to be removed.  Note that build outputs that
# the standard "clean" targets remove should not be listed - those are already
# handled by the standard CMake build process and the output names are
# frequently platform specific.  The "distclean" function should only be used
# to identify files a "make clean" pass would leave behind.
#
# BRL-CAD's exec and lib build targets will handle listing non-generated source
# files specified for those targets as BRL-CAD files, so it is not necessary to
# use the raw "CMAKEFILES" function to list those inputs (unless such files
# may be turned on or off for compilation by feature flags - in that case, the
# files which may be disabled should also be listed in a "CMAKEFILES" list)
#
# For performance reasons, CMake global variables are used to build up the
# various file lists rather than writing them to disk each time an addition
# is made.  Other build logic then retrieves the lists and writes them to
# disk to make them available for processing with build targets such as
# distclean and package_source

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
if (NOT COMMAND is_subpath)
  function(is_subpath candidate_subpath full_path result_var)

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

  endfunction(is_subpath)
endif (NOT COMMAND is_subpath)

#-----------------------------------------------------------------------------
# Distcheck needs to know what files are "supposed" to be present in order to
# make sure the source tree is clean prior to building a distribution tarball,
#
# For this set of functions to work correctly, it is important that any call
# (via target definitions or via explicit calls to CMAKEFILES) supply relative
# paths for files present in the source tree.  Generated files fed into
# compilation targets are not one of the things that should be in lists
# generated by this function, and the only way to reliably recognize them is to
# reject files specified using their full path.  Such files must use their full
# path in the build logic in order for out-of-src-dir builds to function, so as
# long as no full paths are used for files actually IN the source tree this
# method is reliable.  The filtering logic will try to catch improperly
# specified files, but if the build directory and the source directory are one
# and the same this will not be possible.

if (NOT COMMAND check_source_dir_fullpath)
  define_property(GLOBAL PROPERTY CMAKE_IGNORE_FILES BRIEF_DOCS "distcheck ignore files" FULL_DOCS "List of files known to CMake")
  define_property(GLOBAL PROPERTY CMAKE_IGNORE_DIRS BRIEF_DOCS "distcheck ignore dirs" FULL_DOCS "List of directories marked as fully known to CMake")

  # If the build directory is not the same as the source directory, we can
  # enforce the convention that only generated files be specified with their full
  # name.
  function(check_source_dir_fullpath ITEM_PATH)

    # We can only do this if BINARY_DIR != SOURCE_DIR
    if(NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
      return()
    endif(NOT "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")

    # If it's a full path in the binary dir, it's fine
    is_subpath("${CMAKE_BINARY_DIR}" "${ITEM_PATH}" SUBPATH_TEST)
    if(NOT "${SUBPATH_TEST}" STREQUAL "0")
      return()
    endif(NOT "${SUBPATH_TEST}" STREQUAL "0")

    # If it's a full path in the source dir, it's fatal
    is_subpath("${CMAKE_SOURCE_DIR}" "${ITEM_PATH}" SUBPATH_TEST)
    if("${SUBPATH_TEST}" STREQUAL "1")
      message(FATAL_ERROR "${ITEM} is listed in \"${CMAKE_CURRENT_SOURCE_DIR}\" using its absolute path.  Please specify the location of this file using a relative path.")
    endif("${SUBPATH_TEST}" STREQUAL "1")

  endfunction(check_source_dir_fullpath ITEM_PATH)
endif (NOT COMMAND check_source_dir_fullpath)

if (NOT COMMAND cmfile)
  function(cmfile ITEM)

    get_filename_component(ITEM_PATH "${ITEM}" PATH)
    if(NOT "${ITEM_PATH}" STREQUAL "")
      # The hard case - path specified, need some validation.
      CHECK_SOURCE_DIR_FULLPATH("${ITEM_PATH}")

      # Ignore files specified using full paths, since they
      # should be generated files and are not part of the
      # source code repository.
      get_filename_component(ITEM_ABS_PATH "${ITEM_PATH}" ABSOLUTE)
      if("${ITEM_PATH}" STREQUAL "${ITEM_ABS_PATH}")
	return()
      endif("${ITEM_PATH}" STREQUAL "${ITEM_ABS_PATH}")
    endif(NOT "${ITEM_PATH}" STREQUAL "")

    # Handle fatal cases
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}")
      message(FATAL_ERROR "Trying to ignore directory: ${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}")
    endif(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}")
      message(FATAL_ERROR "Attempting to ignore non-existent file ${ITEM}, in directory \"${CMAKE_CURRENT_SOURCE_DIR}\"")
    endif(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}")

    # We're good - log it
    get_filename_component(item_absolute "${CMAKE_CURRENT_SOURCE_DIR}/${ITEM}" ABSOLUTE)
    set_property(GLOBAL APPEND PROPERTY CMAKE_IGNORE_FILES "${item_absolute}")

  endfunction(cmfile ITEM)
endif (NOT COMMAND cmfile)

# This is the function most commonly used in calling code to specify files
# as part of the expected source tree.
if (NOT COMMAND cmakefiles)
  function(CMAKEFILES)

    # CMake flags to add_library/add_executable aren't going to be valid filenames - just
    # yank them up front.
    set(ITEMS ${ARGN})
    if (NOT ITEMS)
      return()
    endif (NOT ITEMS)
    list(REMOVE_ITEM ITEMS SHARED STATIC OBJECT WIN32 UNKNOWN IMPORTED MODULE INTERFACE EXCLUDE_FROM_ALL)
    list(FILTER ITEMS EXCLUDE REGEX "TARGET_OBJECTS")

    foreach(ITEM ${ITEMS})
      cmfile("${ITEM}")
    endforeach(ITEM ${ITEMS})

  endfunction(CMAKEFILES FILESLIST)
endif (NOT COMMAND cmakefiles)

# Routine to tell distcheck to ignore a series of items in a directory.
# Primarily useful when working with src/other dist lists.
if (NOT COMMAND cmakefiles_in_dir)
  function(cmakefiles_in_dir filestoignore targetdir)
    set(CMAKE_IGNORE_LIST "")
    foreach(filepath ${${filestoignore}})
      set(CMAKE_IGNORE_LIST ${CMAKE_IGNORE_LIST} "${targetdir}/${filepath}")
    endforeach(filepath ${filestoignore})
    cmakefiles(${CMAKE_IGNORE_LIST})
  endfunction(cmakefiles_in_dir)
endif (NOT COMMAND cmakefiles_in_dir)

#-----------------------------------------------------------------------------
# The "distclean" build target will clear all CMake-generated files from a
# source directory in the case of an in-source-dir configuration, or will fully
# clear an out-of-source build directory.
# (NOTE:  In the case of the ninja build tool, we can't FULLY clear the build
# directory since ninja itself requires that a .ninja_log file be present in
# the build directory while running... calling code shouldn't specify or fail
# on the presence of that file, as build targets won't be able to clear it.)
if (NOT COMMAND distclean)
  define_property(GLOBAL PROPERTY CMAKE_DISTCLEAN_TARGET_LIST BRIEF_DOCS "Build output files to be cleared" FULL_DOCS "Additional files to remove if clearing ALL configure+build outputs")
  function(distclean)
    foreach(item ${ARGN})
      get_filename_component(item_dir ${item} DIRECTORY)
      if ("${item_dir}" STREQUAL "")
	set(item_path "${CMAKE_CURRENT_BINARY_DIR}/${item}")
      else ("${item_dir}" STREQUAL "")
	set(item_path "${item}")
      endif ("${item_dir}" STREQUAL "")
      set_property(GLOBAL APPEND PROPERTY CMAKE_DISTCLEAN_TARGET_LIST "${item_path}")
    endforeach(item ${ARGN})
  endfunction(distclean)
endif (NOT COMMAND distclean)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
