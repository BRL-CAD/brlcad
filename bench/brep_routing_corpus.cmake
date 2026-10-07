#                  B R E P _ R O U T I N G _ C O R P U S . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# Reproducible prepared-versus-legacy BREP routing truth sweep.  Invoke with:
#
#   cmake -DBUILD_DIR=/path/to/build \
#     -P /path/to/brlcad/bench/brep_routing_corpus.cmake
#
# Optional inputs are DATABASE, OUTPUT_DIR, RANDOM_RAY_COUNT,
# FEATURE_RAY_COUNT, CANDIDATE_DEPTH, REFERENCE_DEPTH, OBJECTS (a CMake list),
# and OBJECT_REGEX.  The output directory receives raw JSONL, a per-object CSV
# plus totals, stderr, and a provenance manifest.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED BUILD_DIR OR "${BUILD_DIR}" STREQUAL "")
  message(FATAL_ERROR "BUILD_DIR must name a configured BRL-CAD build directory")
endif()

if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH SOURCE_DIR)
endif()
if(NOT DEFINED DATABASE OR "${DATABASE}" STREQUAL "")
  set(DATABASE "${SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g")
endif()
if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/brep-routing-corpus")
endif()
if(NOT DEFINED RANDOM_RAY_COUNT)
  set(RANDOM_RAY_COUNT 64)
endif()
if(NOT DEFINED FEATURE_RAY_COUNT)
  set(FEATURE_RAY_COUNT 24)
endif()
if(NOT DEFINED CANDIDATE_DEPTH)
  set(CANDIDATE_DEPTH 8)
endif()
if(NOT DEFINED REFERENCE_DEPTH)
  set(REFERENCE_DEPTH 8)
endif()

foreach(integer_variable
    RANDOM_RAY_COUNT FEATURE_RAY_COUNT CANDIDATE_DEPTH REFERENCE_DEPTH)
  if(NOT "${${integer_variable}}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "${integer_variable} must be a nonnegative integer")
  endif()
endforeach()

file(REAL_PATH "${BUILD_DIR}" BUILD_DIR EXPAND_TILDE)
file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR EXPAND_TILDE)
file(REAL_PATH "${DATABASE}" DATABASE EXPAND_TILDE)
cmake_path(ABSOLUTE_PATH OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE OUTPUT_DIR)

