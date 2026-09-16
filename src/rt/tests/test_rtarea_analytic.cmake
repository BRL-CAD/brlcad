if(NOT DEFINED RTAREA OR NOT DEFINED DBGEN OR NOT DEFINED TEST_DIR)
  message(FATAL_ERROR "RTAREA, DBGEN, and TEST_DIR are required")
endif()

foreach(required_file IN ITEMS "${RTAREA}" "${DBGEN}")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "Required executable not found: ${required_file}")
  endif()
endforeach()

set(analytic_db "${TEST_DIR}/rtarea-analytic.g")
set(analytic_image_side 512)
set(area_fixed_digits 4)
set(area_fixed_padding "0000")
set(area_fixed_scale 10000)
file(REMOVE "${analytic_db}")

execute_process(
  COMMAND "${DBGEN}" "${analytic_db}"
  OUTPUT_VARIABLE dbgen_stdout
  ERROR_VARIABLE dbgen_stderr
  RESULT_VARIABLE dbgen_result
)
if(NOT dbgen_result EQUAL 0)
  message(FATAL_ERROR "Unable to create rtarea analytic database:\n${dbgen_stdout}${dbgen_stderr}")
endif()

function(run_rtarea output_var object_name image_side)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env "LIBRT_EXP_MODE=0" "LIBRT_RAND_MODE=0"
      "${RTAREA}" -P 1 -s "${image_side}" -a 0 -e 0 "${analytic_db}" "${object_name}"
    OUTPUT_VARIABLE run_stdout
    ERROR_VARIABLE run_stderr
    RESULT_VARIABLE run_result
  )
  if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "rtarea ${object_name} failed:\n${run_stdout}${run_stderr}")
  endif()
  set(${output_var} "${run_stdout}${run_stderr}" PARENT_SCOPE)
endfunction()

