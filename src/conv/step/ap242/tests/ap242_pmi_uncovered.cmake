if(NOT DEFINED STEP_G OR NOT DEFINED MGED OR NOT DEFINED TEXT_INPUT OR
   NOT DEFINED POINT_INPUT OR NOT DEFINED RETAINED_INPUT OR
   NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR
    "STEP_G, MGED, TEXT_INPUT, POINT_INPUT, RETAINED_INPUT, and OUTPUT_DIR are required")
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

set(text_output "${OUTPUT_DIR}/ap242_pmi_text.g")
set(text_report "${OUTPUT_DIR}/ap242_pmi_text.json")
file(REMOVE "${text_output}" "${text_report}")
require_command("owned AP242 text import"
  "${STEP_G}" -f --schema ap242e2 --strict --report "${text_report}"
  "${TEXT_INPUT}" "${text_output}")
require_text("${COMMAND_TEXT}" "Loaded 43 instances"
  "owned AP242 text entity census")
file(READ "${text_report}" text_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"ANNOTATION_TEXT_OCCURRENCE\":1"
    "\"COMPOSITE_TEXT\":1"
    "\"TEXT_LITERAL\":1"
    "\"TEXT_LITERAL_WITH_EXTENT\":1"
    "\"AXIS2_PLACEMENT_2D\":1"
    "\"pmi_native_annotations\":1"
    "\"pmi_invalid_records\":0"
    "STEP text resolved as native annotation text with default 3.5 mm height")
  require_text("${text_report_text}" "${expected}" "owned AP242 text report")
endforeach()

set(retained_output "${OUTPUT_DIR}/ap242_pmi_retained_families.g")
set(retained_report "${OUTPUT_DIR}/ap242_pmi_retained_families.json")
file(REMOVE "${retained_output}" "${retained_report}")
require_command("owned AP242 retained presentation families import"
  "${STEP_G}" -f --schema ap242e2 --strict --report "${retained_report}"
  "${RETAINED_INPUT}" "${retained_output}")
file(READ "${retained_report}" retained_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"ANNOTATION_FILL_AREA_OCCURRENCE\":4"
    "\"ANNOTATION_TEXT\":1"
    "\"ANNOTATION_TEXT_CHARACTER\":2"
    "\"DEFINED_CHARACTER_GLYPH\":1"
    "\"CHARACTER_GLYPH_SYMBOL_OUTLINE\":1"
    "\"CHARACTER_GLYPH_SYMBOL_STROKE\":1"
    "\"ANNOTATION_SYMBOL\":3"
    "\"DEFINED_SYMBOL\":2"
    "\"ANNOTATION_SYMBOL_OCCURRENCE\":3"
    "\"ANNOTATION_SUBFIGURE_OCCURRENCE\":1"
    "\"ANNOTATION_CURVE_OCCURRENCE\":2"
    "\"DRAUGHTING_SYMBOL_REPRESENTATION\":1"
    "\"DRAUGHTING_SUBFIGURE_REPRESENTATION\":1"
    "\"ELLIPSE\":1"
    "\"B_SPLINE_CURVE_WITH_KNOTS\":1"
    "\"COMPOSITE_CURVE\":1"
    "\"TEXT_LITERAL_WITH_BLANKING_BOX\":1"
    "\"TEXT_LITERAL_WITH_ASSOCIATED_CURVES\":1"
    "\"TEXT_LITERAL_WITH_DELINEATION\":1"
    "\"COMPOSITE_TEXT_WITH_ASSOCIATED_CURVES\":1"
    "\"COMPOSITE_TEXT_WITH_BLANKING_BOX\":1"
    "\"COMPOSITE_TEXT_WITH_DELINEATION\":1"
    "\"COMPOSITE_TEXT_WITH_EXTENT\":1"
    "\"ANNOTATION_PLACEHOLDER_OCCURRENCE\":3"
    "\"DRAUGHTING_MODEL_ITEM_ASSOCIATION_WITH_PLACEHOLDER\":1"
    "\"DESCRIPTIVE_REPRESENTATION_ITEM\":3"
    "\"LEADER_CURVE\":1"
    "\"LEADER_TERMINATOR\":1"
    "\"PLANAR_BOX\":3"
    "\"unsupported_counts\":{}"
    "\"pmi_native_annotations\":1"
    "\"pmi_invalid_records\":0"
    "STEP symbols resolved as native annotation glyphs or mapped geometry")
  require_text("${retained_report_text}" "${expected}"
    "owned retained presentation report")