function(find_corpus_program result build_dir program_name)
  set(candidates
    "${build_dir}/bin/${program_name}"
    "${build_dir}/bin/${program_name}.exe"
    "${build_dir}/bin/Release/${program_name}.exe"
  )
  foreach(candidate IN LISTS candidates)
    if(EXISTS "${candidate}")
      set(${result} "${candidate}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  message(FATAL_ERROR "Cannot find ${program_name} under ${build_dir}/bin")
endfunction()

function(cache_value result cache_file key)
  if(NOT EXISTS "${cache_file}")
    set(${result} unavailable PARENT_SCOPE)
    return()
  endif()
  file(STRINGS "${cache_file}" matches REGEX "^${key}(:[^=]*)?=")
  if(matches)
    list(GET matches 0 value)
    string(REGEX REPLACE "^[^=]*=" "" value "${value}")
    set(${result} "${value}" PARENT_SCOPE)
  else()
    set(${result} unavailable PARENT_SCOPE)
  endif()
endfunction()

function(source_state result_commit result_dirty result_hash source_dir)
  set(commit unavailable)
  set(dirty unavailable)
  set(state_hash unavailable)
  if(EXISTS "${source_dir}/.git")
    execute_process(
      COMMAND git -C "${source_dir}" rev-parse HEAD
      RESULT_VARIABLE commit_result
      OUTPUT_VARIABLE commit
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(NOT commit_result EQUAL 0)
      set(commit unavailable)
    endif()
    execute_process(
      COMMAND git -C "${source_dir}" status --porcelain --untracked-files=normal
      RESULT_VARIABLE status_result
      OUTPUT_VARIABLE status_text
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(status_result EQUAL 0)
      if(status_text STREQUAL "")
        set(dirty clean)
      else()
        set(dirty dirty)
      endif()
      execute_process(
        COMMAND git -C "${source_dir}" diff --binary --no-ext-diff HEAD
        RESULT_VARIABLE diff_result
        OUTPUT_VARIABLE tracked_diff
        ERROR_QUIET
      )
      execute_process(
        COMMAND git -C "${source_dir}" ls-files --others --exclude-standard
        RESULT_VARIABLE untracked_result
        OUTPUT_VARIABLE untracked_text
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
      )
      if(diff_result EQUAL 0 AND untracked_result EQUAL 0)
        set(state_material "status\n${status_text}\ntracked\n${tracked_diff}")
        string(REPLACE "\r\n" "\n" untracked_text "${untracked_text}")
        string(REPLACE "\n" ";" untracked_files "${untracked_text}")
        foreach(untracked_file IN LISTS untracked_files)
          if(NOT untracked_file STREQUAL "" AND
              EXISTS "${source_dir}/${untracked_file}" AND
              NOT IS_DIRECTORY "${source_dir}/${untracked_file}")
            file(SHA256 "${source_dir}/${untracked_file}" untracked_sha256)
            string(APPEND state_material
              "\nuntracked ${untracked_file} ${untracked_sha256}"
            )
          endif()
        endforeach()
        string(SHA256 state_hash "${state_material}")
      endif()
    endif()
  endif()
  set(${result_commit} "${commit}" PARENT_SCOPE)
  set(${result_dirty} "${dirty}" PARENT_SCOPE)
  set(${result_hash} "${state_hash}" PARENT_SCOPE)
endfunction()

function(json_required result json)
  string(JSON value ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Missing/invalid JSON field '${ARGN}': ${json_error}")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(json_named_count result json section name)
  set(count_field rays)
  if(ARGC GREATER 4)
    set(count_field "${ARGV4}")
  endif()
  string(JSON item_count ERROR_VARIABLE json_error LENGTH "${json}"
    routing_truth "${section}")
  if(NOT json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Missing routing_truth.${section}: ${json_error}")
  endif()
  set(found FALSE)
  set(value 0)
  if(item_count GREATER 0)
    math(EXPR last_item "${item_count} - 1")
    foreach(item RANGE 0 ${last_item})
      json_required(item_name "${json}" routing_truth "${section}" ${item} name)
      if(item_name STREQUAL "${name}")
        json_required(value "${json}" routing_truth "${section}" ${item} "${count_field}")
        set(found TRUE)
        break()
      endif()
    endforeach()
  endif()
  if(NOT found)
    message(FATAL_ERROR "No '${name}' entry in routing_truth.${section}")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(json_named_path_count result json name)
  set(json_path ${ARGN})
  string(JSON item_count ERROR_VARIABLE json_error LENGTH "${json}"
    ${json_path})
  if(NOT json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Missing JSON array '${json_path}': ${json_error}")
  endif()
  set(found FALSE)
  set(value 0)
  if(item_count GREATER 0)
    math(EXPR last_item "${item_count} - 1")
    foreach(item RANGE 0 ${last_item})
      json_required(item_name "${json}" ${json_path} ${item} name)
      if(item_name STREQUAL "${name}")
        json_required(value "${json}" ${json_path} ${item} associations)
        set(found TRUE)
        break()
      endif()
    endforeach()
  endif()
  if(NOT found)
    message(FATAL_ERROR "No '${name}' entry in JSON array '${json_path}'")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(csv_quote result value)
  string(REPLACE "\"" "\"\"" escaped "${value}")
  set(${result} "\"${escaped}\"" PARENT_SCOPE)
endfunction()

macro(add_total total_name value)
  math(EXPR ${total_name} "${${total_name}} + ${value}")
endmacro()

find_corpus_program(brep_audit "${BUILD_DIR}" brep-audit)
if(NOT EXISTS "${DATABASE}")
  message(FATAL_ERROR "Cannot find BREP corpus database: ${DATABASE}")
endif()

file(SHA256 "${brep_audit}" brep_audit_sha256)
file(SHA256 "${DATABASE}" database_sha256)
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" driver_sha256)
cache_value(build_type "${BUILD_DIR}/CMakeCache.txt" CMAKE_BUILD_TYPE)
cache_value(generator "${BUILD_DIR}/CMakeCache.txt" CMAKE_GENERATOR)
cache_value(compiler "${BUILD_DIR}/CMakeCache.txt" CMAKE_CXX_COMPILER)
source_state(source_commit source_dirty source_state_sha256 "${SOURCE_DIR}")

execute_process(
  COMMAND "${brep_audit}" --list "${DATABASE}"
  RESULT_VARIABLE list_result
  OUTPUT_VARIABLE listed_objects_text
  ERROR_VARIABLE list_stderr
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT list_result EQUAL 0)
  message(FATAL_ERROR "Unable to list BREP objects (${list_result}): ${list_stderr}")
endif()
string(REPLACE "\r\n" "\n" listed_objects_text "${listed_objects_text}")
string(REPLACE "\n" ";" listed_objects "${listed_objects_text}")

set(selected_objects)
foreach(object IN LISTS listed_objects)
  if(object STREQUAL "")
    continue()
  endif()
  if(DEFINED OBJECTS AND NOT "${OBJECTS}" STREQUAL "")
    if(NOT "${object}" IN_LIST OBJECTS)
      continue()
    endif()
  endif()
  if(DEFINED OBJECT_REGEX AND NOT "${OBJECT_REGEX}" STREQUAL "")
    if(NOT object MATCHES "${OBJECT_REGEX}")
      continue()
    endif()
  endif()
  list(APPEND selected_objects "${object}")
endforeach()
if(NOT selected_objects)
  message(FATAL_ERROR "Object filters selected no BREP primitives")
endif()
if(DEFINED OBJECTS AND NOT "${OBJECTS}" STREQUAL "")
  foreach(requested_object IN LISTS OBJECTS)
    if(NOT "${requested_object}" IN_LIST listed_objects)
      message(FATAL_ERROR "Requested object is not a BREP primitive: ${requested_object}")
    endif()
  endforeach()
endif()

cmake_host_system_information(RESULT host_name QUERY HOSTNAME)
cmake_host_system_information(RESULT processor_name QUERY PROCESSOR_NAME)
cmake_host_system_information(RESULT logical_cores QUERY NUMBER_OF_LOGICAL_CORES)
string(TIMESTAMP run_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
list(LENGTH selected_objects selected_object_count)

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(jsonl_file "${OUTPUT_DIR}/routing_truth.jsonl")
set(csv_file "${OUTPUT_DIR}/summary.csv")
set(certificate_csv_file "${OUTPUT_DIR}/missing_root_certificate.csv")
set(cleanup_csv_file "${OUTPUT_DIR}/legacy_cleanup.csv")
set(capacity_csv_file "${OUTPUT_DIR}/legacy_capacity.csv")
set(stderr_file "${OUTPUT_DIR}/stderr.log")
set(manifest_file "${OUTPUT_DIR}/manifest.txt")
file(WRITE "${jsonl_file}" "")
file(WRITE "${stderr_file}" "")
file(WRITE "${csv_file}"
  "object,exit_code,status,rays,differing_rays,correction_rays,correction_pairs,missed_corrections,observed_empty,observed_clean_ordinary,observed_mixed_hazard,observed_all_hazard,observed_unsupported,observed_unclassified,correction_empty,correction_clean_ordinary,correction_mixed_hazard,correction_all_hazard,correction_unsupported,correction_unclassified,prepared_miss_legacy_hit,prepared_hit_legacy_miss,both_nonempty_changed,other,none_lane,seeded_lane,complex_lane,oracle_complete,oracle_unresolved,prepared_reversal,legacy_reversal,depth_differences,cell_lookup_failures,roots_unmapped_after_leaf_overflow,missing_root_empty_cells,missing_root_candidate_spans,missing_root_retained_spans,missing_root_restricted_spans,missing_root_inconclusive_spans,missing_root_workspace_overflow,missing_root_signal_rays,missing_root_independent_hit_rays,missing_root_independent_empty_rays,missing_root_unresolved_rays,missing_root_prepared_candidate_faces,missing_root_prepared_candidate_spans,missing_root_prepared_retained_spans,missing_root_prepared_only_spans,missing_root_prepared_only_retained_spans,missing_root_prepared_inconclusive_spans,missing_root_prepared_candidate_failures,missing_root_prepared_signal_rays,missing_root_prepared_independent_hit_rays,missing_root_prepared_independent_empty_rays,missing_root_prepared_unresolved_rays,missing_root_counterfactual_attempts,missing_root_counterfactual_candidate_spans,missing_root_counterfactual_retained_spans,missing_root_counterfactual_restricted_spans,missing_root_counterfactual_inconclusive_spans,missing_root_counterfactual_workspace_overflow,missing_root_counterfactual_true_root_controls,missing_root_counterfactual_true_root_signals,missing_root_counterfactual_true_root_missed,missing_root_counterfactual_empty_controls,missing_root_counterfactual_empty_signals,missing_root_counterfactual_empty_rejected,missing_root_counterfactual_unresolved_controls,missing_root_counterfactual_prepared_candidate_faces,missing_root_counterfactual_prepared_candidate_spans,missing_root_counterfactual_prepared_retained_spans,missing_root_counterfactual_prepared_only_spans,missing_root_counterfactual_prepared_only_retained_spans,missing_root_counterfactual_prepared_inconclusive_spans,missing_root_counterfactual_prepared_candidate_failures,missing_root_counterfactual_prepared_signal_rays,missing_root_counterfactual_prepared_unsignaled_rays,missing_root_counterfactual_true_root_prepared_signals,missing_root_counterfactual_true_root_prepared_missed,missing_root_counterfactual_true_root_prepared_recovered,missing_root_counterfactual_empty_prepared_signals,missing_root_counterfactual_empty_prepared_rejected,verify_attempts\n"
)
set(certificate_scopes real counterfactual)
set(certificate_scalar_metrics
  associations incomplete root_free root_positive
  association_regular_root_count cached_bbox_exclusions hull_exclusions
  subdivision_attempts subdivision_boxes subdivision_hull_exclusions
  subdivision_exclusions degree_attempts degree_complete
  boundary_pieces boundary_splits boundary_edges
  complete_rays root_free_rays root_positive_rays incomplete_rays elapsed_ns
)
set(certificate_failure_reasons
  none input ray_frame status2_mapping singular overlap coefficients
  reparameterization determinant boundary orientation resource
)
set(certificate_degree_failure_details
  none input boundary_piece boundary_subdivision cross_arithmetic cross_zero
  orientation
)
set(certificate_csv_header
  "object,scope,accounting_valid,globally_unique,publication_authority")
foreach(metric IN LISTS certificate_scalar_metrics)
  string(APPEND certificate_csv_header ",${metric}")
endforeach()
string(APPEND certificate_csv_header ",expansion_high_water")
foreach(reason IN LISTS certificate_failure_reasons)
  string(APPEND certificate_csv_header ",failure_${reason}")
endforeach()
foreach(detail IN LISTS certificate_degree_failure_details)
  string(APPEND certificate_csv_header ",degree_failure_${detail}")
endforeach()
file(WRITE "${certificate_csv_file}" "${certificate_csv_header}\n")
file(WRITE "${cleanup_csv_file}"
  "object,depth_role,action,rays,events\n"
)
file(WRITE "${capacity_csv_file}"
  "object,depth_role,reason,rays\n"
)
file(WRITE "${manifest_file}"
  "BREP routing truth corpus manifest\n"
  "format=brlcad-brep-routing-corpus-v1\n"
  "timestamp_utc=${run_timestamp}\n"
  "host=${host_name}\n"
  "processor=${processor_name}\n"
  "logical_cores=${logical_cores}\n"
  "cmake=${CMAKE_VERSION}\n"
  "build_dir=${BUILD_DIR}\n"
  "build_type=${build_type}\n"
  "generator=${generator}\n"
  "compiler=${compiler}\n"
  "source_dir=${SOURCE_DIR}\n"
  "source_commit=${source_commit}\n"
  "source_state=${source_dirty}\n"
  "source_state_sha256=${source_state_sha256}\n"
  "driver=${CMAKE_CURRENT_LIST_FILE}\n"
  "driver_sha256=${driver_sha256}\n"
  "brep_audit=${brep_audit}\n"
  "brep_audit_sha256=${brep_audit_sha256}\n"
  "database=${DATABASE}\n"
  "database_sha256=${database_sha256}\n"
  "random_chord_pairs=${RANDOM_RAY_COUNT}\n"
  "feature_chord_pairs=${FEATURE_RAY_COUNT}\n"
  "candidate_depth=${CANDIDATE_DEPTH}\n"
  "reference_depth=${REFERENCE_DEPTH}\n"
  "cache=disabled (LIBRT_CACHE=off)\n"
  "seeded_verifier=disabled/unset\n"
  "missing_root_certificate_counts=cell_span_associations_not_globally_unique\n"
  "missing_root_certificate_publication_authority=none\n"
  "selected_objects=${selected_object_count}\n"
  "object_filter=${OBJECTS}\n"
  "object_regex=${OBJECT_REGEX}\n"
)

set(total_objects 0)
set(total_nonzero 0)
set(total_rays 0)
set(total_differing 0)
set(total_corrections 0)
set(total_correction_pairs 0)
set(total_missed 0)
set(total_observed_empty 0)
set(total_observed_clean 0)
set(total_observed_mixed 0)
set(total_observed_all_hazard 0)
set(total_observed_unsupported 0)
set(total_observed_unclassified 0)
set(total_empty 0)
set(total_clean 0)
set(total_mixed 0)
set(total_all_hazard 0)
set(total_unsupported 0)
set(total_unclassified 0)
set(total_prepared_miss 0)
set(total_prepared_hit 0)
set(total_both_changed 0)
set(total_other 0)
set(total_none_lane 0)
set(total_seeded_lane 0)
set(total_complex_lane 0)
set(total_oracle_complete 0)
set(total_oracle_unresolved 0)
set(total_prepared_reversal 0)
set(total_legacy_reversal 0)
set(total_depth_differences 0)
set(total_lookup_failures 0)
set(total_roots_unmapped_after_leaf_overflow 0)
set(total_missing_root_empty_cells 0)
set(total_missing_root_candidate_spans 0)
set(total_missing_root_retained_spans 0)
set(total_missing_root_restricted_spans 0)
set(total_missing_root_inconclusive_spans 0)
set(total_missing_root_workspace_overflow 0)
set(total_missing_root_signal_rays 0)
set(total_missing_root_independent_hit_rays 0)
set(total_missing_root_independent_empty_rays 0)
set(total_missing_root_unresolved_rays 0)
set(total_missing_root_counterfactual_attempts 0)
set(total_missing_root_counterfactual_candidate_spans 0)
set(total_missing_root_counterfactual_retained_spans 0)
set(total_missing_root_counterfactual_restricted_spans 0)
set(total_missing_root_counterfactual_inconclusive_spans 0)
set(total_missing_root_counterfactual_workspace_overflow 0)
set(total_missing_root_counterfactual_true_root_controls 0)
set(total_missing_root_counterfactual_true_root_signals 0)
set(total_missing_root_counterfactual_true_root_missed 0)
set(total_missing_root_counterfactual_empty_controls 0)
set(total_missing_root_counterfactual_empty_signals 0)
set(total_missing_root_counterfactual_empty_rejected 0)
set(total_missing_root_counterfactual_unresolved_controls 0)
foreach(missing_root_prepared_metric
    prepared_candidate_faces prepared_candidate_spans
    prepared_retained_spans prepared_only_spans
    prepared_only_retained_spans prepared_inconclusive_spans
    prepared_candidate_failures prepared_signal_rays
    prepared_independent_hit_rays prepared_independent_empty_rays
    prepared_unresolved_rays
    counterfactual_prepared_candidate_faces
    counterfactual_prepared_candidate_spans
    counterfactual_prepared_retained_spans
    counterfactual_prepared_only_spans
    counterfactual_prepared_only_retained_spans
    counterfactual_prepared_inconclusive_spans
    counterfactual_prepared_candidate_failures
    counterfactual_prepared_signal_rays
    counterfactual_prepared_unsignaled_rays
    counterfactual_true_root_prepared_signals
    counterfactual_true_root_prepared_missed
    counterfactual_true_root_prepared_recovered
    counterfactual_empty_prepared_signals
    counterfactual_empty_prepared_rejected)
  set(total_missing_root_${missing_root_prepared_metric} 0)
endforeach()
set(total_verify_attempts 0)
foreach(certificate_scope IN LISTS certificate_scopes)
  set(total_${certificate_scope}_certificate_accounting_invalid 0)
  set(total_${certificate_scope}_certificate_globally_unique 0)
  set(total_${certificate_scope}_certificate_publication_authority 0)
  foreach(metric IN LISTS certificate_scalar_metrics)
    set(total_${certificate_scope}_certificate_${metric} 0)
  endforeach()
  set(total_${certificate_scope}_certificate_expansion_high_water 0)
  foreach(reason IN LISTS certificate_failure_reasons)
    set(total_${certificate_scope}_certificate_failure_${reason} 0)
  endforeach()
  foreach(detail IN LISTS certificate_degree_failure_details)
    set(total_${certificate_scope}_certificate_degree_failure_${detail} 0)
  endforeach()
endforeach()

set(legacy_cleanup_actions
  coincident_grazing
  near_miss_shadow
  near_miss_crack_merge
  near_miss_crack_cancel
  near_miss_nonadjacent_drop
  clean_overlap
  near_hit_shadow
  near_hit_crack_merge
  trimmed_invalid
  parameter_oob
  isolated_grazing
  duplicate_cancel
  duplicate_collapse
  solid_coincident
  solid_unmatched_enter
  solid_unmatched_leave
  solid_interior_winding
  open_same_direction
  open_odd_endpoint
)
set(legacy_capacity_reasons leaf_workspace hit_workspace)
foreach(depth_role reference candidate)
  set(total_${depth_role}_cleanup_mismatch_rays 0)
  foreach(action IN LISTS legacy_cleanup_actions)
    set(total_${depth_role}_cleanup_${action}_rays 0)
    set(total_${depth_role}_cleanup_${action}_events 0)
  endforeach()
  foreach(reason IN LISTS legacy_capacity_reasons)
    set(total_${depth_role}_capacity_${reason}_rays 0)
  endforeach()
endforeach()

foreach(object IN LISTS selected_objects)
  message(STATUS "Routing truth: ${object}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      --unset=LIBRT_BREP_SEEDED_VERIFY
      --unset=LIBRT_BREP_SOLVER
      "LIBRT_CACHE=off"
      "${brep_audit}" --ray-depth-compare --routing-truth
      "--candidate-depth=${CANDIDATE_DEPTH}"
      "--reference-depth=${REFERENCE_DEPTH}"
      "--ray-count=${RANDOM_RAY_COUNT}"
      "--feature-ray-count=${FEATURE_RAY_COUNT}"
      "${DATABASE}" "${object}"
    RESULT_VARIABLE audit_result
    OUTPUT_VARIABLE audit_json
    ERROR_VARIABLE audit_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(STRIP "${audit_json}" audit_json)
  if(audit_json STREQUAL "")
    message(FATAL_ERROR "brep-audit emitted no JSON for ${object}: ${audit_stderr}")
  endif()
  string(JSON audit_format ERROR_VARIABLE audit_json_error GET
    "${audit_json}" format)
  if(NOT audit_json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Invalid JSON for ${object}: ${audit_json_error}\n${audit_stderr}")
  endif()
  if(NOT audit_format STREQUAL "brlcad-brep-ray-depth-comparison-v1")
    message(FATAL_ERROR "Unexpected audit format for ${object}: ${audit_format}")
  endif()
  file(APPEND "${jsonl_file}" "${audit_json}\n")
  if(NOT audit_stderr STREQUAL "")
    file(APPEND "${stderr_file}" "===== ${object} (exit ${audit_result}) =====\n${audit_stderr}\n")
  endif()

  json_required(status "${audit_json}" status)
  json_required(rays "${audit_json}" sampling rays)
  json_required(differing "${audit_json}" routing_truth differing_rays)
  json_required(corrections "${audit_json}" routing_truth authoritative_correction_rays)
  json_required(correction_pairs "${audit_json}" routing_truth authoritative_correction_pairs)
  json_required(missed "${audit_json}" routing_truth missed_correction_rays)
  json_required(oracle_complete "${audit_json}" routing_truth oracle completed_rays)
  json_required(oracle_unresolved "${audit_json}" routing_truth oracle unresolved_correction_rays)
  json_required(prepared_reversal "${audit_json}" routing_truth prepared_reversal_mismatches)
  json_required(legacy_reversal "${audit_json}" routing_truth legacy_reversal_mismatches)
  json_required(depth_differences "${audit_json}" comparison differing_rays)
  json_required(lookup_failures "${audit_json}" reference trace routing_shadow decision cell_lookup_failures)
  json_required(roots_unmapped_after_leaf_overflow "${audit_json}" reference trace routing_shadow decision roots_unmapped_after_leaf_overflow)
  json_required(missing_root_empty_cells "${audit_json}" reference trace routing_shadow decision missing_root_probe empty_correction_cells)
  json_required(missing_root_candidate_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe candidate_spans)
  json_required(missing_root_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe retained_spans)
  json_required(missing_root_restricted_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe restricted_spans)
  json_required(missing_root_inconclusive_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe inconclusive_spans)
  json_required(missing_root_workspace_overflow "${audit_json}" reference trace routing_shadow decision missing_root_probe workspace_overflow)
  json_required(missing_root_signal_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe signal_rays)
  json_required(missing_root_independent_hit_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe independent_hit_rays)
  json_required(missing_root_independent_empty_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe independent_empty_rays)
  json_required(missing_root_unresolved_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe unresolved_rays)
  json_required(missing_root_prepared_candidate_faces "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates faces)
  json_required(missing_root_prepared_candidate_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates spans)
  json_required(missing_root_prepared_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates retained_spans)
  json_required(missing_root_prepared_only_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates prepared_only_spans)
  json_required(missing_root_prepared_only_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates prepared_only_retained_spans)
  json_required(missing_root_prepared_inconclusive_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates inconclusive_spans)
  json_required(missing_root_prepared_candidate_failures "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates candidate_failures)
  json_required(missing_root_prepared_signal_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates signal_rays)
  json_required(missing_root_prepared_independent_hit_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates independent_hit_rays)
  json_required(missing_root_prepared_independent_empty_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates independent_empty_rays)
  json_required(missing_root_prepared_unresolved_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe prepared_candidates unresolved_rays)
  json_required(missing_root_counterfactual_attempts "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual attempts)
  json_required(missing_root_counterfactual_candidate_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual candidate_spans)
  json_required(missing_root_counterfactual_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual retained_spans)
  json_required(missing_root_counterfactual_restricted_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual restricted_spans)
  json_required(missing_root_counterfactual_inconclusive_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual inconclusive_spans)
  json_required(missing_root_counterfactual_workspace_overflow "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual workspace_overflow)
  json_required(missing_root_counterfactual_true_root_controls "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_controls)
  json_required(missing_root_counterfactual_true_root_signals "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_signals)
  json_required(missing_root_counterfactual_true_root_missed "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_missed)
  json_required(missing_root_counterfactual_empty_controls "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual empty_controls)
  json_required(missing_root_counterfactual_empty_signals "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual empty_signals)
  json_required(missing_root_counterfactual_empty_rejected "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual empty_rejected)
  json_required(missing_root_counterfactual_unresolved_controls "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual unresolved_controls)
  json_required(missing_root_counterfactual_prepared_candidate_faces "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates faces)
  json_required(missing_root_counterfactual_prepared_candidate_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates spans)
  json_required(missing_root_counterfactual_prepared_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates retained_spans)
  json_required(missing_root_counterfactual_prepared_only_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates prepared_only_spans)
  json_required(missing_root_counterfactual_prepared_only_retained_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates prepared_only_retained_spans)
  json_required(missing_root_counterfactual_prepared_inconclusive_spans "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates inconclusive_spans)
  json_required(missing_root_counterfactual_prepared_candidate_failures "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates candidate_failures)
  json_required(missing_root_counterfactual_prepared_signal_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates signal_rays)
  json_required(missing_root_counterfactual_prepared_unsignaled_rays "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual prepared_candidates unsignaled_rays)
  json_required(missing_root_counterfactual_true_root_prepared_signals "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_prepared_signals)
  json_required(missing_root_counterfactual_true_root_prepared_missed "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_prepared_missed)
  json_required(missing_root_counterfactual_true_root_prepared_recovered "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual true_root_prepared_recovered)
  json_required(missing_root_counterfactual_empty_prepared_signals "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual empty_prepared_signals)
  json_required(missing_root_counterfactual_empty_prepared_rejected "${audit_json}" reference trace routing_shadow decision missing_root_probe counterfactual empty_prepared_rejected)
  json_required(verify_attempts "${audit_json}" reference trace routing_shadow actual_verify_attempts)
  json_named_count(observed_empty "${audit_json}" observation_strata empty)
  json_named_count(observed_clean "${audit_json}" observation_strata clean_ordinary)
  json_named_count(observed_mixed "${audit_json}" observation_strata mixed_hazard)
  json_named_count(observed_all_hazard "${audit_json}" observation_strata all_hazard)
  json_named_count(observed_unsupported "${audit_json}" observation_strata unsupported)
  json_named_count(observed_unclassified "${audit_json}" observation_strata unclassified)
  json_named_count(empty "${audit_json}" correction_strata empty)
  json_named_count(clean "${audit_json}" correction_strata clean_ordinary)
  json_named_count(mixed "${audit_json}" correction_strata mixed_hazard)
  json_named_count(all_hazard "${audit_json}" correction_strata all_hazard)
  json_named_count(unsupported "${audit_json}" correction_strata unsupported)
  json_named_count(unclassified "${audit_json}" correction_strata unclassified)
  json_named_count(prepared_miss "${audit_json}" correction_shapes prepared_miss_legacy_hit)
  json_named_count(prepared_hit "${audit_json}" correction_shapes prepared_hit_legacy_miss)
  json_named_count(both_changed "${audit_json}" correction_shapes both_nonempty_changed)
  json_named_count(other "${audit_json}" correction_shapes other)
  json_named_count(none_lane "${audit_json}" lanes none correction_rays)
  json_named_count(seeded_lane "${audit_json}" lanes seeded_ordinary correction_rays)
  json_named_count(complex_lane "${audit_json}" lanes complex_prepared correction_rays)

  csv_quote(csv_object "${object}")
  foreach(certificate_scope IN LISTS certificate_scopes)
    set(certificate_path reference trace routing_shadow decision
      missing_root_probe)
    if(certificate_scope STREQUAL "counterfactual")
      list(APPEND certificate_path counterfactual)
    endif()
    list(APPEND certificate_path certificate)

    json_required(certificate_accounting_valid "${audit_json}"
      ${certificate_path} accounting_valid)
    json_required(certificate_globally_unique "${audit_json}"
      ${certificate_path} globally_unique)
    json_required(certificate_publication_authority "${audit_json}"
      ${certificate_path} publication_authority)
    if(certificate_accounting_valid)
      set(certificate_accounting_valid_csv 1)
    else()
      set(certificate_accounting_valid_csv 0)
      add_total(total_${certificate_scope}_certificate_accounting_invalid 1)
    endif()
    if(certificate_globally_unique)
      set(certificate_globally_unique_csv 1)
      add_total(total_${certificate_scope}_certificate_globally_unique 1)
    else()
      set(certificate_globally_unique_csv 0)
    endif()
    if(certificate_publication_authority)
      set(certificate_publication_authority_csv 1)
      add_total(total_${certificate_scope}_certificate_publication_authority 1)
    else()
      set(certificate_publication_authority_csv 0)
    endif()
    set(certificate_csv_row
      "${csv_object},${certificate_scope},${certificate_accounting_valid_csv},${certificate_globally_unique_csv},${certificate_publication_authority_csv}")
    foreach(metric IN LISTS certificate_scalar_metrics)
      json_required(certificate_value "${audit_json}" ${certificate_path}
        ${metric})
      string(APPEND certificate_csv_row ",${certificate_value}")
      add_total(total_${certificate_scope}_certificate_${metric}
        ${certificate_value})
    endforeach()
    json_required(certificate_expansion_high_water "${audit_json}"
      ${certificate_path} expansion_high_water)
    string(APPEND certificate_csv_row ",${certificate_expansion_high_water}")
    if(certificate_expansion_high_water GREATER
        total_${certificate_scope}_certificate_expansion_high_water)
      set(total_${certificate_scope}_certificate_expansion_high_water
        ${certificate_expansion_high_water})
    endif()
    set(certificate_failure_path ${certificate_path} failure_reasons)
    foreach(reason IN LISTS certificate_failure_reasons)
      json_named_path_count(certificate_failure_count "${audit_json}"
        ${reason} ${certificate_failure_path})
      string(APPEND certificate_csv_row ",${certificate_failure_count}")
      add_total(total_${certificate_scope}_certificate_failure_${reason}
        ${certificate_failure_count})
    endforeach()
    set(certificate_detail_path ${certificate_path} degree_failure_details)
    foreach(detail IN LISTS certificate_degree_failure_details)
      json_named_path_count(certificate_detail_count "${audit_json}"
        ${detail} ${certificate_detail_path})
      string(APPEND certificate_csv_row ",${certificate_detail_count}")
      add_total(total_${certificate_scope}_certificate_degree_failure_${detail}
        ${certificate_detail_count})
    endforeach()
    file(APPEND "${certificate_csv_file}" "${certificate_csv_row}\n")
  endforeach()

  foreach(depth_role reference candidate)
    json_required(cleanup_mismatch_rays "${audit_json}" ${depth_role} trace
      legacy_cleanup_action_mismatch_rays)
    add_total(total_${depth_role}_cleanup_mismatch_rays
      ${cleanup_mismatch_rays})
    foreach(action IN LISTS legacy_cleanup_actions)
      json_required(action_rays "${audit_json}" ${depth_role} trace
        legacy_cleanup ${action} rays)
      json_required(action_events "${audit_json}" ${depth_role} trace
        legacy_cleanup ${action} events)
      file(APPEND "${cleanup_csv_file}"
        "${csv_object},${depth_role},${action},${action_rays},${action_events}\n"
      )
      add_total(total_${depth_role}_cleanup_${action}_rays ${action_rays})
      add_total(total_${depth_role}_cleanup_${action}_events ${action_events})
    endforeach()
    foreach(reason IN LISTS legacy_capacity_reasons)
      json_required(capacity_rays "${audit_json}" ${depth_role} trace
        legacy_capacity_fallbacks ${reason})
      file(APPEND "${capacity_csv_file}"
        "${csv_object},${depth_role},${reason},${capacity_rays}\n"
      )
      add_total(total_${depth_role}_capacity_${reason}_rays ${capacity_rays})
    endforeach()
  endforeach()

  csv_quote(csv_status "${status}")
  file(APPEND "${csv_file}"
    "${csv_object},${audit_result},${csv_status},${rays},${differing},${corrections},${correction_pairs},${missed},${observed_empty},${observed_clean},${observed_mixed},${observed_all_hazard},${observed_unsupported},${observed_unclassified},${empty},${clean},${mixed},${all_hazard},${unsupported},${unclassified},${prepared_miss},${prepared_hit},${both_changed},${other},${none_lane},${seeded_lane},${complex_lane},${oracle_complete},${oracle_unresolved},${prepared_reversal},${legacy_reversal},${depth_differences},${lookup_failures},${roots_unmapped_after_leaf_overflow},${missing_root_empty_cells},${missing_root_candidate_spans},${missing_root_retained_spans},${missing_root_restricted_spans},${missing_root_inconclusive_spans},${missing_root_workspace_overflow},${missing_root_signal_rays},${missing_root_independent_hit_rays},${missing_root_independent_empty_rays},${missing_root_unresolved_rays},${missing_root_prepared_candidate_faces},${missing_root_prepared_candidate_spans},${missing_root_prepared_retained_spans},${missing_root_prepared_only_spans},${missing_root_prepared_only_retained_spans},${missing_root_prepared_inconclusive_spans},${missing_root_prepared_candidate_failures},${missing_root_prepared_signal_rays},${missing_root_prepared_independent_hit_rays},${missing_root_prepared_independent_empty_rays},${missing_root_prepared_unresolved_rays},${missing_root_counterfactual_attempts},${missing_root_counterfactual_candidate_spans},${missing_root_counterfactual_retained_spans},${missing_root_counterfactual_restricted_spans},${missing_root_counterfactual_inconclusive_spans},${missing_root_counterfactual_workspace_overflow},${missing_root_counterfactual_true_root_controls},${missing_root_counterfactual_true_root_signals},${missing_root_counterfactual_true_root_missed},${missing_root_counterfactual_empty_controls},${missing_root_counterfactual_empty_signals},${missing_root_counterfactual_empty_rejected},${missing_root_counterfactual_unresolved_controls},${missing_root_counterfactual_prepared_candidate_faces},${missing_root_counterfactual_prepared_candidate_spans},${missing_root_counterfactual_prepared_retained_spans},${missing_root_counterfactual_prepared_only_spans},${missing_root_counterfactual_prepared_only_retained_spans},${missing_root_counterfactual_prepared_inconclusive_spans},${missing_root_counterfactual_prepared_candidate_failures},${missing_root_counterfactual_prepared_signal_rays},${missing_root_counterfactual_prepared_unsignaled_rays},${missing_root_counterfactual_true_root_prepared_signals},${missing_root_counterfactual_true_root_prepared_missed},${missing_root_counterfactual_true_root_prepared_recovered},${missing_root_counterfactual_empty_prepared_signals},${missing_root_counterfactual_empty_prepared_rejected},${verify_attempts}\n"
  )

  add_total(total_objects 1)
  if(NOT audit_result EQUAL 0)
    add_total(total_nonzero 1)
  endif()
  add_total(total_rays ${rays})
  add_total(total_differing ${differing})
  add_total(total_corrections ${corrections})
  add_total(total_correction_pairs ${correction_pairs})
  add_total(total_missed ${missed})
  add_total(total_observed_empty ${observed_empty})
  add_total(total_observed_clean ${observed_clean})
  add_total(total_observed_mixed ${observed_mixed})
  add_total(total_observed_all_hazard ${observed_all_hazard})
  add_total(total_observed_unsupported ${observed_unsupported})
  add_total(total_observed_unclassified ${observed_unclassified})
  add_total(total_empty ${empty})
  add_total(total_clean ${clean})
  add_total(total_mixed ${mixed})
  add_total(total_all_hazard ${all_hazard})
  add_total(total_unsupported ${unsupported})
  add_total(total_unclassified ${unclassified})
  add_total(total_prepared_miss ${prepared_miss})
  add_total(total_prepared_hit ${prepared_hit})
  add_total(total_both_changed ${both_changed})
  add_total(total_other ${other})
  add_total(total_none_lane ${none_lane})
  add_total(total_seeded_lane ${seeded_lane})
  add_total(total_complex_lane ${complex_lane})
  add_total(total_oracle_complete ${oracle_complete})
  add_total(total_oracle_unresolved ${oracle_unresolved})
  add_total(total_prepared_reversal ${prepared_reversal})
  add_total(total_legacy_reversal ${legacy_reversal})
  add_total(total_depth_differences ${depth_differences})
  add_total(total_lookup_failures ${lookup_failures})
  add_total(total_roots_unmapped_after_leaf_overflow ${roots_unmapped_after_leaf_overflow})
  foreach(missing_root_metric
      empty_cells candidate_spans retained_spans restricted_spans
      inconclusive_spans workspace_overflow signal_rays
      independent_hit_rays independent_empty_rays unresolved_rays
      prepared_candidate_faces prepared_candidate_spans
      prepared_retained_spans prepared_only_spans
      prepared_only_retained_spans prepared_inconclusive_spans
      prepared_candidate_failures prepared_signal_rays
      prepared_independent_hit_rays prepared_independent_empty_rays
      prepared_unresolved_rays
      counterfactual_attempts counterfactual_candidate_spans
      counterfactual_retained_spans counterfactual_restricted_spans
      counterfactual_inconclusive_spans counterfactual_workspace_overflow
      counterfactual_true_root_controls counterfactual_true_root_signals
      counterfactual_true_root_missed counterfactual_empty_controls
      counterfactual_empty_signals counterfactual_empty_rejected
      counterfactual_unresolved_controls
      counterfactual_prepared_candidate_faces
      counterfactual_prepared_candidate_spans
      counterfactual_prepared_retained_spans
      counterfactual_prepared_only_spans
      counterfactual_prepared_only_retained_spans
      counterfactual_prepared_inconclusive_spans
      counterfactual_prepared_candidate_failures
      counterfactual_prepared_signal_rays
      counterfactual_prepared_unsignaled_rays
      counterfactual_true_root_prepared_signals
      counterfactual_true_root_prepared_missed
      counterfactual_true_root_prepared_recovered
      counterfactual_empty_prepared_signals
      counterfactual_empty_prepared_rejected)
    add_total(total_missing_root_${missing_root_metric}
      ${missing_root_${missing_root_metric}})
  endforeach()
  add_total(total_verify_attempts ${verify_attempts})
endforeach()

file(APPEND "${csv_file}"
  "\"TOTAL\",${total_nonzero},\"aggregate\",${total_rays},${total_differing},${total_corrections},${total_correction_pairs},${total_missed},${total_observed_empty},${total_observed_clean},${total_observed_mixed},${total_observed_all_hazard},${total_observed_unsupported},${total_observed_unclassified},${total_empty},${total_clean},${total_mixed},${total_all_hazard},${total_unsupported},${total_unclassified},${total_prepared_miss},${total_prepared_hit},${total_both_changed},${total_other},${total_none_lane},${total_seeded_lane},${total_complex_lane},${total_oracle_complete},${total_oracle_unresolved},${total_prepared_reversal},${total_legacy_reversal},${total_depth_differences},${total_lookup_failures},${total_roots_unmapped_after_leaf_overflow},${total_missing_root_empty_cells},${total_missing_root_candidate_spans},${total_missing_root_retained_spans},${total_missing_root_restricted_spans},${total_missing_root_inconclusive_spans},${total_missing_root_workspace_overflow},${total_missing_root_signal_rays},${total_missing_root_independent_hit_rays},${total_missing_root_independent_empty_rays},${total_missing_root_unresolved_rays},${total_missing_root_prepared_candidate_faces},${total_missing_root_prepared_candidate_spans},${total_missing_root_prepared_retained_spans},${total_missing_root_prepared_only_spans},${total_missing_root_prepared_only_retained_spans},${total_missing_root_prepared_inconclusive_spans},${total_missing_root_prepared_candidate_failures},${total_missing_root_prepared_signal_rays},${total_missing_root_prepared_independent_hit_rays},${total_missing_root_prepared_independent_empty_rays},${total_missing_root_prepared_unresolved_rays},${total_missing_root_counterfactual_attempts},${total_missing_root_counterfactual_candidate_spans},${total_missing_root_counterfactual_retained_spans},${total_missing_root_counterfactual_restricted_spans},${total_missing_root_counterfactual_inconclusive_spans},${total_missing_root_counterfactual_workspace_overflow},${total_missing_root_counterfactual_true_root_controls},${total_missing_root_counterfactual_true_root_signals},${total_missing_root_counterfactual_true_root_missed},${total_missing_root_counterfactual_empty_controls},${total_missing_root_counterfactual_empty_signals},${total_missing_root_counterfactual_empty_rejected},${total_missing_root_counterfactual_unresolved_controls},${total_missing_root_counterfactual_prepared_candidate_faces},${total_missing_root_counterfactual_prepared_candidate_spans},${total_missing_root_counterfactual_prepared_retained_spans},${total_missing_root_counterfactual_prepared_only_spans},${total_missing_root_counterfactual_prepared_only_retained_spans},${total_missing_root_counterfactual_prepared_inconclusive_spans},${total_missing_root_counterfactual_prepared_candidate_failures},${total_missing_root_counterfactual_prepared_signal_rays},${total_missing_root_counterfactual_prepared_unsignaled_rays},${total_missing_root_counterfactual_true_root_prepared_signals},${total_missing_root_counterfactual_true_root_prepared_missed},${total_missing_root_counterfactual_true_root_prepared_recovered},${total_missing_root_counterfactual_empty_prepared_signals},${total_missing_root_counterfactual_empty_prepared_rejected},${total_verify_attempts}\n"
)
foreach(certificate_scope IN LISTS certificate_scopes)
  if(total_${certificate_scope}_certificate_accounting_invalid EQUAL 0)
    set(certificate_total_accounting_valid 1)
  else()
    set(certificate_total_accounting_valid 0)
  endif()
  # Association counts are explicitly not globally unique and this audit-only
  # stage has no publication authority, including in the aggregate row.
  set(certificate_csv_row
    "\"TOTAL\",${certificate_scope},${certificate_total_accounting_valid},0,0")
  foreach(metric IN LISTS certificate_scalar_metrics)
    string(APPEND certificate_csv_row
      ",${total_${certificate_scope}_certificate_${metric}}")
  endforeach()
  string(APPEND certificate_csv_row
    ",${total_${certificate_scope}_certificate_expansion_high_water}")
  foreach(reason IN LISTS certificate_failure_reasons)
    string(APPEND certificate_csv_row
      ",${total_${certificate_scope}_certificate_failure_${reason}}")
  endforeach()
  foreach(detail IN LISTS certificate_degree_failure_details)
    string(APPEND certificate_csv_row
      ",${total_${certificate_scope}_certificate_degree_failure_${detail}}")
  endforeach()
  file(APPEND "${certificate_csv_file}" "${certificate_csv_row}\n")
endforeach()
foreach(depth_role reference candidate)
  foreach(action IN LISTS legacy_cleanup_actions)
    file(APPEND "${cleanup_csv_file}"
      "\"TOTAL\",${depth_role},${action},${total_${depth_role}_cleanup_${action}_rays},${total_${depth_role}_cleanup_${action}_events}\n"
    )
  endforeach()
  foreach(reason IN LISTS legacy_capacity_reasons)
    file(APPEND "${capacity_csv_file}"
      "\"TOTAL\",${depth_role},${reason},${total_${depth_role}_capacity_${reason}_rays}\n"
    )
  endforeach()
endforeach()
file(APPEND "${manifest_file}"
  "completed_objects=${total_objects}\n"
  "nonzero_object_exits=${total_nonzero}\n"
  "rays=${total_rays}\n"
  "prepared_legacy_differences=${total_differing}\n"
  "authoritative_correction_rays=${total_corrections}\n"
  "authoritative_correction_pairs=${total_correction_pairs}\n"
  "missed_correction_rays=${total_missed}\n"
  "observed_empty=${total_observed_empty}\n"
  "observed_clean_ordinary=${total_observed_clean}\n"
  "observed_mixed_hazard=${total_observed_mixed}\n"
  "observed_all_hazard=${total_observed_all_hazard}\n"
  "observed_unsupported=${total_observed_unsupported}\n"
  "observed_unclassified=${total_observed_unclassified}\n"
  "correction_empty=${total_empty}\n"
  "correction_clean_ordinary=${total_clean}\n"
  "correction_mixed_hazard=${total_mixed}\n"
  "correction_all_hazard=${total_all_hazard}\n"
  "correction_unsupported=${total_unsupported}\n"
  "correction_unclassified=${total_unclassified}\n"
  "prepared_miss_legacy_hit=${total_prepared_miss}\n"
  "prepared_hit_legacy_miss=${total_prepared_hit}\n"
  "both_nonempty_changed=${total_both_changed}\n"
  "other_correction_shape=${total_other}\n"
  "none_lane_corrections=${total_none_lane}\n"
  "seeded_lane_corrections=${total_seeded_lane}\n"
  "complex_lane_corrections=${total_complex_lane}\n"
  "oracle_complete=${total_oracle_complete}\n"
  "oracle_unresolved=${total_oracle_unresolved}\n"
  "prepared_reversal_mismatches=${total_prepared_reversal}\n"
  "legacy_reversal_mismatches=${total_legacy_reversal}\n"
  "depth_differences=${total_depth_differences}\n"
  "cell_lookup_failures=${total_lookup_failures}\n"
  "roots_unmapped_after_leaf_overflow=${total_roots_unmapped_after_leaf_overflow}\n"
  "missing_root_empty_correction_cells=${total_missing_root_empty_cells}\n"
  "missing_root_candidate_spans=${total_missing_root_candidate_spans}\n"
  "missing_root_retained_spans=${total_missing_root_retained_spans}\n"
  "missing_root_restricted_spans=${total_missing_root_restricted_spans}\n"
  "missing_root_inconclusive_spans=${total_missing_root_inconclusive_spans}\n"
  "missing_root_workspace_overflow=${total_missing_root_workspace_overflow}\n"
  "missing_root_signal_rays=${total_missing_root_signal_rays}\n"
  "missing_root_independent_hit_rays=${total_missing_root_independent_hit_rays}\n"
  "missing_root_independent_empty_rays=${total_missing_root_independent_empty_rays}\n"
  "missing_root_unresolved_rays=${total_missing_root_unresolved_rays}\n"
  "missing_root_prepared_candidate_faces=${total_missing_root_prepared_candidate_faces}\n"
  "missing_root_prepared_candidate_spans=${total_missing_root_prepared_candidate_spans}\n"
  "missing_root_prepared_retained_spans=${total_missing_root_prepared_retained_spans}\n"
  "missing_root_prepared_only_spans=${total_missing_root_prepared_only_spans}\n"
  "missing_root_prepared_only_retained_spans=${total_missing_root_prepared_only_retained_spans}\n"
  "missing_root_prepared_inconclusive_spans=${total_missing_root_prepared_inconclusive_spans}\n"
  "missing_root_prepared_candidate_failures=${total_missing_root_prepared_candidate_failures}\n"
  "missing_root_prepared_signal_rays=${total_missing_root_prepared_signal_rays}\n"
  "missing_root_prepared_independent_hit_rays=${total_missing_root_prepared_independent_hit_rays}\n"
  "missing_root_prepared_independent_empty_rays=${total_missing_root_prepared_independent_empty_rays}\n"
  "missing_root_prepared_unresolved_rays=${total_missing_root_prepared_unresolved_rays}\n"
  "missing_root_counterfactual_attempts=${total_missing_root_counterfactual_attempts}\n"
  "missing_root_counterfactual_candidate_spans=${total_missing_root_counterfactual_candidate_spans}\n"
  "missing_root_counterfactual_retained_spans=${total_missing_root_counterfactual_retained_spans}\n"
  "missing_root_counterfactual_restricted_spans=${total_missing_root_counterfactual_restricted_spans}\n"
  "missing_root_counterfactual_inconclusive_spans=${total_missing_root_counterfactual_inconclusive_spans}\n"
  "missing_root_counterfactual_workspace_overflow=${total_missing_root_counterfactual_workspace_overflow}\n"
  "missing_root_counterfactual_true_root_controls=${total_missing_root_counterfactual_true_root_controls}\n"
  "missing_root_counterfactual_true_root_signals=${total_missing_root_counterfactual_true_root_signals}\n"
  "missing_root_counterfactual_true_root_missed=${total_missing_root_counterfactual_true_root_missed}\n"
  "missing_root_counterfactual_empty_controls=${total_missing_root_counterfactual_empty_controls}\n"
  "missing_root_counterfactual_empty_signals=${total_missing_root_counterfactual_empty_signals}\n"
  "missing_root_counterfactual_empty_rejected=${total_missing_root_counterfactual_empty_rejected}\n"
  "missing_root_counterfactual_unresolved_controls=${total_missing_root_counterfactual_unresolved_controls}\n"
  "missing_root_counterfactual_prepared_candidate_faces=${total_missing_root_counterfactual_prepared_candidate_faces}\n"
  "missing_root_counterfactual_prepared_candidate_spans=${total_missing_root_counterfactual_prepared_candidate_spans}\n"
  "missing_root_counterfactual_prepared_retained_spans=${total_missing_root_counterfactual_prepared_retained_spans}\n"
  "missing_root_counterfactual_prepared_only_spans=${total_missing_root_counterfactual_prepared_only_spans}\n"
  "missing_root_counterfactual_prepared_only_retained_spans=${total_missing_root_counterfactual_prepared_only_retained_spans}\n"
  "missing_root_counterfactual_prepared_inconclusive_spans=${total_missing_root_counterfactual_prepared_inconclusive_spans}\n"
  "missing_root_counterfactual_prepared_candidate_failures=${total_missing_root_counterfactual_prepared_candidate_failures}\n"
  "missing_root_counterfactual_prepared_signal_rays=${total_missing_root_counterfactual_prepared_signal_rays}\n"
  "missing_root_counterfactual_prepared_unsignaled_rays=${total_missing_root_counterfactual_prepared_unsignaled_rays}\n"
  "missing_root_counterfactual_true_root_prepared_signals=${total_missing_root_counterfactual_true_root_prepared_signals}\n"
  "missing_root_counterfactual_true_root_prepared_missed=${total_missing_root_counterfactual_true_root_prepared_missed}\n"
  "missing_root_counterfactual_true_root_prepared_recovered=${total_missing_root_counterfactual_true_root_prepared_recovered}\n"
  "missing_root_counterfactual_empty_prepared_signals=${total_missing_root_counterfactual_empty_prepared_signals}\n"
  "missing_root_counterfactual_empty_prepared_rejected=${total_missing_root_counterfactual_empty_prepared_rejected}\n"
  "seeded_verify_attempts=${total_verify_attempts}\n"
)
foreach(certificate_scope IN LISTS certificate_scopes)
  file(APPEND "${manifest_file}"
    "missing_root_certificate_${certificate_scope}_accounting_invalid=${total_${certificate_scope}_certificate_accounting_invalid}\n"
    "missing_root_certificate_${certificate_scope}_globally_unique_records=${total_${certificate_scope}_certificate_globally_unique}\n"
    "missing_root_certificate_${certificate_scope}_publication_authority_records=${total_${certificate_scope}_certificate_publication_authority}\n"
  )
  foreach(metric IN LISTS certificate_scalar_metrics)
    file(APPEND "${manifest_file}"
      "missing_root_certificate_${certificate_scope}_${metric}=${total_${certificate_scope}_certificate_${metric}}\n"
    )
  endforeach()
  file(APPEND "${manifest_file}"
    "missing_root_certificate_${certificate_scope}_expansion_high_water=${total_${certificate_scope}_certificate_expansion_high_water}\n"
  )
  foreach(reason IN LISTS certificate_failure_reasons)
    file(APPEND "${manifest_file}"
      "missing_root_certificate_${certificate_scope}_failure_${reason}=${total_${certificate_scope}_certificate_failure_${reason}}\n"
    )
  endforeach()
  foreach(detail IN LISTS certificate_degree_failure_details)
    file(APPEND "${manifest_file}"
      "missing_root_certificate_${certificate_scope}_degree_failure_${detail}=${total_${certificate_scope}_certificate_degree_failure_${detail}}\n"
    )
  endforeach()
endforeach()
foreach(depth_role reference candidate)
  file(APPEND "${manifest_file}"
    "${depth_role}_legacy_cleanup_action_mismatch_rays=${total_${depth_role}_cleanup_mismatch_rays}\n"
  )
  foreach(action IN LISTS legacy_cleanup_actions)
    file(APPEND "${manifest_file}"
      "${depth_role}_legacy_cleanup_${action}_rays=${total_${depth_role}_cleanup_${action}_rays}\n"
      "${depth_role}_legacy_cleanup_${action}_events=${total_${depth_role}_cleanup_${action}_events}\n"
    )
  endforeach()
  foreach(reason IN LISTS legacy_capacity_reasons)
    file(APPEND "${manifest_file}"
      "${depth_role}_legacy_capacity_${reason}_rays=${total_${depth_role}_capacity_${reason}_rays}\n"
    )
  endforeach()
endforeach()
file(SHA256 "${cleanup_csv_file}" cleanup_csv_sha256)
file(SHA256 "${capacity_csv_file}" capacity_csv_sha256)
file(SHA256 "${certificate_csv_file}" certificate_csv_sha256)
file(APPEND "${manifest_file}"
  "missing_root_certificate_csv=${certificate_csv_file}\n"
  "missing_root_certificate_csv_sha256=${certificate_csv_sha256}\n"
  "legacy_cleanup_csv=${cleanup_csv_file}\n"
  "legacy_cleanup_csv_sha256=${cleanup_csv_sha256}\n"
  "legacy_capacity_csv=${capacity_csv_file}\n"
  "legacy_capacity_csv_sha256=${capacity_csv_sha256}\n"
)

message(STATUS
  "Routing corpus complete: ${total_objects} objects, ${total_rays} rays, "
  "${total_corrections} corrections, ${total_missed} missed; ${OUTPUT_DIR}"
)

# Local Variables:
# tab-width: 2
# mode: cmake
# indent-tabs-mode: nil
# End:
# ex: shiftwidth=2 tabstop=2
