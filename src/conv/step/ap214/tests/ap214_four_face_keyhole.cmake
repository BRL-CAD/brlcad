if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
    NOT DEFINED REPORT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, REPORT, and OUTPUT are required")
endif()

file(REMOVE "${REPORT}" "${OUTPUT}")
execute_process(
  COMMAND "${STEP_G}" --reject-invalid-objs -O "${OUTPUT}"
    --report "${REPORT}" "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
  TIMEOUT 60
)
if(NOT import_result EQUAL 3)
  message(FATAL_ERROR
    "AP214 four-face keyhole fixture returned ${import_result}, expected 3\n"
    "${import_output}\n${import_error}")
endif()

# AP214 root #3695 contains a conical face whose STEP loop joins two
# full-period boundary circles by using the same radial connector twice.  A
# neighboring toroidal keyhole activates whole-BREP normalization.  The
# importer may keep that bridge only by proving both full-period circles select
# its topology phase, relocating the private analytic surface seam there, and
# regenerating every affected pcurve without changing its 3-D lift.
#
# This minimized dependency closure intentionally contains only four of the
# source solid's 121 faces.  Consequently it must remain structurally valid
# after repair but fail the later solidness check; the complete #3695 corpus
# item verifies successful solid output.
file(READ "${REPORT}" report_text)
foreach(expected
    "\"geometry_attempted\":1"
    "\"geometry_written\":0"
    "\"geometry_skipped\":1"
    "\"reason\":\"closed STEP BREP did not validate as a solid\""
    "\"message\":\"aligned a periodic revolution surface seam with an exact edge\""
    "\"message\":\"regenerated paired seam pcurves from the exact edge\"")
  string(FIND "${report_text}" "${expected}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "report does not contain ${expected}:\n${report_text}")
  endif()
endforeach()

string(FIND "${report_text}" "OpenNURBS structural validation failed"
  structural_failure)
if(NOT structural_failure EQUAL -1)
  message(FATAL_ERROR
    "AP214 keyhole repair remained structurally invalid:\n${report_text}")
endif()
