#                B R E P _ R O U T I N G _ S U I T E . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# Run the routing truth driver over a set of imported databases and preserve
# one aggregate, versioned manifest.  If DATABASES is not supplied, all NIST
# databases installed under BUILD_DIR/share/db/nist are selected.  Each BREP
# is also prepared in a fresh process at both requested depths so prep time,
# process residency, derived routing-store populations, and the exact raw
# serialized cache payload size are recorded independently of ray tracing.
#
#   cmake -DBUILD_DIR=/path/to/build \
#     -P /path/to/source/bench/brep_routing_suite.cmake
#
# Optional inputs are SOURCE_DIR, OUTPUT_DIR, DATABASES (a CMake list),
# RANDOM_RAY_COUNT, FEATURE_RAY_COUNT, CANDIDATE_DEPTH, REFERENCE_DEPTH,
# OBJECTS, OBJECT_REGEX, and MEASURE_CACHE_PREP.  Cache prep measurement is on
# by default and records an isolated cold write followed by a warm BRP6 load.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED BUILD_DIR OR "${BUILD_DIR}" STREQUAL "")
  message(FATAL_ERROR "BUILD_DIR must name a configured BRL-CAD build directory")
endif()
if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH SOURCE_DIR)
endif()
if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/brep-routing-suite")
endif()
if(NOT DEFINED RANDOM_RAY_COUNT)
  set(RANDOM_RAY_COUNT 32)
endif()
if(NOT DEFINED FEATURE_RAY_COUNT)
  set(FEATURE_RAY_COUNT 24)
endif()
if(NOT DEFINED CANDIDATE_DEPTH)
  set(CANDIDATE_DEPTH 3)
endif()
if(NOT DEFINED REFERENCE_DEPTH)
  set(REFERENCE_DEPTH 8)
endif()
if(NOT DEFINED MEASURE_CACHE_PREP)
  set(MEASURE_CACHE_PREP ON)
endif()

foreach(integer_variable
    RANDOM_RAY_COUNT FEATURE_RAY_COUNT CANDIDATE_DEPTH REFERENCE_DEPTH)
  if(NOT "${${integer_variable}}" MATCHES "^[0-9]+$")
    message(FATAL_ERROR "${integer_variable} must be a nonnegative integer")
  endif()
endforeach()

file(REAL_PATH "${BUILD_DIR}" BUILD_DIR EXPAND_TILDE)
file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR EXPAND_TILDE)
cmake_path(ABSOLUTE_PATH OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE OUTPUT_DIR)

if(NOT DEFINED DATABASES OR "${DATABASES}" STREQUAL "")
  file(GLOB DATABASES LIST_DIRECTORIES FALSE
    "${BUILD_DIR}/share/db/nist/NIST_MBE_PMI_*.g")
  list(SORT DATABASES COMPARE NATURAL)
endif()
if(NOT DATABASES)
  message(FATAL_ERROR "No imported BREP databases were selected")
endif()

function(find_suite_program result build_dir program_name)
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

