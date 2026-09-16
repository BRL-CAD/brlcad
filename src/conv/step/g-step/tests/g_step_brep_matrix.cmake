if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED NIRT OR
   NOT DEFINED BREP_TRIMMED OR NOT DEFINED BREP_INVALID OR
   NOT DEFINED BREP_COBB OR NOT DEFINED VOID_INPUT OR
   NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR
    "G_STEP, STEP_G, MGED, NIRT, BRep fixture tools, VOID_INPUT, and OUTPUT_DIR are required")
endif()

set(nirt_cache "${OUTPUT_DIR}/g_step_brep_nirt_cache")
file(MAKE_DIRECTORY "${nirt_cache}")

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}':\n${text}")
  endif()
endfunction()

function(require_entity_count text entity expected description)
  string(REGEX MATCHALL "#[0-9]+=(${entity})\\(" matches "${text}")
  list(LENGTH matches actual)
  if(NOT actual EQUAL expected)
    message(FATAL_ERROR
      "${description}: expected ${expected} ${entity} entities, found ${actual}")
  endif()
endfunction()

function(require_token_count text token expected description)
  string(REGEX MATCHALL "${token}\\(" matches "${text}")
  list(LENGTH matches actual)
  if(NOT actual EQUAL expected)
    message(FATAL_ERROR
      "${description}: expected ${expected} ${token} tokens, found ${actual}")
  endif()
endfunction()

