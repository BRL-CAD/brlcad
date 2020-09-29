#               F I N D P O L Y 2 T R I . C M A K E
# BRL-CAD
#
# Copyright (c) 2013-2020 United States Government as represented by
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
# - Find Constrained Delaunay Triangulation library
#
# The following variables are set:
#
#  POLY2TRI_INCLUDE_DIRS   - where to find poly2tri.h, etc.
#  POLY2TRI_LIBRARIES      - List of libraries when using poly2tri.
#  POLY2TRI_FOUND          - True if poly2tri found.

find_path(POLY2TRI_INCLUDE_DIR poly2tri.h)
find_library(POLY2TRI_LIBRARY NAMES poly2tri)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(POLY2TRI DEFAULT_MSG POLY2TRI_LIBRARY POLY2TRI_INCLUDE_DIR)

if(POLY2TRI_FOUND)
  set(POLY2TRI_INCLUDE_DIRS ${POLY2TRI_INCLUDE_DIR})
  set(POLY2TRI_LIBRARIES    ${POLY2TRI_LIBRARY})
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
