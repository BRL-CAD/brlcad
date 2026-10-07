#                 B R E P _ G T A 0 2 _ O R A C L E . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Reproduce missing-root corrections observed in multiple GTA02 views.
# Each model-space ray is checked twice: a depth-3 legacy result is compared
# with a depth-8 reference, and the production depth-3 selective route is
# required to publish the same interval in both ray directions.  Instance
# transforms are applied explicitly so every oracle runs in the exact geometry
# frame used by the top-level assembly trace.

cmake_minimum_required(VERSION 3.20)

foreach(required_variable BREP_AUDIT DATABASE)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()
if(NOT EXISTS "${BREP_AUDIT}")
  message(FATAL_ERROR "brep-audit does not exist: ${BREP_AUDIT}")
endif()
if(NOT EXISTS "${DATABASE}")
  message(FATAL_ERROR "GTA02 database does not exist: ${DATABASE}")
endif()
file(SHA256 "${BREP_AUDIT}" brep_audit_sha256)
file(SHA256 "${DATABASE}" database_sha256)
cmake_path(GET BREP_AUDIT PARENT_PATH brep_audit_directory)
set(librt_sha256 unavailable)
foreach(candidate
    "${brep_audit_directory}/librt.dll"
    "${brep_audit_directory}/../lib/librt.so"
    "${brep_audit_directory}/../lib/liblibrt.so"
    "${brep_audit_directory}/../lib/liblibrt.dylib")
  if(EXISTS "${candidate}")
    file(SHA256 "${candidate}" librt_sha256)
    break()
  endif()
endforeach()
if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/brep-gta02-oracle")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

function(json_required result json)
  string(JSON value ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT json_error STREQUAL "NOTFOUND")
    string(JOIN "." path ${ARGN})
    message(FATAL_ERROR "Missing or invalid JSON field ${path}: ${json_error}")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(json_length result json)
  string(JSON value ERROR_VARIABLE json_error LENGTH "${json}" ${ARGN})
  if(NOT json_error STREQUAL "NOTFOUND")
    string(JOIN "." path ${ARGN})
    message(FATAL_ERROR "Missing or invalid JSON array ${path}: ${json_error}")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(require_equal label actual expected)
  if(NOT "${actual}" STREQUAL "${expected}")
    message(FATAL_ERROR "${label}: observed '${actual}', expected '${expected}'")
  endif()
endfunction()

function(require_interval label value minimum maximum)
  if(value LESS minimum OR value GREATER maximum)
    message(FATAL_ERROR
      "${label}: observed ${value}, expected [${minimum}, ${maximum}]"
    )
  endif()
endfunction()

set(cases
  "529|GTC01_A_MCB01_PRT_item.s|identity|31.019976775481737,-3.3371940849914381,-87.685734414546232,-0.74240387650601958,-0.51983679072587829,-0.42261826174060968|9.72767|9.72769|10.33436|10.33439|0|0"
  "1717|GTC01_A_MCF01_PRT_item.s|identity|-17.881929347362828,7.4108809152541397,-93.279055551881243,-0.74240387650601958,-0.51983679072587829,-0.42261826174060968|2.04078|2.04081|2.16433|2.16436|38|44"
  "2217|GTC01_A_MCF01_PRT_item.s|identity|-20.186924791691396,7.4108809152541397,-90.877286783168515,-0.74240387650601958,-0.51983679072587829,-0.42261826174060968|1.29429|1.29432|2.03975|2.03978|44|44"
  "3312|GTC01_A_MCF01_PRT_item.s|identity|-24.508791249804858,7.4108809152541397,-84.140234756493044,-0.74240387650601958,-0.51983679072587829,-0.42261826174060968|1.28860|1.28862|2.03340|2.03343|44|44"
  "12153|GTC01_A_MFM01_PRT_item.s|identity|29.364783557296562,1.9597278890010372,18.676947387102842,-0.74240387650601958,-0.51983679072587829,-0.42261826174060968|20.90132|20.90135|21.05837|21.05840|188|188"
  "180|ID_GTC_REAR_RING_PRT_item.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|3.269126449826814,-11.26897547692441,-98.244678132330577,0.79124011523622395,0.55403229322232328,-0.25881904510252041|3.51202|3.51205|4.12580|4.12583|3|3"
  "269|ID_GTC_FRONT_RING_PRT_item.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|-28.457148109681739,-0.19571387854342248,-88.021240557003125,0.79124011523622395,0.55403229322232328,-0.25881904510252041|8.73928|8.73931|8.90409|8.90412|0|0"
  "1376|GTC01_A_MCF01_PRT_item.s|identity|-31.999604379942369,2.1657570942250786,-83.158265216941956,0.79124011523622395,0.55403229322232328,-0.25881904510252041|6.38874|6.38877|7.17171|7.17174|44|44"
  "61814|ID_GTC_FRONT_RING_PRT_item.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|-11.568603783292318,0.9371865631786207,15.71410781938847,0.79124011523622395,0.55403229322232328,-0.25881904510252041|6.55669|6.55672|7.47677|7.47680|14|14"
)