set(input "${OUTPUT_DIR}/g_step_brep_matrix.g")
file(REMOVE "${input}")
execute_process(
  COMMAND "${MGED}" -c "${input}"
    "in sphere.s sph 0 0 0 10; in cylinder.s rcc 30 0 0 0 0 20 6; in cone.s trc 60 0 0 0 0 20 8 3; in torus.s tor 100 0 0 0 0 1 15 4; in box.s rpp 130 150 0 20 0 30; brep sphere.s brep sphere.b; brep cylinder.s brep cylinder.b; brep cone.s brep cone.b; brep torus.s brep torus.b; brep box.s brep box.b"
  RESULT_VARIABLE create_result
  OUTPUT_VARIABLE create_output
  ERROR_VARIABLE create_error
)
if(NOT create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create BRep export matrix:\n${create_output}${create_error}")
endif()

# These BReps exercise singular sphere poles, closed periodic cylinder/cone
# seams, both periodic directions of a torus, and shared planar box topology.
set(topology_checks
  "sphere|1|1|1|2"
  "cylinder|3|3|3|2"
  "cone|3|3|3|2"
  "torus|1|1|2|1"
  "box|6|6|12|8"
)
foreach(schema ap203 ap203e2 ap214 ap242)
  set(step "${OUTPUT_DIR}/g_step_brep_matrix_${schema}.stp")
  set(export_report "${OUTPUT_DIR}/g_step_brep_matrix_${schema}_export.json")
  set(roundtrip "${OUTPUT_DIR}/g_step_brep_matrix_${schema}.roundtrip.g")
  set(import_report "${OUTPUT_DIR}/g_step_brep_matrix_${schema}_import.json")
  file(REMOVE "${step}" "${export_report}" "${roundtrip}" "${import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict --report "${export_report}"
      -o "${step}" "${input}" sphere.b cylinder.b cone.b torus.b box.b
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
  )
  if(NOT export_result EQUAL 0 OR NOT EXISTS "${step}")
    message(FATAL_ERROR
      "${schema} BRep matrix export failed (${export_result}):\n"
      "${export_output}${export_error}")
  endif()
  file(READ "${export_report}" export_report_text)
  foreach(expected
      "\"strict\":true"
      "\"outcome\":\"complete\""
      "\"name\":\"sphere.b\""
      "\"name\":\"cylinder.b\""
      "\"name\":\"cone.b\""
      "\"name\":\"torus.b\""
      "\"name\":\"box.b\""
      "BRep/product representation emitted")
    require_text("${export_report_text}" "${expected}"
      "${schema} BRep matrix export report")
  endforeach()
  file(READ "${step}" step_text)
  require_entity_count("${step_text}" "PRODUCT" 5
    "${schema} BRep matrix products")
  require_entity_count("${step_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION" 5
    "${schema} BRep matrix representations")
  require_entity_count("${step_text}" "MANIFOLD_SOLID_BREP" 5
    "${schema} BRep matrix solids")
  if(step_text MATCHES "#[+]?[0]+=")
    message(FATAL_ERROR "${schema} BRep matrix emitted invalid instance #0")
  endif()

  execute_process(
    COMMAND "${STEP_G}" --strict -O "${roundtrip}" --report "${import_report}"
      "${step}"
    RESULT_VARIABLE import_result
    OUTPUT_VARIABLE import_output
    ERROR_VARIABLE import_error
  )
  if(NOT import_result EQUAL 0 OR NOT EXISTS "${roundtrip}")
    message(FATAL_ERROR
      "${schema} BRep matrix reimport failed (${import_result}):\n"
      "${import_output}${import_error}")
  endif()
  file(READ "${import_report}" import_report_text)
  foreach(expected
      "\"products\":5"
      "\"geometry_attempted\":5,\"geometry_written\":5,\"geometry_skipped\":0,\"outcome\":\"complete\""
      "\"invalid_breps\":0")
    require_text("${import_report_text}" "${expected}"
      "${schema} BRep matrix import report")
  endforeach()

  foreach(check IN LISTS topology_checks)
    string(REPLACE "|" ";" fields "${check}")
    list(POP_FRONT fields object faces surfaces edges vertices)
    execute_process(
      COMMAND "${MGED}" -c "${roundtrip}" brep "${object}_b_item.s" info
      RESULT_VARIABLE info_result
      OUTPUT_VARIABLE info_output
      ERROR_VARIABLE info_error
    )
    set(info_text "${info_output}${info_error}")
    if(NOT info_result EQUAL 0)
      message(FATAL_ERROR
        "could not inspect ${schema} ${object} BRep:\n${info_text}")
    endif()
    foreach(expected
        "Valid: YES, Solid: YES"
        "faces:     ${faces}"
        "surfaces:  ${surfaces}"
        "edges:     ${edges}"
        "vertices:  ${vertices}")
      require_text("${info_text}" "${expected}"
        "${schema} ${object} BRep topology")
    endforeach()
  endforeach()

  # Topology counts can accept a BRep whose trimmed exit face is unusable.
  # Keep the probe off the diamond parameterization's UV symmetry axes: an
  # upward classification ray through a trim vertex is a numerical edge case,
  # not an interior geometry check.
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${nirt_cache}"
      "${NIRT}" -H 0 -b -f csv
      -e "xyz 139 10 100; dir 0 0 -1; s; q"
      "${roundtrip}" box_b_item.s
    RESULT_VARIABLE box_ray_result
    OUTPUT_VARIABLE box_ray_output
    ERROR_VARIABLE box_ray_error
    TIMEOUT 30
  )
  set(box_ray_text "${box_ray_output}\n${box_ray_error}")
  if(NOT box_ray_result EQUAL 0 OR NOT box_ray_text MATCHES
      ",139\\.000000,10\\.000000,30\\.000000,30\\.000000,139\\.000000,10\\.000000,0\\.000000,0\\.000000,30\\.000000,")
    message(FATAL_ERROR
      "${schema} box BRep ray-equivalence test failed:\n${box_ray_text}")
  endif()
endforeach()

# Generate independent OpenNURBS inputs for the topology classes that cannot be
# made with MGED's primitive-to-BRep conversion alone: an open non-rational
# NURBS face with an inner loop, a six-patch rational NURBS surface model, and
# a structurally invalid BRep used to prove transactional rejection.
file(REMOVE
  "${OUTPUT_DIR}/brep_trimmed.g"
  "${OUTPUT_DIR}/brep_invalid.g"
  "${OUTPUT_DIR}/g_step_brep_cobb.g")
foreach(generator IN ITEMS BREP_TRIMMED BREP_INVALID)
  execute_process(
    COMMAND "${${generator}}"
    WORKING_DIRECTORY "${OUTPUT_DIR}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_output
    ERROR_VARIABLE generator_error
  )
  if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR
      "${generator} failed (${generator_result}):\n"
      "${generator_output}${generator_error}")
  endif()
