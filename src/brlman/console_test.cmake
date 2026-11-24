#                   C O N S O L E _ T E S T . C M A K E
# BRL-CAD
#
# Copyright (c) 2025-2026 United States Government as represented by
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
# Dumping a page must be usable by pipes and embedded command consoles.
set(viewer "${BRLMAN}" -L en)
execute_process(COMMAND ${viewer} --dump --width 40 brlman
  RESULT_VARIABLE status OUTPUT_VARIABLE explicit_text ERROR_VARIABLE errors)
if(NOT status EQUAL 0 OR NOT errors STREQUAL "" OR NOT explicit_text MATCHES "SYNOPSIS")
  message(FATAL_ERROR "Explicit dump failed: ${status} ${errors}")
endif()
foreach(arguments "--dump;--width;40;--;brlman" "brlman;--dump;--width;40")
  execute_process(COMMAND ${viewer} ${arguments}
    RESULT_VARIABLE status OUTPUT_VARIABLE marker_text ERROR_VARIABLE errors)
  if(NOT status EQUAL 0 OR NOT marker_text STREQUAL explicit_text)
    message(FATAL_ERROR "Manual option ordering or end marker failed: ${status} ${errors}")
  endif()
endforeach()
execute_process(COMMAND ${viewer} --width 40 brlman
  RESULT_VARIABLE status OUTPUT_VARIABLE captured_text ERROR_VARIABLE errors)
if(NOT status EQUAL 0 OR NOT captured_text STREQUAL explicit_text)
  message(FATAL_ERROR "Captured output did not use plain HTML formatting: ${errors}")
endif()
execute_process(COMMAND ${viewer} --dump brlman
  RESULT_VARIABLE status OUTPUT_VARIABLE default_text ERROR_VARIABLE errors)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "Default-width dump failed: ${errors}")
endif()
execute_process(COMMAND ${viewer} brlman
  RESULT_VARIABLE status OUTPUT_VARIABLE automatic_text ERROR_VARIABLE errors)
if(NOT status EQUAL 0 OR NOT automatic_text STREQUAL default_text)
  message(FATAL_ERROR "Redirected output did not automatically select dump mode: ${errors}")
endif()
string(ASCII 27 escape)
string(ASCII 8 backspace)
if(explicit_text MATCHES "${escape}|${backspace}|<style|<html")
  message(FATAL_ERROR "Dump contains markup or terminal controls")
endif()
foreach(width 0 -1 invalid)
  execute_process(COMMAND ${viewer} --dump --width "${width}" brlman
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
  if(status EQUAL 0 OR NOT output STREQUAL "")
    message(FATAL_ERROR "Invalid width accepted: ${width}")
  endif()
endforeach()
execute_process(COMMAND ${viewer} --gui --dump brlman
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(status EQUAL 0)
  message(FATAL_ERROR "Conflicting output modes accepted")
endif()
execute_process(COMMAND ${viewer} --dump missing-manual-page-for-test
  RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(status EQUAL 0 OR NOT output STREQUAL "" OR errors STREQUAL "")
  message(FATAL_ERROR "Missing page did not report a useful error")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
