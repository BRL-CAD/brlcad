if(NOT DEFINED G_STEP OR NOT DEFINED STEP_G OR NOT DEFINED MGED OR
   NOT DEFINED ASC2G OR NOT DEFINED SOURCE_DB_DIR OR NOT DEFINED OUTPUT_DIR)
  message(FATAL_ERROR
    "G_STEP, STEP_G, MGED, ASC2G, SOURCE_DB_DIR, and OUTPUT_DIR are required")
endif()

function(require_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${description}: missing '${needle}'")
  endif()
endfunction()

function(reject_text text needle description)
  string(FIND "${text}" "${needle}" found)
  if(NOT found EQUAL -1)
    message(FATAL_ERROR "${description}: unexpectedly contains '${needle}'")
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

function(require_region_primitive_equal source_db source_region result_db
    result_object description)
  string(REGEX REPLACE "[.]r$" "" source_stem "${source_region}")
  set(source_result 1)
  foreach(source_leaf "${source_stem}.s" "${source_stem}")
    execute_process(
      COMMAND "${MGED}" -c "${source_db}" db get "${source_leaf}"
      RESULT_VARIABLE source_result
      OUTPUT_VARIABLE source_text
      ERROR_VARIABLE source_error
    )
    if(source_result EQUAL 0)
      break()
    endif()
  endforeach()
  execute_process(
    COMMAND "${MGED}" -c "${result_db}" db get "${result_object}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE result_text
    ERROR_VARIABLE result_error
  )
  string(STRIP "${source_text}" source_text)
  string(STRIP "${result_text}" result_text)
  if(NOT source_result EQUAL 0 OR NOT result EQUAL 0 OR
     NOT source_text STREQUAL result_text)
    message(FATAL_ERROR
      "${description}: exact primitive changed:\n"
      "source=${source_text}\nroundtrip=${result_text}")
  endif()
endfunction()

function(run_export schema input object output native)
  if(native)
    set(native_arg --native-csg)
  else()
    set(native_arg)
  endif()
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" ${native_arg}
      -o "${output}" "${input}" "${object}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  # A non-native boolean tree is intentionally partial until evaluated-BRep
  # export exists.  Permissive mode publishes its usable child geometry and
  # reports status 1; native CSG cases must remain complete.
  if((native AND NOT result EQUAL 0) OR
     (NOT native AND NOT result EQUAL 0 AND NOT result EQUAL 1) OR
     NOT EXISTS "${output}")
    message(FATAL_ERROR
      "${schema} export of ${object} returned ${result}:\n${stdout}${stderr}")
  endif()
endfunction()

function(run_import step output report)
  execute_process(
    COMMAND "${STEP_G}" -O "${output}" --report "${report}" "${step}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "STEP reimport returned ${result}:\n${stdout}${stderr}")
  endif()
  file(READ "${report}" report_text)
  require_text("${report_text}"
    "\"geometry_attempted\":1,\"geometry_written\":1,\"geometry_skipped\":0"
    "round-trip report")
endfunction()

set(boolean_db "${OUTPUT_DIR}/g_step_native_csg_boolean_ops.g")
set(primitive_db "${OUTPUT_DIR}/g_step_native_csg_primitives.g")
set(operator_db "${OUTPUT_DIR}/g_step_native_csg_operators_input.g")
function(convert_fixture name g_file)
  file(REMOVE "${g_file}")
  execute_process(
    COMMAND "${ASC2G}" "${SOURCE_DB_DIR}/${name}.asc" "${g_file}"
    RESULT_VARIABLE convert_result
    OUTPUT_VARIABLE convert_output
    ERROR_VARIABLE convert_error
  )
  if(NOT convert_result EQUAL 0 OR NOT EXISTS "${g_file}")
    message(FATAL_ERROR
      "could not prepare ${name}.g fixture:\n${convert_output}${convert_error}")
  endif()
endfunction()
convert_fixture(boolean-ops "${boolean_db}")
convert_fixture(primitives "${primitive_db}")
convert_fixture(operators "${operator_db}")

# Native CSG is deliberately opt-in.  The existing default remains the
# ordinary BRep-oriented export path.
set(default_step "${OUTPUT_DIR}/g_step_native_csg_default.stp")
file(REMOVE "${default_step}")
run_export(ap214 "${boolean_db}" a-b+c "${default_step}" FALSE)
file(READ "${default_step}" default_text)
reject_text("${default_text}" "CSG_SOLID(" "default AP214 export")
reject_text("${default_text}" "BOOLEAN_RESULT(" "default AP214 export")

set(default203e2_step "${OUTPUT_DIR}/g_step_native_csg_default_ap203e2.stp")
file(REMOVE "${default203e2_step}")
run_export(ap203e2 "${boolean_db}" a-b+c "${default203e2_step}" FALSE)
file(READ "${default203e2_step}" default203e2_text)
reject_text("${default203e2_text}" "CSG_SOLID(" "default AP203e2 export")
reject_text("${default203e2_text}" "BOOLEAN_RESULT(" "default AP203e2 export")

# An unbounded half space is a valid operand but not a legal CSG_SOLID root.
# Reject it transactionally instead of emitting an orphan or changing meaning.
set(half_db "${OUTPUT_DIR}/g_step_native_csg_half_root.g")
set(half_step "${OUTPUT_DIR}/g_step_native_csg_half_root.stp")
file(REMOVE "${half_db}" "${half_step}")
execute_process(
  COMMAND "${MGED}" -c "${half_db}" "put half.s half N {0 0 1} d 0"
  RESULT_VARIABLE half_create_result
  OUTPUT_VARIABLE half_create_output
  ERROR_VARIABLE half_create_error
)
if(NOT half_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create root half-space fixture:\n"
    "${half_create_output}${half_create_error}")
endif()
execute_process(
  COMMAND "${G_STEP}" --schema ap214 --native-csg
    -o "${half_step}" "${half_db}" half.s
  RESULT_VARIABLE half_result
  OUTPUT_VARIABLE half_output
  ERROR_VARIABLE half_error
)
if(half_result EQUAL 0 OR EXISTS "${half_step}")
  message(FATAL_ERROR
    "root half-space export was not rejected transactionally:\n"
    "${half_output}${half_error}")
endif()
set(half_diagnostic "${half_output}${half_error}")
require_text("${half_diagnostic}" "legal only as a boolean operand"
  "root half-space diagnostic")

# boolean-ops.g supplies an exact AP block, an exact AP cylinder, and an ARB8
# that must become a BRep operand.  It therefore exercises both preservation
# of the difference/intersection tree and the mixed CSG/BRep operand facility.
foreach(schema ap203e2 ap214 ap242)
  set(step "${OUTPUT_DIR}/g_step_native_csg_${schema}.stp")
  set(roundtrip "${OUTPUT_DIR}/g_step_native_csg_${schema}.g")
  set(report "${OUTPUT_DIR}/g_step_native_csg_${schema}.json")
  file(REMOVE "${step}" "${roundtrip}" "${report}")
  run_export("${schema}" "${boolean_db}" a-b+c "${step}" TRUE)

  file(READ "${step}" step_text)
  foreach(entity
      "CSG_SHAPE_REPRESENTATION("
      "CSG_SOLID("
      "BLOCK("
      "RIGHT_CIRCULAR_CYLINDER("
      "MANIFOLD_SOLID_BREP(")
    require_text("${step_text}" "${entity}" "${schema} mixed CSG export")
  endforeach()
  require_text("${step_text}" ".DIFFERENCE." "${schema} mixed CSG export")
  require_text("${step_text}" ".INTERSECTION." "${schema} mixed CSG export")
  require_entity_count("${step_text}" "BOOLEAN_RESULT" 2 "${schema} mixed CSG export")
  require_entity_count("${step_text}" "PRODUCT" 1 "${schema} mixed CSG export")

  run_import("${step}" "${roundtrip}" "${report}")

  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" ls
    RESULT_VARIABLE ls_result
    OUTPUT_VARIABLE objects
    ERROR_VARIABLE ls_error
  )
  if(NOT ls_result EQUAL 0)
    message(FATAL_ERROR "could not list ${schema} round-trip database: ${ls_error}")
  endif()
  string(APPEND objects "${ls_error}")
  string(REGEX MATCH "a_b_c_csg_node_step[0-9]+" root_node "${objects}")
  string(REGEX MATCH "a_b_c_csg_primitive_step[0-9]+[.]s" cylinder "${objects}")
  if(root_node STREQUAL "" OR cylinder STREQUAL "")
    message(FATAL_ERROR "${schema} round-trip omitted expected CSG objects:\n${objects}")
  endif()

  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" db get a_b_c_csg_node
    RESULT_VARIABLE difference_result
    OUTPUT_VARIABLE difference_tree
    ERROR_VARIABLE difference_error
  )
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" db get "${root_node}"
    RESULT_VARIABLE root_result
    OUTPUT_VARIABLE root_tree
    ERROR_VARIABLE root_error
  )
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" db get a_b_c_csg_primitive.s
    RESULT_VARIABLE block_result
    OUTPUT_VARIABLE block_text
    ERROR_VARIABLE block_error
  )
  execute_process(
    COMMAND "${MGED}" -c "${roundtrip}" db get "${cylinder}"
    RESULT_VARIABLE cylinder_result
    OUTPUT_VARIABLE cylinder_text
    ERROR_VARIABLE cylinder_error
  )
  if(NOT difference_result EQUAL 0 OR NOT root_result EQUAL 0 OR
     NOT block_result EQUAL 0 OR NOT cylinder_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} reconstructed CSG tree:\n"
      "${difference_error}${root_error}${block_error}${cylinder_error}")
  endif()
  string(APPEND difference_tree "${difference_error}")
  string(APPEND root_tree "${root_error}")
  string(APPEND block_text "${block_error}")
  string(APPEND cylinder_text "${cylinder_error}")
  require_text("${difference_tree}" "tree {-" "${schema} reconstructed difference")
  require_text("${root_tree}" "tree {n" "${schema} reconstructed intersection")
  require_text("${block_text}" "arb8 " "${schema} reconstructed block")
  require_text("${cylinder_text}" "tgc " "${schema} reconstructed cylinder")