endforeach()
execute_process(
  COMMAND "${BREP_COBB}" g_step_brep_cobb.g
  WORKING_DIRECTORY "${OUTPUT_DIR}"
  RESULT_VARIABLE cobb_result
  OUTPUT_VARIABLE cobb_output
  ERROR_VARIABLE cobb_error
)
if(NOT cobb_result EQUAL 0)
  message(FATAL_ERROR
    "BREP_COBB failed (${cobb_result}):\n${cobb_output}${cobb_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242)
  set(open_step "${OUTPUT_DIR}/g_step_brep_open_${schema}.stp")
  set(open_export_report
    "${OUTPUT_DIR}/g_step_brep_open_${schema}_export.json")
  set(open_roundtrip "${OUTPUT_DIR}/g_step_brep_open_${schema}.roundtrip.g")
  set(open_import_report
    "${OUTPUT_DIR}/g_step_brep_open_${schema}_import.json")
  file(REMOVE "${open_step}" "${open_export_report}" "${open_roundtrip}"
    "${open_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${open_export_report}" -o "${open_step}"
      "${OUTPUT_DIR}/brep_trimmed.g" brep_trimmed.s
    RESULT_VARIABLE open_export_result
    OUTPUT_VARIABLE open_export_output
    ERROR_VARIABLE open_export_error
  )
  if(NOT open_export_result EQUAL 0 OR NOT EXISTS "${open_step}")
    message(FATAL_ERROR
      "${schema} open BRep export failed (${open_export_result}):\n"
      "${open_export_output}${open_export_error}")
  endif()
  file(READ "${open_step}" open_step_text)
  foreach(entity_count
      "MANIFOLD_SURFACE_SHAPE_REPRESENTATION|1"
      "SHELL_BASED_SURFACE_MODEL|1"
      "OPEN_SHELL|1"
      "FACE_BOUND|1"
      "FACE_OUTER_BOUND|1"
      "B_SPLINE_SURFACE_WITH_KNOTS|1")
    string(REPLACE "|" ";" fields "${entity_count}")
    list(POP_FRONT fields entity expected)
    require_entity_count("${open_step_text}" "${entity}" "${expected}"
      "${schema} open BRep")
  endforeach()
  require_entity_count("${open_step_text}" "MANIFOLD_SOLID_BREP" 0
    "${schema} open BRep solid misclassification")

  execute_process(
    COMMAND "${STEP_G}" --strict -O "${open_roundtrip}"
      --report "${open_import_report}" "${open_step}"
    RESULT_VARIABLE open_import_result
    OUTPUT_VARIABLE open_import_output
    ERROR_VARIABLE open_import_error
  )
  if(NOT open_import_result EQUAL 0 OR NOT EXISTS "${open_roundtrip}")
    message(FATAL_ERROR
      "${schema} open BRep reimport failed (${open_import_result}):\n"
      "${open_import_output}${open_import_error}")
  endif()
  file(READ "${open_import_report}" open_import_report_text)
  foreach(expected
      "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
      "\"MANIFOLD_SURFACE_SHAPE_REPRESENTATION\":1"
      "\"SHELL_BASED_SURFACE_MODEL\":1"
      "\"outcome\":\"complete\"")
    require_text("${open_import_report_text}" "${expected}"
      "${schema} open BRep import report")
  endforeach()
  execute_process(
    COMMAND "${MGED}" -c "${open_roundtrip}"
      brep brep_trimmed_s_item.s info
    RESULT_VARIABLE open_info_result
    OUTPUT_VARIABLE open_info_output
    ERROR_VARIABLE open_info_error
  )
  set(open_info_text "${open_info_output}${open_info_error}")
  foreach(expected
      "Valid: YES, Solid: NO"
      "faces:     1"
      "edges:     8"
      "vertices:  8"
      "loops:     2")
    require_text("${open_info_text}" "${expected}"
      "${schema} open BRep topology")
  endforeach()

  set(rational_step "${OUTPUT_DIR}/g_step_brep_rational_${schema}.stp")
  set(rational_report
    "${OUTPUT_DIR}/g_step_brep_rational_${schema}_export.json")
  set(rational_roundtrip
    "${OUTPUT_DIR}/g_step_brep_rational_${schema}.roundtrip.g")
  set(rational_import_report
    "${OUTPUT_DIR}/g_step_brep_rational_${schema}_import.json")
  file(REMOVE "${rational_step}" "${rational_report}"
    "${rational_roundtrip}" "${rational_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${rational_report}" -o "${rational_step}"
      "${OUTPUT_DIR}/g_step_brep_cobb.g" cobb.s
    RESULT_VARIABLE rational_export_result
    OUTPUT_VARIABLE rational_export_output
    ERROR_VARIABLE rational_export_error
  )
  if(NOT rational_export_result EQUAL 0 OR NOT EXISTS "${rational_step}")
    message(FATAL_ERROR
      "${schema} rational BRep export failed (${rational_export_result}):\n"
      "${rational_export_output}${rational_export_error}")
  endif()
  file(READ "${rational_step}" rational_step_text)
  require_entity_count("${rational_step_text}"
    "MANIFOLD_SURFACE_SHAPE_REPRESENTATION" 1
    "${schema} rational surface representation")
  require_entity_count("${rational_step_text}" "OPEN_SHELL" 6
    "${schema} rational surface shells")
  require_token_count("${rational_step_text}" "RATIONAL_B_SPLINE_SURFACE" 6
    "${schema} rational surfaces")
  require_token_count("${rational_step_text}" "RATIONAL_B_SPLINE_CURVE" 24
    "${schema} rational boundary curves")

  execute_process(
    COMMAND "${STEP_G}" --strict -O "${rational_roundtrip}"
      --report "${rational_import_report}" "${rational_step}"
    RESULT_VARIABLE rational_import_result
    OUTPUT_VARIABLE rational_import_output
    ERROR_VARIABLE rational_import_error
  )
  if(NOT rational_import_result EQUAL 0 OR NOT EXISTS "${rational_roundtrip}")
    message(FATAL_ERROR
      "${schema} rational BRep reimport failed (${rational_import_result}):\n"
      "${rational_import_output}${rational_import_error}")
  endif()
  file(READ "${rational_import_report}" rational_import_report_text)
  foreach(expected
      "\"products\":1"
      "\"geometry_attempted\":6,\"geometry_written\":6,\"geometry_skipped\":0"
      "\"invalid_breps\":0"
      "\"outcome\":\"complete\"")
    require_text("${rational_import_report_text}" "${expected}"
      "${schema} rational BRep import report")
  endforeach()
  foreach(shell RANGE 1 6)
    execute_process(
      COMMAND "${MGED}" -c "${rational_roundtrip}"
        brep "cobb_s_item_shell${shell}.s" info
      RESULT_VARIABLE rational_info_result
      OUTPUT_VARIABLE rational_info_output
      ERROR_VARIABLE rational_info_error
    )
    set(rational_info_text "${rational_info_output}${rational_info_error}")
    foreach(expected
        "Valid: YES, Solid: NO"
        "faces:     1"
        "edges:     4"
        "vertices:  4")
      require_text("${rational_info_text}" "${expected}"
        "${schema} rational BRep shell ${shell}")
    endforeach()
  endforeach()
endforeach()

# A checked-in AP214 BREP_WITH_VOIDS fixture supplies a known cavity without
# depending on Boolean evaluation.  Exporting its exact BRL-CAD BRep must
# reconstruct the two shells and the required false-oriented void wrapper in
# every target schema.
set(void_source "${OUTPUT_DIR}/g_step_brep_void_source.g")
set(void_source_report "${OUTPUT_DIR}/g_step_brep_void_source.json")
file(REMOVE "${void_source}" "${void_source_report}")
execute_process(
  COMMAND "${STEP_G}" --schema ap214 --strict -O "${void_source}"
    --report "${void_source_report}" "${VOID_INPUT}"
  RESULT_VARIABLE void_source_result
  OUTPUT_VARIABLE void_source_output
  ERROR_VARIABLE void_source_error
)
if(NOT void_source_result EQUAL 0 OR NOT EXISTS "${void_source}")
  message(FATAL_ERROR
    "could not prepare BRep-with-voids source (${void_source_result}):\n"
    "${void_source_output}${void_source_error}")
endif()
execute_process(
  COMMAND "${MGED}" -c "${void_source}"
    "attr rm Void_Tetra_item.s step:style_name step:color_rgb step:layers step:style_source_ids"
  RESULT_VARIABLE void_metadata_result
  OUTPUT_VARIABLE void_metadata_output
  ERROR_VARIABLE void_metadata_error
)
if(NOT void_metadata_result EQUAL 0)
  message(FATAL_ERROR
    "could not isolate void geometry from AP214 presentation metadata:\n"
    "${void_metadata_output}${void_metadata_error}")
endif()

foreach(schema ap203 ap203e2 ap214 ap242)
  set(void_step "${OUTPUT_DIR}/g_step_brep_void_${schema}.stp")
  set(void_export_report "${OUTPUT_DIR}/g_step_brep_void_${schema}_export.json")
  set(void_roundtrip "${OUTPUT_DIR}/g_step_brep_void_${schema}.roundtrip.g")
  set(void_import_report "${OUTPUT_DIR}/g_step_brep_void_${schema}_import.json")
  file(REMOVE "${void_step}" "${void_export_report}" "${void_roundtrip}"
    "${void_import_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --strict
      --report "${void_export_report}" -o "${void_step}"
      "${void_source}" Void_Tetra_item.s
    RESULT_VARIABLE void_export_result
    OUTPUT_VARIABLE void_export_output
    ERROR_VARIABLE void_export_error
  )
  if(NOT void_export_result EQUAL 0 OR NOT EXISTS "${void_step}")
    message(FATAL_ERROR
      "${schema} void BRep export failed (${void_export_result}):\n"
      "${void_export_output}${void_export_error}")
  endif()
  file(READ "${void_step}" void_step_text)
  require_entity_count("${void_step_text}" "BREP_WITH_VOIDS" 1
    "${schema} void solid")
  require_entity_count("${void_step_text}" "ORIENTED_CLOSED_SHELL" 1
    "${schema} oriented void shell")
  require_entity_count("${void_step_text}" "CLOSED_SHELL" 2
    "${schema} closed shells")
  require_text("${void_step_text}" ".F.);"
    "${schema} false-oriented void shell")

  execute_process(
    COMMAND "${STEP_G}" --strict -O "${void_roundtrip}"
      --report "${void_import_report}" "${void_step}"
    RESULT_VARIABLE void_import_result
    OUTPUT_VARIABLE void_import_output
    ERROR_VARIABLE void_import_error
  )
  if(NOT void_import_result EQUAL 0 OR NOT EXISTS "${void_roundtrip}")
    message(FATAL_ERROR
      "${schema} void BRep reimport failed (${void_import_result}):\n"
      "${void_import_output}${void_import_error}")
  endif()
  file(READ "${void_import_report}" void_import_report_text)
  foreach(expected
      "\"BREP_WITH_VOIDS\":1"
      "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
      "\"invalid_breps\":0"
      "\"outcome\":\"complete\"")
    require_text("${void_import_report_text}" "${expected}"
      "${schema} void BRep import report")
  endforeach()
  execute_process(
    COMMAND "${MGED}" -c "${void_roundtrip}"
      brep tetrahedral_cavity_item.s info
    RESULT_VARIABLE void_info_result
    OUTPUT_VARIABLE void_info_output
    ERROR_VARIABLE void_info_error
  )
  set(void_info_text "${void_info_output}${void_info_error}")
  foreach(expected
      "Valid: YES, Solid: YES"
      "faces:     8"
      "edges:     12"
      "vertices:  8")
    require_text("${void_info_text}" "${expected}"
      "${schema} void BRep topology")
  endforeach()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${nirt_cache}"
      "${NIRT}" -H 0 -b -f csv
      -e "xyz 1 2 30; dir 0 0 -1; s; q"
      "${void_roundtrip}" tetrahedral_cavity_item.s
    RESULT_VARIABLE void_material_ray_result
    OUTPUT_VARIABLE void_material_ray_output
    ERROR_VARIABLE void_material_ray_error
    TIMEOUT 30
  )
  set(void_material_ray_text
    "${void_material_ray_output}\n${void_material_ray_error}")
  if(NOT void_material_ray_result EQUAL 0 OR
      NOT void_material_ray_text MATCHES
        ",1\\.000000,2\\.000000,17\\.000000,17\\.000000,1\\.000000,2\\.000000,0\\.000000,0\\.000000,17\\.000000,")
    message(FATAL_ERROR
      "${schema} void BRep material ray failed:\n${void_material_ray_text}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "XDG_CACHE_HOME=${nirt_cache}"
      "${NIRT}" -H 0 -b -f csv
      -e "xyz 2.5 2.75 30; dir 0 0 -1; s; q"
      "${void_roundtrip}" tetrahedral_cavity_item.s
    RESULT_VARIABLE void_cavity_ray_result
    OUTPUT_VARIABLE void_cavity_ray_output
    ERROR_VARIABLE void_cavity_ray_error
    TIMEOUT 30
  )
  set(void_cavity_ray_text
    "${void_cavity_ray_output}\n${void_cavity_ray_error}")
  foreach(expected
      ",2.500000,2.750000,14.750000,14.750000,2.500000,2.750000,3.750000,3.750000,11.000000,"
      ",2.500000,2.750000,2.000000,2.000000,2.500000,2.750000,0.000000,0.000000,2.000000,")
    require_text("${void_cavity_ray_text}" "${expected}"
      "${schema} void BRep cavity ray")
  endforeach()
  if(NOT void_cavity_ray_result EQUAL 0)
    message(FATAL_ERROR
      "${schema} void BRep cavity ray failed:\n${void_cavity_ray_text}")
  endif()
