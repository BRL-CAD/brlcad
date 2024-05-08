#
# This file is modeled after appleseed.
# Visit https://appleseedhq.net/ for additional information and resources.
#
# This software is released under the MIT license.
#
# Copyright (c) 2014-2018 Esteban Tovagliari, The appleseedhq Organization
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

###
# - Find ASSETIMPORT
# Find the native ASSETIMPORT includes and library
#
#  ASSETIMPORT_INCLUDE_DIR   - where to find include headers
#  ASSETIMPORT_LIBRARIES     - List of libraries when using assetimport.
#
###
# - helpful cmake flags when building
#
#  ASSETIMPORT_ROOT          - path to assetimport root if it is built outside of /usr
#
#=============================================================================
set(_ASSETIMPORT_SEARCHES)

# Search ASSETIMPORT_ROOT first if it is set.
if(ASSETIMPORT_ROOT)
  set(_ASSETIMPORT_SEARCH_ROOT PATHS ${ASSETIMPORT_ROOT} NO_DEFAULT_PATH)
  list(APPEND _ASSETIMPORT_SEARCHES _ASSETIMPORT_SEARCH_ROOT)
endif()

set(ASSETIMPORT_NAMES assimp assimpd)

# Try each search configuration.
foreach(search ${_ASSETIMPORT_SEARCHES})
  find_path(ASSETIMPORT_INCLUDE_DIR
    NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
    ${${search}} PATH_SUFFIXES include)
  find_library(ASSETIMPORT_LIBRARY
    NAMES ${ASSETIMPORT_NAMES}
    ${${search}} PATH_SUFFIXES lib lib64 bin)
endforeach()

if (NOT ASSETIMPORT_LIBRARY OR NOT ASSETIMPORT_INCLUDE_DIR)
  # Try a more generic search
  find_path(ASSETIMPORT_INCLUDE_DIR
    NAMES assimp/postprocess.h assimp/scene.h assimp/version.h assimp/config.h assimp/cimport.h
    PATH_SUFFIXES include)
  find_library(ASSETIMPORT_LIBRARY
    NAMES ${ASSETIMPORT_NAMES}
    PATH_SUFFIXES lib lib64 bin)
endif (NOT ASSETIMPORT_LIBRARY OR NOT ASSETIMPORT_INCLUDE_DIR)

# Handle the QUIETLY and REQUIRED arguments and set assetimport_FOUND.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ASSETIMPORT REQUIRED_VARS
  ASSETIMPORT_LIBRARY
  ASSETIMPORT_INCLUDE_DIR
  )

# Set the output variables.
if (assetimport_FOUND)
  set (ASSETIMPORT_INCLUDE_DIRS ${ASSETIMPORT_INCLUDE_DIR})
  set (ASSETIMPORT_LIBRARIES ${ASSETIMPORT_LIBRARY})
else ()
  set (ASSETIMPORT_INCLUDE_DIRS)
  set (ASSETIMPORT_LIBRARIES)
endif ()

mark_as_advanced (
  ASSETIMPORT_INCLUDE_DIR
  ASSETIMPORT_LIBRARY
  )



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