endforeach()

set(retained_object
  "AP242_PMI_Retained_Families_annotation_owned_retained_family_plane")
require_command("owned retained annotation inspection"
  "${MGED}" -c "${retained_output}" "l -v ${retained_object}")
foreach(expected
    "Filled area: 2 loops, 8 indexed points"
    "Filled area: 1 loop, 64 indexed points"
    "Filled area: 1 loop, 4 indexed points"
    "Label text: MAPPED"
    "Label text: MASK"
    "Label text: CURVE"
    "Label text: NEST"
    "Label text: OWNED SEMANTIC"
    "NOTE"
    "font osifont"
    "symbol diameter"
    "symbol plus minus"
    "Style: role 8"
    "symbol text blanking box"
    "Style: role 3"
    "Style: role 6"
    "symbol filled arrow"
    "Style: role 10"
    "symbol annotation placeholder"
    "symbol annotation plane boundary"
    "pattern 1, width 0.4"
    "underline"
    "overline"
    "Label text: UNDER"
    "filled")
  require_text("${COMMAND_TEXT}" "${expected}"
    "owned retained native annotation")
endforeach()
require_command("owned retained annotation plotting"
  "${MGED}" -c "${retained_output}" "draw ${retained_object}")

set(text_object "AP242_PMI_Text_annotation_owned_text_plane")
require_command("owned AP242 text annotation inspection"
  "${MGED}" -c "${text_output}" "l -v ${text_object}")
foreach(expected
    "Style: role 2, pattern 0, color 0/0/0/255, font osifont"
    "Label text: PMI OSIFont"
    "Text size: 4.0"
    "Relative position: bottom left"
    "Label text: READY"
    "Text size: 3.5"
    "Relative position: top center"
    "Text rotation angle: 90.0")
  require_text("${COMMAND_TEXT}" "${expected}" "owned AP242 native text")
endforeach()

# This invokes the annotation plot path and therefore resolves and outlines
# both text segments with the installed OSIFont data, just as MGED draw does.
require_command("owned AP242 OSIFont plotting"
  "${MGED}" -c "${text_output}" "draw ${text_object}")

set(point_output "${OUTPUT_DIR}/ap242_pmi_points.g")
set(point_report "${OUTPUT_DIR}/ap242_pmi_points.json")
file(REMOVE "${point_output}" "${point_report}")
require_command("owned AP242 point import"
  "${STEP_G}" -f --schema ap242e2 --strict --report "${point_report}"
  "${POINT_INPUT}" "${point_output}")
require_text("${COMMAND_TEXT}" "Loaded 40 instances"
  "owned AP242 point entity census")
file(READ "${point_report}" point_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"ANNOTATION_POINT_OCCURRENCE\":1"
    "\"TESSELLATED_POINT_SET\":1"
    "\"pmi_native_annotations\":1"
    "\"pmi_invalid_records\":0"
    "STEP points resolved as annotation marker strokes")
  require_text("${point_report_text}" "${expected}" "owned AP242 point report")
endforeach()

set(point_object "AP242_PMI_Points_annotation_owned_point_plane")
require_command("owned AP242 point annotation inspection"
  "${MGED}" -c "${point_output}" "db get ${point_object}")
foreach(expected
    "{2 4} {4 4} {3 3} {3 5}"
    "{8 4} {10 4} {9 3} {9 5}"
    "{10.5 6.5} {13.5 9.5}"
    "{10.5 9.5} {13.5 6.5}")
  require_text("${COMMAND_TEXT}" "${expected}" "owned AP242 point marker")
