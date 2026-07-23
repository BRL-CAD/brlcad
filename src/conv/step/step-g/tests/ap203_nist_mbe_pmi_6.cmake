if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED NIRT OR NOT DEFINED RT OR
    NOT DEFINED INPUT OR NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, NIRT, RT, INPUT, REPORT, and OUTPUT are required")
endif()

set(ray_output_file "${OUTPUT}.nist6-cap.pix")
set(bot_object "Document_item.bot")
get_filename_component(test_work_dir "${OUTPUT}" DIRECTORY)
file(REMOVE "${REPORT}" "${OUTPUT}" "${ray_output_file}")
execute_process(
  COMMAND "${STEP_G}" -O "${OUTPUT}" --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "NIST MBE PMI 6 import failed (${import_result})\n${import_output}\n${import_error}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0"
    "\"invalid_breps\":0"
    "\"message\":\"inserted an exact singular trim at a surface pole\",\"count\":8"
    "\"message\":\"retargeted an existing singular trim to close the exact pole boundary\",\"count\":2"
    "\"message\":\"selected the exact singular-pole branch matching the STEP face bound\",\"count\":2"
    "\"message\":\"regenerated an isoparametric pcurve from the exact edge\",\"count\":16")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()
string(FIND "${report_text}"
  "existing pcurve and 3-D edge exceeded the declared tolerance after exact regeneration failed"
  measured_pcurve_fallback)
if(NOT measured_pcurve_fallback EQUAL -1)
  message(FATAL_ERROR
    "NIST MBE PMI 6 used the loose-pcurve fallback instead of its exact regeneration path:\n${report_text}")
endif()
string(FIND "${report_text}"
  "shifted an exact pcurve onto a singular trim's periodic branch"
  nonnative_singular_branch)
if(NOT nonnative_singular_branch EQUAL -1)
  message(FATAL_ERROR
    "NIST MBE PMI 6 retained an out-of-domain spherical pcurve branch:\n${report_text}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" brep Document_item.s info
  RESULT_VARIABLE brep_result
  OUTPUT_VARIABLE brep_output
  ERROR_VARIABLE brep_error
  TIMEOUT 30
)
set(brep_text "${brep_output}\n${brep_error}")
if(NOT brep_result EQUAL 0 OR
    NOT brep_text MATCHES "Valid: YES, Solid: YES" OR
    NOT brep_text MATCHES "faces:[ ]+152" OR
    NOT brep_text MATCHES "edges:[ ]+456" OR
    NOT brep_text MATCHES "vertices:[ ]+324")
  message(FATAL_ERROR "NIST MBE PMI 6 BREP validation failed\n${brep_text}")
endif()

# Exercise the same tessellation path used by shaded display.  Face 41 is the
# upper spherical corner cap whose generic pole pullback formerly retained an
# arbitrary longitude at its singular endpoint.  The complete BREP was still
# valid and ray traceable, but shaded-mesh repair detached that cap.
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}"
    "brep Document_item.s bot ${bot_object}"
  WORKING_DIRECTORY "${test_work_dir}"
  RESULT_VARIABLE bot_result
  OUTPUT_VARIABLE bot_output
  ERROR_VARIABLE bot_error
  TIMEOUT 30
)
set(bot_text "${bot_output}\n${bot_error}")
if(NOT bot_result EQUAL 0 OR
    bot_text MATCHES
      "Face (41|126|128): (initial CDT .*FAILED|repair FAILED)!" OR
    bot_text MATCHES "[0-9]+ misoriented edges")
  message(FATAL_ERROR
    "NIST MBE PMI 6 spherical-cap shaded-mesh validation failed\n${bot_text}")
endif()

