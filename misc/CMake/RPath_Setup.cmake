# Copyright (c) 2010-2021 United States Government as represented by
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

include(CMakeParseArguments)

# Relative path specifiers are platform specific.  Encapsulate them in a
# function to hide the details from other code.
function(relative_rpath outvar)

  cmake_parse_arguments(R "" "SUFFIX;LEN" "" ${ARGN})

  if (R_SUFFIX)
    set(DPATH)
    set(CPATH "${R_SUFFIX}")
    while (NOT "${CPATH}" STREQUAL "")
      set(DPATH "../${DPATH}")
      get_filename_component(CPATH "${CPATH}" DIRECTORY)
    endwhile()
  else ()
    set(DPATH "../")
  endif (R_SUFFIX)

  if (APPLE)
    set(RELATIVE_RPATH ";@loader_path/${DPATH}${LIB_DIR}" PARENT_SCOPE)
  else (APPLE)
    set(RELATIVE_RPATH ":\$ORIGIN/${DPATH}${LIB_DIR}" PARENT_SCOPE)
  endif (APPLE)

  # IFF the caller tells us to, lengthen the relative path to a
  # specified length.  This is useful in scenarios where the relative
  # path is the only viable option
  if (R_LEN AND NOT APPLE)
    string(LENGTH "${RELATIVE_RPATH}" CURR_LEN)
    while("${CURR_LEN}" LESS "${R_LEN}")
      set(RELATIVE_RPATH "${RELATIVE_RPATH}:")
      string(LENGTH "${RELATIVE_RPATH}" CURR_LEN)
    endwhile("${CURR_LEN}" LESS "${R_LEN}")
  endif ()

  set(${outvar} "${RELATIVE_RPATH}" PARENT_SCOPE)

endfunction(relative_rpath)

# Set (or restore) a standard BRL-CAD setting for CMAKE_BUILD_RPATH.
function(std_build_rpath)
  # If we're in multiconfig mode, we need to look relative to the current
  # build configuration, not the top level lib directory.
  if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR}")
  else (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/${LIB_DIR}")
  endif (CMAKE_CONFIGURATION_TYPES)

  # Done - let the parent know what the answers are
  set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}" PARENT_SCOPE)

  # Set the final install rpath
  relative_rpath(RELPATH)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELPATH}" PARENT_SCOPE)

endfunction(std_build_rpath)

#---------------------------------------------------------------------
# Settings for CMAKE_RPATH_BUILD that support custom manipulation of
# external project outputs.

# TODO - investigate using BUILD_RPATH and INSTALL_RPATH on a per
# target basis for more precise control (for example, when setting
# up paths for libs in subdirs...  not sure yet if it could be helpful
# but should be explored.

# Note in particular that BUILD_RPATH supports generator expressions,
# in case that's of use to pull other properties on which to base
# rpaths...

# Can use set_property:  https://stackoverflow.com/a/40147991

