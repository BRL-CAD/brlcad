# Convert a mesh containing valid and malformed DXF faces and check the
# resulting BoT.  Negative indices mark hidden edges in DXF.

file(MAKE_DIRECTORY "${TEST_DIR}")
set(database "${TEST_DIR}/face_indices.g")
set(ascii "${TEST_DIR}/face_indices.asc")
file(REMOVE "${database}" "${ascii}")

execute_process(
  COMMAND "${DXF_G}" "${INPUT}" "${database}"
  RESULT_VARIABLE dxf_result
  OUTPUT_VARIABLE dxf_output
  ERROR_VARIABLE dxf_error
)
if(NOT dxf_result STREQUAL "0")
  message(FATAL_ERROR "dxf-g failed: ${dxf_result}\n${dxf_output}\n${dxf_error}")
endif()

execute_process(
  COMMAND "${G2ASC}" "${database}" "${ascii}"
  RESULT_VARIABLE asc_result
  OUTPUT_VARIABLE asc_output
  ERROR_VARIABLE asc_error
)
if(NOT asc_result STREQUAL "0")
  message(FATAL_ERROR "g2asc failed: ${asc_result}\n${asc_output}\n${asc_error}")
endif()

file(STRINGS "${ascii}" bot_lines REGEX "^put \\{bot\\.s[0-9]+\\} bot ")
list(LENGTH bot_lines bot_count)
if(NOT bot_count EQUAL 1)
  message(FATAL_ERROR "Expected one BoT, found ${bot_count}\n${bot_lines}")
endif()

list(GET bot_lines 0 bot_line)
string(FIND "${bot_line}" " F { { 0 1 2 } { 2 3 0 } { 0 2 3 }}" expected_faces)
if(expected_faces EQUAL -1)
  message(FATAL_ERROR "Malformed faces changed the output BoT:\n${bot_line}")
endif()
