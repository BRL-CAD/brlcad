if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED INPUT OR
   NOT DEFINED OUTPUT OR NOT DEFINED REPORT)
  message(FATAL_ERROR "STEP_G, MGED, INPUT, OUTPUT, and REPORT are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(require_command description)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
  )
  if(NOT command_result EQUAL 0)
    message(FATAL_ERROR
      "${description} failed (${command_result}):\n${command_output}${command_error}")
  endif()
  set(COMMAND_TEXT "${command_output}${command_error}" PARENT_SCOPE)
endfunction()

file(REMOVE "${OUTPUT}" "${REPORT}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --strict --report "${REPORT}"
    "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
set(import_text "${import_output}${import_error}")
if(NOT import_result EQUAL 0 OR NOT EXISTS "${OUTPUT}")
  message(FATAL_ERROR
    "AP242 PMI import failed (${import_result}):\n${import_text}")
endif()
require_text("${import_text}" "Loaded 71 instances" "AP242 PMI entity census")
string(FIND "${import_text}" "Factory Method not mapped" unmapped_factory)
if(NOT unmapped_factory EQUAL -1)
  message(FATAL_ERROR "PMI presentation reached an unrelated factory:\n${import_text}")
endif()

file(READ "${REPORT}" report_text)
foreach(expected
    "\"pmi_semantic_records\":8"
    "\"pmi_presentation_records\":14"
    "\"pmi_association_records\":9"
    "\"pmi_native_datums\":3"
    "\"pmi_native_annotations\":1"
    "\"pmi_invalid_records\":0"
    "\"entity_id\":50,\"type\":\"DATUM\""
    "\"native_kind\":\"plane\",\"native_status\":\"resolved_exactly\""
    "\"native_kind\":\"line\",\"native_status\":\"resolved_exactly\""
    "\"native_kind\":\"point\",\"native_status\":\"resolved_exactly\""
    "associated planes are distinct; a derived median plane cannot be inferred safely"
    "\"native_kind\":\"presentation_annotation\",\"native_status\":\"graphical curves and tessellated surface boundaries resolved as annotation strokes\"")
  require_text("${report_text}" "${expected}" "AP242 PMI report")
endforeach()

require_command("plane datum inspection"
  "${MGED}" -c "${OUTPUT}" "db get AP242_PMI_Product_datum_A")
require_text("${COMMAND_TEXT}" "datum data { {plane 0 0 0" "AP242 plane datum")
require_command("line datum inspection"
  "${MGED}" -c "${OUTPUT}" "db get AP242_PMI_Product_datum_B")
require_text("${COMMAND_TEXT}" "datum data { {line 0 0 0" "AP242 line datum")
require_command("point datum inspection"
  "${MGED}" -c "${OUTPUT}" "db get AP242_PMI_Product_datum_C")
require_text("${COMMAND_TEXT}" "datum data { {point 20 3 4}" "AP242 point datum")
require_command("datum inventory"
  "${MGED}" -c "${OUTPUT}" "search / -type datum")
string(FIND "${COMMAND_TEXT}" "_datum_D" invented_median)
if(NOT invented_median EQUAL -1)
  message(FATAL_ERROR "ambiguous median datum was invented:\n${COMMAND_TEXT}")
endif()

require_command("model-space PMI annotation inspection"
  "${MGED}" -c "${OUTPUT}"
  "db get AP242_PMI_Product_annotation_PMI_plane")
foreach(expected
    "mode model"
    "V {0 0 0}"
    "{0 0} {10 0} {10 5} {0 5}"
    "{ line S 3 E 0 }"
    "{ line S 4 E 5 }"
    "{ line S 8 E 9 }"
    "{ line S 12 E 13 }"
    "{ line S 13 E 14 }"
    "{ line S 78 E 15 }")
  require_text("${COMMAND_TEXT}" "${expected}" "AP242 native annotation")
endforeach()

# The four AP242 editions have isolated generated bindings.  The fixture uses
# the physical common subset, so qualify the same retained semantic graph and
# native objects through the other three bindings rather than assuming that a
# successful edition-2 import proves source compatibility.
foreach(schema ap242e1 ap242e3 ap242e4)
  set(edition_output "${OUTPUT}.${schema}.g")
  set(edition_report "${REPORT}.${schema}.json")
  file(REMOVE "${edition_output}" "${edition_report}")
  execute_process(
    COMMAND "${STEP_G}" -f --schema "${schema}" --strict
      --report "${edition_report}" "${INPUT}" "${edition_output}"
    RESULT_VARIABLE edition_result
    OUTPUT_VARIABLE edition_stdout
    ERROR_VARIABLE edition_stderr
  )
  if(NOT edition_result EQUAL 0 OR NOT EXISTS "${edition_output}")
    message(FATAL_ERROR
      "AP242 PMI ${schema} import failed (${edition_result}):\n"
      "${edition_stdout}${edition_stderr}")
  endif()
  file(READ "${edition_report}" edition_report_text)
  foreach(expected
      "\"outcome\":\"complete\""
      "\"pmi_semantic_records\":8"
      "\"pmi_presentation_records\":14"
      "\"pmi_association_records\":9"
      "\"pmi_native_datums\":3"
      "\"pmi_native_annotations\":1"
      "\"pmi_invalid_records\":0"
      "\"native_kind\":\"plane\",\"native_status\":\"resolved_exactly\""
      "\"native_kind\":\"presentation_annotation\",\"native_status\":\"graphical curves and tessellated surface boundaries resolved as annotation strokes\"")
    require_text("${edition_report_text}" "${expected}"
      "AP242 PMI ${schema} report")
  endforeach()
endforeach()

# An invalid tessellated line index is source corruption, not an unsupported
# presentation choice.  Permissive mode keeps the exact source graph and the
# usable product with a partial outcome; strict mode rejects publication.
file(READ "${INPUT}" malformed_text)
string(REPLACE "((1,2,3,4,1))" "((1,2,5))" malformed_text "${malformed_text}")
set(malformed_input "${OUTPUT}.malformed.stp")
set(malformed_output "${OUTPUT}.malformed.g")
set(malformed_report "${REPORT}.malformed.json")
file(WRITE "${malformed_input}" "${malformed_text}")
file(REMOVE "${malformed_output}" "${malformed_report}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --report "${malformed_report}"
    "${malformed_input}" "${malformed_output}"
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_stdout
  ERROR_VARIABLE malformed_stderr
)
if(NOT EXISTS "${malformed_output}")
  message(FATAL_ERROR
    "permissive malformed PMI retention did not publish usable output "
    "(${malformed_result}):\n"
    "${malformed_stdout}${malformed_stderr}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"pmi_native_annotations\":0"
    "\"pmi_invalid_records\":1"
    "\"outcome\":\"partial\""
    "tessellated annotation line index is out of range")
  require_text("${malformed_report_text}" "${expected}" "malformed AP242 PMI report")
endforeach()

set(strict_output "${OUTPUT}.malformed-strict.g")
set(strict_report "${REPORT}.malformed-strict.json")
file(REMOVE "${strict_output}" "${strict_report}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --strict --report "${strict_report}"
    "${malformed_input}" "${strict_output}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_stdout
  ERROR_VARIABLE strict_stderr
)
if(strict_result EQUAL 0 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict malformed PMI unexpectedly succeeded or published output:\n"
    "${strict_stdout}${strict_stderr}")
endif()
file(READ "${strict_report}" strict_report_text)
foreach(expected
    "\"pmi_invalid_records\":1"
    "\"outcome\":\"failed\""
    "tessellated annotation line index is out of range")
  require_text("${strict_report_text}" "${expected}" "strict AP242 PMI report")
endforeach()

# Surface glyphs use separate tessellated point-normal indexing.  Verify that
# malformed triangle references are diagnosed rather than silently dropping
# the filled presentation content and publishing only the surrounding frame.
file(READ "${INPUT}" malformed_surface_text)
string(REPLACE "((1,2,3),(3,2,4))" "((1,2,5))"
  malformed_surface_text "${malformed_surface_text}")
set(malformed_surface_input "${OUTPUT}.malformed-surface.stp")
set(malformed_surface_output "${OUTPUT}.malformed-surface.g")
set(malformed_surface_report "${REPORT}.malformed-surface.json")
file(WRITE "${malformed_surface_input}" "${malformed_surface_text}")
file(REMOVE "${malformed_surface_output}" "${malformed_surface_report}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --report "${malformed_surface_report}"
    "${malformed_surface_input}" "${malformed_surface_output}"
  RESULT_VARIABLE malformed_surface_result
  OUTPUT_VARIABLE malformed_surface_stdout
  ERROR_VARIABLE malformed_surface_stderr
)
if(NOT EXISTS "${malformed_surface_output}")
  message(FATAL_ERROR
    "permissive malformed PMI surface retention did not publish usable output "
    "(${malformed_surface_result}):\n"
    "${malformed_surface_stdout}${malformed_surface_stderr}")
endif()
file(READ "${malformed_surface_report}" malformed_surface_report_text)
foreach(expected
    "\"pmi_native_annotations\":0"
    "\"pmi_invalid_records\":1"
    "\"outcome\":\"partial\""
    "tessellated annotation surface index is out of range")
  require_text("${malformed_surface_report_text}" "${expected}"
    "malformed AP242 PMI surface report")
endforeach()
