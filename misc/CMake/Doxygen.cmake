#                   D O X Y G E N . C M A K E
# BRL-CAD
#
# Copyright (c) 2016-2021 United States Government as represented by
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
# CMake macros pertaining to Doxygen related functionality.


include(CMakeParseArguments)

define_property(GLOBAL PROPERTY DOXYGEN_FEATURES BRIEF_DOCS "Doxygen features" FULL_DOCS "All Doxygen features")

# This macro provides the core custom command and target wiring for most of the DocBook
# build targets.
macro(DOXYGEN_FEATURE label)
  if(${ARGC} GREATER 2)
    # Parse extra arguments
    CMAKE_PARSE_ARGUMENTS(FEATURE "" "" "DESCRIPTION;DIR" ${ARGN})
  endif(${ARGC} GREATER 2)
  if(FEATURE_DESCRIPTION)
    set(FEATURE_STRING "@par ${label}\n${FEATURE_DESCRIPTION}\n\n")
  else(FEATURE_DESCRIPTION)
    set(FEATURE_STRING "@par ${label}\n\n")
  endif(FEATURE_DESCRIPTION)
  set_property(GLOBAL APPEND PROPERTY DOXYGEN_FEATURES "${FEATURE_STRING}")
endmacro(DOXYGEN_FEATURE)

macro(DOXYGEN_FEATURE_SUMMARY filename)
  file(WRITE "${filename}" "/*! @page Features Features\n")
  get_property(DOXYGEN_FEATURE_LIST GLOBAL PROPERTY DOXYGEN_FEATURES)
  mark_as_advanced(DOXYGEN_FEATURE_LIST)
  if(DOXYGEN_FEATURE_LIST)
    list(REMOVE_DUPLICATES DOXYGEN_FEATURE_LIST)
  endif(DOXYGEN_FEATURE_LIST)
  foreach(feature ${DOXYGEN_FEATURE_LIST})
    file(APPEND "${filename}" "${feature}")
  endforeach(feature ${DOXYGEN_FEATURE_LIST})
  file(APPEND "${filename}" "*/\n")
endmacro(DOXYGEN_FEATURE_SUMMARY filename)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
