#                    B R E P _ R A Y T R A C E . C M A K E
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# Reproducible branch-versus-baseline BREP render benchmark.  Invoke with:
#
#   cmake -DBRANCH_BUILD=/path/to/branch-build \
#     -DBASELINE_BUILD=/path/to/main-build \
#     -P /path/to/brlcad/bench/brep_raytrace.cmake
#
# The default run performs one warm-up and seven measured trials for a Cobb
# sphere and a multi-face imported object.  The branch and baseline order is
# reversed on alternate trials.  Prep caching and opt-in BREP profiling are
# disabled.  Set INCLUDE_LEGACY=OFF to omit the branch's legacy-only control,
# INCLUDE_ROUTED=ON to add the opt-in nonduplicated router,
# ROUTED_PUBLICATION=ON to give its certified optional work selective
# publication authority, ROUTED_MISSING_ROOT_PROBE=ON to measure the
# independent prepared-candidate shadow stage, BRANCH_DIRECT_ROUTING=ON to
# make the branch lane its probe-off direct-routing control,
# ROUTED_MISSING_ROOT_CERTIFICATE=ON to enable the bounded cell/span root
# certificate, or CASES=cobb to run only the regular-render case.  The
# `imported` case defaults to the 214-face NIST 7--10 STEP-derived solid and may
# be redirected with IMPORTED_DATABASE and IMPORTED_OBJECT.  PROCESSORS,
# AZIMUTH, and ELEVATION select a scaling/view configuration.  CPU and wall
# throughput are reported separately so multi-CPU scaling is measurable.
# When routed and legacy lanes are both selected, their per-trial CPU- and
# wall-time ratios and the observed median of those paired ratios are also
# reported, preserving the benefit of the deliberately alternating order.  Set
# REQUIRE_ROUTED_LEGACY_MATCH=ON when this driver is used as a qualification
# gate rather than as an exploratory benchmark.  SURFACE_TREE_DEPTH,
# LIGHT_MODEL selects an optional material-independent RT lighting mode.
# DIRECTIONAL_CACHE_DEPTH, DIRECTIONAL_GRAZING_BOX_BUDGET, and
# ROUTING_PACK_CACHE_BYTES independently select the adaptive tree budget, the
# fixed ray-local grazing-work limit, and the optional derived-cache budgets;
# empty values use the executable defaults.

cmake_minimum_required(VERSION 3.20)

foreach(required_variable BRANCH_BUILD BASELINE_BUILD)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} must name a configured build directory")
  endif()
endforeach()

if(NOT DEFINED SOURCE_DIR OR "${SOURCE_DIR}" STREQUAL "")
  cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH SOURCE_DIR)
endif()
if(NOT DEFINED OUTPUT_DIR OR "${OUTPUT_DIR}" STREQUAL "")
  set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/brep-raytrace-benchmark")
endif()
if(NOT DEFINED TRIALS)
  set(TRIALS 7)
endif()
if(NOT DEFINED WARMUPS)
  set(WARMUPS 1)
endif()
if(NOT DEFINED INCLUDE_LEGACY)
  set(INCLUDE_LEGACY ON)
endif()
if(NOT DEFINED INCLUDE_ROUTED)
  set(INCLUDE_ROUTED OFF)
endif()
if(NOT DEFINED ROUTED_PUBLICATION)
  set(ROUTED_PUBLICATION OFF)
endif()
if(NOT DEFINED ROUTED_MISSING_ROOT_PROBE)
  set(ROUTED_MISSING_ROOT_PROBE OFF)
endif()
if(NOT DEFINED ROUTED_MISSING_ROOT_CERTIFICATE)
  set(ROUTED_MISSING_ROOT_CERTIFICATE OFF)
endif()
if(NOT DEFINED BRANCH_DIRECT_ROUTING)
  set(BRANCH_DIRECT_ROUTING OFF)
endif()
if(NOT DEFINED KEEP_IMAGES)
  set(KEEP_IMAGES OFF)
endif()
if(NOT DEFINED REQUIRE_ROUTED_LEGACY_MATCH)
  set(REQUIRE_ROUTED_LEGACY_MATCH OFF)
endif()
if(NOT DEFINED SURFACE_TREE_DEPTH)
  set(SURFACE_TREE_DEPTH "")
endif()
if(NOT DEFINED DIRECTIONAL_CACHE_DEPTH)
  set(DIRECTIONAL_CACHE_DEPTH "")
endif()
if(NOT DEFINED DIRECTIONAL_GRAZING_BOX_BUDGET)
  set(DIRECTIONAL_GRAZING_BOX_BUDGET "")
endif()
if(NOT DEFINED ROUTING_PACK_CACHE_BYTES)
  set(ROUTING_PACK_CACHE_BYTES "")
endif()
if(NOT DEFINED PROCESSORS)
  set(PROCESSORS 1)
endif()
if(NOT DEFINED AZIMUTH)
  set(AZIMUTH 35)
endif()
if(NOT DEFINED ELEVATION)
  set(ELEVATION 25)
endif()
if(NOT DEFINED LIGHT_MODEL)
  set(LIGHT_MODEL "")
endif()
if(NOT DEFINED COBB_SIZE)
  set(COBB_SIZE 512)
endif()
if(NOT DEFINED COBB_GRID)
  set(COBB_GRID 0.004)
endif()
if(NOT DEFINED MULTIFACE_SIZE)
  set(MULTIFACE_SIZE 128)
endif()
if(NOT DEFINED IMPORTED_SIZE)
  set(IMPORTED_SIZE 512)
endif()
if(NOT DEFINED CASES OR "${CASES}" STREQUAL "")
  set(CASES cobb multiface)
endif()

if(TRIALS LESS 1)
  message(FATAL_ERROR "TRIALS must be at least one")
endif()
math(EXPR trials_remainder "${TRIALS} % 2")
if(TRIALS GREATER 1 AND NOT trials_remainder EQUAL 1)
  message(FATAL_ERROR "TRIALS must be odd so the reported median is an observed run")
endif()
if(WARMUPS LESS 0)
  message(FATAL_ERROR "WARMUPS may not be negative")
endif()
foreach(size_variable COBB_SIZE MULTIFACE_SIZE IMPORTED_SIZE)
  if(NOT "${${size_variable}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${size_variable} must be a positive integer")
  endif()