string(CONCAT summary
  "GTA02 exact-ray selective-replay oracle\n"
  "database=${DATABASE}\n"
  "database_sha256=${database_sha256}\n"
  "brep_audit_sha256=${brep_audit_sha256}\n"
  "librt_sha256=${librt_sha256}\n"
  "candidate_depth=3\nreference_depth=8\n"
  "endpoint_band_width<=0.00003\n"
)
foreach(case_spec IN LISTS cases)
  string(REPLACE "|" ";" fields "${case_spec}")
  list(GET fields 0 id)
  list(GET fields 1 object)
  list(GET fields 2 transform)
  list(GET fields 3 ray)
  list(GET fields 4 in_minimum)
  list(GET fields 5 in_maximum)
  list(GET fields 6 out_minimum)
  list(GET fields 7 out_maximum)
  list(GET fields 8 in_face_expected)
  list(GET fields 9 out_face_expected)
  set(transform_argument)
  if(NOT transform STREQUAL "identity")
    list(APPEND transform_argument "--transform=${transform}")
  endif()

  message(STATUS "GTA02 oracle ray ${id}: ${object}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      --unset=LIBRT_BREP_ROUTE_DIRECT
      --unset=LIBRT_BREP_ROUTE_PUBLISH
      --unset=LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE
      --unset=LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE
      "LIBRT_CACHE=off"
      "${BREP_AUDIT}" --ray-depth-compare
      --candidate-depth=3 --reference-depth=8 --ray-count=0
      ${transform_argument}
      "--ray=${ray}" "${DATABASE}" "${object}"
    RESULT_VARIABLE depth_result
    OUTPUT_VARIABLE depth_json
    ERROR_VARIABLE depth_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  file(WRITE "${OUTPUT_DIR}/ray-${id}-depth.json" "${depth_json}\n")
  file(WRITE "${OUTPUT_DIR}/ray-${id}-depth.stderr.log" "${depth_stderr}")
  if(NOT depth_result EQUAL 0 AND NOT depth_result EQUAL 1)
    message(FATAL_ERROR "Depth oracle ray ${id} failed to run (${depth_result})")
  endif()
  json_required(depth_differences "${depth_json}" comparison differing_rays)
  json_required(depth_candidate_forward "${depth_json}"
    selected_pair_records 0 candidate_forward hit_count)
  json_required(depth_candidate_reverse "${depth_json}"
    selected_pair_records 0 candidate_reverse hit_count)
  json_required(depth_reference_forward "${depth_json}"
    selected_pair_records 0 reference_forward hit_count)
  json_required(depth_reference_reverse "${depth_json}"
    selected_pair_records 0 reference_reverse hit_count)
  json_required(depth_candidate_reversal "${depth_json}"
    comparison candidate_reversal_mismatches)
  json_required(depth_reference_reversal "${depth_json}"
    comparison reference_reversal_mismatches)
  require_equal("ray ${id} differing directions" "${depth_differences}" 2)
  require_equal("ray ${id} depth-3 forward hits" "${depth_candidate_forward}" 0)
  require_equal("ray ${id} depth-3 reverse hits" "${depth_candidate_reverse}" 0)
  require_equal("ray ${id} depth-8 forward hits" "${depth_reference_forward}" 2)
  require_equal("ray ${id} depth-8 reverse hits" "${depth_reference_reverse}" 2)
  require_equal("ray ${id} depth-3 reversal mismatches" "${depth_candidate_reversal}" 0)
  require_equal("ray ${id} depth-8 reversal mismatches" "${depth_reference_reversal}" 0)

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      "LIBRT_CACHE=off"
      "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=1"
      "${BREP_AUDIT}" --ray-depth-compare --routing-truth
      --routing-publication --candidate-depth=3 --reference-depth=3
      --ray-count=0 ${transform_argument}
      "--ray=${ray}" "${DATABASE}" "${object}"
    RESULT_VARIABLE route_result
    OUTPUT_VARIABLE route_json
    ERROR_VARIABLE route_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  file(WRITE "${OUTPUT_DIR}/ray-${id}-route.json" "${route_json}\n")
  file(WRITE "${OUTPUT_DIR}/ray-${id}-route.stderr.log" "${route_stderr}")
  if(NOT route_result EQUAL 0)
    message(FATAL_ERROR "Production route ray ${id} failed (${route_result})")
  endif()
  json_required(route_status "${route_json}" status)
  json_required(route_differences "${route_json}" routing_truth differing_rays)
  json_required(route_authoritative "${route_json}"
    routing_truth authoritative_correction_rays)
  json_required(route_selective_oracle "${route_json}"
    routing_truth selective_replay_oracle_rays)
  json_required(publication_valid "${route_json}"
    routing_truth publication valid)
  json_required(publication_corrections "${route_json}"
    routing_truth publication published_correction_rays)
  json_required(published_forward "${route_json}"
    routing_truth published_selected_pair_records 0 candidate_forward hit_count)
  json_required(published_reverse "${route_json}"
    routing_truth published_selected_pair_records 0 candidate_reverse hit_count)
  json_required(legacy_forward "${route_json}"
    routing_truth published_selected_pair_records 0 reference_forward hit_count)
  json_required(legacy_reverse "${route_json}"
    routing_truth published_selected_pair_records 0 reference_reverse hit_count)
  require_equal("ray ${id} route status" "${route_status}" pass)
  require_equal("ray ${id} routed/legacy differing directions" "${route_differences}" 2)
  require_equal("ray ${id} authoritative corrections" "${route_authoritative}" 2)
  require_equal("ray ${id} selective replay oracle" "${route_selective_oracle}" 2)
  require_equal("ray ${id} publication valid" "${publication_valid}" ON)
  require_equal("ray ${id} published corrections" "${publication_corrections}" 2)
  require_equal("ray ${id} published forward hits" "${published_forward}" 2)
  require_equal("ray ${id} published reverse hits" "${published_reverse}" 2)
  require_equal("ray ${id} retained forward hits" "${legacy_forward}" 0)
  require_equal("ray ${id} retained reverse hits" "${legacy_reverse}" 0)

  foreach(result_kind depth route)
    if(result_kind STREQUAL "depth")
      set(result_json "${depth_json}")
      set(record_path selected_pair_records)
      set(implementation reference)
    else()
      set(result_json "${route_json}")
      set(record_path routing_truth published_selected_pair_records)
      set(implementation candidate)
    endif()
    json_required(in_distance "${result_json}" ${record_path} 0
      ${implementation}_forward intervals 0 in)
    json_required(out_distance "${result_json}" ${record_path} 0
      ${implementation}_forward intervals 0 out)
    json_required(in_face "${result_json}" ${record_path} 0
      ${implementation}_forward intervals 0 in_face)
    json_required(out_face "${result_json}" ${record_path} 0
      ${implementation}_forward intervals 0 out_face)
    require_interval("ray ${id} ${result_kind} entry" "${in_distance}"
      "${in_minimum}" "${in_maximum}")
    require_interval("ray ${id} ${result_kind} exit" "${out_distance}"
      "${out_minimum}" "${out_maximum}")
    require_equal("ray ${id} ${result_kind} entry face" "${in_face}"
      "${in_face_expected}")
    require_equal("ray ${id} ${result_kind} exit face" "${out_face}"
      "${out_face_expected}")
  endforeach()

  string(APPEND summary
    "ray=${id} object=${object} transform=${transform} result=PASS interval=${in_distance}:${out_distance} faces=${in_face}:${out_face}\n"
  )
endforeach()

# This shallow torus crossing is a genuine correction, but the generic
# depth-8 SurfaceTree solver remains conservatively inconclusive.  Validate it
# with a different geometric formulation: brep-audit recognizes the exact
# torus and solves its ray quartic independently of the B-Rep subdivision
# solver.  The two UV endpoints are also far inside the trim (recorded by the
# companion multiview qualification archive).
set(torus_id 59287)
set(torus_object GTC01_A_MFM01_PRT_item.s)
set(torus_ray "23.386058854421663,-11.305230636106373,-5.4748071874330222,0.79124011523622395,0.55403229322232328,-0.25881904510252041")
set(torus_in_minimum 4.48526)
set(torus_in_maximum 4.48529)
set(torus_out_minimum 4.57673)
set(torus_out_maximum 4.57676)

message(STATUS "GTA02 analytic torus oracle ray ${torus_id}: ${torus_object}")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    --unset=LIBRT_BREP_DEBUG
    --unset=LIBRT_BREP_PROFILE
    --unset=LIBRT_BREP_ROUTE_PROFILE
    --unset=LIBRT_BREP_ROUTE_DIRECT
    --unset=LIBRT_BREP_ROUTE_PUBLISH
    "${BREP_AUDIT}" --mesh-ray-face=188
    --tess-abs=0.001 --tess-rel=0 --tess-norm=0
    "--ray=${torus_ray}" "${DATABASE}" "${torus_object}"
  RESULT_VARIABLE torus_result
  OUTPUT_VARIABLE torus_json
  ERROR_VARIABLE torus_stderr
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
file(WRITE "${OUTPUT_DIR}/ray-${torus_id}-analytic.json" "${torus_json}\n")
file(WRITE "${OUTPUT_DIR}/ray-${torus_id}-analytic.stderr.log" "${torus_stderr}")
require_equal("ray ${torus_id} analytic process result" "${torus_result}" 0)
json_required(torus_status "${torus_json}" status)
json_required(torus_available "${torus_json}" analytic_torus available)
json_required(torus_roots_valid "${torus_json}" analytic_torus root_solver_valid)
json_length(torus_root_count "${torus_json}" analytic_torus roots)
require_equal("ray ${torus_id} analytic status" "${torus_status}" ok)
require_equal("ray ${torus_id} torus recognition" "${torus_available}" ON)
require_equal("ray ${torus_id} torus root solve" "${torus_roots_valid}" ON)
require_equal("ray ${torus_id} torus quartic root count" "${torus_root_count}" 4)
json_required(torus_in_distance "${torus_json}" analytic_torus roots 2 distance)
json_required(torus_in_imaginary "${torus_json}" analytic_torus roots 2 imaginary)
json_required(torus_in_residual "${torus_json}" analytic_torus roots 2 residual)
json_required(torus_out_distance "${torus_json}" analytic_torus roots 3 distance)
json_required(torus_out_imaginary "${torus_json}" analytic_torus roots 3 imaginary)
json_required(torus_out_residual "${torus_json}" analytic_torus roots 3 residual)
require_interval("ray ${torus_id} analytic entry" "${torus_in_distance}"
  "${torus_in_minimum}" "${torus_in_maximum}")
require_interval("ray ${torus_id} analytic exit" "${torus_out_distance}"
  "${torus_out_minimum}" "${torus_out_maximum}")
require_interval("ray ${torus_id} analytic entry imaginary"
  "${torus_in_imaginary}" -0.00000001 0.00000001)
require_interval("ray ${torus_id} analytic exit imaginary"
  "${torus_out_imaginary}" -0.00000001 0.00000001)
require_interval("ray ${torus_id} analytic entry residual"
  "${torus_in_residual}" -0.00001 0.00001)
require_interval("ray ${torus_id} analytic exit residual"
  "${torus_out_residual}" -0.00001 0.00001)

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    --unset=LIBRT_BREP_DEBUG
    --unset=LIBRT_BREP_PROFILE
    --unset=LIBRT_BREP_ROUTE_PROFILE
    "LIBRT_CACHE=off"
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=1"
    "${BREP_AUDIT}" --ray-depth-compare --routing-truth
    --routing-publication --candidate-depth=3 --reference-depth=3
    --ray-count=0 "--ray=${torus_ray}" "${DATABASE}" "${torus_object}"
  RESULT_VARIABLE torus_route_result
  OUTPUT_VARIABLE torus_route_json
  ERROR_VARIABLE torus_route_stderr
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
file(WRITE "${OUTPUT_DIR}/ray-${torus_id}-route.json" "${torus_route_json}\n")
file(WRITE "${OUTPUT_DIR}/ray-${torus_id}-route.stderr.log" "${torus_route_stderr}")
require_equal("ray ${torus_id} route process result" "${torus_route_result}" 0)
json_required(torus_route_status "${torus_route_json}" status)
json_required(torus_route_differences "${torus_route_json}"
  routing_truth differing_rays)
json_required(torus_route_authoritative "${torus_route_json}"
  routing_truth authoritative_correction_rays)
json_required(torus_route_selective "${torus_route_json}"
  routing_truth selective_replay_oracle_rays)
json_required(torus_publication_valid "${torus_route_json}"
  routing_truth publication valid)
json_required(torus_publication_corrections "${torus_route_json}"
  routing_truth publication published_correction_rays)
json_required(torus_route_forward_hits "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_forward hit_count)
json_required(torus_route_reverse_hits "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_reverse hit_count)
json_required(torus_legacy_forward_hits "${torus_route_json}"
  routing_truth published_selected_pair_records 0 reference_forward hit_count)
json_required(torus_legacy_reverse_hits "${torus_route_json}"
  routing_truth published_selected_pair_records 0 reference_reverse hit_count)
require_equal("ray ${torus_id} route status" "${torus_route_status}" pass)
require_equal("ray ${torus_id} routed/legacy differing directions"
  "${torus_route_differences}" 2)
require_equal("ray ${torus_id} authoritative corrections"
  "${torus_route_authoritative}" 2)
require_equal("ray ${torus_id} selective replay oracle"
  "${torus_route_selective}" 2)
require_equal("ray ${torus_id} publication valid" "${torus_publication_valid}" ON)
require_equal("ray ${torus_id} published corrections"
  "${torus_publication_corrections}" 2)
require_equal("ray ${torus_id} published forward hits"
  "${torus_route_forward_hits}" 2)
require_equal("ray ${torus_id} published reverse hits"
  "${torus_route_reverse_hits}" 2)
require_equal("ray ${torus_id} retained forward hits"
  "${torus_legacy_forward_hits}" 0)
require_equal("ray ${torus_id} retained reverse hits"
  "${torus_legacy_reverse_hits}" 0)
json_required(torus_route_in_distance "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_forward intervals 0 in)
json_required(torus_route_out_distance "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_forward intervals 0 out)
json_required(torus_route_in_face "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_forward intervals 0 in_face)
json_required(torus_route_out_face "${torus_route_json}"
  routing_truth published_selected_pair_records 0 candidate_forward intervals 0 out_face)
require_interval("ray ${torus_id} routed entry" "${torus_route_in_distance}"
  "${torus_in_minimum}" "${torus_in_maximum}")
require_interval("ray ${torus_id} routed exit" "${torus_route_out_distance}"
  "${torus_out_minimum}" "${torus_out_maximum}")
require_equal("ray ${torus_id} routed entry face" "${torus_route_in_face}" 188)
require_equal("ray ${torus_id} routed exit face" "${torus_route_out_face}" 188)
string(APPEND summary
  "ray=${torus_id} object=${torus_object} transform=identity result=PASS reference=analytic_torus interval=${torus_route_in_distance}:${torus_route_out_distance} faces=188:188\n"
)

# General prepared completions are not corrections: they must remain exactly
# equivalent to both the depth-8 reference and retained depth-3 legacy result.
# These are every non-replay completion observed in the four-view run.
set(equivalent_cases
  "axis-z-31|ID_GTC_FRONT_RING_PRT_item.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|26.886172111755656,5.0935919729787997,16.709098776548089,-2.4980018054066017e-16,0,-1|2"
  "axis-z-13636|ID_GTC_FRONT_RING_PRT_item.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|-27.319561871649512,4.7630692047872882,16.709098776548089,-2.4980018054066017e-16,0,-1|4"
  "reverse-oblique-2730|ID_GTC_STACKING_PRT_item_step71172.s|1,0,0,-0.0025404346994660001,0,0,1,-0.2455442734827,0,-1,0,-2.059937826933,0,0,0,1|-19.472359531332842,-27.999722983853388,-80.587406818851932,0.79124011523622395,0.55403229322232328,-0.25881904510252041|2"
)
foreach(case_spec IN LISTS equivalent_cases)
  string(REPLACE "|" ";" fields "${case_spec}")
  list(GET fields 0 id)
  list(GET fields 1 object)
  list(GET fields 2 transform)
  list(GET fields 3 ray)
  list(GET fields 4 expected_hits)

  message(STATUS "GTA02 equivalent prepared ray ${id}: ${object}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      --unset=LIBRT_BREP_ROUTE_DIRECT
      --unset=LIBRT_BREP_ROUTE_PUBLISH
      --unset=LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE
      --unset=LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE
      "LIBRT_CACHE=off"
      "${BREP_AUDIT}" --ray-depth-compare
      --candidate-depth=3 --reference-depth=8 --ray-count=0
      "--transform=${transform}" "--ray=${ray}" "${DATABASE}" "${object}"
    RESULT_VARIABLE equivalent_depth_result
    OUTPUT_VARIABLE equivalent_depth_json
    ERROR_VARIABLE equivalent_depth_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  file(WRITE "${OUTPUT_DIR}/ray-${id}-depth.json" "${equivalent_depth_json}\n")
  file(WRITE "${OUTPUT_DIR}/ray-${id}-depth.stderr.log" "${equivalent_depth_stderr}")
  require_equal("ray ${id} depth process result" "${equivalent_depth_result}" 0)
  json_required(equivalent_depth_status "${equivalent_depth_json}" status)
  json_required(equivalent_depth_differences "${equivalent_depth_json}"
    comparison differing_rays)
  json_required(equivalent_depth_candidate_reversal "${equivalent_depth_json}"
    comparison candidate_reversal_mismatches)
  json_required(equivalent_depth_reference_reversal "${equivalent_depth_json}"
    comparison reference_reversal_mismatches)
  foreach(direction candidate_forward candidate_reverse
      reference_forward reference_reverse)
    json_required(observed_hits "${equivalent_depth_json}"
      selected_pair_records 0 ${direction} hit_count)
    require_equal("ray ${id} depth ${direction} hits"
      "${observed_hits}" "${expected_hits}")
  endforeach()
  require_equal("ray ${id} depth status" "${equivalent_depth_status}" pass)
  require_equal("ray ${id} depth differences"
    "${equivalent_depth_differences}" 0)
  require_equal("ray ${id} depth-3 reversal mismatches"
    "${equivalent_depth_candidate_reversal}" 0)
  require_equal("ray ${id} depth-8 reversal mismatches"
    "${equivalent_depth_reference_reversal}" 0)

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      "LIBRT_CACHE=off"
      "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=1"
      "${BREP_AUDIT}" --ray-depth-compare --routing-truth
      --routing-publication --candidate-depth=3 --reference-depth=3
      --ray-count=0 "--transform=${transform}" "--ray=${ray}"
      "${DATABASE}" "${object}"
    RESULT_VARIABLE equivalent_route_result
    OUTPUT_VARIABLE equivalent_route_json
    ERROR_VARIABLE equivalent_route_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  file(WRITE "${OUTPUT_DIR}/ray-${id}-route.json" "${equivalent_route_json}\n")
  file(WRITE "${OUTPUT_DIR}/ray-${id}-route.stderr.log" "${equivalent_route_stderr}")
  require_equal("ray ${id} route process result" "${equivalent_route_result}" 0)
  json_required(equivalent_route_status "${equivalent_route_json}" status)
  json_required(equivalent_route_differences "${equivalent_route_json}"
    routing_truth differing_rays)
  json_required(equivalent_route_authoritative "${equivalent_route_json}"
    routing_truth authoritative_correction_rays)
  json_required(equivalent_publication_valid "${equivalent_route_json}"
    routing_truth publication valid)
  json_required(equivalent_optional_attempts "${equivalent_route_json}"
    routing_truth publication optional_attempts)
  json_required(equivalent_optional_complete "${equivalent_route_json}"
    routing_truth publication optional_complete)
  json_required(equivalent_eligible_publications "${equivalent_route_json}"
    routing_truth publication eligible_publications)
  json_required(equivalent_prepared_matches "${equivalent_route_json}"
    routing_truth publication prepared_matches)
  json_required(equivalent_publication_mismatches "${equivalent_route_json}"
    routing_truth publication mismatches)
  require_equal("ray ${id} route status" "${equivalent_route_status}" pass)
  require_equal("ray ${id} routed/legacy differences"
    "${equivalent_route_differences}" 0)
  require_equal("ray ${id} authoritative corrections"
    "${equivalent_route_authoritative}" 0)
  require_equal("ray ${id} publication valid"
    "${equivalent_publication_valid}" ON)
  require_equal("ray ${id} optional attempts" "${equivalent_optional_attempts}" 2)
  require_equal("ray ${id} optional completions" "${equivalent_optional_complete}" 2)
  require_equal("ray ${id} eligible publications"
    "${equivalent_eligible_publications}" 2)
  require_equal("ray ${id} prepared publication matches"
    "${equivalent_prepared_matches}" 2)
  require_equal("ray ${id} publication mismatches"
    "${equivalent_publication_mismatches}" 0)
  foreach(direction candidate_forward candidate_reverse
      reference_forward reference_reverse)
    json_required(observed_hits "${equivalent_route_json}"
      routing_truth published_selected_pair_records 0 ${direction} hit_count)
    require_equal("ray ${id} route ${direction} hits"
      "${observed_hits}" "${expected_hits}")
  endforeach()
  string(APPEND summary
    "ray=${id} object=${object} transform=${transform} result=PASS reference=depth8 equivalence_hits=${expected_hits}\n"
  )
endforeach()

file(WRITE "${OUTPUT_DIR}/summary.txt" "${summary}")
message(STATUS "GTA02 exact-ray selective-replay oracle: PASS")