function(extract_summary_area output_text area_label output_var)
  string(REGEX MATCH "${area_label}[^=\r\n]*=[ ]*([0-9.]+) square mm" area_match "${output_text}")
  if(area_match STREQUAL "")
    message(FATAL_ERROR "Unable to parse ${area_label} from rtarea output:\n${output_text}")
  endif()
  set(${output_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(require_area_between actual minimum maximum description)
  if(actual LESS minimum OR actual GREATER maximum)
    message(FATAL_ERROR "${description} is ${actual} mm^2; expected ${minimum} through ${maximum} mm^2")
  endif()
endfunction()

function(require_region_count output_text area_type expected_count)
  string(REGEX MATCH "Number of ${area_type} Regions:[ ]*([0-9]+)" count_match "${output_text}")
  if(count_match STREQUAL "" OR NOT CMAKE_MATCH_1 EQUAL expected_count)
    message(FATAL_ERROR "Expected ${expected_count} ${area_type} regions in:\n${output_text}")
  endif()
endfunction()

function(area_error_fixed actual expected_fixed output_var)
  string(REGEX MATCH "^([0-9]+)\.([0-9]+)$" value_match "${actual}")
  if(value_match STREQUAL "")
    message(FATAL_ERROR "Unable to convert area '${actual}' to fixed point")
  endif()

  set(integer_part "${CMAKE_MATCH_1}")
  set(fractional_part "${CMAKE_MATCH_2}${area_fixed_padding}")
  string(SUBSTRING "${fractional_part}" 0 "${area_fixed_digits}" fractional_part)
  # Prefixing the fraction avoids CMake interpreting leading zeroes as octal.
  math(EXPR actual_fixed "${integer_part} * ${area_fixed_scale} + 1${fractional_part} - ${area_fixed_scale}")
  math(EXPR area_error "${actual_fixed} - ${expected_fixed}")
  if(area_error LESS 0)
    math(EXPR area_error "-${area_error}")
  endif()
  set(${output_var} "${area_error}" PARENT_SCOPE)
endfunction()

function(require_convergence object_name description expected_fixed max_scaled_error)
  set(first_presented_error "")
  set(first_exposed_error "")

  foreach(convergence_side ${ARGN})
    run_rtarea(convergence_output "${object_name}" "${convergence_side}")
    extract_summary_area("${convergence_output}" "Cumulative Presented Areas" convergence_presented)
    extract_summary_area("${convergence_output}" "Total Exposed Area" convergence_exposed)

    foreach(area_type IN ITEMS presented exposed)
      area_error_fixed("${convergence_${area_type}}" "${expected_fixed}" current_error)
      math(EXPR scaled_error "${current_error} * ${convergence_side}")
      if(scaled_error GREATER max_scaled_error)
        message(
          FATAL_ERROR
          "${description} ${area_type} area at ${convergence_side}x${convergence_side} "
          "does not satisfy the convergence envelope"
        )
      endif()

      if(first_${area_type}_error STREQUAL "")
        set(first_${area_type}_error "${current_error}")
      endif()
      set(last_${area_type}_error "${current_error}")
    endforeach()
  endforeach()

  foreach(area_type IN ITEMS presented exposed)
    if(NOT last_${area_type}_error LESS first_${area_type}_error)
      message(FATAL_ERROR "${description} ${area_type} area did not improve from the coarsest to densest image")
    endif()
  endforeach()
endfunction()

# These bounds are two percent around the analytic projected areas.  rtarea is
# a pixel estimator, so the tolerance accommodates boundary rasterization while
# remaining independent of any prior rtarea output.
set(sphere_area_min 7696.9020) # 0.98 * pi * 50^2
set(sphere_area_max 8011.0613) # 1.02 * pi * 50^2
set(cube_area_min 9800.0) # 0.98 * 100^2
set(cube_area_max 10200.0) # 1.02 * 100^2
set(two_sphere_area_min 15393.8040) # 0.98 * 2 * pi * 50^2
set(two_sphere_area_max 16022.1225) # 1.02 * 2 * pi * 50^2

run_rtarea(sphere_output sphere.r "${analytic_image_side}")
extract_summary_area("${sphere_output}" "Cumulative Presented Areas" sphere_presented)
extract_summary_area("${sphere_output}" "Total Exposed Area" sphere_exposed)
require_area_between("${sphere_presented}" "${sphere_area_min}" "${sphere_area_max}" "Sphere presented area")
require_area_between("${sphere_exposed}" "${sphere_area_min}" "${sphere_area_max}" "Sphere exposed area")
require_region_count("${sphere_output}" Presented 1)
require_region_count("${sphere_output}" Exposed 1)

run_rtarea(cube_output cube.r "${analytic_image_side}")
extract_summary_area("${cube_output}" "Cumulative Presented Areas" cube_presented)
extract_summary_area("${cube_output}" "Total Exposed Area" cube_exposed)
require_area_between("${cube_presented}" "${cube_area_min}" "${cube_area_max}" "Cube presented area")
require_area_between("${cube_exposed}" "${cube_area_min}" "${cube_area_max}" "Cube exposed area")
require_region_count("${cube_output}" Presented 1)
require_region_count("${cube_output}" Exposed 1)

# At azimuth/elevation 0, the two spheres have identical Y-Z projections.
# Both must be reported as presented, while only the front sphere is exposed.
run_rtarea(occluded_output occluded.g "${analytic_image_side}")
extract_summary_area("${occluded_output}" "Cumulative Presented Areas" occluded_presented)
extract_summary_area("${occluded_output}" "Total Exposed Area" occluded_exposed)
require_area_between(
  "${occluded_presented}"
  "${two_sphere_area_min}"
  "${two_sphere_area_max}"
  "Occluded spheres presented area"
)
require_area_between("${occluded_exposed}" "${sphere_area_min}" "${sphere_area_max}" "Occluded spheres exposed area")
require_region_count("${occluded_output}" Presented 2)
require_region_count("${occluded_output}" Exposed 1)

# For a convergent pixel estimator, error should be bounded by C/image_side.
# Fixed-point arithmetic keeps this check portable without an external numeric
# interpreter.  The constants allow for boundary aliasing while requiring a
# progressively tighter error bound at every image density.
set(sphere_area_fixed 78539816) # pi * 50^2, scaled by 10^4
set(sphere_scaled_error_limit 100000000) # 10000 mm^2-pixels, scaled by 10^4
set(cube_area_fixed 100000000) # 100^2, scaled by 10^4
set(cube_scaled_error_limit 350000000) # 35000 mm^2-pixels, scaled by 10^4
set(convergence_sides 32 64 128 256 512 1024 2048)

require_convergence(
  sphere.r
  Sphere
  "${sphere_area_fixed}"
  "${sphere_scaled_error_limit}"
  ${convergence_sides}
)
require_convergence(
  cube.r
  Cube
  "${cube_area_fixed}"
  "${cube_scaled_error_limit}"
  ${convergence_sides}
)

file(REMOVE "${analytic_db}")
