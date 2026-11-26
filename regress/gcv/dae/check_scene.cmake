function(scene_query output_variable)
  execute_process(
    COMMAND "${MGED}" -c "${OUTPUT}" ${ARGN}
    COMMAND_ERROR_IS_FATAL ANY
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(check_bounds path expected)
  scene_query(bounds bb -eq "${path}")
  if(NOT bounds STREQUAL expected)
    message(FATAL_ERROR "${path} bounds are ${bounds}, expected ${expected}")
  endif()
endfunction()

file(REMOVE "${OUTPUT}")
execute_process(
  COMMAND "${GCV}" --input-format=assetimport "${INPUT}" "${OUTPUT}"
  COMMAND_ERROR_IS_FATAL ANY
)

# Both the parent and the child place the same source mesh.
scene_query(meshes ls -s)
string(REGEX MATCHALL "[^ \t\r\n]+" mesh_names "${meshes}")
list(LENGTH mesh_names mesh_count)
if(NOT mesh_count EQUAL 1)
  message(FATAL_ERROR "Expected one shared BoT, found ${mesh_count}: ${mesh_names}")
endif()
scene_query(face_count bot get faces "${mesh_names}")
if(NOT face_count STREQUAL "4")
  message(FATAL_ERROR "Expected a four-face BoT, found ${face_count} faces")
endif()

check_bounds("all.g/parent.r" "min {9.000000 20.000000 80.000000} max {10.000000 21.000000 81.000000}")
check_bounds("all.g/child.r" "min {7.000000 21.000000 83.000000} max {8.000000 22.000000 84.000000}")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
