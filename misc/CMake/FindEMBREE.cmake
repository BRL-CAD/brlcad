#                  F I N D E M B R E E . C M A K E
# BRL-CAD
#
# Copyright (c) 2015 United States Government as represented by
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
###########################################################################
# Embree ray tracer
#
# The following variables are set:
#
#  EMBREE_INCLUDE_DIRS   - where to find rtcore.h, etc.
#  EMBREE_LIBRARIES      - Embree library.
#  EMBREE_FOUND          - True if Embree found.

# TODO - look for various versions of Embree - simd enabled vs CPU, etc.

# If 'EMBREE_HOME' not set, use the env variable of that name if available
if(NOT DEFINED EMBREE_HOME)
  if(NOT $ENV{EMBREE_HOME} STREQUAL "")
    set (EMBREE_HOME $ENV{EMBREE_HOME})
  endif(NOT $ENV{EMBREE_HOME} STREQUAL "")
endif(NOT DEFINED EMBREE_HOME)

find_library(EMBREE_LIBRARY
  NAMES embree
  PATHS ${EMBREE_HOME}
  PATH_SUFFIXES lib lib/x64 embree/lib/x64)
find_path(EMBREE_INCLUDE_DIR
  embree2/rtcore.h
  PATHS ${EMBREE_HOME}
  PATH_SUFFIXES include embree/include)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EMBREE DEFAULT_MSG EMBREE_LIBRARY EMBREE_INCLUDE_DIR)

if(EMBREE_FOUND)
  set(EMBREE_LIBRARIES ${EMBREE_LIBRARY})
  set(EMBREE_INCLUDE_DIRS ${EMBREE_INCLUDE_DIR})
endif(EMBREE_FOUND)

###########################################################################

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
