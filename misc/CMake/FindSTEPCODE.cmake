#               F I N D S T E P C O D E . C M A K E
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
# The following variables are set:
#
#  STEPCODE_INCLUDE_DIRS   - where to find stepcode headers
#  STEPCODE_LIBRARIES      - List of libraries when using stepcode.
#  STEPCODE_FOUND          - True if stepcode found.


# A user may set ``STEPCODE_ROOT`` to a stepcode installation root to tell this
# module where to look.
# =============================================================================

# Check that each of the libraries has an expected header,
# as well as the toplevel defined headers. (TODO - are the
# latter needed beyond the stepcode build itself?  Trim this
# to the minimum actually needed...)
set(STEPCODE_HDRS
  base/sc_benchmark.h
  cldai/sdaiObject.h
  cleditor/STEPfile.h
  clstepcore/sdai.h
  clutils/gennodearray.h
  exppp/exppp.h
  express/express.h
  sc_cf.h
  )

set(STEPCODE_LIBS
  base
  express
  exppp
  stepcore
  stepeditor
  stepdai
  steputils
  )

set(STEPCODE_EXEC
  exp2cxx
  exppp
  )

# Search STEPCODE_ROOT first if it is set.
set(_STEPCODE_SEARCHES)
if(STEPCODE_ROOT)
  set(_STEPCODE_SEARCH_ROOT PATHS ${STEPCODE_ROOT} NO_DEFAULT_PATH)
  list(APPEND _STEPCODE_SEARCHES _STEPCODE_SEARCH_ROOT)
endif()

# Try each search configuration.
foreach(search ${_STEPCODE_SEARCHES})
  find_path(STEPCODE_BASE_DIR NAMES sc_benchmark.h ${${search}} PATH_SUFFIXES include include/stepcode/base)
  find_path(STEPCODE_DAI_DIR NAMES sdaiObject.h ${${search}} PATH_SUFFIXES include include/stepcode/cldai)
  find_path(STEPCODE_EDITOR_DIR NAMES STEPfile.h ${${search}} PATH_SUFFIXES include include/stepcode/cleditor)
  find_path(STEPCODE_STEPCORE_DIR NAMES sdai.h ${${search}} PATH_SUFFIXES include include/stepcode/clstepcore)
  find_path(STEPCODE_UTILS_DIR NAMES gennodearray.h ${${search}} PATH_SUFFIXES include include/stepcode/clutils)
  find_path(STEPCODE_EXPPP_DIR NAMES exppp.h ${${search}} PATH_SUFFIXES include include/stepcode/exppp)
  find_path(STEPCODE_EXPRESS_DIR NAMES express.h ${${search}} PATH_SUFFIXES include include/stepcode/express)
  find_path(STEPCODE_INCLUDE_DIR NAMES sc_cf.h ${${search}} PATH_SUFFIXES include include/stepcode)
  #TODO - should be an all-or-nothing for the set...
endforeach()

# Allow STEPCODE_LIBRARY to be set manually, as the location of the netpbm library
foreach(search ${_STEPCODE_SEARCHES})
  find_library(STEPCODE_BASE_LIBRARY NAMES base NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_EXPRESS_LIBRARY NAMES express NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_EXPPP_LIBRARY NAMES exppp NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_CORE_LIBRARY NAMES stepcore NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_EDITOR_LIBRARY NAMES stepeditor NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_DAI_LIBRARY NAMES stepdai NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  find_library(STEPCODE_UTILS_LIBRARY NAMES steputils NAMES_PER_DIR ${${search}} PATH_SUFFIXES lib)
  #TODO - should be an all-or-nothing for the set...
endforeach()

# Allow STEPCODE_LIBRARY to be set manually, as the location of the netpbm library
foreach(search ${_STEPCODE_SEARCHES})
  find_program(EXP2CXX_EXECUTABLE exp2cxx ${${search}} PATH_SUFFIXES bin)
  find_program(EXPPP_EXECUTABLE exppp ${${search}} PATH_SUFFIXES bin)
  #TODO - should be an all-or-nothing for the set...
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(STEPCODE DEFAULT_MSG
  STEPCODE_BASE_DIR
  STEPCODE_DAI_DIR
  STEPCODE_EDITOR_DIR
  STEPCODE_STEPCORE_DIR
  STEPCODE_UTILS_DIR
  STEPCODE_EXPPP_DIR
  STEPCODE_EXPRESS_DIR
  STEPCODE_INCLUDE_DIR
  STEPCODE_BASE_LIBRARY
  STEPCODE_EXPRESS_LIBRARY
  STEPCODE_EXPPP_LIBRARY
  STEPCODE_CORE_LIBRARY
  STEPCODE_EDITOR_LIBRARY
  STEPCODE_DAI_LIBRARY
  STEPCODE_UTILS_LIBRARY
  EXP2CXX_EXECUTABLE
  EXPPP_EXECUTABLE
  )

if (STEPCODE_FOUND)
  set(STEPCODE_INCLUDE_DIRS
    ${STEPCODE_INCLUDE_DIR}
    ${STEPCODE_BASE_DIR}
    ${STEPCODE_STEPCORE_DIR}
    ${STEPCODE_EDITOR_DIR}
    ${STEPCODE_UTILS_DIR}
    ${STEPCODE_DAI_DIR}
    )
  set(STEPCODE_LIBRARIES
    ${STEPCODE_BASE_LIBRARY}
    ${STEPCODE_EXPRESS_LIBRARY}
    ${STEPCODE_EXPPP_LIBRARY}
    ${STEPCODE_CORE_LIBRARY}
    ${STEPCODE_EDITOR_LIBRARY}
    ${STEPCODE_DAI_LIBRARY}
    ${STEPCODE_UTILS_LIBRARY}
    )
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