endforeach()

# Exercise every implicit primitive common to AP203e2 and AP214 using an
# independently constructed BRL-CAD tree.  This supplements the mixed-BRep
# test above and makes AP203e2 exporter coverage explicit.
set(core203e2_db "${OUTPUT_DIR}/g_step_native_csg_ap203e2_core.g")
set(core203e2_step "${OUTPUT_DIR}/g_step_native_csg_ap203e2_core.stp")
set(core203e2_g "${OUTPUT_DIR}/g_step_native_csg_ap203e2_core_roundtrip.g")
set(core203e2_report "${OUTPUT_DIR}/g_step_native_csg_ap203e2_core.json")
file(REMOVE "${core203e2_db}" "${core203e2_step}" "${core203e2_g}"
  "${core203e2_report}")
execute_process(
  COMMAND "${MGED}" -c "${core203e2_db}"
    "in body.s rpp 0 20 0 20 0 20; in bore.s rcc 0 0 0 0 0 20 3; in boss.s sph 30 0 5 5; in ring.s tor 40 0 0 0 0 1 8 2; in wedge.s arb8 60 0 0 70 0 0 70 8 0 60 8 0 60 0 6 64 0 6 64 8 6 60 8 6; in frustum.s trc 80 0 0 0 0 10 5 3; in clip.s half 0 0 1 10; r native.r u body.s - bore.s u boss.s u ring.s u wedge.s u frustum.s + clip.s"
  RESULT_VARIABLE core203e2_create_result
  OUTPUT_VARIABLE core203e2_create_output
  ERROR_VARIABLE core203e2_create_error
)
if(NOT core203e2_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create AP203e2 core CSG fixture:\n"
    "${core203e2_create_output}${core203e2_create_error}")
