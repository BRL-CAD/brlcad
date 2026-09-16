if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

set(reject_report "${REPORT}.reject")
set(strict_report "${REPORT}.strict")
set(strict_output "${OUTPUT}.strict")
file(REMOVE "${REPORT}" "${reject_report}" "${strict_report}" "${OUTPUT}" "${strict_output}")

execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 1)
  message(FATAL_ERROR "default invalid preservation returned ${import_result}:\n${import_output}${import_error}")
endif()
file(READ "${REPORT}" report_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"preserve\""
    "\"effective_invalid_brep_policy\":\"preserve\""
    "\"outcome\":\"partial\""
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"status\":\"preserved_invalid\""
    "\"preserved_invalid_items\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":1,\"invalid_breps_rejected\":0")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "preservation report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show Single_Winding_Cylinder_Boundary_item"
  OUTPUT_VARIABLE wrapper_attrs ERROR_VARIABLE wrapper_error)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show Single_Winding_Cylinder_Boundary_item.s"
  OUTPUT_VARIABLE primitive_attrs ERROR_VARIABLE primitive_error)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "get Single_Winding_Cylinder_Boundary_item"
  OUTPUT_VARIABLE wrapper_get ERROR_VARIABLE wrapper_get_error)
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "attr show Single_Winding_Cylinder_Boundary"
  OUTPUT_VARIABLE product_attrs ERROR_VARIABLE product_error)
set(attribute_text "${wrapper_attrs}${wrapper_error}${primitive_attrs}${primitive_error}")
set(wrapper_text "${wrapper_get}${wrapper_get_error}")
set(product_text "${product_attrs}${product_error}")
foreach(expected
    "step:geometry_status[ ]+invalid_preserved"
    "step:import_status[ ]+invalid"
    "step:source_validity[ ]+unresolved"
    "step:invalidity[ ]+opennurbs_structure"
    "step:invalid_reason[ ]+OpenNURBS structural validation failed")
  if(NOT attribute_text MATCHES "${expected}")
    message(FATAL_ERROR "preserved object attributes omit ${expected}:\n${attribute_text}")
  endif()
endforeach()
if(NOT wrapper_text MATCHES "comb region no" OR
   NOT product_text MATCHES "step:import_status[ ]+invalid" OR
   NOT product_text MATCHES "step:invalid_geometry_count[ ]+1")
  message(FATAL_ERROR "invalid wrapper/product semantics are wrong:\n${wrapper_get}${wrapper_get_error}${product_attrs}${product_error}")
endif()

execute_process(
  COMMAND "${STEP_G}" -D --reject-invalid-objs --report "${reject_report}" "${INPUT}"
  RESULT_VARIABLE reject_result OUTPUT_VARIABLE reject_output ERROR_VARIABLE reject_error)
if(NOT reject_result EQUAL 3)
  message(FATAL_ERROR "explicit rejection returned ${reject_result}, expected 3:\n${reject_output}${reject_error}")
endif()
file(READ "${reject_report}" reject_text)
foreach(expected
    "\"requested_invalid_brep_policy\":\"reject\""
    "\"effective_invalid_brep_policy\":\"reject\""
    "\"geometry_attempted\":1,\"geometry_written\":0,\"geometry_skipped\":1"
    "\"invalid_breps\":1,\"invalid_breps_written\":0,\"invalid_breps_rejected\":1"
    "\"reason\":\"OpenNURBS structural validation failed\"")
  string(FIND "${reject_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "rejection report does not contain ${expected}:\n${reject_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" --strict -O "${strict_output}" --report "${strict_report}" "${INPUT}"
  RESULT_VARIABLE strict_result OUTPUT_VARIABLE strict_log ERROR_VARIABLE strict_error)
if(NOT strict_result EQUAL 3 OR EXISTS "${strict_output}")
  message(FATAL_ERROR "strict invalid rejection returned ${strict_result} or published output:\n${strict_log}${strict_error}")
endif()
file(READ "${strict_report}" strict_text)
if(NOT strict_text MATCHES "\"requested_invalid_brep_policy\":\"preserve\"" OR
   NOT strict_text MATCHES "\"effective_invalid_brep_policy\":\"reject\"")
  message(FATAL_ERROR "strict report policy is wrong:\n${strict_text}")
endif()