endforeach()

# Invalid BReps are never serialized as syntactically plausible closed solids.
# Permissive mode retains a valid peer and reports partial coverage; strict
# mode is transactional and publishes nothing.
set(mixed_input "${OUTPUT_DIR}/g_step_brep_invalid_mixed.g")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy
    "${OUTPUT_DIR}/brep_trimmed.g" "${mixed_input}"
  RESULT_VARIABLE mixed_copy_result
)
if(NOT mixed_copy_result EQUAL 0)
  message(FATAL_ERROR "could not prepare mixed invalid-BRep database")
endif()
execute_process(
  COMMAND "${MGED}" -c "${mixed_input}"
    "dbconcat ${OUTPUT_DIR}/brep_invalid.g invalid_"
  RESULT_VARIABLE mixed_concat_result
  OUTPUT_VARIABLE mixed_concat_output
  ERROR_VARIABLE mixed_concat_error
)
if(NOT mixed_concat_result EQUAL 0)
  message(FATAL_ERROR
    "could not append invalid BRep fixture:\n"
    "${mixed_concat_output}${mixed_concat_error}")
endif()

set(invalid_step "${OUTPUT_DIR}/g_step_brep_invalid_partial.stp")
set(invalid_report "${OUTPUT_DIR}/g_step_brep_invalid_partial.json")
set(invalid_strict_step "${OUTPUT_DIR}/g_step_brep_invalid_strict.stp")
set(invalid_strict_report "${OUTPUT_DIR}/g_step_brep_invalid_strict.json")
file(REMOVE "${invalid_step}" "${invalid_report}" "${invalid_strict_step}"
  "${invalid_strict_report}")
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --report "${invalid_report}"
    -o "${invalid_step}" "${mixed_input}"
    brep_trimmed.s invalid_brep_invalid.s
  RESULT_VARIABLE invalid_result
  OUTPUT_VARIABLE invalid_output
  ERROR_VARIABLE invalid_error
)
if(NOT invalid_result EQUAL 1 OR NOT EXISTS "${invalid_step}")
  message(FATAL_ERROR
    "permissive invalid-BRep export returned ${invalid_result}:\n"
    "${invalid_output}${invalid_error}")
