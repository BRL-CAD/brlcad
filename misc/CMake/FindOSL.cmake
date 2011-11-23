#                   F I N D O S L . C M A K E
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
# ================================================
# Find OSL Dependences
# ================================================
include(util_macros)
include(FindOpenEXR)
include(FindTBB)
include(FindOIIO)

# ================================================
# Find OSL
# ================================================

# If 'OSL' not set, use the env variable of that name if available
if (NOT OSLHOME)
    if (NOT $ENV{OSLHOME} STREQUAL "")
        set (OSLHOME $ENV{OSLHOME})
    endif ()
endif ()

message("OSL_HOME = ${OSLHOME}")

# Find OSL library and its dependencies
FIND_LIBRARY(OSLEXEC_LIBRARY 
	NAMES oslexec
	PATHS ${OSLHOME}/lib)
FIND_LIBRARY(OSLCOMP_LIBRARY 
	NAMES oslcomp
	PATHS ${OSLHOME}/lib)
FIND_LIBRARY(OSLQUERY_LIBRARY 
	NAMES oslquery
	PATHS ${OSLHOME}/lib)

FIND_PATH (OSL_INCLUDES
	NAMES oslexec.h
	PATHS ${OSLHOME}/include/OSL)

if (OSLEXEC_LIBRARY AND OSLCOMP_LIBRARY AND OSLQUERY_LIBRARY AND OSL_INCLUDES)
   message("Found OSL")
   message("OSL EXEC = ${OSLEXEC_LIBRARY}")
   message("OSL COMP = ${OSLCOMP_LIBRARY}")
   message("OSL QUERY = ${OSLQUERY_LIBRARY}")
   message("OSL INCLUDES = ${OSL_INCLUDES}")
endif()





# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
