#                 F I N D R E G E X . C M A K E
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
# - Find regex 
# Find the native REGEX includes and library
#
#  REGEX_INCLUDE_DIR   - where to find regex.h, etc.
#  REGEX_LIBRARY      - List of libraries when using regex.
#  REGEX_FOUND          - True if regex found.
#
#=============================================================================

# Because at least one specific framework (Ruby) on OSX has been observed
# to include its own regex.h copy, check frameworks last - /usr/include
# is preferred to a package-specific copy for a generic regex search
SET(CMAKE_FIND_FRAMEWORK LAST)
FIND_PATH(REGEX_INCLUDE_DIR regex.h)

SET(REGEX_NAMES c regex compat)
FOREACH(rname ${REGEX_NAMES})
	IF(NOT REGEX_LIBRARY)
		CHECK_LIBRARY_EXISTS(${rname} regcomp "" HAVE_REGEX_LIB)
		IF(HAVE_REGEX_LIB)
			FIND_LIBRARY(REGEX_LIBRARY NAMES ${rname})
		ENDIF(HAVE_REGEX_LIB)
	ENDIF(NOT REGEX_LIBRARY)
ENDFOREACH(rname ${REGEX_NAMES})

MARK_AS_ADVANCED(REGEX_LIBRARY REGEX_INCLUDE_DIR)

# handle the QUIETLY and REQUIRED arguments and set REGEX_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(REGEX DEFAULT_MSG REGEX_INCLUDE_DIR REGEX_LIBRARY)


# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
