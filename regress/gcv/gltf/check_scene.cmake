function(run output_variable)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "${ARGN} failed (${status})\n${output}")
  endif()
  set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(check_faces db expected)
  run(objects "${MGED}" -c "${db}" ls -s)
  string(REGEX MATCHALL "[^ \t\r\n]+" names "${objects}")
  set(counts)
  foreach(name IN LISTS names)
    run(count "${MGED}" -c "${db}" bot get faces "${name}")
    list(APPEND counts "${count}")
  endforeach()
  list(SORT counts)
  if(NOT counts STREQUAL expected)
    message(FATAL_ERROR "${db} has BoT face counts ${counts}, expected ${expected}")
  endif()
  set(meshes "${names}" PARENT_SCOPE)
endfunction()

function(check_bounds db expected)
  run(bounds "${MGED}" -c "${db}" bb -eq ${ARGN})
  if(NOT bounds STREQUAL expected)
    message(FATAL_ERROR "${db} ${ARGN} bounds are ${bounds}, expected ${expected}")
  endif()
endfunction()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(flat "${TEST_DIR}/flat.g")
set(scene "${TEST_DIR}/scene.g")
file(REMOVE "${flat}" "${scene}")
run(unused "${GLTF_G}" "${INPUT}" "${flat}")
run(unused "${GCV}" --input-format=gltf "${INPUT}" "${scene}")

# The standalone converter bakes each instance. The plugin shares the
# referenced mesh's BoT and applies each instance's placement in the tree.
set(scene_bounds "min {3.000000 13.000000 30.000000} max {9.000000 22.000000 37.000000}")
check_faces("${flat}" "4;4;5")
check_bounds("${flat}" "${scene_bounds}" ${meshes})
check_faces("${scene}" "1;4;4")
check_bounds("${scene}" "${scene_bounds}" all)
check_bounds("${scene}" "min {6.000000 18.000000 33.000000} max {8.000000 21.000000 37.000000}" all/Region_0/Region_1)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
