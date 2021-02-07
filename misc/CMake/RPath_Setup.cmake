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

#---------------------------------------------------------------------
# The following logic is what allows binaries to run successfully in
# the build directory AND install directory.  Thanks to plplot for
# identifying the necessity of setting CMAKE_INSTALL_NAME_DIR on OSX.
# Documentation of these options is available at
# http://www.cmake.org/Wiki/CMake_RPATH_handling

if(NOT COMMAND cmake_set_rpath)

  function(cmake_set_rpath)

    if(NOT CMAKE_RPATH_SET)

      # We want the full RPATH set in the build tree so we can run programs without
      # needing to set LD_LIBRARY_PATH
      set(CMAKE_SKIP_BUILD_RPATH FALSE PARENT_SCOPE)

      # We DON'T want the final install directory RPATH set in the build directory
      # - it should only be set to the installation value when actually installed.
      set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE PARENT_SCOPE)

      # Set RPATH value to use when installing.  This should be set to always
      # prefer the version in the installed path when possible, but fall back on a
      # location relative to the loading file's path if the installed version is
      # not present.  How to do so is platform specific.
      if(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}:\$ORIGIN/../${LIB_DIR}" PARENT_SCOPE)
      else(NOT APPLE)
	set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_DIR};@loader_path/../${LIB_DIR}" PARENT_SCOPE)
      endif(NOT APPLE)

      # On OSX, we need to set INSTALL_NAME_DIR instead of RPATH for CMake < 3.0
      # http://www.cmake.org/cmake/help/cmake-2-8-docs.html#variable:CMAKE_INSTALL_NAME_DIR
      # http://www.cmake.org/cmake/help/v3.2/policy/CMP0042.html
      if ("${CMAKE_VERSION}" VERSION_LESS 3.0)
	set(CMAKE_INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/${LIB_DIR}" PARENT_SCOPE)
      endif ("${CMAKE_VERSION}" VERSION_LESS 3.0)

      # Add the automatically determined parts of the RPATH which point to
      # directories outside the build tree to the install RPATH
      set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE PARENT_SCOPE)

      # RPATH setup is now complete
      set(CMAKE_RPATH_SET 1 PARENT_SCOPE)

    endif(NOT CMAKE_RPATH_SET)

  endfunction(cmake_set_rpath)

endif(NOT COMMAND cmake_set_rpath)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
