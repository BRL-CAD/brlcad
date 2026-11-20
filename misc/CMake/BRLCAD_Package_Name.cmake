#              B R L C A D _ P A C K A G E _ N A M E . C M A K E
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
# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
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

include_guard(GLOBAL)

function(brlcad_binary_package_suffix output_var source_dir)
  set(_suffix)
  find_package(Git QUIET)
  if(GIT_EXECUTABLE)
    set(
      _git_command
      "${CMAKE_COMMAND}" -E env
      GIT_TERMINAL_PROMPT=0
      GIT_ASKPASS=false
      SSH_ASKPASS=false
      --
      "${GIT_EXECUTABLE}"
      -c credential.helper=
      -c credential.interactive=false
    )
    execute_process(
      COMMAND ${_git_command} rev-parse --show-prefix
      WORKING_DIRECTORY "${source_dir}"
      RESULT_VARIABLE _checkout_result
      OUTPUT_VARIABLE _checkout_prefix
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_checkout_result EQUAL 0 AND _checkout_prefix STREQUAL "")
      execute_process(
        COMMAND ${_git_command} tag --points-at HEAD --list "rel-*"
        WORKING_DIRECTORY "${source_dir}"
        RESULT_VARIABLE _release_result
        OUTPUT_VARIABLE _release_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(_release_result EQUAL 0 AND _release_tags STREQUAL "")
        execute_process(
          COMMAND ${_git_command} rev-parse --short HEAD
          WORKING_DIRECTORY "${source_dir}"
          RESULT_VARIABLE _revision_result
          OUTPUT_VARIABLE _revision
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET
        )
        if(_revision_result EQUAL 0 AND _revision MATCHES "^[0-9a-fA-F]+$")
          set(_suffix "_${_revision}")
        endif()
      endif()
    endif()
  endif()
  set(${output_var} "${_suffix}" PARENT_SCOPE)
endfunction()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
