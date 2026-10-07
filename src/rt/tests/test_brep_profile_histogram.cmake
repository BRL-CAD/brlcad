# Verify opt-in BREP profiling reports bounded work and time distributions.

foreach(required_variable RT DB TEST_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

set(profile_image "${TEST_DIR}/brep-profile-histogram.pix")
file(REMOVE "${profile_image}")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_PROFILE=1"
    "LIBRT_BREP_ROUTE_DIRECT="
    "LIBRT_BREP_ROUTE_PUBLISH="
    "LIBRT_BREP_ROUTE_PROFILE="
    "${RT}" -B -P 1 -s 16 -a 35 -e 25 -o "${profile_image}"
    "${DB}" recursive_child_linkage_complex_subtractions.s
  RESULT_VARIABLE profile_result
  OUTPUT_VARIABLE profile_stdout
  ERROR_VARIABLE profile_stderr
)
set(profile_log "${profile_stdout}${profile_stderr}")
if(NOT profile_result EQUAL 0 OR NOT EXISTS "${profile_image}")
  message(FATAL_ERROR
    "BREP histogram profile failed (${profile_result})\n${profile_log}"
  )
endif()

string(REGEX MATCH "BREP prepared result: reason=0 rays=([0-9]+)"
  selected_match "${profile_log}")
set(selected_rays "${CMAKE_MATCH_1}")
string(REGEX MATCH "BREP shot fallback: reason=6 rays=([0-9]+)"
  uncertified_match "${profile_log}")
set(uncertified_rays "${CMAKE_MATCH_1}")
if("${selected_match}" STREQUAL "" OR "${uncertified_match}" STREQUAL "")
  message(FATAL_ERROR
    "Profile did not exercise selected and uncertified populations\n${profile_log}"
  )
endif()

function(require_histogram reason name metric unit finite_max expected_rays)
  string(REGEX MATCH
    "BREP prepared distribution: reason=${reason} name=${name} metric=${metric} unit=${unit} rays=([0-9]+) p50-upper=[0-9]+ p90-upper=[0-9]+ p99-upper=[0-9]+ finite-max=${finite_max} overflow=[0-9]+"
    histogram_match "${profile_log}"
  )
  if("${histogram_match}" STREQUAL "")
    message(FATAL_ERROR
      "Missing ${name}/${metric} BREP histogram\n${profile_log}"
    )
  endif()
  if(NOT "${CMAKE_MATCH_1}" STREQUAL "${expected_rays}")
    message(FATAL_ERROR
      "${name}/${metric} histogram population ${CMAKE_MATCH_1} does not match ${expected_rays}"
    )
  endif()
endfunction()

foreach(metric subdivision isolated)
  require_histogram(0 none ${metric} boxes 1024 "${selected_rays}")
  require_histogram(6 uncertified ${metric} boxes 1024 "${uncertified_rays}")
endforeach()
require_histogram(0 none sunk microseconds 1048576 "${selected_rays}")
require_histogram(6 uncertified sunk microseconds 1048576 "${uncertified_rays}")

foreach(capacity_reason
    "0|leaf-workspace"
    "1|hit-workspace")
  string(REPLACE "|" ";" capacity_fields "${capacity_reason}")
  list(GET capacity_fields 0 reason)
  list(GET capacity_fields 1 name)
  string(REGEX MATCH
    "BREP legacy capacity fallback: reason=${reason} name=${name} rays=[0-9]+"
    capacity_match "${profile_log}"
  )
  if("${capacity_match}" STREQUAL "")
    message(FATAL_ERROR
      "Missing ${name} legacy capacity telemetry\n${profile_log}"
    )
  endif()
endforeach()

set(legacy_cleanup_names
  coincident-grazing
  near-miss-shadow
  near-miss-crack-merge
  near-miss-crack-cancel
  near-miss-nonadjacent-drop
  clean-overlap
  near-hit-shadow
  near-hit-crack-merge
  trimmed-invalid
  parameter-oob
  isolated-grazing
  duplicate-cancel
  duplicate-collapse
  solid-coincident
  solid-unmatched-enter
  solid-unmatched-leave
  solid-interior-winding
  open-same-direction
  open-odd-endpoint
)
set(action 0)
foreach(name IN LISTS legacy_cleanup_names)
  string(REGEX MATCH
    "BREP legacy cleanup action: reason=${action} name=${name} rays=[0-9]+ events=[0-9]+"
    cleanup_match "${profile_log}"
  )
  if("${cleanup_match}" STREQUAL "")
    message(FATAL_ERROR
      "Missing ${name} legacy cleanup telemetry\n${profile_log}"
    )
  endif()
  math(EXPR action "${action} + 1")
endforeach()

file(REMOVE "${profile_image}")