endif()
run_export(ap203e2 "${core203e2_db}" native.r "${core203e2_step}" TRUE)
file(READ "${core203e2_step}" core203e2_text)
foreach(entity
    "BLOCK("
    "RIGHT_CIRCULAR_CYLINDER("
    "SPHERE("
    "TORUS("
    "RIGHT_ANGULAR_WEDGE("
    "RIGHT_CIRCULAR_CONE("
    "HALF_SPACE_SOLID("
    "BOOLEAN_RESULT("
    "CSG_SOLID(")
  require_text("${core203e2_text}" "${entity}" "AP203e2 core CSG export")
endforeach()
reject_text("${core203e2_text}" "MANIFOLD_SOLID_BREP("
  "AP203e2 core CSG export")
run_import("${core203e2_step}" "${core203e2_g}" "${core203e2_report}")

# Keep product occurrence structure outside native Boolean shapes.  The
# assembly has two transformed uses of one CSG region: it must produce exactly
# two STEP products (assembly and region), not flatten the region into the
# assembly or create orphan products for the CSG operands.
set(layer_db "${OUTPUT_DIR}/g_step_native_csg_layered.g")
file(REMOVE "${layer_db}")
execute_process(
  COMMAND "${MGED}" -c "${layer_db}"
    "in block.s rpp 0 20 0 20 0 20; in bore.s rcc 10 10 0 0 0 20 4; put cut.r comb region yes tree {- {l block.s} {l bore.s}}; put assembly.g comb region no tree {u {l cut.r} {l cut.r {1 0 0 30  0 1 0 0  0 0 1 0  0 0 0 1}}}"
  RESULT_VARIABLE layer_create_result
  OUTPUT_VARIABLE layer_create_output
  ERROR_VARIABLE layer_create_error
)
if(NOT layer_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create layered native CSG fixture:\n"
    "${layer_create_output}${layer_create_error}")