endforeach()

# The fixture is intentionally limited to the physical subset shared by the
# four AP242 editions.  Exercise every generated binding so late-bound access
# to text and marker SELECT values stays edition-neutral.
foreach(schema ap242e1 ap242e3 ap242e4)
  foreach(kind text points)
    if(kind STREQUAL "text")
      set(edition_input "${TEXT_INPUT}")
    else()
      set(edition_input "${POINT_INPUT}")
    endif()
    set(edition_output "${OUTPUT_DIR}/ap242_pmi_${kind}.${schema}.g")
    set(edition_report "${OUTPUT_DIR}/ap242_pmi_${kind}.${schema}.json")
    file(REMOVE "${edition_output}" "${edition_report}")
    require_command("owned AP242 ${kind} ${schema} import"
      "${STEP_G}" -f --schema "${schema}" --strict
      --report "${edition_report}" "${edition_input}" "${edition_output}")
    file(READ "${edition_report}" edition_report_text)
    foreach(expected
        "\"outcome\":\"complete\""
        "\"pmi_native_annotations\":1"
        "\"pmi_invalid_records\":0")
      require_text("${edition_report_text}" "${expected}"
        "owned AP242 ${kind} ${schema} report")
    endforeach()
  endforeach()
endforeach()

# Bad point indices are corrupt presentation data, not an unsupported marker.
# Verify permissive retention and strict rejection use the established policy.
file(READ "${POINT_INPUT}" malformed_point_text)
string(REPLACE "(1,3));" "(1,4));" malformed_point_text
  "${malformed_point_text}")
set(malformed_input "${OUTPUT_DIR}/ap242_pmi_points.malformed.stp")
set(malformed_output "${OUTPUT_DIR}/ap242_pmi_points.malformed.g")
set(malformed_report "${OUTPUT_DIR}/ap242_pmi_points.malformed.json")
file(WRITE "${malformed_input}" "${malformed_point_text}")
file(REMOVE "${malformed_output}" "${malformed_report}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --report "${malformed_report}"
    "${malformed_input}" "${malformed_output}"
  RESULT_VARIABLE malformed_result
  OUTPUT_VARIABLE malformed_output_text
  ERROR_VARIABLE malformed_error_text
)
if(NOT EXISTS "${malformed_output}")
  message(FATAL_ERROR
    "permissive malformed AP242 point import did not publish usable output "
    "(${malformed_result}):\n${malformed_output_text}${malformed_error_text}")
endif()
file(READ "${malformed_report}" malformed_report_text)
foreach(expected
    "\"outcome\":\"partial\""
    "\"pmi_native_annotations\":0"
    "\"pmi_invalid_records\":1"
    "tessellated annotation point index is out of range")
  require_text("${malformed_report_text}" "${expected}"
    "malformed AP242 point report")
endforeach()

set(strict_output "${OUTPUT_DIR}/ap242_pmi_points.malformed-strict.g")
set(strict_report "${OUTPUT_DIR}/ap242_pmi_points.malformed-strict.json")
file(REMOVE "${strict_output}" "${strict_report}")
execute_process(
  COMMAND "${STEP_G}" -f --schema ap242e2 --strict --report "${strict_report}"
    "${malformed_input}" "${strict_output}"
  RESULT_VARIABLE strict_result
  OUTPUT_VARIABLE strict_output_text
  ERROR_VARIABLE strict_error_text
)
if(strict_result EQUAL 0 OR EXISTS "${strict_output}")
  message(FATAL_ERROR
    "strict malformed AP242 point import unexpectedly published output:\n"
    "${strict_output_text}${strict_error_text}")
endif()
file(READ "${strict_report}" strict_report_text)
foreach(expected
    "\"outcome\":\"failed\""
    "\"pmi_invalid_records\":1"
    "tessellated annotation point index is out of range")
  require_text("${strict_report_text}" "${expected}"
    "strict malformed AP242 point report")
endforeach()
