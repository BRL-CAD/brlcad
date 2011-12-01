#                 D I F F C A C H E . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
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
MACRO(DIFF_CACHE_FILE)
    SET(CACHE_FILE "")
    SET(INCREMENT_COUNT_FILE 0)
    SET(VAR_TYPES BOOL STRING FILEPATH PATH STATIC)
    IF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev)
	FILE(READ ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev CACHE_FILE)
	STRING(REPLACE ";" "semicolon" ENT1 "${CACHE_FILE}")
	STRING(REGEX REPLACE "\r?\n" ";" ENT "${ENT1}")
	FOREACH(line ${ENT})
	    IF(NOT ${line} MATCHES "^#")
		IF(NOT ${line} MATCHES "^//")
		    STRING(REGEX REPLACE ":.*" "" var ${line})
		    STRING(REGEX REPLACE ".*:" ":" val ${line})
		    FOREACH(type ${VAR_TYPES})
			IF(NOT "${val}" STREQUAL "")
			    STRING(REPLACE ":${type}=" "" val ${val})
			ENDIF(NOT "${val}" STREQUAL "")
		    ENDFOREACH(type ${VAR_TYPES})
		    IF(NOT ${line} MATCHES ":INTERNAL")
			STRING(REPLACE ";" "semicolon" currvar "${${var}}")
			IF(NOT "${currvar}" STREQUAL "${val}")
			    #MESSAGE("Found difference: ${var}")
			    #MESSAGE("Cache: ${val}")
			    #MESSAGE("Current: ${${var}}")
			    SET(INCREMENT_COUNT_FILE 1)
			ENDIF(NOT "${currvar}" STREQUAL "${val}")
		    ENDIF(NOT ${line} MATCHES ":INTERNAL")
		ENDIF(NOT ${line} MATCHES "^//")
	    ENDIF(NOT ${line} MATCHES "^#")
	ENDFOREACH(line ${ENT})
    ENDIF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt.prev)
ENDMACRO()


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