endif()
foreach(schema ap203e2 ap214 ap242)
  set(layer_step "${OUTPUT_DIR}/g_step_native_csg_layered_${schema}.stp")
  set(layer_export_report
    "${OUTPUT_DIR}/g_step_native_csg_layered_${schema}_export.json")
  set(layer_g "${OUTPUT_DIR}/g_step_native_csg_layered_${schema}.g")
  set(layer_report "${OUTPUT_DIR}/g_step_native_csg_layered_${schema}.json")
  file(REMOVE "${layer_step}" "${layer_export_report}" "${layer_g}"
    "${layer_report}")
  execute_process(
    COMMAND "${G_STEP}" --schema "${schema}" --native-csg --strict
      --report "${layer_export_report}" -o "${layer_step}" "${layer_db}" assembly.g
    RESULT_VARIABLE layer_export_result
    OUTPUT_VARIABLE layer_export_output
    ERROR_VARIABLE layer_export_error
  )
  if(NOT layer_export_result EQUAL 0 OR NOT EXISTS "${layer_step}")
    message(FATAL_ERROR
      "${schema} layered native CSG export failed (${layer_export_result}):\n"
      "${layer_export_output}${layer_export_error}")
  endif()
  file(READ "${layer_step}" layer_text)
  require_entity_count("${layer_text}" "PRODUCT" 2
    "${schema} layered native CSG products")
  require_entity_count("${layer_text}" "CSG_SOLID" 1
    "${schema} layered native CSG shape")
  require_entity_count("${layer_text}" "BOOLEAN_RESULT" 1
    "${schema} layered native CSG Boolean")
  require_entity_count("${layer_text}" "NEXT_ASSEMBLY_USAGE_OCCURRENCE" 2
    "${schema} layered native CSG occurrences")
  require_entity_count("${layer_text}" "CONTEXT_DEPENDENT_SHAPE_REPRESENTATION" 2
    "${schema} layered native CSG placements")
  reject_text("${layer_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION("
    "${schema} layered native CSG operand products")
  file(READ "${layer_export_report}" layer_export_report_text)
  foreach(expected
      "\"outcome\":\"complete\""
      "\"name\":\"assembly.g\""
      "assembly/product representation emitted"
      "\"name\":\"cut.r\""
      "exported as a schema-native CSG root"
      "preserved as an operand of a schema-native CSG tree")
    require_text("${layer_export_report_text}" "${expected}"
      "${schema} layered native CSG coverage")
  endforeach()

  run_import("${layer_step}" "${layer_g}" "${layer_report}")
  file(READ "${layer_report}" layer_import_report_text)
  foreach(expected "\"products\":2" "\"occurrences\":2")
    require_text("${layer_import_report_text}" "${expected}"
      "${schema} layered native CSG import report")
  endforeach()
  execute_process(
    COMMAND "${MGED}" -c "${layer_g}" db get assembly_g
    RESULT_VARIABLE layer_tree_result
    OUTPUT_VARIABLE layer_tree
    ERROR_VARIABLE layer_tree_error
  )
  string(APPEND layer_tree "${layer_tree_error}")
  if(NOT layer_tree_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} layered assembly:\n${layer_tree}")
  endif()
  foreach(expected "tree {u {l cut_r} {l cut_r {" "1 0 0 30")
    require_text("${layer_tree}" "${expected}"
      "${schema} layered native CSG assembly")
  endforeach()
  execute_process(
    COMMAND "${MGED}" -c "${layer_g}" db get cut_r_csg_node
    RESULT_VARIABLE layer_csg_result
    OUTPUT_VARIABLE layer_csg_tree
    ERROR_VARIABLE layer_csg_error
  )
  string(APPEND layer_csg_tree "${layer_csg_error}")
  if(NOT layer_csg_result EQUAL 0)
    message(FATAL_ERROR
      "could not inspect ${schema} layered CSG child:\n${layer_csg_tree}")
  endif()
  require_text("${layer_csg_tree}" "tree {-"
    "${schema} layered native CSG child")
