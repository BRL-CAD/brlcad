if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED NIRT OR NOT DEFINED INPUT OR
   NOT DEFINED WORK_DIRECTORY)
  message(FATAL_ERROR
    "STEP_G, MGED, NIRT, INPUT, and WORK_DIRECTORY are required")
endif()

foreach(edition IN ITEMS ap242e1 ap242e2 ap242e3 ap242e4)
  set(output "${WORK_DIRECTORY}/ap242_swept_solids_${edition}.g")
  set(report "${WORK_DIRECTORY}/ap242_swept_solids_${edition}.json")
  file(REMOVE "${output}" "${report}")
  execute_process(
    COMMAND "${STEP_G}" --schema "${edition}" --strict -O "${output}"
      --report "${report}" "${INPUT}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0 OR NOT EXISTS "${output}")
    message(FATAL_ERROR
      "strict ${edition} swept-solid import returned ${import_result}:\n"
      "${import_output}${import_error}")
  endif()

  file(READ "${report}" report_text)
  foreach(expected
      "\"strict\":true"
      "\"geometry_attempted\":4"
      "\"geometry_written\":4"
      "\"geometry_skipped\":0"
      "\"EXTRUDED_FACE_SOLID\":1"
      "\"REVOLVED_AREA_SOLID\":1"
      "\"SWEPT_DISK_SOLID\":2"
      "\"outcome\":\"complete\"")
    string(FIND "${report_text}" "${expected}" found)
    if(found EQUAL -1)
      message(FATAL_ERROR "${edition} report omits ${expected}:\n${report_text}")
    endif()
  endforeach()

  execute_process(
    COMMAND "${MGED}" -c "${output}" brep AP242_Sweeps_swept_item.s info
    RESULT_VARIABLE brep_result
    OUTPUT_VARIABLE brep_output
    ERROR_VARIABLE brep_error
  )
  set(brep_text "${brep_output}\n${brep_error}")
  if(NOT brep_result EQUAL 0 OR
     NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
     NOT brep_text MATCHES "faces:[ ]+5" OR
     NOT brep_text MATCHES "edges:[ ]+9")
    message(FATAL_ERROR "${edition} extruded-face BREP is invalid:\n${brep_text}")
  endif()

  execute_process(
    COMMAND "${MGED}" -c "${output}" db get AP242_Sweeps_swept_item_step56
    RESULT_VARIABLE disk_result
    OUTPUT_VARIABLE disk_output
    ERROR_VARIABLE disk_error
  )
  set(disk_text "${disk_output}\n${disk_error}")
  if(NOT disk_result EQUAL 0 OR
     NOT disk_text MATCHES "AP242_Sweeps_swept_item_step56_outer.s" OR
     NOT disk_text MATCHES "AP242_Sweeps_swept_item_step56_inner.s")
    message(FATAL_ERROR "${edition} swept-disk Boolean is invalid:\n${disk_text}")
  endif()

  execute_process(
    COMMAND "${MGED}" -c "${output}"
      brep AP242_Sweeps_swept_item_step59.s info
    RESULT_VARIABLE circular_result
    OUTPUT_VARIABLE circular_output
    ERROR_VARIABLE circular_error
  )
  set(circular_text "${circular_output}\n${circular_error}")
  if(NOT circular_result EQUAL 0 OR
     NOT circular_text MATCHES "Valid: YES, Solid: YES" OR
     NOT circular_text MATCHES "faces:[ ]+2" OR
     NOT circular_text MATCHES "edges:[ ]+4")
    message(FATAL_ERROR
      "${edition} circular swept-disk BREP is invalid:\n${circular_text}")
  endif()

  execute_process(
    COMMAND "${NIRT}" -H 0 -b -f csv
      -e "xyz 10 0 0; dir 1 0 0; s; q" "${output}"
      AP242_Sweeps_swept_item_step59.s
    RESULT_VARIABLE circular_ray_result
    OUTPUT_VARIABLE circular_ray_output
    ERROR_VARIABLE circular_ray_error
  )
  set(circular_ray_text "${circular_ray_output}\n${circular_ray_error}")
  if(NOT circular_ray_result EQUAL 0 OR
     NOT circular_ray_text MATCHES
       ",15[.]000000,0[.]000000,0[.]000000,[-0-9.]+,15[.]750000" OR
     NOT circular_ray_text MATCHES
       ",16[.]250000,0[.]000000,0[.]000000,[-0-9.]+,17[.]000000" OR
     NOT circular_ray_text MATCHES
       ",23[.]000000,0[.]000000,0[.]000000,[-0-9.]+,23[.]750000" OR
     NOT circular_ray_text MATCHES
       ",24[.]250000,0[.]000000,0[.]000000,[-0-9.]+,25[.]000000")
    message(FATAL_ERROR
      "${edition} circular swept-disk void ray failed:\n"
      "${circular_ray_output}${circular_ray_error}")
  endif()

  execute_process(
    COMMAND "${MGED}" -c "${output}"
      brep AP242_Sweeps_swept_item_step166.s info
    RESULT_VARIABLE revolution_result
    OUTPUT_VARIABLE revolution_output
    ERROR_VARIABLE revolution_error
  )
  set(revolution_text "${revolution_output}\n${revolution_error}")
  if(NOT revolution_result EQUAL 0 OR
     NOT revolution_text MATCHES "Valid: YES, Solid: YES" OR
     NOT revolution_text MATCHES "faces:[ ]+2" OR
     NOT revolution_text MATCHES "edges:[ ]+4")
    message(FATAL_ERROR
      "${edition} revolved-area void BREP is invalid:\n${revolution_text}")
  endif()
endforeach()

# Exercise exact angular caps on a hollow circular sweep.  The material ray
# starts in the centerline void and must not enter at the start cap.
file(READ "${INPUT}" partial_circle_text)
string(REPLACE
  "#59=SWEPT_DISK_SOLID('circular annular sweep',#58,1.,0.25,0.,6.283185307179586);"
  "#59=SWEPT_DISK_SOLID('circular annular sweep',#58,1.,0.25,0.,1.5707963267948966);"
  partial_circle_text "${partial_circle_text}")
set(partial_circle_input
  "${WORK_DIRECTORY}/ap242_swept_disk_partial_circle.stp")
set(partial_circle_output
  "${WORK_DIRECTORY}/ap242_swept_disk_partial_circle.g")
set(partial_circle_report
  "${WORK_DIRECTORY}/ap242_swept_disk_partial_circle.json")
file(WRITE "${partial_circle_input}" "${partial_circle_text}")
file(REMOVE "${partial_circle_output}" "${partial_circle_report}")
execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 --strict -O "${partial_circle_output}"
    --report "${partial_circle_report}" "${partial_circle_input}"
  RESULT_VARIABLE partial_circle_result
  OUTPUT_VARIABLE partial_circle_output_text
  ERROR_VARIABLE partial_circle_error
)
if(NOT partial_circle_result EQUAL 0 OR NOT EXISTS "${partial_circle_output}")
  message(FATAL_ERROR
    "strict partial circular sweep returned ${partial_circle_result}:\n"
    "${partial_circle_output_text}${partial_circle_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${partial_circle_output}"
    brep AP242_Sweeps_swept_item_step59.s info
  RESULT_VARIABLE partial_circle_brep_result
  OUTPUT_VARIABLE partial_circle_brep_output
  ERROR_VARIABLE partial_circle_brep_error
)
set(partial_circle_brep_text
  "${partial_circle_brep_output}\n${partial_circle_brep_error}")
