#                R E S E T C A C H E . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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
macro(RESET_CACHE_FILE)
  set(CACHE_FILE "")
  if(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
    file(READ ${CMAKE_BINARY_DIR}/CMakeCache.txt CACHE_FILE)
    string(REGEX REPLACE ";" "-" ENT1 "${CACHE_FILE}")
    string(REGEX REPLACE "\r?\n" ";" ENT "${ENT1}")
    foreach(line ${ENT})
      if(NOT ${line} MATCHES "^CMAKE_")
	if(NOT ${line} MATCHES "^//")
	  if(${line} MATCHES "FILEPATH=")
	    if(${line} MATCHES "LIB")
	      string(REGEX REPLACE ":.*" "" var ${line})
	      set(${var} NOTFOUND CACHE FILEPATH "reset" FORCE)
	    endif(${line} MATCHES "LIB")
	  endif(${line} MATCHES "FILEPATH=")
	endif(NOT ${line} MATCHES "^//")
      endif(NOT ${line} MATCHES "^CMAKE_")
    endforeach(line ${ENT})
  endif(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
endmacro()


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
