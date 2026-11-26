file(MAKE_DIRECTORY "${TEST_DIR}")
set(database "${TEST_DIR}/textured.g")
set(render "${TEST_DIR}/textured.pix")
set(image_side 64)
file(REMOVE "${database}" "${render}")
execute_process(
  COMMAND "${GCV}" --input-format=gltf "${INPUT}" "${database}"
  COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
  COMMAND "${RT}" -P 1 -s "${image_side}" -o "${render}"
    -c "viewsize 256" -c "eye_pt 0 0 200"
    -c "viewrot 1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"
    "${database}" all
  COMMAND_ERROR_IS_FATAL ANY
)

file(READ "${render}" pixels HEX)
function(check_pixel x y expected)
  math(EXPR offset "6 * (${y} * ${image_side} + ${x})")
  string(SUBSTRING "${pixels}" "${offset}" 6 actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Raytraced pixel (${x}, ${y}) is ${actual}, expected ${expected}")
  endif()
endfunction()

# Sample each quadrant of the original quad and its rotated instance.
check_pixel(10 24 "0000ff")
check_pixel(24 24 "ffff00")
check_pixel(10 40 "ff0000")
check_pixel(24 40 "00ff00")
check_pixel(40 24 "ff0000")
check_pixel(54 24 "0000ff")
check_pixel(40 40 "00ff00")
check_pixel(54 40 "ffff00")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
