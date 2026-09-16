if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED INPUT OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR
    "G_STEP, STEP_G, MGED, INPUT, and OUTPUT_DIR are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

set(imported "${OUTPUT_DIR}/g_step_imported_product_layering.g")
set(import_report
  "${OUTPUT_DIR}/g_step_imported_product_layering_import.json")
set(exported "${OUTPUT_DIR}/g_step_imported_product_layering.stp")
set(export_report
  "${OUTPUT_DIR}/g_step_imported_product_layering_export.json")
set(roundtrip_report
  "${OUTPUT_DIR}/g_step_imported_product_layering_roundtrip.json")
set(roundtrip_db
  "${OUTPUT_DIR}/g_step_imported_product_layering_roundtrip.g")
file(REMOVE "${imported}" "${import_report}" "${exported}"
  "${export_report}" "${roundtrip_report}" "${roundtrip_db}")

execute_process(
  COMMAND "${STEP_G}" --strict -O "${imported}" --report "${import_report}"
    "${INPUT}"
  RESULT_VARIABLE import_result
  OUTPUT_VARIABLE import_output
  ERROR_VARIABLE import_error
)
if(NOT import_result EQUAL 0)
  message(FATAL_ERROR
    "initial STEP import failed (${import_result}):\n"
    "${import_output}${import_error}")
endif()

execute_process(
  COMMAND "${MGED}" -c "${imported}"
    "attr show Extruded_Face"
  RESULT_VARIABLE product_result
  OUTPUT_VARIABLE product_output
  ERROR_VARIABLE product_error
)
execute_process(
  COMMAND "${MGED}" -c "${imported}"
    "attr show Extruded_Face_swept_item"
  RESULT_VARIABLE item_result
  OUTPUT_VARIABLE item_output
  ERROR_VARIABLE item_error
)
if(NOT product_result EQUAL 0 OR NOT item_result EQUAL 0)
  message(FATAL_ERROR
    "could not inspect imported product roles:\n"
    "${product_output}${product_error}${item_output}${item_error}")
endif()
require_text("${product_output}${product_error}" "step:object_role"
  "imported product role")
require_text("${product_output}${product_error}" "product"
  "imported product role")
require_text("${item_output}${item_error}" "step:object_role"
  "imported representation-item role")
require_text("${item_output}${item_error}" "representation_item"
  "imported representation-item role")

# A non-identity member transform makes g-step use the complex transforming
# representation relationship.  Without a CDSR/usage this remains secondary
# geometry of the same product, not an assembly occurrence.
execute_process(
  COMMAND "${MGED}" -c "${imported}"
    "adjust Extruded_Face tree {l Extruded_Face_swept_item {1 0 0 5 0 1 0 0 0 0 1 0 0 0 0 1}}"
  RESULT_VARIABLE transform_result
  OUTPUT_VARIABLE transform_output
  ERROR_VARIABLE transform_error
)
if(NOT transform_result EQUAL 0)
  message(FATAL_ERROR
    "could not place the representation member (${transform_result}):\n"
    "${transform_output}${transform_error}")
endif()

execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict --report "${export_report}"
    -o "${exported}" "${imported}" Extruded_Face
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_output
  ERROR_VARIABLE export_error
)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR
    "layered STEP export failed (${export_result}):\n"
    "${export_output}${export_error}")
endif()

file(READ "${export_report}" export_report_text)
foreach(expected
    "\"outcome\":\"complete\""
    "\"products_updated\":1"
    "\"occurrences_omitted\":0")
  require_text("${export_report_text}" "${expected}"
    "layered export report")
endforeach()

file(READ "${exported}" exported_text)
string(REGEX MATCHALL "=PRODUCT\\(" products "${exported_text}")
list(LENGTH products product_count)
if(NOT product_count EQUAL 1)
  message(FATAL_ERROR
    "representation layering emitted ${product_count} products instead of 1")
endif()
string(REGEX MATCHALL "NEXT_ASSEMBLY_USAGE_OCCURRENCE\\(" usages
  "${exported_text}")
list(LENGTH usages usage_count)
if(NOT usage_count EQUAL 0)
  message(FATAL_ERROR
    "representation layering emitted ${usage_count} product usages")
endif()
require_text("${exported_text}" "REPRESENTATION_RELATIONSHIP_WITH_TRANSFORMATION"
  "transformed product-to-geometry representation relationship")
require_text("${exported_text}" "SHAPE_REPRESENTATION_RELATIONSHIP()"
  "transformed product-to-geometry shape relationship component")

execute_process(
  COMMAND "${STEP_G}" --strict -O "${roundtrip_db}"
    --report "${roundtrip_report}" "${exported}"
  RESULT_VARIABLE roundtrip_result
  OUTPUT_VARIABLE roundtrip_output
  ERROR_VARIABLE roundtrip_error
)
if(NOT roundtrip_result EQUAL 0)
  message(FATAL_ERROR
    "layered STEP reimport failed (${roundtrip_result}):\n"
    "${roundtrip_output}${roundtrip_error}")
endif()
file(READ "${roundtrip_report}" roundtrip_report_text)
foreach(expected
    "\"products\":1"
    "\"occurrences\":0"
    "\"geometry_attempted\":1"
    "\"geometry_written\":1"
    "\"geometry_skipped\":0")
  require_text("${roundtrip_report_text}" "${expected}"
    "layered reimport coverage")
endforeach()

execute_process(
  COMMAND "${MGED}" -c "${roundtrip_db}" "get Extruded_Face"
  RESULT_VARIABLE roundtrip_tree_result
  OUTPUT_VARIABLE roundtrip_tree_output
  ERROR_VARIABLE roundtrip_tree_error
)
if(NOT roundtrip_tree_result EQUAL 0)
  message(FATAL_ERROR
    "could not inspect the round-trip product tree:\n"
    "${roundtrip_tree_output}${roundtrip_tree_error}")
endif()
require_text("${roundtrip_tree_output}${roundtrip_tree_error}" "1 0 0 5"
  "round-trip representation-member transform")
