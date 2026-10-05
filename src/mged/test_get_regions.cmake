if(NOT DEFINED MGED OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "MGED and TEST_DIR are required")
endif()

set(test_db "${TEST_DIR}/get-regions-dash.g")
file(REMOVE "${test_db}")

string(
  CONCAT mged_commands
         "in leaf.s sph 0 0 0 1; "
         "r leaf.r u leaf.s; "
         "db put -branch comb region no tree {l leaf.r}; "
         "db put top comb region no tree {l -branch}; "
         "get_regions top"
)

execute_process(
  COMMAND
    "${MGED}" -c "${test_db}"
    "${mged_commands}"
  OUTPUT_VARIABLE get_regions_output
  ERROR_VARIABLE get_regions_error
  RESULT_VARIABLE get_regions_result
  TIMEOUT 30
)

file(REMOVE "${test_db}")

if(NOT get_regions_result EQUAL 0)
  message(FATAL_ERROR "MGED get_regions command failed:\n${get_regions_output}\n${get_regions_error}")
endif()

set(get_regions_log "${get_regions_output}\n${get_regions_error}")
if(get_regions_log MATCHES "Unrecognized option|Usage:")
  message(FATAL_ERROR "MGED treated the leading-dash object name as an option:\n${get_regions_log}")
endif()
if(NOT get_regions_log MATCHES "leaf\\.r")
  message(FATAL_ERROR "MGED get_regions did not report the nested region:\n${get_regions_log}")
endif()
