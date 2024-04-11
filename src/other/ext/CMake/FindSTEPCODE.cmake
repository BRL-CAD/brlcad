#                   F I N D V D S . C M A K E
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
# - Find STEPCODE
#
# TODO - this is a stub.  Doing this correctly means looking for
# multiple libraries and headers

# The following variables are set:
#
#  STEPCODE_INCLUDE_DIRS   - where to find vds.h, etc.
#  STEPCODE_LIBRARIES      - List of libraries when using vds.
#  STEPCODE_FOUND          - True if vds found.

find_path(STEPCODE_INCLUDE_DIR stepcode.h)
find_library(STEPCODE_LIBRARY NAMES stepcode)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(STEPCODE DEFAULT_MSG STEPCODE_LIBRARY STEPCODE_INCLUDE_DIR)

if (STEPCODE_FOUND)
  set(STEPCODE_INCLUDE_DIRS ${STEPCODE_INCLUDE_DIR})
  set(STEPCODE_LIBRARIES    ${STEPCODE_LIBRARY})
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
