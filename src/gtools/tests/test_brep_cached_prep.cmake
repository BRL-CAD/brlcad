# Verify that the cache-aware brep-audit path distinguishes a cold build from
# a warm BRP6 load and reconstructs the same depth-specific routing data.

foreach(required BREP_AUDIT DATABASE OBJECT CACHE_DIR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

cmake_path(ABSOLUTE_PATH CACHE_DIR
  BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" NORMALIZE
  OUTPUT_VARIABLE cache_dir)
file(REAL_PATH "${CMAKE_CURRENT_BINARY_DIR}" binary_dir)
cmake_path(IS_PREFIX binary_dir "${cache_dir}" NORMALIZE cache_is_local)
if(NOT cache_is_local)
  message(FATAL_ERROR "Refusing cache directory outside the test build: ${cache_dir}")
endif()
file(REMOVE_RECURSE "${cache_dir}")
file(TO_NATIVE_PATH "${cache_dir}" native_cache_dir)

function(run_cached_prep output_name)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      --unset=LIBRT_BREP_DEBUG
      --unset=LIBRT_BREP_PROFILE
      --unset=LIBRT_BREP_ROUTE_PROFILE
      "LIBRT_CACHE=${native_cache_dir}"
      "${BREP_AUDIT}" --cached-prep-only --surface-tree-depth=3
      "${DATABASE}" "${OBJECT}"
    RESULT_VARIABLE prep_result
    OUTPUT_VARIABLE prep_json
    ERROR_VARIABLE prep_stderr
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(STRIP "${prep_json}" prep_json)
  if(NOT prep_result EQUAL 0 OR prep_json STREQUAL "")
    file(REMOVE_RECURSE "${cache_dir}")
    message(FATAL_ERROR
      "Cache prep failed (${prep_result}): ${prep_stderr}\n${prep_json}")
  endif()
  string(JSON prep_format ERROR_VARIABLE json_error GET "${prep_json}" format)
  if(NOT json_error STREQUAL "NOTFOUND" OR
      NOT prep_format STREQUAL "brlcad-brep-ray-prep-audit-v1")
    file(REMOVE_RECURSE "${cache_dir}")
    message(FATAL_ERROR "Invalid cache-prep JSON: ${json_error}\n${prep_json}")
  endif()
  set(${output_name} "${prep_json}" PARENT_SCOPE)
endfunction()

run_cached_prep(cold_json)
run_cached_prep(warm_json)

foreach(field
    cache_lookup_requested status recorded_adaptive_depth
    surface_tree_build_microseconds surface_tree_nodes surface_tree_leaves
    routing_feature_cells routing_cell_span_packs
    routing_cell_span_control_intervals routing_cell_span_subpatch_boxes)
  string(JSON cold_${field} GET "${cold_json}" ${field})
  string(JSON warm_${field} GET "${warm_json}" ${field})
endforeach()
string(JSON cold_hit GET "${cold_json}" cache_hit)
string(JSON warm_hit GET "${warm_json}" cache_hit)

if(NOT cold_cache_lookup_requested OR NOT warm_cache_lookup_requested OR
    NOT cold_status STREQUAL "ok" OR NOT warm_status STREQUAL "ok" OR
    cold_hit OR NOT warm_hit OR
    NOT cold_recorded_adaptive_depth EQUAL 3 OR
    NOT warm_recorded_adaptive_depth EQUAL 3 OR
    NOT cold_surface_tree_build_microseconds GREATER 0 OR
    NOT warm_surface_tree_build_microseconds EQUAL 0)
  file(REMOVE_RECURSE "${cache_dir}")
  message(FATAL_ERROR
    "Cold/warm cache classification failed\ncold=${cold_json}\nwarm=${warm_json}")
endif()

foreach(field
    surface_tree_nodes surface_tree_leaves routing_feature_cells
    routing_cell_span_packs routing_cell_span_control_intervals
    routing_cell_span_subpatch_boxes)
  if(NOT "${cold_${field}}" STREQUAL "${warm_${field}}")
    file(REMOVE_RECURSE "${cache_dir}")
    message(FATAL_ERROR
      "Cold/warm ${field} mismatch: ${cold_${field}} != ${warm_${field}}")
  endif()
endforeach()

file(REMOVE_RECURSE "${cache_dir}")
message(STATUS "BREP cache prep cold/warm reconstruction passed")