# Faces 49 and 126 are the two halves of the upper small spherical cap.  A
# supplied full-period singular trim formerly overlapped an inserted half-
# period connector on face 126.  That passed BREP validation but caused shaded
# CDT to reject the face.  Probe an interior point in each half, away from the
# shared pole and equator where a neighbouring triangle might hide an omission.
execute_process(
  COMMAND "${NIRT}" -H 0 -b -f csv
    -e "xyz 158.75 50 220.25; dir 0 1 0; s; xyz 158.75 50 224.25; dir 0 1 0; s; q"
    "${OUTPUT}" "${bot_object}"
  RESULT_VARIABLE paired_cap_ray_result
  OUTPUT_VARIABLE paired_cap_ray_output
  ERROR_VARIABLE paired_cap_ray_error
  TIMEOUT 30
)
set(paired_cap_ray_text "${paired_cap_ray_output}\n${paired_cap_ray_error}")
if(NOT paired_cap_ray_result EQUAL 0 OR
    NOT paired_cap_ray_text MATCHES
      "158.750000,71\\.[0-9]+,220.250000" OR
    NOT paired_cap_ray_text MATCHES
      "158.750000,71\\.[0-9]+,224.250000")
  message(FATAL_ERROR
    "NIST MBE PMI 6 paired spherical-cap shaded rays failed\n${paired_cap_ray_text}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "bot get faces ${bot_object}"
  RESULT_VARIABLE bot_info_result
  OUTPUT_VARIABLE bot_info_output
  ERROR_VARIABLE bot_info_error
  TIMEOUT 30
)
string(STRIP "${bot_info_output}" bot_face_count)
if(NOT bot_info_result EQUAL 0)
  message(FATAL_ERROR
    "NIST MBE PMI 6 shaded mesh query failed\n${bot_info_output}\n${bot_info_error}")
endif()
if(bot_face_count LESS 1)
  message(FATAL_ERROR
    "NIST MBE PMI 6 shaded mesh was not written\n${bot_info_output}\n${bot_info_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${OUTPUT}" "bot split ${bot_object}"
  RESULT_VARIABLE bot_split_result
  OUTPUT_VARIABLE bot_split_output
  ERROR_VARIABLE bot_split_error
  TIMEOUT 30
)
set(bot_split_text "${bot_split_output}\n${bot_split_error}")
if(NOT bot_split_result EQUAL 0 OR
    NOT bot_split_text MATCHES "fully connected topologically")
  message(FATAL_ERROR
    "NIST MBE PMI 6 shaded mesh contains a disconnected component\n${bot_split_text}")
endif()

# This deterministic ray enters the lower spherical cap which used to select
# the complementary half of a pole-bounded periodic surface.  The BREP could
# still report structurally valid and solid in that state, but rt had to flip
# the back-facing entry normal and the cap shaded as disconnected.
execute_process(
  COMMAND "${RT}" -P 1 -B -s 300 -b "161 66" -a 35 -e 25
    -o "${ray_output_file}" "${OUTPUT}" Document
  RESULT_VARIABLE ray_result
  OUTPUT_VARIABLE ray_output
  ERROR_VARIABLE ray_error
  TIMEOUT 30
)
set(ray_text "${ray_output}\n${ray_error}")
if(NOT ray_result EQUAL 0 OR
    NOT ray_text MATCHES "1 solid/ray intersections: 1 hits \\+ 0 miss" OR
    ray_text MATCHES "flip N")
  message(FATAL_ERROR
    "NIST MBE PMI 6 spherical-cap ray validation failed\n${ray_text}")
endif()

# Probe the formerly detached face 41 directly through the generated shaded
# mesh.  This axis ray exits at its spherical pole (34.925, 34.925, 165.1), so
# it cannot be satisfied by one of the model's other spherical corner caps.
execute_process(
  COMMAND "${NIRT}" -H 0 -b -f csv
    -e "xyz 34.925 34.925 160; dir 0 0 1; s; q"
    "${OUTPUT}" "${bot_object}"
  RESULT_VARIABLE bot_ray_result
  OUTPUT_VARIABLE bot_ray_output
  ERROR_VARIABLE bot_ray_error
  TIMEOUT 30
)
set(bot_ray_text "${bot_ray_output}\n${bot_ray_error}")
if(NOT bot_ray_result EQUAL 0 OR
    NOT bot_ray_text MATCHES "Document_item.bot" OR
    NOT bot_ray_text MATCHES "34.925000,34.925000,165.100000")
  message(FATAL_ERROR
    "NIST MBE PMI 6 shaded spherical-cap ray validation failed\n${bot_ray_text}")
endif()
file(REMOVE "${ray_output_file}")
