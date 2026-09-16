if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED REPORT OR NOT DEFINED OUTPUT OR NOT DEFINED WIDE_INPUT OR
   NOT DEFINED WIDE_REPORT)
  message(FATAL_ERROR
    "STEP_G, MGED, INPUT, REPORT, OUTPUT, WIDE_INPUT, and WIDE_REPORT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}" "${WIDE_INPUT}" "${WIDE_REPORT}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "permissive same-vertex spline import returned ${import_result}\n"
    "${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"inferred_curves\":1"
    "zero_length_topology_spur_removal"
    "\"edge_entity_id\":21"
    "removed an open spline spur from a zero-length STEP topology edge")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    brep Same_Vertex_Spline_Spur_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT brep_text MATCHES "faces:[ ]+1")
  message(FATAL_ERROR
    "same-vertex spline recovery did not produce a valid surface\n${brep_text}")
endif()

foreach(object Same_Vertex_Spline_Spur_item.s Same_Vertex_Spline_Spur_item)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" attr show "${object}"
    OUTPUT_VARIABLE attr_output
    ERROR_VARIABLE attr_error
  )
  set(attr_text "${attr_output}\n${attr_error}")
  foreach(expected
      "step:geometry_status[ ]+inferred"
      "step:inferred_curve_ids[ ]+21"
      "step:inferred_curve_kinds[ ]+21=zero_length_topology_spur_removal"
      "step:inferred_curve_details.*neighboring-feature limit.*item-scale limit")
    if(NOT attr_text MATCHES "${expected}")
      message(FATAL_ERROR
        "${object} inference provenance is missing ${expected}\n${attr_text}")
    endif()
  endforeach()
endforeach()

# A large model cannot authorize an arbitrary one-vertex spur merely through
# its item bounding box.  Make the same source curve four times wider, beyond
# the fixture's item-scale ceiling; the inference transaction must be discarded
# and the ordinary invalid partial candidate preserved without inferred tags.
file(READ "${INPUT}" wide_text)
string(REPLACE "(0.005,0.,0.)" "(0.02,0.,0.)" wide_text "${wide_text}")
string(REPLACE "(0.003333333333333,0.,0.)" "(0.013333333333333,0.,0.)"
  wide_text "${wide_text}")
string(REPLACE "(0.001666666666667,0.,0.)" "(0.006666666666667,0.,0.)"
  wide_text "${wide_text}")
file(WRITE "${WIDE_INPUT}" "${wide_text}")
execute_process(
  COMMAND "${STEP_G}" -D --report "${WIDE_REPORT}" "${WIDE_INPUT}"
  RESULT_VARIABLE wide_result
  OUTPUT_VARIABLE wide_output
  ERROR_VARIABLE wide_error
)
if(NOT wide_result EQUAL 1)
  message(FATAL_ERROR
    "out-of-bounds one-vertex spur returned ${wide_result}, expected a "
    "preserved invalid candidate\n${wide_output}\n${wide_error}")
endif()
file(READ "${WIDE_REPORT}" wide_report_text)
foreach(expected
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "\"invalid_breps\":1,\"invalid_breps_written\":1"
    "\"inferred_curves\":0")
  string(FIND "${wide_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "out-of-bounds report does not contain ${expected}:\n${wide_report_text}")
  endif()
endforeach()
if(wide_report_text MATCHES "accepted permissive zero_length_topology_spur")
  message(FATAL_ERROR
    "out-of-bounds one-vertex spur was incorrectly accepted as inferred geometry")
endif()
