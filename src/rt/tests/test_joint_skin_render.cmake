if(NOT DEFINED GENERATOR OR NOT DEFINED RT OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "GENERATOR, RT, and TEST_DIR are required")
endif()

set(database "${TEST_DIR}/joint-skin-render.g")
set(bind_image "${TEST_DIR}/joint-skin-bind.pix")
set(animated_image "${TEST_DIR}/joint-skin-animated.pix")
set(moved_image "${TEST_DIR}/joint-skin-moved.pix")
set(image_side 64)
math(EXPR expected_size "${image_side} * ${image_side} * 3")
file(REMOVE "${database}" "${bind_image}" "${animated_image}" "${moved_image}")

execute_process(
  COMMAND "${GENERATOR}" "${database}"
  OUTPUT_VARIABLE generator_output
  ERROR_VARIABLE generator_error
  RESULT_VARIABLE generator_result
)
if(NOT generator_result EQUAL 0)
  message(FATAL_ERROR "rigged database generation failed:\n${generator_output}${generator_error}")
endif()

foreach(pose IN ITEMS bind animated moved)
  if(pose STREQUAL "bind")
    set(object bind-pose.bot)
    set(image "${bind_image}")
  elseif(pose STREQUAL "animated")
    set(object animated-pose.bot)
    set(image "${animated_image}")
  else()
    set(object moved-pose.bot)
    set(image "${moved_image}")
  endif()
  execute_process(
    COMMAND
      "${RT}" -P 1 -s "${image_side}" -a 35 -e 25 -C 255/255/255
      -o "${image}" "${database}" "${object}"
    OUTPUT_VARIABLE render_output
    ERROR_VARIABLE render_error
    RESULT_VARIABLE render_result
  )
  if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "${pose} skin pose render failed:\n${render_output}${render_error}")
  endif()
  file(SIZE "${image}" image_size)
  if(NOT image_size EQUAL expected_size)
    message(FATAL_ERROR "${pose} skin pose render has unexpected size ${image_size}")
  endif()
endforeach()

file(SHA256 "${bind_image}" bind_hash)
file(SHA256 "${animated_image}" animated_hash)
file(SHA256 "${moved_image}" moved_hash)
if(bind_hash STREQUAL animated_hash OR animated_hash STREQUAL moved_hash)
  message(FATAL_ERROR "sampled animation did not produce a distinct rendered skin pose")
endif()

file(REMOVE "${database}" "${bind_image}" "${animated_image}" "${moved_image}")


