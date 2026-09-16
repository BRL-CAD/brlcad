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
    "\"repairs\":0"
    "\"inferred_curves\":1"
    "\"brep_construction_pullback\":{\"calls\":2"
    "topology_vertex_bridge_3d_curve"
    "\"inference_limit_mm\":12.5"
    "inserted a topology-anchored segment")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep
    Permissive_Inferred_Curve_item.s info
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT brep_text MATCHES "faces:[ ]+4")
  message(FATAL_ERROR
    "permissive inferred curve did not produce a valid solid\n${brep_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show
    Permissive_Inferred_Curve_item.s
  OUTPUT_VARIABLE attr_output
  ERROR_VARIABLE attr_error
)
set(attr_text "${attr_output}\n${attr_error}")
foreach(expected
    "step:geometry_status[ ]+inferred"
    "step:inferred_curve_ids[ ]+30"
    "step:inferred_curve_kinds[ ]+30=topology_vertex_bridge_3d_curve"
    "step:inferred_curve_details.*inference_limit_mm=12.5")
  if(NOT attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "inference provenance is missing ${expected}\n${attr_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" attr show
    Permissive_Inferred_Curve_item
  OUTPUT_VARIABLE combination_attr_output
  ERROR_VARIABLE combination_attr_error
)
set(combination_attr_text
  "${combination_attr_output}\n${combination_attr_error}")
foreach(expected
    "step:geometry_status[ ]+inferred"
    "step:inferred_curve_ids[ ]+30"
    "step:inferred_curve_kinds[ ]+30=topology_vertex_bridge_3d_curve"
    "step:inferred_curve_details.*inference_limit_mm=12.5")
  if(NOT combination_attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "combination inference provenance is missing ${expected}\n${combination_attr_text}")
  endif()
endforeach()

# Reverse the source spline parameterization while preserving the same STEP
# topology.  The vertex-selected interval is decreasing and must use the same
# bounded bridge proof as the increasing form.
set(reversed_input "${REPORT}.reversed.stp")
set(reversed_report "${REPORT}.reversed.json")
set(reversed_output "${OUTPUT}.reversed.g")
file(READ "${INPUT}" reversed_text)
string(REPLACE
  "#28=B_SPLINE_CURVE_WITH_KNOTS('',1,(#9,#2),"
  "#28=B_SPLINE_CURVE_WITH_KNOTS('',1,(#2,#9),"
  reversed_text "${reversed_text}")
file(WRITE "${reversed_input}" "${reversed_text}")
file(REMOVE "${reversed_report}" "${reversed_output}")
execute_process(
  COMMAND "${STEP_G}" -O "${reversed_output}"
    --report "${reversed_report}" "${reversed_input}"
  RESULT_VARIABLE reversed_result
  OUTPUT_VARIABLE reversed_output_text
  ERROR_VARIABLE reversed_error
)
if(NOT reversed_result EQUAL 0)
  message(FATAL_ERROR
    "reversed inferred curve returned ${reversed_result}\n${reversed_output_text}\n${reversed_error}")