if(NOT partial_circle_brep_result EQUAL 0 OR
   NOT partial_circle_brep_text MATCHES "Valid: YES, Solid: YES" OR
   NOT partial_circle_brep_text MATCHES "faces:[ ]+4" OR
   NOT partial_circle_brep_text MATCHES "edges:[ ]+6")
  message(FATAL_ERROR
    "partial circular swept-disk BREP is invalid:\n"
    "${partial_circle_brep_text}")
endif()
execute_process(
  COMMAND "${NIRT}" -H 0 -b -f csv
    -e "xyz 24 -1 0; dir 0 1 0; s; q" "${partial_circle_output}"
    AP242_Sweeps_swept_item_step59.s
  RESULT_VARIABLE partial_circle_ray_result
  OUTPUT_VARIABLE partial_circle_ray_output
  ERROR_VARIABLE partial_circle_ray_error
)
set(partial_circle_ray_text
  "${partial_circle_ray_output}\n${partial_circle_ray_error}")
if(NOT partial_circle_ray_result EQUAL 0 OR
   NOT partial_circle_ray_text MATCHES
     ",24[.]000000,1[.]4361[0-9]+,0[.]000000,[-0-9.]+,24[.]000000,3[.]000000")
  message(FATAL_ERROR
    "partial circular swept-disk void ray failed:\n"
    "${partial_circle_ray_output}${partial_circle_ray_error}")