function(manifest_value result manifest key)
  file(STRINGS "${manifest}" matches REGEX "^${key}=" LIMIT_COUNT 1)
  if(NOT matches)
    message(FATAL_ERROR "${manifest} has no ${key} field")
  endif()
  list(GET matches 0 value)
  string(REGEX REPLACE "^[^=]*=" "" value "${value}")
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(json_required result json)
  string(JSON value ERROR_VARIABLE json_error GET "${json}" ${ARGN})
  if(NOT json_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Missing/invalid JSON field '${ARGN}': ${json_error}")
  endif()
  set(${result} "${value}" PARENT_SCOPE)
endfunction()

function(csv_quote result value)
  string(REPLACE "\"" "\"\"" escaped "${value}")
  set(${result} "\"${escaped}\"" PARENT_SCOPE)
endfunction()

macro(add_suite_total total_name value)
  math(EXPR ${total_name} "${${total_name}} + ${value}")
endmacro()

find_suite_program(brep_audit "${BUILD_DIR}" brep-audit)
set(routing_driver "${SOURCE_DIR}/bench/brep_routing_corpus.cmake")
if(NOT EXISTS "${routing_driver}")
  message(FATAL_ERROR "Cannot find routing corpus driver: ${routing_driver}")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(run_root "${OUTPUT_DIR}/runs")
file(MAKE_DIRECTORY "${run_root}")
set(database_csv "${OUTPUT_DIR}/databases.csv")
set(resource_csv "${OUTPUT_DIR}/prep_resources.csv")
set(resource_jsonl "${OUTPUT_DIR}/prep_resources.jsonl")
set(cache_resource_jsonl "${OUTPUT_DIR}/cache_prep_resources.jsonl")
set(combined_jsonl "${OUTPUT_DIR}/routing_truth.jsonl")
set(stderr_file "${OUTPUT_DIR}/stderr.log")
set(manifest_file "${OUTPUT_DIR}/manifest.txt")
file(WRITE "${database_csv}"
  "database,database_sha256,objects,nonzero_exits,rays,differences,corrections,missed_corrections,prepared_reversals,legacy_reversals,depth_differences,cell_lookup_failures,overflow_only_unmapped_roots,child_manifest_sha256,child_summary_sha256,child_jsonl_sha256\n")
file(WRITE "${resource_csv}"
  "database,object,depth,status,prep_seconds,peak_rss_bytes,resident_before_bytes,resident_prepared_bytes,resident_after_free_bytes,retained_bytes,retained_used_bytes,retained_unused_capacity_bytes,retained_brep_bytes,retained_surface_tree_bytes,retained_curve_tree_bytes,retained_edge_bytes,retained_surface_bytes,retained_hierarchy_bytes,retained_routing_bytes,retained_vertex_bytes,surface_tree_unused_bytes,curve_tree_unused_bytes,edge_unused_bytes,surface_unused_bytes,hierarchy_unused_bytes,routing_unused_bytes,vertex_unused_bytes,routing_feature_cell_unused_bytes,routing_binding_unused_bytes,routing_pack_unused_bytes,routing_control_unused_bytes,routing_subpatch_unused_bytes,routing_bvh_unused_bytes,cache_variant,cache_payload_bytes,cache_payload_compressed_bytes,surface_tree_nodes,surface_tree_leaves,routing_feature_cells,routing_unsupported_cells,routing_cell_span_packs,routing_control_intervals,routing_subpatch_boxes,cache_cold_hit,cache_cold_prep_seconds,cache_cold_peak_rss_bytes,cache_cold_retained_bytes,cache_warm_hit,cache_warm_prep_seconds,cache_warm_peak_rss_bytes,cache_warm_retained_bytes\n")
file(WRITE "${resource_jsonl}" "")
file(WRITE "${cache_resource_jsonl}" "")
file(WRITE "${combined_jsonl}" "")
file(WRITE "${stderr_file}" "")

set(total_databases 0)
set(total_objects 0)
set(total_nonzero_exits 0)
set(total_rays 0)
set(total_differences 0)
set(total_corrections 0)
set(total_missed 0)
set(total_prepared_reversals 0)
set(total_legacy_reversals 0)
set(total_depth_differences 0)
set(total_lookup_failures 0)
set(total_overflow_unmapped 0)
set(total_resource_runs 0)
set(total_resource_failures 0)
set(total_cache_prep_runs 0)
set(total_cache_prep_failures 0)
set(total_cache_payload_bytes 0)
set(total_compressed_cache_payload_bytes 0)
set(maximum_cache_payload_bytes 0)
set(maximum_compressed_cache_payload_bytes 0)
set(maximum_peak_rss_bytes 0)
set(maximum_retained_bytes 0)
set(maximum_cache_cold_prep_seconds 0)
set(maximum_cache_warm_prep_seconds 0)
set(maximum_cache_warm_peak_rss_bytes 0)

set(resource_depths ${CANDIDATE_DEPTH} ${REFERENCE_DEPTH})
list(REMOVE_DUPLICATES resource_depths)

foreach(database IN LISTS DATABASES)
  file(REAL_PATH "${database}" database EXPAND_TILDE)
  if(NOT EXISTS "${database}")
    message(FATAL_ERROR "Imported BREP database does not exist: ${database}")
  endif()
  cmake_path(GET database STEM database_label)
  string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" database_label
    "${database_label}")
  set(child_dir "${run_root}/${database_label}")
  message(STATUS "Imported routing suite: ${database_label}")
  set(child_command
    "${CMAKE_COMMAND}"
    "-DBUILD_DIR=${BUILD_DIR}"
    "-DSOURCE_DIR=${SOURCE_DIR}"
    "-DDATABASE=${database}"
    "-DOUTPUT_DIR=${child_dir}"
    "-DRANDOM_RAY_COUNT=${RANDOM_RAY_COUNT}"
    "-DFEATURE_RAY_COUNT=${FEATURE_RAY_COUNT}"
    "-DCANDIDATE_DEPTH=${CANDIDATE_DEPTH}"
    "-DREFERENCE_DEPTH=${REFERENCE_DEPTH}"
  )
  if(DEFINED OBJECTS AND NOT "${OBJECTS}" STREQUAL "")
    string(REPLACE ";" "\\;" escaped_objects "${OBJECTS}")
    list(APPEND child_command "-DOBJECTS:STRING=${escaped_objects}")
  endif()
  if(DEFINED OBJECT_REGEX AND NOT "${OBJECT_REGEX}" STREQUAL "")
    list(APPEND child_command "-DOBJECT_REGEX=${OBJECT_REGEX}")
  endif()
  list(APPEND child_command -P "${routing_driver}")
  execute_process(
    COMMAND ${child_command}
    RESULT_VARIABLE child_result
    OUTPUT_VARIABLE child_stdout
    ERROR_VARIABLE child_stderr
  )
  file(APPEND "${stderr_file}"
    "===== ${database_label} (driver exit ${child_result}) =====\n${child_stderr}\n")
  if(NOT child_result EQUAL 0)
    message(FATAL_ERROR
      "Routing corpus driver failed for ${database} (${child_result})")
  endif()

  set(child_manifest "${child_dir}/manifest.txt")
  set(child_summary "${child_dir}/summary.csv")
  set(child_jsonl "${child_dir}/routing_truth.jsonl")
  foreach(child_file IN ITEMS child_manifest child_summary child_jsonl)
    if(NOT EXISTS "${${child_file}}")
      message(FATAL_ERROR "Missing ${child_file} output for ${database}")
    endif()
  endforeach()
  file(SHA256 "${database}" database_sha256)
  file(SHA256 "${child_manifest}" child_manifest_sha256)
  file(SHA256 "${child_summary}" child_summary_sha256)
  file(SHA256 "${child_jsonl}" child_jsonl_sha256)
  manifest_value(child_objects "${child_manifest}" completed_objects)
  manifest_value(child_nonzero "${child_manifest}" nonzero_object_exits)
  manifest_value(child_rays "${child_manifest}" rays)
  manifest_value(child_differences "${child_manifest}"
    prepared_legacy_differences)
  manifest_value(child_corrections "${child_manifest}"
    authoritative_correction_rays)
  manifest_value(child_missed "${child_manifest}" missed_correction_rays)
  manifest_value(child_prepared_reversals "${child_manifest}"
    prepared_reversal_mismatches)
  manifest_value(child_legacy_reversals "${child_manifest}"
    legacy_reversal_mismatches)
  manifest_value(child_depth_differences "${child_manifest}"
    depth_differences)
  manifest_value(child_lookup_failures "${child_manifest}"
    cell_lookup_failures)
  manifest_value(child_overflow_unmapped "${child_manifest}"
    roots_unmapped_after_leaf_overflow)

  csv_quote(csv_database "${database}")
  file(APPEND "${database_csv}"
    "${csv_database},${database_sha256},${child_objects},${child_nonzero},${child_rays},${child_differences},${child_corrections},${child_missed},${child_prepared_reversals},${child_legacy_reversals},${child_depth_differences},${child_lookup_failures},${child_overflow_unmapped},${child_manifest_sha256},${child_summary_sha256},${child_jsonl_sha256}\n")
  file(READ "${child_jsonl}" child_jsonl_text)
  file(APPEND "${combined_jsonl}" "${child_jsonl_text}")

  add_suite_total(total_databases 1)
  add_suite_total(total_objects ${child_objects})
  add_suite_total(total_nonzero_exits ${child_nonzero})
  add_suite_total(total_rays ${child_rays})
  add_suite_total(total_differences ${child_differences})
  add_suite_total(total_corrections ${child_corrections})
  add_suite_total(total_missed ${child_missed})
  add_suite_total(total_prepared_reversals ${child_prepared_reversals})
  add_suite_total(total_legacy_reversals ${child_legacy_reversals})
  add_suite_total(total_depth_differences ${child_depth_differences})
  add_suite_total(total_lookup_failures ${child_lookup_failures})
  add_suite_total(total_overflow_unmapped ${child_overflow_unmapped})

  file(STRINGS "${child_jsonl}" object_records)
  foreach(object_record IN LISTS object_records)
    json_required(object "${object_record}" object)
    foreach(resource_depth IN LISTS resource_depths)
      execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
          --unset=LIBRT_BREP_DEBUG
          --unset=LIBRT_BREP_PROFILE
          --unset=LIBRT_BREP_ROUTE_PROFILE
          --unset=LIBRT_BREP_SEEDED_VERIFY
          --unset=LIBRT_BREP_SOLVER
          "LIBRT_CACHE=off"
          "${brep_audit}" --prep-only
          "--surface-tree-depth=${resource_depth}"
          "${database}" "${object}"
        RESULT_VARIABLE prep_result
        OUTPUT_VARIABLE prep_json
        ERROR_VARIABLE prep_stderr
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      string(STRIP "${prep_json}" prep_json)
      if(prep_json STREQUAL "")
        message(FATAL_ERROR
          "Prep resource audit emitted no JSON for ${database}/${object} at depth ${resource_depth}: ${prep_stderr}")
      endif()
      string(JSON prep_format ERROR_VARIABLE prep_json_error GET
        "${prep_json}" format)
      if(NOT prep_json_error STREQUAL "NOTFOUND" OR
          NOT prep_format STREQUAL "brlcad-brep-ray-prep-audit-v1")
        message(FATAL_ERROR
          "Invalid prep resource JSON for ${database}/${object}: ${prep_json_error}")
      endif()
      file(APPEND "${resource_jsonl}" "${prep_json}\n")
      if(NOT prep_stderr STREQUAL "")
        file(APPEND "${stderr_file}"
          "===== prep ${database_label}/${object}/depth-${resource_depth} (exit ${prep_result}) =====\n${prep_stderr}\n")
      endif()

      json_required(prep_status "${prep_json}" status)
      json_required(prep_seconds "${prep_json}" seconds)
      json_required(peak_rss "${prep_json}" peak_rss_bytes)
      json_required(resident_before "${prep_json}" resident_before_bytes)
      json_required(resident_prepared "${prep_json}" resident_prepared_bytes)
      json_required(resident_after "${prep_json}" resident_after_free_bytes)
      json_required(retained_bytes "${prep_json}" retained_bytes)
      json_required(retained_used_bytes "${prep_json}" retained_used_bytes)
      json_required(retained_unused_bytes "${prep_json}"
        retained_unused_capacity_bytes)
      json_required(retained_brep_bytes "${prep_json}" retained_brep_bytes)
      json_required(retained_surface_tree_bytes "${prep_json}"
        retained_surface_tree_bytes)
      json_required(retained_curve_tree_bytes "${prep_json}"
        retained_curve_tree_bytes)
      json_required(retained_edge_bytes "${prep_json}" retained_edge_bytes)
      json_required(retained_surface_bytes "${prep_json}" retained_surface_bytes)
      json_required(retained_hierarchy_bytes "${prep_json}"
        retained_hierarchy_bytes)
      json_required(retained_routing_bytes "${prep_json}" retained_routing_bytes)
      json_required(retained_vertex_bytes "${prep_json}" retained_vertex_bytes)
      json_required(surface_tree_unused_bytes "${prep_json}"
        retained_surface_tree_unused_capacity_bytes)
      json_required(curve_tree_unused_bytes "${prep_json}"
        retained_curve_tree_unused_capacity_bytes)
      json_required(edge_unused_bytes "${prep_json}"
        retained_edge_unused_capacity_bytes)
      json_required(surface_unused_bytes "${prep_json}"
        retained_surface_unused_capacity_bytes)
      json_required(hierarchy_unused_bytes "${prep_json}"
        retained_hierarchy_unused_capacity_bytes)
      json_required(routing_unused_bytes "${prep_json}"
        retained_routing_unused_capacity_bytes)
      json_required(vertex_unused_bytes "${prep_json}"
        retained_vertex_unused_capacity_bytes)
      json_required(routing_feature_cell_unused_bytes "${prep_json}"
        retained_routing_feature_cell_unused_capacity_bytes)
      json_required(routing_binding_unused_bytes "${prep_json}"
        retained_routing_binding_unused_capacity_bytes)
      json_required(routing_pack_unused_bytes "${prep_json}"
        retained_routing_pack_unused_capacity_bytes)
      json_required(routing_control_unused_bytes "${prep_json}"
        retained_routing_control_unused_capacity_bytes)
      json_required(routing_subpatch_unused_bytes "${prep_json}"
        retained_routing_subpatch_unused_capacity_bytes)
      json_required(routing_bvh_unused_bytes "${prep_json}"
        retained_routing_bvh_unused_capacity_bytes)
      json_required(cache_payload "${prep_json}" cache_payload_bytes)
      json_required(compressed_cache_payload "${prep_json}"
        cache_payload_compressed_bytes)
      json_required(nodes "${prep_json}" surface_tree_nodes)
      json_required(leaves "${prep_json}" surface_tree_leaves)
      json_required(cells "${prep_json}" routing_feature_cells)
      json_required(unsupported_cells "${prep_json}"
        routing_unsupported_cells)
      json_required(packs "${prep_json}" routing_cell_span_packs)
      json_required(control_intervals "${prep_json}"
        routing_cell_span_control_intervals)
      json_required(subpatch_boxes "${prep_json}"
        routing_cell_span_subpatch_boxes)
      string(JSON cache_variant ERROR_VARIABLE cache_variant_error GET
        "${prep_json}" cache_variant)
      if(NOT cache_variant_error STREQUAL "NOTFOUND")
        set(cache_variant "")
      endif()

      set(cache_cold_hit false)
      set(cache_cold_seconds 0)
      set(cache_cold_peak_rss 0)
      set(cache_cold_retained_bytes 0)
      set(cache_warm_hit false)
      set(cache_warm_seconds 0)
      set(cache_warm_peak_rss 0)
      set(cache_warm_retained_bytes 0)
      if(MEASURE_CACHE_PREP)
        string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" object_label
          "${object}")
        set(prep_cache_dir
          "${OUTPUT_DIR}/prep-cache/${database_label}-${object_label}-depth-${resource_depth}")
        cmake_path(IS_PREFIX OUTPUT_DIR "${prep_cache_dir}" NORMALIZE
          cache_is_local)
        if(NOT cache_is_local)
          message(FATAL_ERROR
            "Refusing prep cache outside suite output: ${prep_cache_dir}")
        endif()
        file(REMOVE_RECURSE "${prep_cache_dir}")
        file(TO_NATIVE_PATH "${prep_cache_dir}" native_prep_cache_dir)

        execute_process(
          COMMAND "${CMAKE_COMMAND}" -E env
            --unset=LIBRT_BREP_DEBUG
            --unset=LIBRT_BREP_PROFILE
            --unset=LIBRT_BREP_ROUTE_PROFILE
            --unset=LIBRT_BREP_SEEDED_VERIFY
            --unset=LIBRT_BREP_SOLVER
            "LIBRT_CACHE=${native_prep_cache_dir}"
            "${brep_audit}" --cached-prep-only
            "--surface-tree-depth=${resource_depth}"
            "${database}" "${object}"
          RESULT_VARIABLE cache_cold_result
          OUTPUT_VARIABLE cache_cold_json
          ERROR_VARIABLE cache_cold_stderr
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
          COMMAND "${CMAKE_COMMAND}" -E env
            --unset=LIBRT_BREP_DEBUG
            --unset=LIBRT_BREP_PROFILE
            --unset=LIBRT_BREP_ROUTE_PROFILE
            --unset=LIBRT_BREP_SEEDED_VERIFY
            --unset=LIBRT_BREP_SOLVER
            "LIBRT_CACHE=${native_prep_cache_dir}"
            "${brep_audit}" --cached-prep-only
            "--surface-tree-depth=${resource_depth}"
            "${database}" "${object}"
          RESULT_VARIABLE cache_warm_result
          OUTPUT_VARIABLE cache_warm_json
          ERROR_VARIABLE cache_warm_stderr
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        string(STRIP "${cache_cold_json}" cache_cold_json)
        string(STRIP "${cache_warm_json}" cache_warm_json)
        file(APPEND "${cache_resource_jsonl}"
          "${cache_cold_json}\n${cache_warm_json}\n")
        if(NOT cache_cold_stderr STREQUAL "")
          file(APPEND "${stderr_file}"
            "===== cache-cold ${database_label}/${object}/depth-${resource_depth} (exit ${cache_cold_result}) =====\n${cache_cold_stderr}\n")
        endif()
        if(NOT cache_warm_stderr STREQUAL "")
          file(APPEND "${stderr_file}"
            "===== cache-warm ${database_label}/${object}/depth-${resource_depth} (exit ${cache_warm_result}) =====\n${cache_warm_stderr}\n")
        endif()

        if(cache_cold_json STREQUAL "" OR cache_warm_json STREQUAL "")
          file(REMOVE_RECURSE "${prep_cache_dir}")
          message(FATAL_ERROR
            "Cache prep emitted no JSON for ${database}/${object} at depth ${resource_depth}")
        endif()
        json_required(cache_cold_status "${cache_cold_json}" status)
        json_required(cache_warm_status "${cache_warm_json}" status)
        json_required(cache_cold_hit "${cache_cold_json}" cache_hit)
        json_required(cache_warm_hit "${cache_warm_json}" cache_hit)
        json_required(cache_cold_seconds "${cache_cold_json}" seconds)
        json_required(cache_warm_seconds "${cache_warm_json}" seconds)
        json_required(cache_cold_peak_rss "${cache_cold_json}"
          peak_rss_bytes)
        json_required(cache_cold_retained_bytes "${cache_cold_json}"
          retained_bytes)
        json_required(cache_warm_peak_rss "${cache_warm_json}"
          peak_rss_bytes)
        json_required(cache_warm_retained_bytes "${cache_warm_json}"
          retained_bytes)
        math(EXPR total_cache_prep_runs "${total_cache_prep_runs} + 2")
        if(NOT cache_cold_result EQUAL 0 OR
            NOT cache_warm_result EQUAL 0 OR
            NOT cache_cold_status STREQUAL "ok" OR
            NOT cache_warm_status STREQUAL "ok" OR cache_cold_hit OR
            NOT cache_warm_hit)
          math(EXPR total_cache_prep_failures
            "${total_cache_prep_failures} + 1")
        endif()

        foreach(population_field
            recorded_adaptive_depth surface_tree_nodes surface_tree_leaves
            routing_feature_cells routing_unsupported_cells
            routing_cell_span_packs routing_cell_span_control_intervals
            routing_cell_span_subpatch_boxes)
          json_required(fresh_population "${prep_json}" ${population_field})
          json_required(cold_population "${cache_cold_json}"
            ${population_field})
          json_required(warm_population "${cache_warm_json}"
            ${population_field})
          if(NOT "${fresh_population}" STREQUAL "${cold_population}" OR
              NOT "${fresh_population}" STREQUAL "${warm_population}")
            file(REMOVE_RECURSE "${prep_cache_dir}")
            message(FATAL_ERROR
              "Fresh/cold/warm ${population_field} mismatch for ${database}/${object} at depth ${resource_depth}: ${fresh_population}/${cold_population}/${warm_population}")
          endif()
        endforeach()
        if(cache_cold_seconds GREATER maximum_cache_cold_prep_seconds)
          set(maximum_cache_cold_prep_seconds ${cache_cold_seconds})
        endif()
        if(cache_warm_seconds GREATER maximum_cache_warm_prep_seconds)
          set(maximum_cache_warm_prep_seconds ${cache_warm_seconds})
        endif()
        if(cache_warm_peak_rss GREATER maximum_cache_warm_peak_rss_bytes)
          set(maximum_cache_warm_peak_rss_bytes ${cache_warm_peak_rss})
        endif()
        file(REMOVE_RECURSE "${prep_cache_dir}")
      endif()
      csv_quote(csv_object "${object}")
      csv_quote(csv_status "${prep_status}")
      file(APPEND "${resource_csv}"
        "${csv_database},${csv_object},${resource_depth},${csv_status},${prep_seconds},${peak_rss},${resident_before},${resident_prepared},${resident_after},${retained_bytes},${retained_used_bytes},${retained_unused_bytes},${retained_brep_bytes},${retained_surface_tree_bytes},${retained_curve_tree_bytes},${retained_edge_bytes},${retained_surface_bytes},${retained_hierarchy_bytes},${retained_routing_bytes},${retained_vertex_bytes},${surface_tree_unused_bytes},${curve_tree_unused_bytes},${edge_unused_bytes},${surface_unused_bytes},${hierarchy_unused_bytes},${routing_unused_bytes},${vertex_unused_bytes},${routing_feature_cell_unused_bytes},${routing_binding_unused_bytes},${routing_pack_unused_bytes},${routing_control_unused_bytes},${routing_subpatch_unused_bytes},${routing_bvh_unused_bytes},${cache_variant},${cache_payload},${compressed_cache_payload},${nodes},${leaves},${cells},${unsupported_cells},${packs},${control_intervals},${subpatch_boxes},${cache_cold_hit},${cache_cold_seconds},${cache_cold_peak_rss},${cache_cold_retained_bytes},${cache_warm_hit},${cache_warm_seconds},${cache_warm_peak_rss},${cache_warm_retained_bytes}\n")
      add_suite_total(total_resource_runs 1)
      if(NOT prep_result EQUAL 0 OR NOT prep_status STREQUAL "ok")
        add_suite_total(total_resource_failures 1)
      endif()
      add_suite_total(total_cache_payload_bytes ${cache_payload})
      add_suite_total(total_compressed_cache_payload_bytes
        ${compressed_cache_payload})
      if(cache_payload GREATER maximum_cache_payload_bytes)
        set(maximum_cache_payload_bytes ${cache_payload})
      endif()
      if(peak_rss GREATER maximum_peak_rss_bytes)
        set(maximum_peak_rss_bytes ${peak_rss})
      endif()
      if(compressed_cache_payload GREATER maximum_compressed_cache_payload_bytes)
        set(maximum_compressed_cache_payload_bytes ${compressed_cache_payload})
      endif()
      if(retained_bytes GREATER maximum_retained_bytes)
        set(maximum_retained_bytes ${retained_bytes})
      endif()
    endforeach()
  endforeach()
endforeach()

file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" driver_sha256)
file(SHA256 "${database_csv}" database_csv_sha256)
file(SHA256 "${resource_csv}" resource_csv_sha256)
file(SHA256 "${resource_jsonl}" resource_jsonl_sha256)
file(SHA256 "${cache_resource_jsonl}" cache_resource_jsonl_sha256)
file(SHA256 "${combined_jsonl}" combined_jsonl_sha256)
string(TIMESTAMP run_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${manifest_file}"
  "BREP imported routing suite manifest\n"
  "format=brlcad-brep-routing-suite-v1\n"
  "timestamp_utc=${run_timestamp}\n"
  "source_dir=${SOURCE_DIR}\n"
  "build_dir=${BUILD_DIR}\n"
  "driver=${CMAKE_CURRENT_LIST_FILE}\n"
  "driver_sha256=${driver_sha256}\n"
  "routing_driver=${routing_driver}\n"
  "random_chord_pairs=${RANDOM_RAY_COUNT}\n"
  "feature_chord_pairs=${FEATURE_RAY_COUNT}\n"
  "candidate_depth=${CANDIDATE_DEPTH}\n"
  "reference_depth=${REFERENCE_DEPTH}\n"
  "cache=disabled for ray and fresh-prep measurements\n"
  "database_count=${total_databases}\n"
  "completed_objects=${total_objects}\n"
  "nonzero_object_exits=${total_nonzero_exits}\n"
  "rays=${total_rays}\n"
  "prepared_legacy_differences=${total_differences}\n"
  "authoritative_correction_rays=${total_corrections}\n"
  "missed_correction_rays=${total_missed}\n"
  "prepared_reversal_mismatches=${total_prepared_reversals}\n"
  "legacy_reversal_mismatches=${total_legacy_reversals}\n"
  "depth_differences=${total_depth_differences}\n"
  "cell_lookup_failures=${total_lookup_failures}\n"
  "roots_unmapped_after_leaf_overflow=${total_overflow_unmapped}\n"
  "prep_resource_runs=${total_resource_runs}\n"
  "prep_resource_failures=${total_resource_failures}\n"
  "cache_prep_measurement=${MEASURE_CACHE_PREP}\n"
  "cache_prep_runs=${total_cache_prep_runs}\n"
  "cache_prep_failures=${total_cache_prep_failures}\n"
  "maximum_cache_cold_prep_seconds=${maximum_cache_cold_prep_seconds}\n"
  "maximum_cache_warm_prep_seconds=${maximum_cache_warm_prep_seconds}\n"
  "maximum_cache_warm_peak_rss_bytes=${maximum_cache_warm_peak_rss_bytes}\n"
  "sum_raw_cache_payload_bytes=${total_cache_payload_bytes}\n"
  "sum_compressed_cache_payload_bytes=${total_compressed_cache_payload_bytes}\n"
  "maximum_raw_cache_payload_bytes=${maximum_cache_payload_bytes}\n"
  "maximum_compressed_cache_payload_bytes=${maximum_compressed_cache_payload_bytes}\n"
  "maximum_retained_prepared_bytes=${maximum_retained_bytes}\n"
  "maximum_process_peak_rss_bytes=${maximum_peak_rss_bytes}\n"
  "database_csv=${database_csv}\n"
  "database_csv_sha256=${database_csv_sha256}\n"
  "prep_resource_csv=${resource_csv}\n"
  "prep_resource_csv_sha256=${resource_csv_sha256}\n"
  "prep_resource_jsonl=${resource_jsonl}\n"
  "prep_resource_jsonl_sha256=${resource_jsonl_sha256}\n"
  "cache_prep_resource_jsonl=${cache_resource_jsonl}\n"
  "cache_prep_resource_jsonl_sha256=${cache_resource_jsonl_sha256}\n"
  "routing_truth_jsonl=${combined_jsonl}\n"
  "routing_truth_jsonl_sha256=${combined_jsonl_sha256}\n"
)

if(total_nonzero_exits OR total_resource_failures OR
    total_cache_prep_failures OR total_differences OR
    total_missed OR total_prepared_reversals OR total_legacy_reversals OR
    total_depth_differences OR total_lookup_failures)
  message(FATAL_ERROR
    "Imported routing suite failed: objects=${total_objects}, rays=${total_rays}, object_exits=${total_nonzero_exits}, resource_failures=${total_resource_failures}, cache_prep_failures=${total_cache_prep_failures}, differences=${total_differences}, missed=${total_missed}, prepared_reversals=${total_prepared_reversals}, legacy_reversals=${total_legacy_reversals}, depth_differences=${total_depth_differences}, lookup_failures=${total_lookup_failures}; ${OUTPUT_DIR}")
endif()

message(STATUS
  "Imported routing suite passed: ${total_databases} databases, ${total_objects} objects, ${total_rays} rays, ${total_resource_runs} fresh prep runs, ${total_cache_prep_runs} cache prep runs; ${OUTPUT_DIR}")

# Local Variables:
# tab-width: 2
# mode: cmake
# indent-tabs-mode: nil
# End:
# ex: shiftwidth=2 tabstop=2