endif()
file(READ "${reversed_report}" reversed_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "topology_vertex_bridge_3d_curve")
  string(FIND "${reversed_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "reversed report does not contain ${expected}:\n${reversed_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${reversed_output}" brep
    Permissive_Inferred_Curve_item.s info
  OUTPUT_VARIABLE reversed_brep_output
  ERROR_VARIABLE reversed_brep_error
)
set(reversed_brep_text "${reversed_brep_output}\n${reversed_brep_error}")
if(NOT reversed_brep_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR
    "reversed inferred curve did not produce a valid solid\n${reversed_brep_text}")
endif()

# Exercise the terminal-fragment form preserved by the terminal-fragment fixture: both topology vertices
# project to the same end of a short source spline, and the retained fragment
# extends slightly beyond that exact endpoint.  Generate this ownable variant
# from the tetrahedron fixture so its only difference is the source fragment.
set(same_parameter_input "${REPORT}.same_parameter.stp")
set(same_parameter_report "${REPORT}.same_parameter.json")
set(same_parameter_output "${OUTPUT}.same_parameter.g")
file(READ "${INPUT}" same_parameter_text)
string(REPLACE
  "#9=CARTESIAN_POINT('',(9.,0.,0.));"
  "#9=CARTESIAN_POINT('',(10.5,0.,0.));"
  same_parameter_text "${same_parameter_text}")
file(WRITE "${same_parameter_input}" "${same_parameter_text}")
file(REMOVE "${same_parameter_report}" "${same_parameter_output}")
execute_process(
  COMMAND "${STEP_G}" -O "${same_parameter_output}"
    --report "${same_parameter_report}" "${same_parameter_input}"
  RESULT_VARIABLE same_parameter_result
  OUTPUT_VARIABLE same_parameter_output_text
  ERROR_VARIABLE same_parameter_error
)
if(NOT same_parameter_result EQUAL 0)
  message(FATAL_ERROR
    "same-parameter terminal fragment returned ${same_parameter_result}\n${same_parameter_output_text}\n${same_parameter_error}")
endif()
file(READ "${same_parameter_report}" same_parameter_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "topology_vertex_bridge_3d_curve"
    "both topology vertices selected one source parameter"
    "\"inference_limit_mm\":12.5")
  string(FIND "${same_parameter_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "same-parameter report does not contain ${expected}:\n${same_parameter_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${same_parameter_output}" brep
    Permissive_Inferred_Curve_item.s info
  OUTPUT_VARIABLE same_parameter_brep_output
  ERROR_VARIABLE same_parameter_brep_error
)
set(same_parameter_brep_text
  "${same_parameter_brep_output}\n${same_parameter_brep_error}")
if(NOT same_parameter_brep_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR
    "same-parameter terminal fragment did not produce a valid solid\n${same_parameter_brep_text}")
endif()

# Split tetrahedron edge #30 at a second topology vertex only 1e-7 mm from
# its start while retaining one complete 10 mm source spline for both pieces.
# The tiny piece has a usable vertex-selected interval, so this specifically
# exercises dense collapse of that bounded interval rather than collapse of
# the unrelated complete source curve.
set(bounded_collapse_input "${REPORT}.bounded_collapse.stp")
set(bounded_collapse_report "${REPORT}.bounded_collapse.json")
set(bounded_collapse_output "${OUTPUT}.bounded_collapse.g")
file(READ "${INPUT}" bounded_collapse_text)
string(REPLACE
  "#9=CARTESIAN_POINT('',(9.,0.,0.));"
  "#9=CARTESIAN_POINT('',(0.,0.,0.));\n#12=CARTESIAN_POINT('',(0.0000001,0.,0.));\n#29=VERTEX_POINT('',#12);"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#30=EDGE_CURVE('',#5,#6,#28,.T.);"
  "#30=EDGE_CURVE('',#5,#29,#28,.T.);"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#35=EDGE_CURVE('',#7,#8,#27,.T.);"
  "#35=EDGE_CURVE('',#7,#8,#27,.T.);\n#36=EDGE_CURVE('',#29,#6,#28,.T.);"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#40=ORIENTED_EDGE('',*,*,#31,.T.);"
  "#39=ORIENTED_EDGE('',*,*,#36,.F.);\n#40=ORIENTED_EDGE('',*,*,#31,.T.);"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#43=EDGE_LOOP('',(#40,#41,#42));"
  "#43=EDGE_LOOP('',(#40,#41,#39,#42));"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#63=EDGE_LOOP('',(#60,#61,#62));"
  "#69=ORIENTED_EDGE('',*,*,#36,.T.);\n#63=EDGE_LOOP('',(#60,#69,#61,#62));"
  bounded_collapse_text "${bounded_collapse_text}")
string(REPLACE
  "#48=ADVANCED_FACE('',(#44),#47,.T.);"
  "#48=ADVANCED_FACE('',(#44),#47,.F.);"
  bounded_collapse_text "${bounded_collapse_text}")
file(WRITE "${bounded_collapse_input}" "${bounded_collapse_text}")
file(REMOVE "${bounded_collapse_report}" "${bounded_collapse_output}")
execute_process(
  COMMAND "${STEP_G}" -O "${bounded_collapse_output}"
    --report "${bounded_collapse_report}" "${bounded_collapse_input}"
  RESULT_VARIABLE bounded_collapse_result
  OUTPUT_VARIABLE bounded_collapse_output_text
  ERROR_VARIABLE bounded_collapse_error
)
if(NOT bounded_collapse_result EQUAL 0)
  message(FATAL_ERROR
    "bounded-interval collapse returned ${bounded_collapse_result}\n${bounded_collapse_output_text}\n${bounded_collapse_error}")
endif()
file(READ "${bounded_collapse_report}" bounded_collapse_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "removed a densely validated sub-tolerance STEP edge and merged its coincident topology vertices")
  string(FIND "${bounded_collapse_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "bounded-collapse report does not contain ${expected}:\n${bounded_collapse_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${bounded_collapse_output}" brep
    Permissive_Inferred_Curve_item.s info
  OUTPUT_VARIABLE bounded_collapse_brep_output
  ERROR_VARIABLE bounded_collapse_brep_error
)
set(bounded_collapse_brep_text
  "${bounded_collapse_brep_output}\n${bounded_collapse_brep_error}")
if(NOT bounded_collapse_brep_text MATCHES "Valid: YES, Solid: YES")
  message(FATAL_ERROR
    "bounded-interval collapse did not produce a valid solid\n${bounded_collapse_brep_text}")
endif()

# Remove two tetrahedron faces and their reciprocal source-edge uses.  The
# remaining non-planar four-edge boundary is therefore provably authored rather
# than an importer omission, but does not determine a unique replacement
# surface.  Inference still repairs edge #30; permissive mode must preserve a
# tagged open BREP rather than report it as a solid or omit it.
set(authored_open_input "${REPORT}.authored_open.stp")
set(authored_open_report "${REPORT}.authored_open.json")
set(authored_open_output "${OUTPUT}.authored_open.g")
file(READ "${INPUT}" authored_open_text)
string(REPLACE
  "#60=ORIENTED_EDGE('',*,*,#30,.T.);"
  "#60=ORIENTED_EDGE('',*,*,#31,.T.);"
  authored_open_text "${authored_open_text}")
string(REPLACE
  "#62=ORIENTED_EDGE('',*,*,#32,.F.);"
  "#62=ORIENTED_EDGE('',*,*,#31,.F.);"
  authored_open_text "${authored_open_text}")
string(REPLACE
  "#70=ORIENTED_EDGE('',*,*,#33,.T.);"
  "#70=ORIENTED_EDGE('',*,*,#31,.T.);"
  authored_open_text "${authored_open_text}")
string(REPLACE
  "#71=ORIENTED_EDGE('',*,*,#35,.T.);"
  "#71=ORIENTED_EDGE('',*,*,#31,.T.);"
  authored_open_text "${authored_open_text}")
string(REPLACE
  "#90=CLOSED_SHELL('',(#48,#58,#68,#79));"
  "#90=CLOSED_SHELL('',(#48,#58));"
  authored_open_text "${authored_open_text}")
file(WRITE "${authored_open_input}" "${authored_open_text}")
file(REMOVE "${authored_open_report}" "${authored_open_output}")
execute_process(
  COMMAND "${STEP_G}" -O "${authored_open_output}"
    --report "${authored_open_report}" "${authored_open_input}"
  RESULT_VARIABLE authored_open_result
  OUTPUT_VARIABLE authored_open_output_text
  ERROR_VARIABLE authored_open_error
)
if(NOT authored_open_result EQUAL 1)
  message(FATAL_ERROR
    "authored-open inference returned ${authored_open_result}, expected 1\n${authored_open_output_text}\n${authored_open_error}")
endif()
file(READ "${authored_open_report}" authored_open_report_text)
foreach(expected
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":1"
    "\"invalid_breps_written\":1"
    "topology_vertex_bridge_3d_curve"
    "every remaining boundary edge is independently proven to have one authored source use")
  string(FIND "${authored_open_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "authored-open report does not contain ${expected}:\n${authored_open_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${MGED}" -c "${authored_open_output}" brep
    Permissive_Inferred_Curve_item.s info
  OUTPUT_VARIABLE authored_open_brep_output
  ERROR_VARIABLE authored_open_brep_error
)
set(authored_open_brep_text
  "${authored_open_brep_output}\n${authored_open_brep_error}")
if(NOT authored_open_brep_text MATCHES "Valid: YES, Solid: NO" OR
   NOT authored_open_brep_text MATCHES "faces:[ ]+2")
  message(FATAL_ERROR
    "authored-open inference was not preserved as a valid open BREP\n${authored_open_brep_text}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${authored_open_output}" attr show
    Permissive_Inferred_Curve_item.s
  OUTPUT_VARIABLE authored_open_attr_output
  ERROR_VARIABLE authored_open_attr_error
)
set(authored_open_attr_text
  "${authored_open_attr_output}\n${authored_open_attr_error}")
foreach(expected
    "step:invalidity[ ]+authored_open_topology"
    "step:invalid_reason.*exactly one authored ORIENTED_EDGE use")
  if(NOT authored_open_attr_text MATCHES "${expected}")
    message(FATAL_ERROR
      "authored-open provenance is missing ${expected}\n${authored_open_attr_text}")
  endif()
endforeach()

function(expect_inference_rejection policy_name)
  set(policy_report "${REPORT}.${policy_name}.json")
  file(REMOVE "${policy_report}")
  execute_process(
    COMMAND "${STEP_G}" -D ${ARGN} --report "${policy_report}"
      "${INPUT}"
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
      "\"inferred_curves\":0"
      "STEP topology vertices miss the bounded source curve")
    string(FIND "${policy_report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR
        "${policy_name} report does not contain ${expected}:\n${policy_report_text}")
    endif()
  endforeach()
endfunction()

expect_inference_rejection(exact --exact)
expect_inference_rejection(strict --strict)
expect_inference_rejection(repair_none --repair none)
expect_inference_rejection(reject_invalid --reject-invalid-objs)