endif()

# A tube radius equal to the circular directrix radius creates a degenerate
# horn torus.  It must never be published as an exact solid.
file(READ "${INPUT}" self_intersect_text)
string(REPLACE
  "#59=SWEPT_DISK_SOLID('circular annular sweep',#58,1.,0.25,0.,6.283185307179586);"
  "#59=SWEPT_DISK_SOLID('circular annular sweep',#58,4.,0.25,0.,6.283185307179586);"
  self_intersect_text "${self_intersect_text}")
set(self_intersect_input
  "${WORK_DIRECTORY}/ap242_swept_disk_self_intersect.stp")
set(self_intersect_output
  "${WORK_DIRECTORY}/ap242_swept_disk_self_intersect.g")
set(self_intersect_report
  "${WORK_DIRECTORY}/ap242_swept_disk_self_intersect.json")
set(self_intersect_strict_output
  "${WORK_DIRECTORY}/ap242_swept_disk_self_intersect_strict.g")
set(self_intersect_strict_report
  "${WORK_DIRECTORY}/ap242_swept_disk_self_intersect_strict.json")
file(WRITE "${self_intersect_input}" "${self_intersect_text}")
file(REMOVE "${self_intersect_output}" "${self_intersect_report}"
  "${self_intersect_strict_output}" "${self_intersect_strict_report}")
execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 -O "${self_intersect_output}"
    --report "${self_intersect_report}" "${self_intersect_input}"
  RESULT_VARIABLE self_intersect_result
  OUTPUT_VARIABLE self_intersect_output_text
  ERROR_VARIABLE self_intersect_error
)
if(NOT self_intersect_result EQUAL 1 OR NOT EXISTS "${self_intersect_output}")
  message(FATAL_ERROR
    "permissive self-intersecting sweep returned ${self_intersect_result}:\n"
    "${self_intersect_output_text}${self_intersect_error}")
endif()
file(READ "${self_intersect_report}" self_intersect_report_text)
foreach(expected
    "\"geometry_attempted\":4"
    "\"geometry_written\":3"
    "\"geometry_skipped\":1"
    "\"outcome\":\"partial\""
    "circular swept disk is degenerate or self-intersects at its curvature radius")
  string(FIND "${self_intersect_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive self-intersecting sweep report omits ${expected}:\n"
      "${self_intersect_report_text}")
  endif()
endforeach()
execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 --strict
    -O "${self_intersect_strict_output}"
    --report "${self_intersect_strict_report}" "${self_intersect_input}"
  RESULT_VARIABLE self_intersect_strict_result
  OUTPUT_VARIABLE self_intersect_strict_output_text
  ERROR_VARIABLE self_intersect_strict_error
)
if(NOT self_intersect_strict_result EQUAL 4 OR
   EXISTS "${self_intersect_strict_output}")
  message(FATAL_ERROR
    "strict self-intersecting sweep returned ${self_intersect_strict_result} "
    "or published output:\n${self_intersect_strict_output_text}"
    "${self_intersect_strict_error}")
