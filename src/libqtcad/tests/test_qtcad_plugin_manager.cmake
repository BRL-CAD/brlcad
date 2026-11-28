#           T E S T _ Q T C A D _ P L U G I N _ M A N A G E R . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.

message(STATUS "Plugin manager test executable: ${TEST_EXECUTABLE}")
message(STATUS "QTCAD_TEST_PLUGIN_DIR: $ENV{QTCAD_TEST_PLUGIN_DIR}")
message(STATUS "BRLCAD_ROOT: $ENV{BRLCAD_ROOT}")
message(STATUS "QT_QPA_PLATFORM: $ENV{QT_QPA_PLATFORM}")

# Report process status even when a missing DLL prevents main() from running.
execute_process(
  COMMAND "${TEST_EXECUTABLE}" -o -,txt -v1
  RESULT_VARIABLE test_result
)
if(NOT "${test_result}" STREQUAL "0")
  message(FATAL_ERROR "Plugin manager test failed: ${test_result}")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
