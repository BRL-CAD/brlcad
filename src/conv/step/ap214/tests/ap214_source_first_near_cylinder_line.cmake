if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "permissive step-g returned ${import_result}\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"edge_entity_id\":8089"
    "\"kind\":\"topology_coherent_surface_boundary_pcurve\""
    "\"discrepancy_mm\":0.023"
    "\"inference_limit_mm\":0.077")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "source-first report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep
    Source_First_Near_Cylinder_Line_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1" OR
   NOT brep_text MATCHES "edges:[ ]+4")
  message(FATAL_ERROR
    "source-first near-cylinder face is not a valid open BREP\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show
    Source_First_Near_Cylinder_Line_item.s
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
foreach(expected
    "step:geometry_status[ ]+inferred"
    "step:inferred_curve_ids[ ]+8089 8134 8151"
    "step:inferred_curve_kinds[ ]+8089=topology_coherent_surface_boundary_pcurve"
    "step:inferred_curve_details.*#8089.*discrepancy_mm=0.023")
  if(NOT attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "source-first inference provenance is missing ${expected}\n${attr_text}")
  endif()
endforeach()

function(expect_inference_rejection policy_name)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --report "${policy_report}" "${INPUT}"
    RESULT_VARIABLE policy_result
    OUTPUT_VARIABLE policy_output
    ERROR_VARIABLE policy_error
  )
  if(NOT policy_result EQUAL 3)
    message(FATAL_ERROR
      "${policy_name} unexpectedly accepted inferred geometry (status ${policy_result})\n${policy_output}\n${policy_error}")
  endif()
  file(READ "${policy_report}" policy_report_text)
  foreach(expected
      "\"geometry_written\":0"
      "\"geometry_skipped\":1"
      "\"inferred_curves\":0")
    string(FIND "${policy_report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report does not contain ${expected}:\n${policy_report_text}")
    endif()
  endforeach()
endfunction()

expect_inference_rejection(exact --exact)
expect_inference_rejection(strict --strict)
expect_inference_rejection(reject_invalid --reject-invalid-objs)