endif()
file(READ "${self_intersect_strict_report}"
  self_intersect_strict_report_text)
foreach(expected
    "\"strict\":true"
    "\"exit_status\":4"
    "\"geometry_skipped\":1"
    "\"outcome\":\"failed\""
    "circular swept disk is degenerate or self-intersects at its curvature radius")
  string(FIND "${self_intersect_strict_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "strict self-intersecting sweep report omits ${expected}:\n"
      "${self_intersect_strict_report_text}")
  endif()
endforeach()

# Break one corner of the inner profile loop by much more than the declared
# model tolerance.  Permissive mode must retain the three independent usable
# sweeps and report the rejected revolution; strict mode must publish nothing.
file(READ "${INPUT}" open_profile_text)
string(REPLACE
  "#152=CARTESIAN_POINT('',(5.5,1.,0.));"
  "#152=CARTESIAN_POINT('',(5.5,1.25,0.));"
  open_profile_text "${open_profile_text}")
set(open_profile_input
  "${WORK_DIRECTORY}/ap242_swept_open_inner_profile.stp")
set(open_profile_output
  "${WORK_DIRECTORY}/ap242_swept_open_inner_profile.g")
set(open_profile_report
  "${WORK_DIRECTORY}/ap242_swept_open_inner_profile.json")
set(open_profile_strict_output
  "${WORK_DIRECTORY}/ap242_swept_open_inner_profile_strict.g")
set(open_profile_strict_report
  "${WORK_DIRECTORY}/ap242_swept_open_inner_profile_strict.json")
file(WRITE "${open_profile_input}" "${open_profile_text}")
file(REMOVE "${open_profile_output}" "${open_profile_report}"
  "${open_profile_strict_output}" "${open_profile_strict_report}")

execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 -O "${open_profile_output}"
    --report "${open_profile_report}" "${open_profile_input}"
  RESULT_VARIABLE open_profile_result
  OUTPUT_VARIABLE open_profile_output_text
  ERROR_VARIABLE open_profile_error
)
if(NOT open_profile_result EQUAL 1 OR NOT EXISTS "${open_profile_output}")
  message(FATAL_ERROR
    "permissive open inner profile returned ${open_profile_result}:\n"
    "${open_profile_output_text}${open_profile_error}")
endif()
file(READ "${open_profile_report}" open_profile_report_text)
foreach(expected
    "\"geometry_attempted\":4"
    "\"geometry_written\":3"
    "\"geometry_skipped\":1"
    "\"outcome\":\"partial\""
    "REVOLVED_AREA_SOLID"
    "profile boundary gap exceeds the model tolerance")
  string(FIND "${open_profile_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive open inner profile report omits ${expected}:\n"
      "${open_profile_report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 --strict
    -O "${open_profile_strict_output}" --report "${open_profile_strict_report}"
    "${open_profile_input}"
  RESULT_VARIABLE open_profile_strict_result
  OUTPUT_VARIABLE open_profile_strict_output_text
  ERROR_VARIABLE open_profile_strict_error
)
if(NOT open_profile_strict_result EQUAL 4 OR
   EXISTS "${open_profile_strict_output}")
  message(FATAL_ERROR
    "strict open inner profile returned ${open_profile_strict_result} or "
    "published output:\n${open_profile_strict_output_text}"
    "${open_profile_strict_error}")
endif()
file(READ "${open_profile_strict_report}" open_profile_strict_report_text)
foreach(expected
    "\"strict\":true"
    "\"exit_status\":4"
    "\"geometry_skipped\":1"
    "\"outcome\":\"failed\""
    "profile boundary gap exceeds the model tolerance")
  string(FIND "${open_profile_strict_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "strict open inner profile report omits ${expected}:\n"
      "${open_profile_strict_report_text}")
  endif()