# To allow CMake to manipulate RPaths, we need to make sure they are all the
# same length and long enough to hold the longest candidate path we will need.
#
# NOTE: Since we haven't yet created any lib subdirectories that may be created
# by 3rd party builds, we can't check them for explicit length - for now, we
# use 20 as an upper bound on how long a directory name we might need there,
# but that will need to be updated in the event a more verbosely named library
# subdirectory shows up.
function(longest_rpath outvar)

  cmake_parse_arguments(R "" "SUFFIX" "" ${ARGN})

  # Microsoft platforms don't do RPATH
  if (MSVC)
    return()
  endif (MSVC)

  # The possible roots are CMAKE_INSTALL_PREFIX and CMAKE_BINARY_DIR.
  # Find out which is longer
  string(LENGTH "${CMAKE_INSTALL_PREFIX}" ILEN)
  string(LENGTH "${CMAKE_BINARY_DIR}" BLEN)
  if ("${ILEN}" GREATER "${BLEN}")
    set(ROOT_LEN "${ILEN}")
  else ("${ILEN}" GREATER "${BLEN}")
    set(ROOT_LEN "${BLEN}")
  endif ("${ILEN}" GREATER "${BLEN}")

  # Find the longest of the configuration string names
  # (this will be zero in a non-multiconfig build)
  set(CONF_LEN 0)
  foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
    string(LENGTH "/${cfg}" CLEN)
    if ("${CLEN}" GREATER "${CONF_LEN}")
      set(CONF_LEN ${CLEN})
    endif ("${CLEN}" GREATER "${CONF_LEN}")
  endforeach(cfg ${CMAKE_CONFIGURATION_TYPES})

  # The length of LIB_DIR itself needs to be factored in
  if (R_SUFFIX)
    string(LENGTH "/${LIB_DIR}/${R_SUFFIX}" LIB_LEN)
  else (R_SUFFIX)
    string(LENGTH "/${LIB_DIR}" LIB_LEN)
  endif (R_SUFFIX)

  # Hardcoded estimate of maximum lib subdir length
  #
  #  *** UPDATE THIS IF UNABLE TO SET RPATH FOR LIBRARIES IN SUBDIRS ***
  #
  set(SUBLIB_LEN 20)

  # Include the relative path specifier length:
  if (R_SUFFIX)
    relative_rpath(RELPATH SUFFIX "${R_SUFFIX}")
  else (R_SUFFIX)
    relative_rpath(RELPATH)
  endif (R_SUFFIX)
  string(LENGTH "${RELPATH}" REL_LEN)

  math(EXPR len "${ROOT_LEN} + ${CONF_LEN} + ${LIB_LEN} + ${SUBLIB_LEN} + ${REL_LEN}")

  set(${outvar} ${len} PARENT_SCOPE)

endfunction(longest_rpath)


function(ext_build_rpath)

  # Microsoft platforms don't do RPATH
  if (MSVC)
    return()
  endif (MSVC)

  cmake_parse_arguments(R "" "SUFFIX" "" ${ARGN})

  if (R_SUFFIX)
    longest_rpath(LLEN SUFFIX "${R_SUFFIX}")
  else (R_SUFFIX)
    longest_rpath(LLEN)
  endif (R_SUFFIX)

  # If we're in multiconfig mode, we want to target the config subdir, not just
  # the build dir itself.
  if (CMAKE_CONFIGURATION_TYPES)
    if (R_SUFFIX)
      set(BUILD_RPATH "${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR}/${R_SUFFIX}")
    else (R_SUFFIX)
      set(BUILD_RPATH "${CMAKE_BINARY_DIR}/$<CONFIG>/${LIB_DIR}")
    endif (R_SUFFIX)
  else (CMAKE_CONFIGURATION_TYPES)
    if (R_SUFFIX)
      set(BUILD_RPATH "${CMAKE_BINARY_DIR}/${LIB_DIR}/${R_SUFFIX}")
    else (R_SUFFIX)
      set(BUILD_RPATH "${CMAKE_BINARY_DIR}/${LIB_DIR}")
    endif (R_SUFFIX)
  endif (CMAKE_CONFIGURATION_TYPES)

  # This is the key to the process - the ":" characters appended to the build
  # time path result in a path string in the compile outputs that has
  # sufficient length to hold the install directory, while is what allows
  # CMake's file command to manipulate the paths.  At the same time, the colon
  # lengthened paths do not break the functioning of the shorter build path.
  # Normally this is an internal CMake detail, but we need it to supply to
  # external build systems so their outputs can be manipulated as if they were
  # outputs of our own build.
  string(LENGTH "${BUILD_RPATH}" CURR_LEN)
  if (NOT APPLE)
    while("${CURR_LEN}" LESS "${LLEN}")
      set(BUILD_RPATH "${BUILD_RPATH}:")
      string(LENGTH "${BUILD_RPATH}" CURR_LEN)
    endwhile("${CURR_LEN}" LESS "${LLEN}")
  endif (NOT APPLE)

  # Done - let the parent know what the answers are
  set(CMAKE_BUILD_RPATH "${BUILD_RPATH}" PARENT_SCOPE)

  # Set the final install rpath
  relative_rpath(RELPATH)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}${RELPATH}" PARENT_SCOPE)

endfunction(ext_build_rpath)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
