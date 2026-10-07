# Verify routing packs share one scene-wide byte budget across multiple B-Reps.

foreach(required_variable RT DB TEST_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

set(scene_budget 1048576)
set(scene_image "${TEST_DIR}/brep-scene-pack-cache.pix")
file(REMOVE "${scene_image}")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_PREP_PROFILE=1"
    "LIBRT_BREP_ROUTING_PACK_CACHE_BYTES=${scene_budget}"
    "LIBRT_BREP_SURFACE_TREE_DEPTH=3"
    "${RT}" -B -P 2 -s 1 -a 35 -e 25 -o "${scene_image}"
    "${DB}"
    recursive_child_linkage_complex_subtractions.s
    multi_level_subtractions.s
  RESULT_VARIABLE render_result
  OUTPUT_VARIABLE render_stdout
  ERROR_VARIABLE render_stderr
)
set(render_log "${render_stdout}${render_stderr}")
if(NOT render_result EQUAL 0 OR NOT EXISTS "${scene_image}")
  message(FATAL_ERROR
    "Scene pack-cache render failed (${render_result})\n${render_log}"
  )
endif()

string(REGEX MATCH
  "BREP routing pack scene cache: budget=([0-9]+) used=([0-9]+) objects=([0-9]+) populated=([0-9]+)"
  cache_match "${render_log}"
)
if("${cache_match}" STREQUAL "")
  message(FATAL_ERROR "Missing scene pack-cache telemetry\n${render_log}")
endif()
set(observed_budget "${CMAKE_MATCH_1}")
set(observed_used "${CMAKE_MATCH_2}")
set(observed_objects "${CMAKE_MATCH_3}")
set(observed_populated "${CMAKE_MATCH_4}")
if(NOT observed_budget EQUAL scene_budget)
  message(FATAL_ERROR
    "Scene pack-cache budget ${observed_budget} != ${scene_budget}"
  )
endif()
if(observed_used GREATER scene_budget OR observed_used EQUAL 0)
  message(FATAL_ERROR
    "Scene pack-cache use ${observed_used} is outside (0, ${scene_budget}]"
  )
endif()
if(observed_objects LESS 2 OR observed_populated LESS 2 OR
   observed_populated GREATER observed_objects)
  message(FATAL_ERROR
    "Scene pack-cache population is invalid: objects=${observed_objects}, populated=${observed_populated}"
  )
endif()

set(tiny_image "${TEST_DIR}/brep-scene-pack-cache-tiny.pix")
file(REMOVE "${tiny_image}")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_PREP_PROFILE=1"
    "LIBRT_BREP_ROUTING_PACK_CACHE_BYTES=1"
    "LIBRT_BREP_SURFACE_TREE_DEPTH=3"
    "${RT}" -B -P 2 -s 1 -a 35 -e 25 -o "${tiny_image}"
    "${DB}"
    recursive_child_linkage_complex_subtractions.s
    multi_level_subtractions.s
  RESULT_VARIABLE tiny_result
  OUTPUT_VARIABLE tiny_stdout
  ERROR_VARIABLE tiny_stderr
)
set(tiny_log "${tiny_stdout}${tiny_stderr}")
if(NOT tiny_result EQUAL 0 OR NOT EXISTS "${tiny_image}")
  message(FATAL_ERROR
    "Sub-minimum scene pack-cache render failed (${tiny_result})\n${tiny_log}"
  )
endif()
string(REGEX MATCH
  "BREP routing pack scene cache: budget=1 used=([0-9]+) objects=([0-9]+) populated=([0-9]+)"
  tiny_match "${tiny_log}"
)
if("${tiny_match}" STREQUAL "" OR NOT CMAKE_MATCH_1 EQUAL 0 OR
   NOT CMAKE_MATCH_2 EQUAL observed_objects OR NOT CMAKE_MATCH_3 EQUAL 0)
  message(FATAL_ERROR
    "Sub-minimum scene pack-cache allocation is invalid\n${tiny_log}"
  )
endif()
file(SHA256 "${scene_image}" scene_sha256)
file(SHA256 "${tiny_image}" tiny_sha256)
if(NOT scene_sha256 STREQUAL tiny_sha256)
  message(FATAL_ERROR
    "Scene pack-cache result changed at a sub-minimum budget: ${scene_sha256} != ${tiny_sha256}"
  )
endif()

file(REMOVE "${scene_image}" "${tiny_image}")