endforeach()

# Add an unsupported exact elliptical directrix and a trimmed/drafted subtype.
# Permissive mode must retain the four useful solids and report these two,
# while strict mode publishes nothing.
file(READ "${INPUT}" unsupported_text)
string(CONCAT unsupported_entities
  "#49=EXTRUDED_FACE_SOLID_WITH_TRIM_CONDITIONS('trimmed face sweep',"
  "#38,#40,5.,LENGTH_MEASURE(1.),LENGTH_MEASURE(2.),.BLIND.,"
  ".THROUGH_ALL.,0.,0.);\n"
  "#168=ELLIPSE('unsupported elliptical directrix',#57,4.,2.);\n"
  "#169=SWEPT_DISK_SOLID('unsupported elliptical sweep',#168,1.,0.25,"
  "0.,1.);\n\n")
string(REPLACE
  "#60=(LENGTH_UNIT()"
  "${unsupported_entities}#60=(LENGTH_UNIT()"
  unsupported_text "${unsupported_text}")
string(REPLACE
  "#65=SHAPE_REPRESENTATION('',(#50,#56,#59,#166),#64);"
  "#65=SHAPE_REPRESENTATION('',(#50,#56,#59,#166,#49,#169),#64);"
  unsupported_text "${unsupported_text}")
set(unsupported_input "${WORK_DIRECTORY}/ap242_swept_unsupported.stp")
set(unsupported_output "${WORK_DIRECTORY}/ap242_swept_unsupported.g")
set(unsupported_report "${WORK_DIRECTORY}/ap242_swept_unsupported.json")
set(strict_output "${WORK_DIRECTORY}/ap242_swept_unsupported_strict.g")
set(strict_report "${WORK_DIRECTORY}/ap242_swept_unsupported_strict.json")
file(WRITE "${unsupported_input}" "${unsupported_text}")
file(REMOVE "${unsupported_output}" "${unsupported_report}"
  "${strict_output}" "${strict_report}")

execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 -O "${unsupported_output}"
    --report "${unsupported_report}" "${unsupported_input}"
  RESULT_VARIABLE unsupported_result
  OUTPUT_VARIABLE unsupported_output_text
  ERROR_VARIABLE unsupported_error
)
if(NOT unsupported_result EQUAL 1 OR NOT EXISTS "${unsupported_output}")
  message(FATAL_ERROR
    "permissive nonlinear sweep returned ${unsupported_result}:\n"
    "${unsupported_output_text}${unsupported_error}")
endif()
file(READ "${unsupported_report}" unsupported_report_text)
foreach(expected
    "\"geometry_attempted\":6"
    "\"geometry_written\":4"
    "\"geometry_skipped\":2"
    "\"outcome\":\"partial\""
    "EXTRUDED_FACE_SOLID_WITH_TRIM_CONDITIONS"
    "swept-solid subtype has no exact importer"
    "only exact linear and circular SWEPT_DISK_SOLID directrices are currently supported")
  string(FIND "${unsupported_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR
      "permissive nonlinear sweep report omits ${expected}:\n"
      "${unsupported_report_text}")
  endif()
endforeach()

execute_process(
  COMMAND "${STEP_G}" --schema ap242e4 --strict -O "${strict_output}"
    --report "${strict_report}" "${unsupported_input}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output_text
  ERROR_VARIABLE strict_error
)
if(NOT strict_result EQUAL 4 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict nonlinear sweep returned ${strict_result} or published output:\n"
    "${strict_output_text}${strict_error}")
endif()
file(READ "${strict_report}" strict_report_text)
foreach(expected
    "\"strict\":true"
    "\"exit_status\":4"
    "\"geometry_skipped\":2"
    "\"outcome\":\"failed\"")
  string(FIND "${strict_report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "strict nonlinear sweep report omits ${expected}:\n${strict_report_text}")
  endif()
endforeach()
