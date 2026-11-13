# T E M P _ C L E A N U P . C M A K E
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
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{TMPDIR} "${WORK}")
set(ENV{TEMP} "${WORK}")
set(ENV{TMP} "${WORK}")
set(sentinel "retain unrelated temporary files")
file(WRITE "${WORK}/unrelated.txt" "${sentinel}")
execute_process(COMMAND "${BU_TEST}" test_temp_filename auto-clean
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE output)
if(NOT "${result}" STREQUAL "0")
  message(FATAL_ERROR "Temporary-file child failed (${result}): ${output}")
endif()
file(GLOB retained "${WORK}/*")
if(NOT retained STREQUAL "${WORK}/unrelated.txt")
  message(FATAL_ERROR "Temporary files survived process exit: ${retained}")
endif()
file(READ "${WORK}/unrelated.txt" contents)
if(NOT contents STREQUAL sentinel)
  message(FATAL_ERROR "Process-exit cleanup changed an unrelated temporary file")
endif()
file(REMOVE_RECURSE "${WORK}")
