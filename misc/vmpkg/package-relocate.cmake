#                     P A C K A G E - R E L O C A T E . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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

# Native dependency installation embeds CPack's physical staging prefix in
# SDK metadata.  Tar users can extract anywhere, so resolve it from each file.
if(NOT IS_DIRECTORY "${CPACK_TEMPORARY_DIRECTORY}")
  message(FATAL_ERROR "CPack staging directory is unavailable")
endif()

function(relocate_package_configs extension anchor)
  string(REGEX REPLACE "/+$" "" package_prefix "${CPACK_TEMPORARY_DIRECTORY}")
  file(GLOB_RECURSE configs "${package_prefix}/*.${extension}")
  foreach(config IN LISTS configs)
    get_filename_component(directory "${config}" DIRECTORY)
    file(RELATIVE_PATH relative_prefix "${directory}" "${package_prefix}")
    string(REGEX REPLACE "/+$" "" relative_prefix "${relative_prefix}")
    set(relocated_prefix "${anchor}")
    if(relative_prefix)
      string(APPEND relocated_prefix "/${relative_prefix}")
    endif()
    file(READ "${config}" original)
    string(REPLACE "${package_prefix}" "${relocated_prefix}" relocated "${original}")
    if(NOT relocated STREQUAL original)
      file(WRITE "${config}" "${relocated}")
      message(STATUS "Relocated SDK metadata: ${config}")
    endif()
  endforeach()
endfunction()

relocate_package_configs(cmake "\${CMAKE_CURRENT_LIST_DIR}")
relocate_package_configs(pc "\${pcfiledir}")