endif()
file(READ "${invalid_report}" invalid_report_text)
foreach(expected
    "\"outcome\":\"partial\""
    "\"name\":\"brep_trimmed.s\""
    "\"status\":\"handled\""
    "\"name\":\"invalid_brep_invalid.s\""
    "\"status\":\"unsupported\""
    "OpenNURBS BRep is invalid and cannot be exported safely")
  require_text("${invalid_report_text}" "${expected}"
    "permissive invalid-BRep report")
endforeach()
file(READ "${invalid_step}" invalid_step_text)
require_entity_count("${invalid_step_text}" "PRODUCT" 1
  "permissive invalid-BRep products")
require_entity_count("${invalid_step_text}" "OPEN_SHELL" 1
  "permissive invalid-BRep retained surface")

execute_process(
  COMMAND "${G_STEP}" --schema ap214 --strict
    --report "${invalid_strict_report}" -o "${invalid_strict_step}"
    "${mixed_input}" brep_trimmed.s invalid_brep_invalid.s
  RESULT_VARIABLE invalid_strict_result
  OUTPUT_VARIABLE invalid_strict_output
  ERROR_VARIABLE invalid_strict_error
)
if(NOT invalid_strict_result EQUAL 4 OR EXISTS "${invalid_strict_step}")
  message(FATAL_ERROR
    "strict invalid-BRep export returned ${invalid_strict_result} or "
    "published output:\n${invalid_strict_output}${invalid_strict_error}")
endif()
file(READ "${invalid_strict_report}" invalid_strict_report_text)
foreach(expected
    "\"strict\":true"
    "\"exit_status\":4"
    "\"outcome\":\"partial\""
    "OpenNURBS BRep is invalid and cannot be exported safely")
  require_text("${invalid_strict_report_text}" "${expected}"
    "strict invalid-BRep report")
endforeach()
