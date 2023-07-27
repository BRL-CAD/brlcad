#                V E R S I O N S . C M A K E
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
# Synopsis:
#
# VERSIONS(fpath MAJOR MINOR PATCH)
#
#-----------------------------------------------------------------------------
function(VERSIONS fpath MAJOR MINOR PATCH)

  if (NOT EXISTS ${fpath})
    message("VERSIONS: ${fpath} not found")
    return()
  endif (NOT EXISTS ${fpath})

  file(STRINGS "${fpath}" VLINES)

  set(LMAJ 0)
  set(LMIN 0)
  set(LPAT 0)

  foreach(V ${VLINES})
    if ("${V}" MATCHES "^.[ ]*define")
      if ("${V}" MATCHES "MAJOR")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LMAJ "${V}")
	#message("MAJOR: ${LMAJ}")
      endif ("${V}" MATCHES "MAJOR")
      if ("${V}" MATCHES "MINOR")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LMIN "${V}")
	#message("MINOR: ${LMIN}")
      endif ("${V}" MATCHES "MINOR")
      if ("${V}" MATCHES "PATCH")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LPAT "${V}")
      endif ("${V}" MATCHES "PATCH")
      if ("${V}" MATCHES "REVISION")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LPAT "${V}")
      endif ("${V}" MATCHES "REVISION")
      if ("${V}" MATCHES "RELEASE")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LPAT "${V}")
      endif ("${V}" MATCHES "RELEASE")
      if ("${V}" MATCHES "REV")
	string(REGEX REPLACE "[^0-9]*([0-9.]+)[^0-9]*" "\\1" LPAT "${V}")
      endif ("${V}" MATCHES "REV")

      if ("${LMAJ}" GREATER 0 AND "${LMIN}" GREATER 0 AND "${LPAT}" GREATER 0)
	set(${MAJOR} "${LMAJ}" PARENT_SCOPE)
	set(${MINOR} "${LMIN}" PARENT_SCOPE)
	set(${PATCH} "${LPAT}" PARENT_SCOPE)
	#message("MAJOR: ${LMAJ}")
	#message("MINOR: ${LMIN}")
	#message("PATCH: ${LPAT}")
	return()
      endif ("${LMAJ}" GREATER 0 AND "${LMIN}" GREATER 0 AND "${LPAT}" GREATER 0)

    endif ("${V}" MATCHES "^.[ ]*define")
  endforeach(V ${VLINES})

endfunction(VERSIONS)


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
