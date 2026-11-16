function(set_rough_parameter name value)
  foreach(db IN ITEMS "${ACTUAL_DB}" "${REFERENCE_DB}")
    run("${VEXEC}" set-material "${db}" "OSL.param.${name}" "${value}")
  endforeach()
endfunction()

function(render_rough_pair label shape projection position)
  foreach(variant IN ITEMS actual reference)
    string(TOUPPER "${variant}" upper_variant)
    set(db "${${upper_variant}_DB}")
    foreach(material IN ITEMS osl.material receiver.material)
      run("${VEXEC}" set-named-material "${db}" "${material}" OSL.param.in_position "int ${position}")
    endforeach()
    set(image "${WORK}/${label}-${variant}.exr")
    run_in("${WORK}" "${EXEC}" ${RENDER_ARGS} -p ${projection}
      -o "${image}" "${db}" "${shape}.r" target.r)
    run("${VEXEC}" check-image "${image}" 1 1)
    run("${TOOL}" --fail 0 --hardfail 0 --warn 0 --hardwarn 0
      "${image}" --ch R,G,B --minc ${MIN_RADIANCE}
      --pattern "constant:color=${MIN_RADIANCE},${MIN_RADIANCE},${MIN_RADIANCE}" 1x1 3 --diff)
  endforeach()
  compare_radiance_mean("${WORK}/${label}-actual.exr"
    "${WORK}/${label}-reference.exr" ${RELATIVE_BOUND} FALSE)
endfunction()

function(create_rough_databases mode)
  foreach(db IN ITEMS "${ACTUAL_DB}" "${REFERENCE_DB}")
    run("${VEXEC}" create-material "${db}" art_rough_footprint_reference)
    if(mode EQUAL 2)
      # The receiver lies inside the glass, before any second interface.
      run("${VEXEC}" in "${db}" target.s rpp ${TRANSMISSION_RECEIVER_BOUNDS})
      run("${VEXEC}" r "${db}" target.r u target.s)
    else()
      # The primary camera looks through the hole; reflected rays hit the
      # plane or the walls of its finite thickness.
      run("${VEXEC}" in "${db}" outer.s rpp -200 200 -200 200 30 31)
      run("${VEXEC}" in "${db}" hole.s rpp ${REFLECTION_HOLE_BOUNDS})
      run("${VEXEC}" r "${db}" target.r u outer.s - hole.s)
    endif()
    run("${VEXEC}" material "${db}" assign target.r receiver.material)
    run("${VEXEC}" material "${db}" remove receiver.material optical OSL.param.in_target)
    run("${VEXEC}" set-named-material "${db}" receiver.material OSL art_rough_footprint_reference)
    run("${VEXEC}" set-named-material "${db}" receiver.material OSL.param.in_receiver "int 1")
  endforeach()
  run("${VEXEC}" set-material "${REFERENCE_DB}" OSL.param.in_reference "int 1")
endfunction()
