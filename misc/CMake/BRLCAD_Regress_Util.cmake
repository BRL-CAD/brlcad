#         B R L C A D _ R E G R E S S _ U T I L . C M A K E
# BRL-CAD
#
# Copyright (c) 2010-2021 United States Government as represented by
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

include(CMakeParseArguments)

function(ensearch efile efound)

  # set policies for the CBD setting behavior -
  # avoids warnings
  cmake_policy(SET CMP0011 NEW)
  cmake_policy(SET CMP0053 NEW)
  set(CBD "@CMAKE_BINARY_DIR@")
  if(NOT EXISTS "${CBD}")
    set(CBD "${CMAKE_CURRENT_BINARY_DIR}")
  endif(NOT EXISTS "${CBD}")

  cmake_parse_arguments(ENS "REQUIRED" "" "" ${ARGN})
  set(sdirs
    "${CBD}/bin"
    ../bin
    "${CBD}/../bin"
    ../bench
    "${CBD}/../bench")
  foreach(sd ${sdirs})
    if(EXISTS ${sd}/${efile})
      set(${efound} "${sd}/${efile}" PARENT_SCOPE)
      return()
    endif(EXISTS ${sd}/${efile})
  endforeach(sd ${sdirs})

  if(ENS_REQUIRED)
    message(FATAL_ERROR "Unable to find ${efile}, aborting")
  else(ENS_REQUIRED)
    message("WARNING: could not find ${efile}")
  endif(ENS_REQUIRED)
endfunction(ensearch efile efound)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