endforeach()

# The operators fixture retains its nested region wrappers and leaf matrix.
# The wrappers are expanded, the sphere remains implicit, and the transformed
# EHY becomes a closed BRep operand.  Its reconstructed bounds demonstrate
# both that the EHY is a manifold solid and that its leaf matrix was applied.
set(operator_step "${OUTPUT_DIR}/g_step_native_csg_operators.stp")
set(operator_g "${OUTPUT_DIR}/g_step_native_csg_operators.g")
set(operator_report "${OUTPUT_DIR}/g_step_native_csg_operators.json")
file(REMOVE "${operator_step}" "${operator_g}" "${operator_report}")
run_export(ap214 "${operator_db}" subtraction "${operator_step}" TRUE)
file(READ "${operator_step}" operator_text)
foreach(expected "SPHERE(" "MANIFOLD_SOLID_BREP(" "CSG_SOLID(" ".DIFFERENCE.")
  require_text("${operator_text}" "${expected}" "transformed nested CSG export")
endforeach()
require_entity_count("${operator_text}" "ADVANCED_FACE" 2
  "transformed EHY BRep operand")
require_entity_count("${operator_text}" "CLOSED_SHELL" 1
  "transformed EHY BRep operand")
require_entity_count("${operator_text}" "PRODUCT" 1 "transformed nested CSG export")
run_import("${operator_step}" "${operator_g}" "${operator_report}")
execute_process(
  COMMAND "${MGED}" -c "${operator_g}" db get subtraction_csg_primitive.s
  RESULT_VARIABLE operator_sphere_result
  OUTPUT_VARIABLE operator_sphere
  ERROR_VARIABLE operator_sphere_error
)
execute_process(
  COMMAND "${MGED}" -c "${operator_g}" bb -q -e subtraction_csg_brep.s
  RESULT_VARIABLE operator_bbox_result
  OUTPUT_VARIABLE operator_bbox
  ERROR_VARIABLE operator_bbox_error
)
if(NOT operator_sphere_result EQUAL 0 OR NOT operator_bbox_result EQUAL 0)
  message(FATAL_ERROR
    "could not inspect transformed nested CSG round trip:\n"
    "${operator_sphere_error}${operator_bbox_error}")
endif()
string(APPEND operator_sphere "${operator_sphere_error}")
string(APPEND operator_bbox "${operator_bbox_error}")
require_text("${operator_sphere}" "ell V {0 0 0}" "nested CSG sphere")
require_text("${operator_sphere}" "A {1000 0 0}" "nested CSG sphere")
require_text("${operator_bbox}" "min {-250." "transformed EHY BRep bounds")
require_text("${operator_bbox}" "max {1750." "transformed EHY BRep bounds")

