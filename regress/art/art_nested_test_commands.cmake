function(set_nested_parameter db material name value)
  run("${VEXEC}" set-named-material "${db}" "${material}.material" "OSL.param.${name}" "${value}")
endfunction()

function(set_nested_profile name)
  set(layers outer inner core)
  if(ARGN)
    set(layers ${ARGN})
  endif()
  foreach(layer IN LISTS layers)
    if(name STREQUAL "equal" OR layer STREQUAL "core")
      set(ior 1)
    elseif(layer STREQUAL "outer")
      set(ior 1.5)
    else()
      set(ior 2.4)
    endif()
    if(name STREQUAL "clear")
      set(color "1 1 1")
      set(distance 0)
    elseif(layer STREQUAL "outer")
      set(color "0.25 0.5 0.8")
      set(distance 20)
    elseif(layer STREQUAL inner)
      set(color "0.8 0.6 0.4")
      set(distance 10)
    else()
      set(color "0.4 0.9 0.7")
      set(distance 4)
    endif()
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_ior "float ${ior}")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_roughness "float 0")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_transmittance_amount "float 1")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_surface_transmittance "color 1 1 1")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_reflection_tint "color 0 0 0")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_volume_transmittance "color ${color}")
    set_nested_parameter("${ACTUAL_DB}" "${layer}" in_volume_transmittance_distance "float ${distance}")
    set_nested_parameter("${REFERENCE_DB}" outer "in_${layer}_ior" "float ${ior}")
    set_nested_parameter("${REFERENCE_DB}" outer "in_${layer}_transmittance" "color ${color}")
    set_nested_parameter("${REFERENCE_DB}" outer "in_${layer}_distance" "float ${distance}")
  endforeach()
endfunction()

# Independent shader and BSDF arithmetic can straddle a binary16 rounding
# boundary, particularly in HDR and translated scenes. Allow one relative
# half-float step (ten fraction bits) plus the absolute arithmetic bound.
function(compare_optical_transport actual reference)
  set(HALF_RELATIVE_STEP 0.0009765625)
  set(relative_bound ${HALF_RELATIVE_STEP})
  # Footprint comparisons may include an explicit first-order budget.
  if(ARGC GREATER 2)
    set(relative_bound "${ARGV2}")
  endif()
  run("${TOOL}" --fail 1 --hardfail 1 --warn 1 --hardwarn 1
    "${actual}" "${reference}" --absdiff
    "${reference}" --abs --mulc ${relative_bound} --addc ${TOLERANCE} --div
    --pattern constant:color=0,0,0,0 ${SIZE}x${SIZE} 4 --diff)
endfunction()

function(compare_radiance_mean image reference bound should_fail)
  run_command("${CMAKE_CURRENT_BINARY_DIR}" "${should_fail}" "${TOOL}"
    --fail ${bound} --hardfail ${bound} --warn ${bound} --hardwarn ${bound}
    "${image}" --ch R,G,B --resize:filter=box 1x1 "${reference}" --ch R,G,B --resize:filter=box 1x1 --absdiff
    "${reference}" --ch R,G,B --resize:filter=box 1x1 --abs --div
    --pattern constant:color=0,0,0 1x1 3 --diff)
  if(should_fail AND NOT command_output MATCHES "FAILURE")
    message(FATAL_ERROR "Radiance comparison did not report an image mismatch: ${command_output}")
  endif()
endfunction()
