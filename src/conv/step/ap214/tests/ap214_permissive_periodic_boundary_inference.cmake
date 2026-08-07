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
    "permissive periodic-boundary import returned ${import_result}\n"
    "${import_output}\n${import_error}")
endif()

# The authored spline is a continuous edge between the asserted vertices, but
# it departs from the exact periodic NURBS boundary by 1 mm.  That exceeds the
# 0.233238 mm bounded safe-repair limit while remaining below the explicit
# 1.73205 mm feature-and-item-bounded inference ceiling.  Default permissive
# mode must
# therefore construct the topology-coherent exact boundary, tag the inference,
# and still produce a structurally valid BREP.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"inferred_curves\":1"
    "\"brep_construction_pullback\":{\"calls\":2"
    "topology_coherent_surface_boundary_pcurve"
    "\"discrepancy_mm\":1"
    "\"safe_limit_mm\":0.233238"
    "\"inference_limit_mm\":1.73205"
    "accepted permissive topology_coherent_surface_boundary_pcurve inference only after the rebuilt complete BREP passed validation")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep
    Permissive_Periodic_Boundary_Inference_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR
    "periodic-boundary inference did not produce a valid open BREP\n"
    "${brep_text}")
endif()

foreach(object
    Permissive_Periodic_Boundary_Inference_item.s
    Permissive_Periodic_Boundary_Inference_item)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" attr show "${object}"
    OUTPUT_VARIABLE attr_output
    ERROR_VARIABLE attr_error
  )
  set(attr_text "${attr_output}\n${attr_error}")
  foreach(expected
      "step:geometry_status[ ]+inferred"
      "step:inferred_curve_ids[ ]+41"
      "step:inferred_curve_kinds[ ]+41=topology_coherent_surface_boundary_pcurve"
      "step:inferred_curve_details.*discrepancy_mm=1"
      "step:inferred_curve_details.*safe_limit_mm=0.233238"
      "step:inferred_curve_details.*inference_limit_mm=1.73205")
    if(NOT attr_text MATCHES "${expected}")
      message(FATAL_ERROR
        "${object} inference provenance is missing ${expected}\n${attr_text}")
    endif()
  endforeach()
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
      "${policy_name} unexpectedly accepted inferred geometry "
      "(status ${policy_result})\n${policy_output}\n${policy_error}")
  endif()
  file(READ "${policy_report}" policy_report_text)
  foreach(expected
      "\"geometry_written\":0"
      "\"geometry_skipped\":1"
      "\"inferred_curves\":0"
      "source curve/surface separation 1 exceeds")
    string(FIND "${policy_report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report does not contain ${expected}:\n"
        "${policy_report_text}")
    endif()
  endforeach()
endfunction()

function(expect_invalid_preservation policy_name)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --report "${policy_report}" "${INPUT}"
    RESULT_VARIABLE policy_result
    OUTPUT_VARIABLE policy_output
    ERROR_VARIABLE policy_error
  )
  # Exact/no-repair modes forbid inference, but the default invalid-object
  # policy remains permissive.  Preserve and tag the partial BREP for
  # inspection; strict/reject-invalid policies below must omit it.
  if(NOT policy_result EQUAL 1)
    message(FATAL_ERROR
      "${policy_name} did not report preserved partial output "
      "(status ${policy_result})\n${policy_output}\n${policy_error}")
  endif()
  file(READ "${policy_report}" policy_report_text)
  foreach(expected
      "\"geometry_written\":1"
      "\"geometry_skipped\":0"
      "\"invalid_breps\":1"
      "\"invalid_breps_written\":1"
      "\"inferred_curves\":0")
    string(FIND "${policy_report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report does not contain ${expected}:\n"
        "${policy_report_text}")
    endif()
  endforeach()
endfunction()

expect_invalid_preservation(exact --exact)
expect_inference_rejection(strict --strict)
expect_invalid_preservation(repair_none --repair none)
expect_inference_rejection(reject_invalid --reject-invalid-objs)