# The exporters declare degrees in their representation context.  Verify that
# the AP214 reader applies that context to RIGHT_CIRCULAR_CONE.semi_angle.
set(cone214 "${OUTPUT_DIR}/g_step_native_csg_cone214.stp")
set(cone214_g "${OUTPUT_DIR}/g_step_native_csg_cone214.g")
set(cone214_report "${OUTPUT_DIR}/g_step_native_csg_cone214.json")
file(REMOVE "${cone214}" "${cone214_g}" "${cone214_report}")
run_export(ap214 "${primitive_db}" trc.r "${cone214}" TRUE)
file(READ "${cone214}" cone214_text)
require_text("${cone214_text}" "RIGHT_CIRCULAR_CONE(" "AP214 cone export")
require_text("${cone214_text}" "CONVERSION_BASED_UNIT('DEGREES'" "AP214 angle context")
run_import("${cone214}" "${cone214_g}" "${cone214_report}")
execute_process(
  COMMAND "${MGED}" -c "${cone214_g}" db get trc_r_csg_primitive.s
  RESULT_VARIABLE cone_get_result
  OUTPUT_VARIABLE cone_get
  ERROR_VARIABLE cone_get_error
)
if(NOT cone_get_result EQUAL 0)
  message(FATAL_ERROR "could not inspect AP214 cone: ${cone_get_error}")
endif()
string(APPEND cone_get "${cone_get_error}")
foreach(expected "tgc V {500 500 0}" "H {0 0 666.666666" "A {0 -500 0}"
    "C {0 -166.666666")
  require_text("${cone_get}" "${expected}" "AP214 reconstructed cone")
endforeach()

# A general ellipsoid has no AP214 CSG primitive.  A one-leaf wrapper must
# reuse one ordinary BRep representation (without leaking a duplicate), while
# AP242 can retain the exact implicit.
set(ell214 "${OUTPUT_DIR}/g_step_native_csg_ell214.stp")
file(REMOVE "${ell214}")
run_export(ap214 "${primitive_db}" ell.r "${ell214}" TRUE)
file(READ "${ell214}" ell214_text)
require_entity_count("${ell214_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION" 1
  "AP214 ellipsoid fallback")
require_entity_count("${ell214_text}" "PRODUCT" 1 "AP214 ellipsoid fallback")
reject_text("${ell214_text}" "CSG_SOLID(" "AP214 ellipsoid fallback")

set(ell242 "${OUTPUT_DIR}/g_step_native_csg_ell242.stp")
set(ell242_g "${OUTPUT_DIR}/g_step_native_csg_ell242.g")
set(ell242_report "${OUTPUT_DIR}/g_step_native_csg_ell242.json")
file(REMOVE "${ell242}" "${ell242_g}" "${ell242_report}")
run_export(ap242 "${primitive_db}" ell.r "${ell242}" TRUE)
file(READ "${ell242}" ell242_text)
require_text("${ell242_text}" "ELLIPSOID(" "AP242 ellipsoid export")
reject_text("${ell242_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION("
  "AP242 ellipsoid export")
run_import("${ell242}" "${ell242_g}" "${ell242_report}")
require_region_primitive_equal("${primitive_db}" ell.r "${ell242_g}"
  ell_r_csg_primitive.s "AP242 reconstructed ellipsoid")

# AP242 also has an exact eccentric/elliptical cone primitive.  primitives.g's
# tec is a representative TGC subset member and should return as a TGC rather
# than being demoted to a BRep.
set(tec242 "${OUTPUT_DIR}/g_step_native_csg_tec242.stp")
set(tec242_g "${OUTPUT_DIR}/g_step_native_csg_tec242.g")
set(tec242_report "${OUTPUT_DIR}/g_step_native_csg_tec242.json")
file(REMOVE "${tec242}" "${tec242_g}" "${tec242_report}")
run_export(ap242 "${primitive_db}" tec.r "${tec242}" TRUE)
file(READ "${tec242}" tec242_text)
require_text("${tec242_text}" "ECCENTRIC_CONE(" "AP242 eccentric-cone export")
reject_text("${tec242_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION("
  "AP242 eccentric-cone export")
