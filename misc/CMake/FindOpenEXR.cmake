#               F I N D O P E N E X R . C M A K E
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
# IlmBase and OpenEXR setup

# TODO: Place the OpenEXR stuff into a separate FindOpenEXR.cmake module.

# example of using setup_var instead:
#setup_var (ILMBASE_VERSION 1.0.1 "Version of the ILMBase library")
setup_string (ILMBASE_VERSION 1.0.1
              "Version of the ILMBase library")
mark_as_advanced (ILMBASE_VERSION)
setup_path (ILMBASE_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the ILMBase library install")
mark_as_advanced (ILMBASE_HOME)
find_path (ILMBASE_INCLUDE_AREA OpenEXR/half.h
           ${ILMBASE_HOME}/include/ilmbase-${ILMBASE_VERSION}
           ${ILMBASE_HOME}/include
          )
if (ILMBASE_CUSTOM)
    set (IlmBase_Libraries ${ILMBASE_CUSTOM_LIBRARIES})
    separate_arguments(IlmBase_Libraries)
else ()
    set (IlmBase_Libraries Imath Half IlmThread Iex)
endif ()

foreach (_lib ${IlmBase_Libraries})
    find_library (current_lib ${_lib}
                  PATHS ${ILMBASE_HOME}/lib ${ILMBASE_HOME}/lib64
                        ${ILMBASE_LIB_AREA}
                  )
    list(APPEND ILMBASE_LIBRARIES ${current_lib})
    # this effectively unsets ${current_lib} so that find_library()
    # won't use the previous search results from the cache
    set (current_lib current_lib-NOTFOUND)
endforeach ()

message (STATUS "ILMBASE_INCLUDE_AREA = ${ILMBASE_INCLUDE_AREA}")
message (STATUS "ILMBASE_LIBRARIES = ${ILMBASE_LIBRARIES}")

if (ILMBASE_INCLUDE_AREA AND ILMBASE_LIBRARIES)
    set (ILMBASE_FOUND true)
    include_directories ("${ILMBASE_INCLUDE_AREA}")
    include_directories ("${ILMBASE_INCLUDE_AREA}/OpenEXR")
else ()
    message (FATAL_ERROR "ILMBASE not found!")
endif ()

macro (LINK_ILMBASE target)
    target_link_libraries (${target} ${ILMBASE_LIBRARIES})
endmacro ()

setup_string (OPENEXR_VERSION 1.6.1 "OpenEXR version number")
setup_string (OPENEXR_VERSION_DIGITS 010601 "OpenEXR version preprocessor number")
mark_as_advanced (OPENEXR_VERSION)
mark_as_advanced (OPENEXR_VERSION_DIGITS)
# FIXME -- should instead do the search & replace automatically, like this
# way it was done in the old makefiles:
#     OPENEXR_VERSION_DIGITS ?= 0$(subst .,0,${OPENEXR_VERSION})
setup_path (OPENEXR_HOME "${THIRD_PARTY_TOOLS_HOME}"
            "Location of the OpenEXR library install")
mark_as_advanced (OPENEXR_HOME)

find_path (OPENEXR_INCLUDE_AREA OpenEXR/OpenEXRConfig.h
           ${OPENEXR_HOME}/include
           ${ILMBASE_HOME}/include/openexr-${OPENEXR_VERSION}
          )
if (OPENEXR_CUSTOM)
    find_library (OPENEXR_LIBRARY ${OPENEXR_CUSTOM_LIBRARY}
                  PATHS ${OPENEXR_HOME}/lib
                        ${OPENEXR_HOME}/lib64
                        ${OPENEXR_LIB_AREA}
                 )
else ()
    find_library (OPENEXR_LIBRARY IlmImf
                  PATHS ${OPENEXR_HOME}/lib
                        ${OPENEXR_HOME}/lib64
                        ${OPENEXR_LIB_AREA}
                 )
endif ()

message (STATUS "OPENEXR_INCLUDE_AREA = ${OPENEXR_INCLUDE_AREA}")
message (STATUS "OPENEXR_LIBRARY = ${OPENEXR_LIBRARY}")
if (OPENEXR_INCLUDE_AREA AND OPENEXR_LIBRARY)
    set (OPENEXR_FOUND true)
    include_directories (${OPENEXR_INCLUDE_AREA})
    include_directories (${OPENEXR_INCLUDE_AREA}/OpenEXR)
else ()
    message (STATUS "OPENEXR not found!")
endif ()
add_definitions ("-DOPENEXR_VERSION=${OPENEXR_VERSION_DIGITS}")
find_package (ZLIB)
macro (LINK_OPENEXR target)
    target_link_libraries (${target} ${OPENEXR_LIBRARY} ${ZLIB_LIBRARIES})
endmacro ()


# end IlmBase and OpenEXR setup

# Local Variables:
# tab-width: 8
# mode: sh
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
