#               F I N D A D A P T A G R A M S . C M A K E
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
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
# - Find Adaptagrams libraries
#
# The following variables are set:
#
# AVOID_INCLUDE_DIR           - where to find libavoid.h, etc.
# AVOID_LIBRARY               - where to find libavoid.a
# The following variables are set:
#
#  ADAPTAGRAMS_INCLUDE_DIRS   - where to find libavoid.h, etc.
#  ADAPTAGRAMS_LIBRARIES      - List of libraries when using Adaptagrams.
#  ADAPTAGRAMS_FOUND          - True if libavoid is found.

find_path(AVOID_INCLUDE_DIR libavoid.h PATH_SUFFIXES libavoid)
find_library(AVOID_LIBRARY NAMES avoid)

set(ADAPTAGRAMS_INCLUDE_DIRS ${AVOID_INCLUDE_DIR} CACHE STRING "Adaptagrams headers")
set(ADAPTAGRAMS_LIBRARIES    ${AVOID_LIBRARY} CACHE STRING "Adaptagrams libs")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ADAPTAGRAMS DEFAULT_MSG AVOID_LIBRARY AVOID_INCLUDE_DIR)

MARK_AS_ADVANCED(ADAPTAGRAMS_LIBRARIES ADAPTAGRAMS_INCLUDE_DIRS AVOID_LIBRARY AVOID_INCLUDE_DIR)
# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
