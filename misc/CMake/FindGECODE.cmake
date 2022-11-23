#                   F I N D G E C O D E . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2022 United States Government as represented by
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
# - Find Gecode constraint programming libraries
#
# The following variables are set:
#
# GECODE_INCLUDE_DIR
# GECODE_LIBRARIES

find_path(GECODE_INCLUDE_DIR gecode/search.hh)

find_library(GECODE_SUPPORT_LIB NAMES gecodesupport)
find_library(GECODE_KERNEL_LIB NAMES gecodekernel)
find_library(GECODE_SEARCH_LIB NAMES gecodesearch)
find_library(GECODE_INT_LIB NAMES gecodeint)
find_library(GECODE_SET_LIB NAMES gecodeset)
find_library(GECODE_FLOAT_LIB NAMES gecodefloat)
find_library(GECODE_MINIMODEL_LIB NAMES gecodeminimodel)
find_library(GECODE_DRIVER_LIB NAMES gecodedriver)
find_library(GECODE_FLATZINC_LIB NAMES gecodeflatzinc)

set(GECODE_LIBRARIES
  ${GECODE_SUPPORT_LIB}
  ${GECODE_KERNEL_LIB}
  ${GECODE_SEARCH_LIB}
  ${GECODE_INT_LIB}
  ${GECODE_SET_LIB}
  ${GECODE_FLOAT_LIB}
  ${GECODE_MINIMODEL_LIB}
  ${GECODE_DRIVER_LIB}
  ${GECODE_FLATZINC_LIB}
)

set(GECODE_LIBRARIES "${GECODE_LIBRARIES}" CACHE STRING "Gecode libraries")


include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GECODE DEFAULT_MSG
  GECODE_SUPPORT_LIB
  GECODE_KERNEL_LIB
  GECODE_SEARCH_LIB
  GECODE_INT_LIB
  GECODE_SET_LIB
  GECODE_FLOAT_LIB
  GECODE_MINIMODEL_LIB
  GECODE_DRIVER_LIB
  GECODE_FLATZINC_LIB
  )

mark_as_advanced(
  GECODE_SUPPORT_LIB
  GECODE_KERNEL_LIB
  GECODE_SEARCH_LIB
  GECODE_INT_LIB
  GECODE_SET_LIB
  GECODE_FLOAT_LIB
  GECODE_MINIMODEL_LIB
  GECODE_DRIVER_LIB
  GECODE_FLATZINC_LIB
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