run_import("${tec242}" "${tec242_g}" "${tec242_report}")
execute_process(
  COMMAND "${MGED}" -c "${tec242_g}" db get tec_r_csg_primitive.s
  RESULT_VARIABLE tec_get_result
  OUTPUT_VARIABLE tec_get
  ERROR_VARIABLE tec_get_error
)
if(NOT tec_get_result EQUAL 0)
  message(FATAL_ERROR "could not inspect AP242 eccentric cone: ${tec_get_error}")
endif()
string(APPEND tec_get "${tec_get_error}")
foreach(expected "tgc V {500 250 0}" "H {0 0 1000}" "C {0 125 0}")
  require_text("${tec_get}" "${expected}" "AP242 reconstructed eccentric cone")
endforeach()

# A centred rectangular ARB5 is exactly AP242's RECTANGULAR_PYRAMID.  A
# laterally offset apex is still a valid general ARB5, but it must remain a
# BRep because the AP242 primitive requires the apex above the base centre.
set(pyramid242 "${OUTPUT_DIR}/g_step_native_csg_pyramid242.stp")
set(pyramid242_g "${OUTPUT_DIR}/g_step_native_csg_pyramid242.g")
set(pyramid242_report "${OUTPUT_DIR}/g_step_native_csg_pyramid242.json")
file(REMOVE "${pyramid242}" "${pyramid242_g}" "${pyramid242_report}")
run_export(ap242 "${primitive_db}" arb5.r "${pyramid242}" TRUE)
file(READ "${pyramid242}" pyramid242_text)
require_text("${pyramid242_text}" "RECTANGULAR_PYRAMID(" "AP242 pyramid export")
reject_text("${pyramid242_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION("
  "AP242 pyramid export")
run_import("${pyramid242}" "${pyramid242_g}" "${pyramid242_report}")
execute_process(
  COMMAND "${MGED}" -c "${pyramid242_g}" db get arb5_r_csg_primitive.s
  RESULT_VARIABLE pyramid_get_result
  OUTPUT_VARIABLE pyramid_get
  ERROR_VARIABLE pyramid_get_error
)
if(NOT pyramid_get_result EQUAL 0)
  message(FATAL_ERROR "could not inspect AP242 pyramid: ${pyramid_get_error}")
endif()
string(APPEND pyramid_get "${pyramid_get_error}")
foreach(expected "arb8 V1 {0 0 0}" "V2 {0 1000 0}" "V3 {0 1000 1000}"
    "V4 {0 0 1000}" "V5 {1000 500 500}")
  require_text("${pyramid_get}" "${expected}" "AP242 reconstructed pyramid")
endforeach()

set(offcentre_db "${OUTPUT_DIR}/g_step_native_csg_offcentre_pyramid.g")
set(offcentre242 "${OUTPUT_DIR}/g_step_native_csg_offcentre_pyramid.stp")
file(REMOVE "${offcentre_db}" "${offcentre242}")
execute_process(
  COMMAND "${MGED}" -c "${offcentre_db}"
    "in off.s arb8 0 0 0 0 1000 0 0 1000 1000 0 0 1000 1000 600 500 1000 600 500 1000 600 500 1000 600 500; r off.r u off.s"
  RESULT_VARIABLE offcentre_create_result
  OUTPUT_VARIABLE offcentre_create_output
  ERROR_VARIABLE offcentre_create_error
)
if(NOT offcentre_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create off-centre pyramid fixture:\n"
    "${offcentre_create_output}${offcentre_create_error}")
endif()
run_export(ap242 "${offcentre_db}" off.r "${offcentre242}" TRUE)
file(READ "${offcentre242}" offcentre242_text)
require_entity_count("${offcentre242_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION" 1
  "AP242 off-centre ARB5 fallback")
reject_text("${offcentre242_text}" "RECTANGULAR_PYRAMID("
  "AP242 off-centre ARB5 fallback")

