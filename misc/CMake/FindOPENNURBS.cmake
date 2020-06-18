#             F I N D O P E N N U R B S . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2020 United States Government as represented by
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
# - Find OpenNURBS
#
# The following variables are set:
#
#  OPENNURBS_INCLUDE_DIRS   - where to find opennurbs.h, etc.
#  OPENNURBS_LIBRARIES      - List of libraries when using openNURBS
#  OPENNURBS_FOUND          - True if openNURBS found.

#  OpenNURBS requires zlib
find_package(ZLIB)

find_path(OPENNURBS_INCLUDE_DIR opennurbs.h)
find_library(OPENNURBS_LIBRARY NAMES opennurbs openNURBS OpenNURBS)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OPENNURBS DEFAULT_MSG OPENNURBS_LIBRARY OPENNURBS_INCLUDE_DIR)

if(OPENNURBS_FOUND)
  set(OPENNURBS_INCLUDE_DIRS ${OPENNURBS_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR})
  list(REMOVE_DUPLICATES OPENNURBS_INCLUDE_DIR)
  set(OPENNURBS_LIBRARIES    ${OPENNURBS_LIBRARY} ${ZLIB_LIBRARY})
endif()
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
