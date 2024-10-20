#                   F I N D G T E . C M A K E
# BRL-CAD
#
# Copyright (c) 2024 United States Government as represented by
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
# Find GeometricTools Mathematics headers
#
# Once done this will define
#
#  GTE_FOUND - system has GTE Mathematics headers
#  GTE_INCLUDE_DIR - the GTE include directory
#
# and the following imported target:
#
#  GTE::GTE- Provides include dir for GeometricTools Mathematics headers

set(_GTE_SEARCHES)

# Search GTE_ROOT first if it is set.
if(GTE_ROOT)
  set(_GTE_SEARCH_ROOT PATHS ${GTE_ROOT} NO_DEFAULT_PATH)
  list(APPEND _GTE_SEARCHES _GTE_SEARCH_ROOT)
endif()

# Try each search configuration.
foreach(search ${_GTE_SEARCHES})
  find_path(GTE_INCLUDE_DIR NAMES Mathematics/ConvexHull3.h ${${search}} PATH_SUFFIXES include include/GTE)
endforeach()

mark_as_advanced(GTE_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set ZLIB_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTE REQUIRED_VARS GTE_INCLUDE_DIR)

if(GTE_FOUND)
  set(GTE_INCLUDE_DIRS ${GTE_INCLUDE_DIR})

  if(NOT TARGET GTE::GTE)
    add_library(GTE::GTE UNKNOWN IMPORTED)
    set_target_properties(GTE::GTE PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${GTE_INCLUDE_DIRS}")
  endif()
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
