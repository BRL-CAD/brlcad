# Check that large property IDs produce complete, referenced object names.

file(MAKE_DIRECTORY "${TEST_DIR}")
set(database "${TEST_DIR}/large_pids.g")
set(ascii "${TEST_DIR}/large_pids.asc")
file(REMOVE "${database}" "${ascii}")

execute_process(
  COMMAND "${NASTRAN_G}" -i "${INPUT}" -o "${database}"
  RESULT_VARIABLE nastran_result
  OUTPUT_VARIABLE nastran_output
  ERROR_VARIABLE nastran_error
)
if(NOT nastran_result STREQUAL "0")
  message(FATAL_ERROR "nastran-g failed: ${nastran_result}\n${nastran_output}\n${nastran_error}")
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

file(READ "${ascii}" ascii_contents)
foreach(expected IN ITEMS
    "put {pshell.2147483647} "
    "put {pbar_group.2147483647} "
    "tree {l pshell.2147483647}"
    "tree {l pbar_group.2147483647}"
    "tree {l cbar.2}")
  string(FIND "${ascii_contents}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "Missing expected geometry: ${expected}")
  endif()
endforeach()
