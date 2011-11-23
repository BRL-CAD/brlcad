#                   F I N D T B B . C M A K E
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
###########################################################################
# TBB (Intel Thread Building Blocks) setup

setup_path (TBB_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the TBB library install")
mark_as_advanced (TBB_HOME)
if (USE_TBB)
    set (TBB_VERSION 22_004oss)
    if (MSVC)
        find_library (TBB_LIBRARY
                      NAMES tbb
                      PATHS ${TBB_HOME}/lib
                      PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                      ${TBB_HOME}/tbb-${TBB_VERSION}/lib/
                     )
        find_library (TBB_DEBUG_LIBRARY
                      NAMES tbb_debug
                      PATHS ${TBB_HOME}/lib
                      PATHS ${THIRD_PARTY_TOOLS_HOME}/lib/
                      ${TBB_HOME}/tbb-${TBB_VERSION}/lib/)
    endif (MSVC)
    find_path (TBB_INCLUDES tbb/tbb_stddef.h
               ${TBB_HOME}/include/tbb${TBB_VERSION}
               ${THIRD_PARTY_TOOLS}/include/tbb${TBB_VERSION}
               ${PROJECT_SOURCE_DIR}/include
               ${OPENIMAGEIOHOME}/include/OpenImageIO
              )
    if (TBB_INCLUDES OR TBB_LIBRARY)
        set (TBB_FOUND TRUE)
        message (STATUS "TBB includes = ${TBB_INCLUDES}")
        message (STATUS "TBB library = ${TBB_LIBRARY}")
        add_definitions ("-DUSE_TBB=1")
    else ()
        message (STATUS "TBB not found")
    endif ()
else ()
    add_definitions ("-DUSE_TBB=0")
    message (STATUS "TBB will not be used")
    set(TBB_INCLUDES "")
    set(TBB_LIBRARY "")
endif ()

# end TBB setup
###########################################################################

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
