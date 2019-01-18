# Copyright (c) 2010-2018 United States Government as represented by
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

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.
# http://www.cmake.org/Wiki/CMake_RPATH_handling

include(CMakeParseArguments)

if(NOT COMMAND cmake_set_rpath)

  function(cmake_set_rpath)

    # See if we have a suffix for the paths
    cmake_parse_arguments(R "" "SUFFIX" "" ${ARGN})

    # We want the full RPATH set in the build tree so we can run programs without
    # needing to set LD_LIBRARY_PATH
    set(CMAKE_SKIP_BUILD_RPATH FALSE PARENT_SCOPE)

    # We DON'T want the final install directory RPATH set in the build directory
    # - it should only be set to the installation value when actually installed.
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE PARENT_SCOPE)

    # Add the automatically determined parts of the RPATH which point to
    # directories outside the build tree to the install RPATH
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE PARENT_SCOPE)


    # Set RPATH value to use when installing.  This should be set to always
    # prefer the version in the installed path when possible, but fall back on a
    # location relative to the loading file's path if the installed version is
    # not present.  How to do so is platform specific.
    if(NOT R_SUFFIX)
      if(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}:\$ORIGIN/../${LIB_DIR}")
      else(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR};@loader_path/../${LIB_DIR}")
      endif(NOT APPLE)
    else(NOT R_SUFFIX)
      if(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${R_SUFFIX}:\$ORIGIN/../${LIB_DIR}/${R_SUFFIX}")
      else(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}/${R_SUFFIX};@loader_path/../${LIB_DIR}/${R_SUFFIX}")
      endif(NOT APPLE)
    endif(NOT R_SUFFIX)

    # Determine what the build time RPATH will be that is used to support
    # CMake's RPATH manipulation, so it can be used in external projects.
    if(NOT R_SUFFIX)
      set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/${LIB_DIR}")
    else(NOT R_SUFFIX)
      set(CMAKE_BUILD_RPATH "${CMAKE_BINARY_DIR}/${LIB_DIR}/${suffix}")
    endif(NOT R_SUFFIX)
    string(LENGTH "${CMAKE_INSTALL_RPATH}" INSTALL_LEN)
    string(LENGTH "${CMAKE_BUILD_RPATH}" CURR_LEN)
    while("${CURR_LEN}" LESS "${INSTALL_LEN}")
      # This is the key to the process - the ":" characters appended to the
      # build time path result in a path string in the compile outputs that
      # has sufficient length to hold the install directory, while is what
      # allows CMake's file command to manipulate the paths.  At the same time,
      # the colon lengthened paths do not break the functioning of the shorter
      # build path.  Normally this is an internal CMake detail, but we need it
      # to supply to external build systems so their outputs can be manipulated
      # as if they were outputs of our own build.
      set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}:")
      string(LENGTH "${CMAKE_BUILD_RPATH}" CURR_LEN)
    endwhile("${CURR_LEN}" LESS "${INSTALL_LEN}")

    # Done - let the parent know what the answers are
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}" PARENT_SCOPE)
    set(CMAKE_BUILD_RPATH "${CMAKE_BUILD_RPATH}" PARENT_SCOPE)

  endfunction(cmake_set_rpath)

endif(NOT COMMAND cmake_set_rpath)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