# AP242 faceted primitives cover tetrahedra and convex hexahedra.  The share
# database supplies the tetrahedron.  Its general ARB8 is intentionally
# non-planar, so construct a planar tapered hexahedron to cover the second
# schema subtype without pretending that the share example is eligible.
set(tetra242 "${OUTPUT_DIR}/g_step_native_csg_tetra242.stp")
set(tetra242_g "${OUTPUT_DIR}/g_step_native_csg_tetra242.g")
set(tetra242_report "${OUTPUT_DIR}/g_step_native_csg_tetra242.json")
file(REMOVE "${tetra242}" "${tetra242_g}" "${tetra242_report}")
run_export(ap242 "${primitive_db}" arb4.r "${tetra242}" TRUE)
file(READ "${tetra242}" tetra242_text)
require_text("${tetra242_text}" "TETRAHEDRON(" "AP242 tetrahedron export")
run_import("${tetra242}" "${tetra242_g}" "${tetra242_report}")
execute_process(
  COMMAND "${MGED}" -c "${tetra242_g}" db get arb4_r_csg_primitive.s
  RESULT_VARIABLE tetra_get_result
  OUTPUT_VARIABLE tetra_get
  ERROR_VARIABLE tetra_get_error
)
if(NOT tetra_get_result EQUAL 0)
  message(FATAL_ERROR "could not inspect AP242 tetrahedron: ${tetra_get_error}")
endif()
string(APPEND tetra_get "${tetra_get_error}")
foreach(expected "arb8 V1 {0 0 0}" "V2 {0 1000 0}" "V3 {0 1000 1000}"
    "V5 {1000 1000 0}")
  require_text("${tetra_get}" "${expected}" "AP242 reconstructed tetrahedron")
endforeach()

set(nonplanar242 "${OUTPUT_DIR}/g_step_native_csg_nonplanar242.stp")
file(REMOVE "${nonplanar242}")
run_export(ap242 "${primitive_db}" arb8.r "${nonplanar242}" TRUE)
file(READ "${nonplanar242}" nonplanar242_text)
require_entity_count("${nonplanar242_text}" "ADVANCED_BREP_SHAPE_REPRESENTATION" 1
  "AP242 non-planar ARB8 fallback")
reject_text("${nonplanar242_text}" "CONVEX_HEXAHEDRON("
  "AP242 non-planar ARB8 fallback")

set(faceted_db "${OUTPUT_DIR}/g_step_native_csg_faceted.g")
set(hexa242 "${OUTPUT_DIR}/g_step_native_csg_hexa242.stp")
set(hexa242_g "${OUTPUT_DIR}/g_step_native_csg_hexa242.g")
set(hexa242_report "${OUTPUT_DIR}/g_step_native_csg_hexa242.json")
file(REMOVE "${faceted_db}" "${hexa242}" "${hexa242_g}" "${hexa242_report}")
execute_process(
  COMMAND "${MGED}" -c "${faceted_db}"
    "in hexa.s arb8 0 0 0 2000 0 0 2000 1000 0 0 1000 0 200 200 1000 1800 200 1000 1800 800 1000 200 800 1000; r hexa.r u hexa.s"
  RESULT_VARIABLE faceted_create_result
  OUTPUT_VARIABLE faceted_create_output
  ERROR_VARIABLE faceted_create_error
)
if(NOT faceted_create_result EQUAL 0)
  message(FATAL_ERROR
    "could not create convex-hexahedron fixture:\n"
    "${faceted_create_output}${faceted_create_error}")
endif()
run_export(ap242 "${faceted_db}" hexa.r "${hexa242}" TRUE)
file(READ "${hexa242}" hexa242_text)
require_text("${hexa242_text}" "CONVEX_HEXAHEDRON(" "AP242 convex-hexahedron export")
run_import("${hexa242}" "${hexa242_g}" "${hexa242_report}")
execute_process(
  COMMAND "${MGED}" -c "${hexa242_g}" db get hexa_r_csg_primitive.s
  RESULT_VARIABLE hexa_get_result
  OUTPUT_VARIABLE hexa_get
  ERROR_VARIABLE hexa_get_error
)
if(NOT hexa_get_result EQUAL 0)
  message(FATAL_ERROR "could not inspect AP242 convex hexahedron: ${hexa_get_error}")
endif()
string(APPEND hexa_get "${hexa_get_error}")
foreach(expected "arb8 V1 {0 0 0}" "V2 {2000 0 0}" "V5 {200 200 1000}"
    "V7 {1800 800 1000}")
  require_text("${hexa_get}" "${expected}" "AP242 reconstructed convex hexahedron")
endforeach()
