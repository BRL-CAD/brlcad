#                 D I F F C A C H E . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

# The string UNINITIALIZED is appended to variable values
# supplied on the command line - this is a problem for the
# case where the variable has not been set before, since it
# appears to go straight into the cache and probably should
# trigger a diff, but there doesn't seem to be any way to
# recognize it was already set when CMake is re-run.  Also,
# repeated CMake runs with the same value still trigger the
# rebuilding if the "new" value is different from the original
# one written to the cachem file.  Even when the value hasn't
# been changed from the original setting, the "current" value
# of the UNINITIALIZED variable does not have the cache
# prefix.  Until we can find a way to recognize and handle
# this situation, don't let the UNINITIALIZED values be a
# reason for triggering a header update.

macro(DIFF_CACHE_FILE)
  set(CACHE_FILE "")
  set(INCREMENT_COUNT_FILE 0)
  set(VAR_TYPES BOOL STRING FILEPATH PATH STATIC)
  if(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev)
    file(READ ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev CACHE_FILE)
    string(REPLACE ";" "semicolon" ENT1 "${CACHE_FILE}")
    string(REGEX REPLACE "\r?\n" ";" ENT "${ENT1}")
    foreach(line ${ENT})
      if(NOT ${line} MATCHES "^#")
	if(NOT ${line} MATCHES "^//")
	  string(REGEX REPLACE ":.*" "" var ${line})
	  string(REGEX REPLACE ".*:" ":" val ${line})
	  foreach(type ${VAR_TYPES})
	    if(NOT "${val}" STREQUAL "")
	      string(REPLACE ":${type}=" "" val ${val})
	    endif(NOT "${val}" STREQUAL "")
	  endforeach(type ${VAR_TYPES})
	  if(val)
	    if("${val}" STREQUAL "' '")
	      set(val " ")
	    endif("${val}" STREQUAL "' '")
	  endif(val)
	  if(NOT ${line} MATCHES ":INTERNAL" AND NOT ${line} MATCHES ":UNINITIALIZED")
	    string(REPLACE ";" "semicolon" currvar "${${var}}")
	    if(NOT "${currvar}" STREQUAL "${val}")
	      #message("Found difference: ${var}")
	      #message("Cache: \"${val}\"")
	      #message("Current: \"${${var}}\"")
	      set(INCREMENT_COUNT_FILE 1)
	    endif(NOT "${currvar}" STREQUAL "${val}")
	  endif(NOT ${line} MATCHES ":INTERNAL" AND NOT ${line} MATCHES ":UNINITIALIZED")
	endif(NOT ${line} MATCHES "^//")
      endif(NOT ${line} MATCHES "^#")
    endforeach(line ${ENT})
  endif(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev)
endmacro()


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