endforeach()
if(NOT "${PROCESSORS}" MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "PROCESSORS must be a positive integer")
endif()
if(NOT "${SURFACE_TREE_DEPTH}" STREQUAL "" AND
    NOT "${SURFACE_TREE_DEPTH}" MATCHES "^[0-8]$")
  message(FATAL_ERROR "SURFACE_TREE_DEPTH must be empty or an integer from 0 through 8")
endif()
if(NOT "${DIRECTIONAL_CACHE_DEPTH}" STREQUAL "" AND
    NOT "${DIRECTIONAL_CACHE_DEPTH}" MATCHES "^[0-4]$")
  message(FATAL_ERROR "DIRECTIONAL_CACHE_DEPTH must be empty or an integer from 0 through 4")
endif()
if(NOT "${DIRECTIONAL_GRAZING_BOX_BUDGET}" STREQUAL "" AND
    (NOT "${DIRECTIONAL_GRAZING_BOX_BUDGET}" MATCHES "^[1-9][0-9]*$" OR
     DIRECTIONAL_GRAZING_BOX_BUDGET GREATER 1024))
  message(FATAL_ERROR
    "DIRECTIONAL_GRAZING_BOX_BUDGET must be empty or an integer from 1 through 1024"
  )
endif()
if(NOT "${ROUTING_PACK_CACHE_BYTES}" STREQUAL "" AND
    NOT "${ROUTING_PACK_CACHE_BYTES}" MATCHES "^[0-9]+$")
  message(FATAL_ERROR "ROUTING_PACK_CACHE_BYTES must be empty or a nonnegative byte count")
endif()
foreach(view_variable AZIMUTH ELEVATION)
  if(NOT "${${view_variable}}" MATCHES "^-?[0-9]+([.][0-9]+)?$")
    message(FATAL_ERROR "${view_variable} must be numeric")
  endif()
endforeach()
if(NOT "${LIGHT_MODEL}" STREQUAL "" AND
   NOT "${LIGHT_MODEL}" MATCHES "^[0-9]+$")
  message(FATAL_ERROR "LIGHT_MODEL must be empty or a nonnegative integer")
endif()
if(REQUIRE_ROUTED_LEGACY_MATCH AND
    (NOT INCLUDE_LEGACY OR NOT INCLUDE_ROUTED))
  message(FATAL_ERROR
    "REQUIRE_ROUTED_LEGACY_MATCH requires INCLUDE_LEGACY=ON and INCLUDE_ROUTED=ON"
  )
endif()

file(REAL_PATH "${BRANCH_BUILD}" BRANCH_BUILD EXPAND_TILDE)
file(REAL_PATH "${BASELINE_BUILD}" BASELINE_BUILD EXPAND_TILDE)
file(REAL_PATH "${SOURCE_DIR}" SOURCE_DIR EXPAND_TILDE)
cmake_path(ABSOLUTE_PATH OUTPUT_DIR NORMALIZE OUTPUT_VARIABLE OUTPUT_DIR)

function(find_benchmark_program result build_dir program_name)
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

find_benchmark_program(branch_rt "${BRANCH_BUILD}" rt)
find_benchmark_program(baseline_rt "${BASELINE_BUILD}" rt)
find_benchmark_program(branch_cobb "${BRANCH_BUILD}" brep_cobb)
file(SHA256 "${branch_rt}" branch_rt_sha256)
file(SHA256 "${baseline_rt}" baseline_rt_sha256)

