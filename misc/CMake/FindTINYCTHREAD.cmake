#             F I N D T I N Y C T H R E A D . C M A K E
# BRL-CAD
#
# Copyright (c) 2013-2016 United States Government as represented by
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
# - Find C11 style thread portability wrapper 
#
# The following variables are set:
#
#  TINYCTHREAD_INCLUDE_DIRS   - where to find tinycthread.h.
#  TINYCTHREAD_LIBRARIES      - List of libraries when using tinycthread.
#  TINYCTHREAD_FOUND          - True if tinycthread found.

find_path(TINYCTHREAD_INCLUDE_DIR tinycthread.h)
find_library(TINYCTHREAD_LIBRARY NAMES tinycthread)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TINYCTHREAD DEFAULT_MSG TINYCTHREAD_LIBRARY TINYCTHREAD_INCLUDE_DIR)

IF (TINYCTHREAD_FOUND)
  set(TINYCTHREAD_INCLUDE_DIRS ${TINYCTHREAD_INCLUDE_DIR})
  set(TINYCTHREAD_LIBRARIES    ${TINYCTHREAD_LIBRARY})
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
