#                R E S E T C A C H E . C M A K E
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
MACRO(RESET_CACHE_FILE)
	SET(CACHE_FILE "")
	IF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
		FILE(READ ${CMAKE_BINARY_DIR}/CMakeCache.txt CACHE_FILE)
		STRING(REGEX REPLACE ";" "-" ENT1 "${CACHE_FILE}")
		STRING(REGEX REPLACE "\r?\n" ";" ENT "${ENT1}")
		FOREACH(line ${ENT})
			IF(NOT ${line} MATCHES "^CMAKE_")
				IF(NOT ${line} MATCHES "^//")
					IF(${line} MATCHES "FILEPATH=")
						IF(${line} MATCHES "LIB")
							STRING(REGEX REPLACE ":.*" "" var ${line})
							SET(${var} NOTFOUND CACHE FILEPATH "reset" FORCE)
						ENDIF(${line} MATCHES "LIB")
					ENDIF(${line} MATCHES "FILEPATH=")
				ENDIF(NOT ${line} MATCHES "^//")
			ENDIF(NOT ${line} MATCHES "^CMAKE_")
		ENDFOREACH(line ${ENT})
	ENDIF(EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
ENDMACRO()


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