function(hash_benchmark_librt result build_dir)
  set(candidates
    "${build_dir}/bin/librt.dll"
    "${build_dir}/bin/Release/librt.dll"
    "${build_dir}/lib/librt.so"
    "${build_dir}/lib/liblibrt.so"
    "${build_dir}/lib/liblibrt.dylib"
  )
  foreach(candidate IN LISTS candidates)
    if(EXISTS "${candidate}")
      file(SHA256 "${candidate}" library_sha256)
      set(${result} "${library_sha256}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${result} unavailable PARENT_SCOPE)
endfunction()

hash_benchmark_librt(branch_librt_sha256 "${BRANCH_BUILD}")
hash_benchmark_librt(baseline_librt_sha256 "${BASELINE_BUILD}")

set(multiface_database "${SOURCE_DIR}/src/libbrep/tests/nurbs_csg.g")
if(NOT EXISTS "${multiface_database}")
  message(FATAL_ERROR "Cannot find the multi-face fixture: ${multiface_database}")
endif()
if(NOT DEFINED IMPORTED_DATABASE OR "${IMPORTED_DATABASE}" STREQUAL "")
  set(IMPORTED_DATABASE "${BRANCH_BUILD}/share/db/nist/NIST_MBE_PMI_7-10.g")
endif()
if(NOT DEFINED IMPORTED_OBJECT OR "${IMPORTED_OBJECT}" STREQUAL "")
  set(IMPORTED_OBJECT "Solid1_3.s")
endif()
file(REAL_PATH "${IMPORTED_DATABASE}" imported_database EXPAND_TILDE)
if("imported" IN_LIST CASES AND NOT EXISTS "${imported_database}")
  message(FATAL_ERROR "Cannot find the imported fixture: ${imported_database}")
endif()

foreach(case_name IN LISTS CASES)
  if(NOT case_name STREQUAL "cobb" AND
      NOT case_name STREQUAL "multiface" AND
      NOT case_name STREQUAL "imported")
    message(FATAL_ERROR
      "Unknown benchmark case '${case_name}'; use cobb, multiface, and/or imported"
    )
  endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(cobb_database "${OUTPUT_DIR}/brep_cobb.g")
file(REMOVE "${cobb_database}")
execute_process(
  COMMAND "${branch_cobb}" "${cobb_database}"
  RESULT_VARIABLE cobb_result
  OUTPUT_VARIABLE cobb_stdout
  ERROR_VARIABLE cobb_stderr
)
if(NOT cobb_result EQUAL 0 OR NOT EXISTS "${cobb_database}")
  message(FATAL_ERROR
    "Unable to generate ${cobb_database} (${cobb_result})\n${cobb_stdout}${cobb_stderr}"
  )
endif()

file(SHA256 "${cobb_database}" cobb_database_sha256)
file(SHA256 "${multiface_database}" multiface_database_sha256)
if(EXISTS "${imported_database}")
  file(SHA256 "${imported_database}" imported_database_sha256)
else()
  set(imported_database_sha256 unavailable)
endif()
set(cobb_object "cobb.r")
set(multiface_object "recursive_child_linkage_complex_subtractions.s")
set(imported_object "${IMPORTED_OBJECT}")
set(cobb_arguments -B -P ${PROCESSORS} -s ${COBB_SIZE} -g ${COBB_GRID}
  -a ${AZIMUTH} -e ${ELEVATION})
set(multiface_arguments -B -P ${PROCESSORS} -s ${MULTIFACE_SIZE}
  -a ${AZIMUTH} -e ${ELEVATION})
set(imported_arguments -B -P ${PROCESSORS} -s ${IMPORTED_SIZE}
  -a ${AZIMUTH} -e ${ELEVATION})
if(NOT "${LIGHT_MODEL}" STREQUAL "")
  list(APPEND cobb_arguments -l ${LIGHT_MODEL})
  list(APPEND multiface_arguments -l ${LIGHT_MODEL})
  list(APPEND imported_arguments -l ${LIGHT_MODEL})
endif()

function(cache_value result cache_file key)
  if(NOT EXISTS "${cache_file}")
    set(${result} "unavailable" PARENT_SCOPE)
    return()
  endif()
  file(STRINGS "${cache_file}" matches REGEX "^${key}(:[^=]*)?=")
  if(matches)
    list(GET matches 0 value)
    string(REGEX REPLACE "^[^=]*=" "" value "${value}")
    set(${result} "${value}" PARENT_SCOPE)
  else()
    set(${result} "unavailable" PARENT_SCOPE)
  endif()
endfunction()

function(build_metadata prefix build_dir)
  set(cache_file "${build_dir}/CMakeCache.txt")
  cache_value(source_dir "${cache_file}" CMAKE_HOME_DIRECTORY)
  cache_value(build_type "${cache_file}" CMAKE_BUILD_TYPE)
  cache_value(generator "${cache_file}" CMAKE_GENERATOR)
  cache_value(compiler "${cache_file}" CMAKE_CXX_COMPILER)

  set(commit unavailable)
  set(dirty unavailable)
  set(diff_sha256 unavailable)
  if(EXISTS "${source_dir}/.git")
    execute_process(
      COMMAND git -C "${source_dir}" rev-parse HEAD
      RESULT_VARIABLE git_result
      OUTPUT_VARIABLE commit
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(NOT git_result EQUAL 0)
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
        string(SHA256 diff_sha256 "${state_material}")
      endif()
    endif()
  endif()

  set(${prefix}_source_dir "${source_dir}" PARENT_SCOPE)
  set(${prefix}_commit "${commit}" PARENT_SCOPE)
  set(${prefix}_dirty "${dirty}" PARENT_SCOPE)
  set(${prefix}_state_sha256 "${diff_sha256}" PARENT_SCOPE)
  set(${prefix}_build_type "${build_type}" PARENT_SCOPE)
  set(${prefix}_generator "${generator}" PARENT_SCOPE)
  set(${prefix}_compiler "${compiler}" PARENT_SCOPE)
endfunction()

build_metadata(branch "${BRANCH_BUILD}")
build_metadata(baseline "${BASELINE_BUILD}")

cmake_host_system_information(RESULT host_name QUERY HOSTNAME)
cmake_host_system_information(RESULT processor_name QUERY PROCESSOR_NAME)
cmake_host_system_information(RESULT processor_description QUERY PROCESSOR_DESCRIPTION)
cmake_host_system_information(RESULT logical_cores QUERY NUMBER_OF_LOGICAL_CORES)
cmake_host_system_information(RESULT physical_memory QUERY TOTAL_PHYSICAL_MEMORY)
string(TIMESTAMP run_timestamp "%Y-%m-%dT%H:%M:%SZ" UTC)

set(manifest_file "${OUTPUT_DIR}/manifest.txt")
file(WRITE "${manifest_file}"
  "BREP raytrace benchmark manifest\n"
  "timestamp_utc=${run_timestamp}\n"
  "host=${host_name}\n"
  "processor=${processor_name}\n"
  "processor_description=${processor_description}\n"
  "logical_cores=${logical_cores}\n"
  "physical_memory_mib=${physical_memory}\n"
  "cmake=${CMAKE_VERSION}\n"
  "trials=${TRIALS}\n"
  "warmups=${WARMUPS}\n"
  "cases=${CASES}\n"
  "include_legacy=${INCLUDE_LEGACY}\n"
  "include_routed=${INCLUDE_ROUTED}\n"
  "branch_direct_routing=${BRANCH_DIRECT_ROUTING}\n"
  "routed_publication=${ROUTED_PUBLICATION}\n"
  "routed_missing_root_probe=${ROUTED_MISSING_ROOT_PROBE}\n"
  "routed_missing_root_certificate=${ROUTED_MISSING_ROOT_CERTIFICATE}\n"
  "keep_images=${KEEP_IMAGES}\n"
  "require_routed_legacy_match=${REQUIRE_ROUTED_LEGACY_MATCH}\n"
  "cache=disabled (LIBRT_CACHE=off)\n"
  "surface_tree_depth=${SURFACE_TREE_DEPTH}\n"
  "directional_cache_depth=${DIRECTIONAL_CACHE_DEPTH}\n"
  "directional_grazing_box_budget=${DIRECTIONAL_GRAZING_BOX_BUDGET}\n"
  "routing_pack_cache_bytes=${ROUTING_PACK_CACHE_BYTES}\n"
  "processors=${PROCESSORS}\n"
  "azimuth=${AZIMUTH}\n"
  "elevation=${ELEVATION}\n"
  "light_model=${LIGHT_MODEL}\n"
  "branch_build=${BRANCH_BUILD}\n"
  "branch_rt=${branch_rt}\n"
  "branch_rt_sha256=${branch_rt_sha256}\n"
  "branch_librt_sha256=${branch_librt_sha256}\n"
  "branch_source=${branch_source_dir}\n"
  "branch_commit=${branch_commit}\n"
  "branch_state=${branch_dirty}\n"
  "branch_state_sha256=${branch_state_sha256}\n"
  "branch_build_type=${branch_build_type}\n"
  "branch_generator=${branch_generator}\n"
  "branch_compiler=${branch_compiler}\n"
  "baseline_build=${BASELINE_BUILD}\n"
  "baseline_rt=${baseline_rt}\n"
  "baseline_rt_sha256=${baseline_rt_sha256}\n"
  "baseline_librt_sha256=${baseline_librt_sha256}\n"
  "baseline_source=${baseline_source_dir}\n"
  "baseline_commit=${baseline_commit}\n"
  "baseline_state=${baseline_dirty}\n"
  "baseline_state_sha256=${baseline_state_sha256}\n"
  "baseline_build_type=${baseline_build_type}\n"
  "baseline_generator=${baseline_generator}\n"
  "baseline_compiler=${baseline_compiler}\n"
  "cobb_database=${cobb_database}\n"
  "cobb_database_sha256=${cobb_database_sha256}\n"
  "cobb_object=${cobb_object}\n"
  "cobb_arguments=${cobb_arguments}\n"
  "multiface_database=${multiface_database}\n"
  "multiface_database_sha256=${multiface_database_sha256}\n"
  "multiface_object=${multiface_object}\n"
  "multiface_arguments=${multiface_arguments}\n"
  "imported_database=${imported_database}\n"
  "imported_database_sha256=${imported_database_sha256}\n"
  "imported_object=${imported_object}\n"
  "imported_arguments=${imported_arguments}\n"
)

set(results_file "${OUTPUT_DIR}/results.csv")
file(WRITE "${results_file}"
  "case,implementation,trial,order,rays,intersections,hits,misses,shot_cpu_seconds,shot_elapsed_seconds,rays_per_cpu_second,rays_per_wall_second,result_sha256,log\n"
)

function(run_benchmark_case case_name implementation trial order measured)
  set(route_direct "")
  set(route_publish "")
  set(route_missing_root_probe "")
  set(route_missing_root_certificate "")
  if(implementation STREQUAL "branch")
    set(rt_program "${branch_rt}")
    set(solver "")
    if(BRANCH_DIRECT_ROUTING)
      set(route_direct 1)
    endif()
  elseif(implementation STREQUAL "baseline")
    set(rt_program "${baseline_rt}")
    set(solver "")
  elseif(implementation STREQUAL "legacy")
    set(rt_program "${branch_rt}")
    set(solver legacy)
  elseif(implementation STREQUAL "routed")
    set(rt_program "${branch_rt}")
    set(solver "")
    set(route_direct 1)
    if(ROUTED_PUBLICATION)
      set(route_publish 1)
    endif()
    if(ROUTED_MISSING_ROOT_PROBE)
      set(route_missing_root_probe 1)
    endif()
    if(ROUTED_MISSING_ROOT_CERTIFICATE)
      set(route_missing_root_certificate 1)
    endif()
  else()
    message(FATAL_ERROR "Internal error: unknown implementation ${implementation}")
  endif()

  set(database "${${case_name}_database}")
  set(object_name "${${case_name}_object}")
  set(render_arguments ${${case_name}_arguments})
  if(measured)
    set(run_kind trial)
  else()
    set(run_kind warmup)
  endif()
  set(stem "${case_name}-${implementation}-${run_kind}-${trial}")
  set(log_file "${OUTPUT_DIR}/${stem}.log")
  set(result_file "${OUTPUT_DIR}/${stem}.pix")
  file(REMOVE "${log_file}" "${result_file}")

  message(STATUS
    "${case_name} ${implementation} ${run_kind} ${trial}: order ${order}"
  )
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
      "LIBRT_CACHE=off"
      "LIBRT_BREP_PROFILE="
      "LIBRT_BREP_ROUTE_PROFILE="
      "LIBRT_BREP_ROUTE_DIRECT=${route_direct}"
      "LIBRT_BREP_ROUTE_PUBLISH=${route_publish}"
      "LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE=${route_missing_root_probe}"
      "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=${route_missing_root_certificate}"
      "LIBRT_BREP_SURFACE_TREE_DEPTH=${SURFACE_TREE_DEPTH}"
      "LIBRT_BREP_DIRECTIONAL_CACHE_DEPTH=${DIRECTIONAL_CACHE_DEPTH}"
      "LIBRT_BREP_DIRECTIONAL_GRAZING_BOX_BUDGET=${DIRECTIONAL_GRAZING_BOX_BUDGET}"
      "LIBRT_BREP_ROUTING_PACK_CACHE_BYTES=${ROUTING_PACK_CACHE_BYTES}"
      "LIBRT_BREP_DEBUG="
      "LIBRT_BREP_SOLVER=${solver}"
      "${rt_program}" ${render_arguments} -o "${result_file}"
      "${database}" "${object_name}"
    RESULT_VARIABLE render_result
    OUTPUT_FILE "${log_file}"
    ERROR_FILE "${log_file}"
  )
  if(NOT render_result EQUAL 0)
    message(FATAL_ERROR
      "${case_name} ${implementation} failed (${render_result}); see ${log_file}"
    )
  endif()
  if(NOT EXISTS "${result_file}")
    message(FATAL_ERROR "${case_name} ${implementation} produced no result image")
  endif()

  file(READ "${log_file}" render_log)
  string(REGEX MATCH
    "SHOT: cpu = *([0-9.]+) sec, elapsed = *([0-9.]+) sec"
    shot_match "${render_log}"
  )
  set(shot_cpu "${CMAKE_MATCH_1}")
  set(shot_elapsed "${CMAKE_MATCH_2}")
  string(REGEX MATCH
    "([0-9]+) solid/ray intersections: ([0-9]+) hits \\+ ([0-9]+) miss"
    hit_match "${render_log}"
  )
  set(intersections "${CMAKE_MATCH_1}")
  set(hits "${CMAKE_MATCH_2}")
  set(misses "${CMAKE_MATCH_3}")
  string(REGEX MATCH
    "Frame +0: +([0-9]+) rays +in +[0-9.]+ sec = +([0-9.]+) rays/CPU_sec"
    rate_match "${render_log}"
  )
  set(rays "${CMAKE_MATCH_1}")
  set(rate "${CMAKE_MATCH_2}")
  string(REGEX MATCH
    "Frame +0: +[0-9]+ rays +in +[0-9.]+ sec = +([0-9.]+) rays/sec [(]wallclock[)]"
    wall_rate_match "${render_log}"
  )
  set(wall_rate "${CMAKE_MATCH_1}")
  if(shot_cpu STREQUAL "" OR shot_elapsed STREQUAL "" OR
      intersections STREQUAL "" OR rays STREQUAL "" OR rate STREQUAL "" OR
      wall_rate STREQUAL "")
    message(FATAL_ERROR
      "Cannot parse ${case_name} ${implementation} metrics "
      "(cpu='${shot_cpu}', elapsed='${shot_elapsed}', "
      "intersections='${intersections}', rays='${rays}', "
      "cpu_rate='${rate}', wall_rate='${wall_rate}'); see ${log_file}"
    )
  endif()

  file(SHA256 "${result_file}" result_sha256)
  if(NOT KEEP_IMAGES)
    file(REMOVE "${result_file}")
  endif()
  if(NOT measured)
    return()
  endif()

  file(TO_CMAKE_PATH "${log_file}" csv_log_file)
  file(APPEND "${results_file}"
    "${case_name},${implementation},${trial},${order},${rays},${intersections},${hits},${misses},${shot_cpu},${shot_elapsed},${rate},${wall_rate},${result_sha256},${csv_log_file}\n"
  )

  set(rate_variable "${case_name}_${implementation}_rates")
  set(wall_rate_variable "${case_name}_${implementation}_wall_rates")
  set(cpu_variable "${case_name}_${implementation}_cpu")
  set(elapsed_variable "${case_name}_${implementation}_elapsed")
  set(signature_variable "${case_name}_${implementation}_signatures")
  set(aggregate_variable "${case_name}_${implementation}_aggregates")
  set(rate_values "${${rate_variable}}")
  set(wall_rate_values "${${wall_rate_variable}}")
  set(cpu_values "${${cpu_variable}}")
  set(elapsed_values "${${elapsed_variable}}")
  set(signature_values "${${signature_variable}}")
  set(aggregate_values "${${aggregate_variable}}")
  list(APPEND rate_values "${rate}")
  list(APPEND wall_rate_values "${wall_rate}")
  list(APPEND cpu_values "${shot_cpu}")
  list(APPEND elapsed_values "${shot_elapsed}")
  list(APPEND signature_values
    "${rays}:${intersections}:${hits}:${misses}:${result_sha256}"
  )
  list(APPEND aggregate_values "${rays}:${intersections}:${hits}:${misses}")
  set(${rate_variable} "${rate_values}" PARENT_SCOPE)
  set(${wall_rate_variable} "${wall_rate_values}" PARENT_SCOPE)
  set(${cpu_variable} "${cpu_values}" PARENT_SCOPE)
  set(${elapsed_variable} "${elapsed_values}" PARENT_SCOPE)
  set(${signature_variable} "${signature_values}" PARENT_SCOPE)
  set(${aggregate_variable} "${aggregate_values}" PARENT_SCOPE)
endfunction()

set(order 0)
if(WARMUPS GREATER 0)
  foreach(case_name IN LISTS CASES)
    foreach(warmup RANGE 1 ${WARMUPS})
      foreach(implementation branch baseline)
        math(EXPR order "${order} + 1")
        run_benchmark_case("${case_name}" "${implementation}" "${warmup}" "${order}" FALSE)
      endforeach()
      if(INCLUDE_LEGACY)
        math(EXPR order "${order} + 1")
        run_benchmark_case("${case_name}" legacy "${warmup}" "${order}" FALSE)
      endif()
      if(INCLUDE_ROUTED)
        math(EXPR order "${order} + 1")
        run_benchmark_case("${case_name}" routed "${warmup}" "${order}" FALSE)
      endif()
    endforeach()
  endforeach()
endif()

foreach(case_name IN LISTS CASES)
  foreach(trial RANGE 1 ${TRIALS})
    math(EXPR trial_parity "${trial} % 2")
    if(trial_parity EQUAL 1)
      set(pair_order branch baseline)
    else()
      set(pair_order baseline branch)
    endif()
    foreach(implementation IN LISTS pair_order)
      math(EXPR order "${order} + 1")
      run_benchmark_case("${case_name}" "${implementation}" "${trial}" "${order}" TRUE)
    endforeach()
    # Balance the in-branch control pair just as we balance branch/baseline.
    # Running routed after legacy on every trial can otherwise turn CPU
    # frequency or thermal drift into apparent routing overhead.
    set(route_pair_order)
    if(INCLUDE_LEGACY AND INCLUDE_ROUTED)
      if(trial_parity EQUAL 1)
        set(route_pair_order legacy routed)
      else()
        set(route_pair_order routed legacy)
      endif()
    elseif(INCLUDE_LEGACY)
      set(route_pair_order legacy)
    elseif(INCLUDE_ROUTED)
      set(route_pair_order routed)
    endif()
    foreach(implementation IN LISTS route_pair_order)
      math(EXPR order "${order} + 1")
      run_benchmark_case("${case_name}" "${implementation}" "${trial}" "${order}" TRUE)
    endforeach()
  endforeach()
endforeach()

function(observed_median result)
  set(values ${ARGN})
  list(SORT values COMPARE NATURAL ORDER ASCENDING)
  list(LENGTH values value_count)
  math(EXPR middle "${value_count} / 2")
  list(GET values ${middle} median)
  set(${result} "${median}" PARENT_SCOPE)
endfunction()

function(observed_range minimum_result maximum_result)
  set(values ${ARGN})
  list(SORT values COMPARE NATURAL ORDER ASCENDING)
  list(GET values 0 minimum)
  list(LENGTH values value_count)
  math(EXPR last "${value_count} - 1")
  list(GET values ${last} maximum)
  set(${minimum_result} "${minimum}" PARENT_SCOPE)
  set(${maximum_result} "${maximum}" PARENT_SCOPE)
endfunction()

function(observed_p95 result)
  set(values ${ARGN})
  list(SORT values COMPARE NATURAL ORDER ASCENDING)
  list(LENGTH values value_count)
  # Nearest-rank p95: ceil(0.95 * N), converted to a zero-based index.
  math(EXPR index "(${value_count} * 95 + 99) / 100 - 1")
  list(GET values ${index} percentile)
  set(${result} "${percentile}" PARENT_SCOPE)
endfunction()

function(rate_hundredths result value)
  string(REGEX REPLACE "[^0-9]" "" scaled "${value}")
  set(${result} "${scaled}" PARENT_SCOPE)
endfunction()

# Convert the decimal second values emitted by rt to integer microseconds so
# paired ratios do not depend on external calculators or floating-point
# behavior that varies among CMake versions.
function(seconds_to_microseconds result value)
  if(NOT "${value}" MATCHES "^([0-9]+)([.]([0-9]+))?$")
    message(FATAL_ERROR "Cannot convert '${value}' to microseconds")
  endif()
  set(whole "${CMAKE_MATCH_1}")
  set(fraction "${CMAKE_MATCH_3}")
  string(LENGTH "${fraction}" fraction_length)
  if(fraction_length GREATER 6)
    string(SUBSTRING "${fraction}" 0 6 fraction)
  elseif(fraction_length LESS 6)
    math(EXPR padding_length "6 - ${fraction_length}")
    string(REPEAT "0" ${padding_length} padding)
    string(APPEND fraction "${padding}")
  endif()
  string(REGEX REPLACE "^0+" "" whole "${whole}")
  string(REGEX REPLACE "^0+" "" fraction "${fraction}")
  if(whole STREQUAL "")
    set(whole 0)
  endif()
  if(fraction STREQUAL "")
    set(fraction 0)
  endif()
  math(EXPR microseconds "${whole} * 1000000 + ${fraction}")
  set(${result} "${microseconds}" PARENT_SCOPE)
endfunction()

function(format_ratio result ratio_ten_thousandths)
  math(EXPR ratio_whole "${ratio_ten_thousandths} / 10000")
  math(EXPR ratio_fraction "${ratio_ten_thousandths} % 10000")
  set(ratio_fraction "${ratio_fraction}")
  string(LENGTH "${ratio_fraction}" fraction_length)
  if(fraction_length LESS 4)
    math(EXPR padding_length "4 - ${fraction_length}")
    string(REPEAT "0" ${padding_length} padding)
    string(PREPEND ratio_fraction "${padding}")
  endif()
  set(${result} "${ratio_whole}.${ratio_fraction}x" PARENT_SCOPE)
endfunction()

# Pair values by trial before taking the median.  This makes transient host
# load affect one ratio instead of moving the independent implementation
# medians to unrelated trials.  NUMERATOR/DENOMINATOR are variable names.
function(paired_time_ratios median_result details_result numerator denominator)
  set(numerator_values ${${numerator}})
  set(denominator_values ${${denominator}})
  list(LENGTH numerator_values numerator_count)
  list(LENGTH denominator_values denominator_count)
  if(NOT numerator_count EQUAL denominator_count OR numerator_count LESS 1)
    message(FATAL_ERROR
      "Cannot pair ${numerator} (${numerator_count} values) with "
      "${denominator} (${denominator_count} values)"
    )
  endif()

  set(scaled_ratios)
  set(details)
  math(EXPR last_index "${numerator_count} - 1")
  foreach(index RANGE 0 ${last_index})
    list(GET numerator_values ${index} numerator_seconds)
    list(GET denominator_values ${index} denominator_seconds)
    seconds_to_microseconds(numerator_us "${numerator_seconds}")
    seconds_to_microseconds(denominator_us "${denominator_seconds}")
    if(denominator_us EQUAL 0)
      message(FATAL_ERROR
        "Cannot compute paired ratio ${index}: denominator is zero"
      )
    endif()
    math(EXPR ratio
      "(${numerator_us} * 10000 + ${denominator_us} / 2) / ${denominator_us}"
    )
    list(APPEND scaled_ratios "${ratio}")
    format_ratio(ratio_text "${ratio}")
    math(EXPR trial_number "${index} + 1")
    list(APPEND details "trial ${trial_number}=${ratio_text}")
  endforeach()

  observed_median(median_scaled ${scaled_ratios})
  format_ratio(median_text "${median_scaled}")
  string(JOIN ", " details_text ${details})
  set(${median_result} "${median_text}" PARENT_SCOPE)
  set(${details_result} "${details_text}" PARENT_SCOPE)
  set(${median_result}_SCALED "${median_scaled}" PARENT_SCOPE)
endfunction()

set(summary_file "${OUTPUT_DIR}/summary.txt")
file(WRITE "${summary_file}"
  "BREP raytrace benchmark summary\n"
  "Generated ${run_timestamp}; full metadata is in manifest.txt.\n"
  "Elapsed p95 uses the nearest-rank definition over measured trials.\n\n"
)

set(routed_legacy_contract_failure FALSE)

foreach(case_name IN LISTS CASES)
  observed_median(branch_median ${${case_name}_branch_rates})
  observed_median(baseline_median ${${case_name}_baseline_rates})
  observed_range(branch_minimum branch_maximum
    ${${case_name}_branch_rates})
  observed_range(baseline_minimum baseline_maximum
    ${${case_name}_baseline_rates})
  observed_median(branch_wall_median ${${case_name}_branch_wall_rates})
  observed_median(baseline_wall_median ${${case_name}_baseline_wall_rates})
  observed_range(branch_wall_minimum branch_wall_maximum
    ${${case_name}_branch_wall_rates})
  observed_range(baseline_wall_minimum baseline_wall_maximum
    ${${case_name}_baseline_wall_rates})
  observed_p95(branch_elapsed_p95 ${${case_name}_branch_elapsed})
  observed_p95(baseline_elapsed_p95 ${${case_name}_baseline_elapsed})
  rate_hundredths(branch_scaled "${branch_median}")
  rate_hundredths(baseline_scaled "${baseline_median}")
  rate_hundredths(branch_wall_scaled "${branch_wall_median}")
  rate_hundredths(baseline_wall_scaled "${baseline_wall_median}")
  math(EXPR relative_tenths
    "(${branch_scaled} * 1000 + ${baseline_scaled} / 2) / ${baseline_scaled}"
  )
  math(EXPR relative_whole "${relative_tenths} / 10")
  math(EXPR relative_fraction "${relative_tenths} % 10")
  math(EXPR wall_relative_tenths
    "(${branch_wall_scaled} * 1000 + ${baseline_wall_scaled} / 2) / ${baseline_wall_scaled}"
  )
  math(EXPR wall_relative_whole "${wall_relative_tenths} / 10")
  math(EXPR wall_relative_fraction "${wall_relative_tenths} % 10")

  set(branch_signatures ${${case_name}_branch_signatures})
  set(baseline_signatures ${${case_name}_baseline_signatures})
  list(REMOVE_DUPLICATES branch_signatures)
  list(REMOVE_DUPLICATES baseline_signatures)
  list(LENGTH branch_signatures branch_signature_count)
  list(LENGTH baseline_signatures baseline_signature_count)
  if(NOT branch_signature_count EQUAL 1 OR NOT baseline_signature_count EQUAL 1)
    set(repeatability "FAIL (a repeated implementation produced differing results)")
  else()
    set(repeatability PASS)
  endif()
  list(GET branch_signatures 0 branch_signature)
  list(GET baseline_signatures 0 baseline_signature)
  if(branch_signature STREQUAL baseline_signature)
    set(cross_result "match")
  else()
    set(cross_result "DIFFER")
  endif()
  set(branch_aggregates ${${case_name}_branch_aggregates})
  set(baseline_aggregates ${${case_name}_baseline_aggregates})
  list(REMOVE_DUPLICATES branch_aggregates)
  list(REMOVE_DUPLICATES baseline_aggregates)
  list(GET branch_aggregates 0 branch_aggregate)
  list(GET baseline_aggregates 0 baseline_aggregate)
  if(branch_aggregate STREQUAL baseline_aggregate)
    set(cross_aggregate match)
  else()
    set(cross_aggregate DIFFER)
  endif()

  if(relative_tenths GREATER_EQUAL 950)
    set(speed_gate PASS)
  else()
    set(speed_gate FAIL)
  endif()
  if(cross_aggregate STREQUAL "match")
    set(aggregate_gate PASS)
  else()
    set(aggregate_gate FAIL)
  endif()

  file(APPEND "${summary_file}"
    "${case_name}\n"
    "  branch median:   ${branch_median} rays/CPU-second\n"
    "  branch range:    ${branch_minimum}--${branch_maximum}\n"
    "  branch wall median:   ${branch_wall_median} rays/wall-second\n"
    "  branch wall range:    ${branch_wall_minimum}--${branch_wall_maximum}\n"
    "  branch elapsed p95:   ${branch_elapsed_p95} seconds\n"
    "  baseline median: ${baseline_median} rays/CPU-second\n"
    "  baseline range:  ${baseline_minimum}--${baseline_maximum}\n"
    "  baseline wall median: ${baseline_wall_median} rays/wall-second\n"
    "  baseline wall range:  ${baseline_wall_minimum}--${baseline_wall_maximum}\n"
    "  baseline elapsed p95: ${baseline_elapsed_p95} seconds\n"
    "  CPU relative:    ${relative_whole}.${relative_fraction}%\n"
    "  wall relative:   ${wall_relative_whole}.${wall_relative_fraction}%\n"
    "  repeated result: ${repeatability}\n"
    "  branch/baseline ray and hit aggregates: ${cross_aggregate}\n"
    "  branch/baseline result digests: ${cross_result}\n"
    "  speed-only gate (>=95%): ${speed_gate}\n"
    "  baseline aggregate gate: ${aggregate_gate}\n"
  )
  if(INCLUDE_LEGACY)
    observed_median(legacy_median ${${case_name}_legacy_rates})
    observed_range(legacy_minimum legacy_maximum
      ${${case_name}_legacy_rates})
    observed_median(legacy_wall_median
      ${${case_name}_legacy_wall_rates})
    observed_range(legacy_wall_minimum legacy_wall_maximum
      ${${case_name}_legacy_wall_rates})
    observed_p95(legacy_elapsed_p95 ${${case_name}_legacy_elapsed})
    set(legacy_signatures ${${case_name}_legacy_signatures})
    set(legacy_aggregates ${${case_name}_legacy_aggregates})
    list(REMOVE_DUPLICATES legacy_signatures)
    list(REMOVE_DUPLICATES legacy_aggregates)
    list(LENGTH legacy_signatures legacy_signature_count)
    if(legacy_signature_count EQUAL 1)
      set(legacy_repeatability PASS)
    else()
      set(legacy_repeatability
        "FAIL (legacy-only runs produced differing results)")
    endif()
    list(GET legacy_signatures 0 legacy_signature)
    list(GET legacy_aggregates 0 legacy_aggregate)
    if(branch_signature STREQUAL legacy_signature)
      set(branch_legacy_result match)
    else()
      set(branch_legacy_result DIFFER)
    endif()
    if(branch_aggregate STREQUAL legacy_aggregate)
      set(branch_legacy_aggregate match)
    else()
      set(branch_legacy_aggregate DIFFER)
    endif()
    file(APPEND "${summary_file}"
      "  branch legacy-only median: ${legacy_median} rays/CPU-second\n"
      "  branch legacy-only range:  ${legacy_minimum}--${legacy_maximum}\n"
      "  branch legacy-only wall median: ${legacy_wall_median} rays/wall-second\n"
      "  branch legacy-only wall range:  ${legacy_wall_minimum}--${legacy_wall_maximum}\n"
      "  branch legacy-only elapsed p95: ${legacy_elapsed_p95} seconds\n"
      "  legacy-only repeated result: ${legacy_repeatability}\n"
      "  branch/legacy ray and hit aggregates: ${branch_legacy_aggregate}\n"
      "  branch/legacy result digests: ${branch_legacy_result}\n"
    )
  endif()
  if(INCLUDE_ROUTED)
    if(ROUTED_PUBLICATION)
      set(routed_authority_description "selective-authority")
    else()
      set(routed_authority_description "legacy-authority")
    endif()
    observed_median(routed_median ${${case_name}_routed_rates})
    observed_range(routed_minimum routed_maximum
      ${${case_name}_routed_rates})
    observed_median(routed_wall_median
      ${${case_name}_routed_wall_rates})
    observed_range(routed_wall_minimum routed_wall_maximum
      ${${case_name}_routed_wall_rates})
    observed_p95(routed_elapsed_p95 ${${case_name}_routed_elapsed})
    rate_hundredths(routed_scaled "${routed_median}")
    rate_hundredths(routed_wall_scaled "${routed_wall_median}")
    math(EXPR routed_relative_tenths
      "(${routed_scaled} * 1000 + ${baseline_scaled} / 2) / ${baseline_scaled}"
    )
    math(EXPR routed_relative_whole "${routed_relative_tenths} / 10")
    math(EXPR routed_relative_fraction "${routed_relative_tenths} % 10")
    math(EXPR routed_wall_relative_tenths
      "(${routed_wall_scaled} * 1000 + ${baseline_wall_scaled} / 2) / ${baseline_wall_scaled}"
    )
    math(EXPR routed_wall_relative_whole
      "${routed_wall_relative_tenths} / 10")
    math(EXPR routed_wall_relative_fraction
      "${routed_wall_relative_tenths} % 10")
    if(routed_relative_tenths GREATER_EQUAL 1000)
      set(routed_speed_gate PASS)
    else()
      set(routed_speed_gate FAIL)
    endif()
    if(routed_wall_relative_tenths GREATER_EQUAL 1000)
      set(routed_wall_speed_gate PASS)
    else()
      set(routed_wall_speed_gate FAIL)
    endif()
    set(routed_signatures ${${case_name}_routed_signatures})
    set(routed_aggregates ${${case_name}_routed_aggregates})
    list(REMOVE_DUPLICATES routed_signatures)
    list(REMOVE_DUPLICATES routed_aggregates)
    list(LENGTH routed_signatures routed_signature_count)
    if(routed_signature_count EQUAL 1)
      set(routed_repeatability PASS)
    else()
      set(routed_repeatability
        "FAIL (routed runs produced differing results)")
    endif()
    list(GET routed_signatures 0 routed_signature)
    list(GET routed_aggregates 0 routed_aggregate)
    if(routed_signature STREQUAL baseline_signature)
      set(routed_baseline_result match)
    else()
      set(routed_baseline_result DIFFER)
    endif()
    if(routed_aggregate STREQUAL baseline_aggregate)
      set(routed_baseline_aggregate match)
    else()
      set(routed_baseline_aggregate DIFFER)
    endif()
    file(APPEND "${summary_file}"
      "  routed ${routed_authority_description} median: ${routed_median} rays/CPU-second\n"
      "  routed ${routed_authority_description} range:  ${routed_minimum}--${routed_maximum}\n"
      "  routed ${routed_authority_description} wall median: ${routed_wall_median} rays/wall-second\n"
      "  routed ${routed_authority_description} wall range:  ${routed_wall_minimum}--${routed_wall_maximum}\n"
      "  routed ${routed_authority_description} elapsed p95: ${routed_elapsed_p95} seconds\n"
      "  routed CPU relative to baseline: ${routed_relative_whole}.${routed_relative_fraction}%\n"
      "  routed wall relative to baseline: ${routed_wall_relative_whole}.${routed_wall_relative_fraction}%\n"
      "  routed repeated result: ${routed_repeatability}\n"
      "  routed/baseline ray and hit aggregates: ${routed_baseline_aggregate}\n"
      "  routed/baseline result digests: ${routed_baseline_result}\n"
      "  routed CPU gate (>=100% baseline): ${routed_speed_gate}\n"
      "  routed wall gate (>=100% baseline): ${routed_wall_speed_gate}\n"
    )
    if(INCLUDE_LEGACY)
      paired_time_ratios(routed_legacy_cpu_paired_median
        routed_legacy_cpu_paired_details
        "${case_name}_routed_cpu" "${case_name}_legacy_cpu")
      paired_time_ratios(routed_legacy_wall_paired_median
        routed_legacy_wall_paired_details
        "${case_name}_routed_elapsed" "${case_name}_legacy_elapsed")
      rate_hundredths(legacy_scaled "${legacy_median}")
      rate_hundredths(legacy_wall_scaled "${legacy_wall_median}")
      math(EXPR routed_legacy_tenths
        "(${routed_scaled} * 1000 + ${legacy_scaled} / 2) / ${legacy_scaled}"
      )
      math(EXPR routed_legacy_whole "${routed_legacy_tenths} / 10")
      math(EXPR routed_legacy_fraction "${routed_legacy_tenths} % 10")
      math(EXPR routed_legacy_wall_tenths
        "(${routed_wall_scaled} * 1000 + ${legacy_wall_scaled} / 2) / ${legacy_wall_scaled}"
      )
      math(EXPR routed_legacy_wall_whole
        "${routed_legacy_wall_tenths} / 10")
      math(EXPR routed_legacy_wall_fraction
        "${routed_legacy_wall_tenths} % 10")
      if(routed_signature STREQUAL legacy_signature)
        set(routed_legacy_result match)
      else()
        set(routed_legacy_result DIFFER)
      endif()
      if(routed_aggregate STREQUAL legacy_aggregate)
        set(routed_legacy_aggregate match)
      else()
        set(routed_legacy_aggregate DIFFER)
      endif()
      if(routed_legacy_tenths GREATER_EQUAL 950)
        set(routed_legacy_speed_gate PASS)
      else()
        set(routed_legacy_speed_gate FAIL)
      endif()
      if(routed_legacy_wall_tenths GREATER_EQUAL 950)
        set(routed_legacy_wall_speed_gate PASS)
      else()
        set(routed_legacy_wall_speed_gate FAIL)
      endif()
      if(routed_legacy_cpu_paired_median_SCALED LESS_EQUAL 12000)
        set(routed_legacy_paired_cpu_gate PASS)
      else()
        set(routed_legacy_paired_cpu_gate FAIL)
      endif()
      if(routed_legacy_wall_paired_median_SCALED LESS_EQUAL 12000)
        set(routed_legacy_paired_wall_gate PASS)
      else()
        set(routed_legacy_paired_wall_gate FAIL)
      endif()
      if(NOT routed_legacy_result STREQUAL "match" OR
          NOT routed_repeatability STREQUAL "PASS" OR
          NOT legacy_repeatability STREQUAL "PASS")
        set(routed_legacy_contract_failure TRUE)
      endif()
      file(APPEND "${summary_file}"
        "  routed CPU relative to branch legacy-only: ${routed_legacy_whole}.${routed_legacy_fraction}%\n"
        "  routed wall relative to branch legacy-only: ${routed_legacy_wall_whole}.${routed_legacy_wall_fraction}%\n"
        "  routed/legacy paired CPU-time ratios: ${routed_legacy_cpu_paired_details}\n"
        "  routed/legacy median paired CPU-time ratio: ${routed_legacy_cpu_paired_median}\n"
        "  routed/legacy paired wall-time ratios: ${routed_legacy_wall_paired_details}\n"
        "  routed/legacy median paired wall-time ratio: ${routed_legacy_wall_paired_median}\n"
        "  routed/legacy ray and hit aggregates: ${routed_legacy_aggregate}\n"
        "  routed/legacy result digests: ${routed_legacy_result}\n"
        "  routed CPU near-legacy gate (>=95%): ${routed_legacy_speed_gate}\n"
        "  routed wall near-legacy gate (>=95%): ${routed_legacy_wall_speed_gate}\n"
        "  routed paired CPU-time release gate (<=1.20x): ${routed_legacy_paired_cpu_gate}\n"
        "  routed paired wall-time release gate (<=1.20x): ${routed_legacy_paired_wall_gate}\n"
      )
    endif()
  endif()
  file(APPEND "${summary_file}" "\n")
endforeach()

if(REQUIRE_ROUTED_LEGACY_MATCH AND routed_legacy_contract_failure)
  message(FATAL_ERROR
    "Routed/legacy exact-output or repeatability contract failed; see ${summary_file}"
  )
endif()

message(STATUS "Benchmark complete: ${summary_file}")
