include("${CMAKE_CURRENT_LIST_DIR}/art_test_commands.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/art_nested_test_commands.cmake")

function(art_standard_native_project path shader distribution alpha mode eye_x eye_z)
  set(REFERENCE_EYE_X "${eye_x}")
  set(REFERENCE_EYE_Z "${eye_z}")
  set(REFERENCE_GROUPS "")
  set(REFERENCE_INSTANCES "")
  foreach(layer ior priority x y z IN ZIP_LISTS layers indices priorities axes_x axes_y axes_z)
    set(distribution_parameter "")
    if(shader STREQUAL "art_standard_transmission")
      set(distribution_parameter "<parameter name=\"in_distribution\" value=\"string ${distribution}\" />")
    endif()
    string(APPEND REFERENCE_GROUPS "
      <shader_group name=\"${layer}.group\">
        <shader type=\"surface\" name=\"${shader}\" layer=\"surface\">
          <parameter name=\"in_ior\" value=\"float ${ior}\" />
          <parameter name=\"in_alpha\" value=\"float ${alpha}\" />
          <parameter name=\"in_mode\" value=\"int ${mode}\" />
          <parameter name=\"in_tangent\" value=\"vector 1 1 1\" />
          ${distribution_parameter}
        </shader>
      </shader_group>
      <material name=\"${layer}.material\" model=\"osl_material\">
        <parameter name=\"osl_surface\" value=\"${layer}.group\" />
        <parameter name=\"surface_shader\" value=\"surface_shader\" />
      </material>
")
    string(APPEND REFERENCE_INSTANCES "
      <object_instance name=\"${layer}\" object=\"${REFERENCE_OBJECT_NAME}\">
        <parameter name=\"medium_priority\" value=\"${priority}\" />
        <transform><matrix>${x} 0 0 0 0 ${y} 0 0 0 0 ${z} 0 0 0 0 1</matrix></transform>
        <assign_material slot=\"default\" side=\"front\" material=\"${layer}.material\" />
        <assign_material slot=\"default\" side=\"back\" material=\"${layer}.material\" />
      </object_instance>
")
  endforeach()
  # Relative shader/mesh paths keep the project usable after relocation.
  configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/art_standard_nested.appleseed.in" "${path}" @ONLY)
endfunction()

function(art_standard_compare_paths actual reference)
  run("${TOOL}" --fail ${PIXEL_RELATIVE_BOUND} --hardfail ${PIXEL_RELATIVE_BOUND}
    --warn ${PIXEL_RELATIVE_BOUND} --hardwarn ${PIXEL_RELATIVE_BOUND}
    "${actual}" --ch R,G,B "${reference}" --ch R,G,B --absdiff
    "${reference}" --ch R,G,B --abs --addc ${REFERENCE_FLOOR} --div
    --pattern constant:color=0,0,0 ${SIZE}x${SIZE} 3 --diff)
  compare_radiance_mean("${actual}" "${reference}" ${MEAN_RELATIVE_BOUND} FALSE)
endfunction()

function(art_standard_render_native path image)
  run("${NATIVE_RENDERER}" --output "${image}" "${path}")
  run("${VEXEC}" check-image "${image}" ${SIZE} ${SIZE})
  expect_failure("${TOOL}" --fail ${REFERENCE_FLOOR} --hardfail ${REFERENCE_FLOOR}
    "${image}" --ch R,G,B "${WORK}/black.exr" --diff)
endfunction()

function(art_standard_rough_paths shapes size samples)
  set(source_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  if(shapes STREQUAL "box")
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}/shaders" "${WORK}/cache")
    set(ENV{BU_DIR_CACHE} "${WORK}/cache")
  else()
    # Reuse the established nested quadric construction.
    include("${source_dir}/art_curved_test_commands.cmake")
  endif()
  foreach(shader IN ITEMS art_standard_transmission art_canonical_glass)
    configure_file("${CMAKE_CURRENT_BINARY_DIR}/${shader}.oso" "${WORK}/shaders/${shader}.oso" COPYONLY)
  endforeach()
  set(SIZE ${size})
  set(SAMPLES ${samples})
  set(WORKERS 2)
  set(BOUNCES 16)
  set(MEAN_RELATIVE_BOUND 0.002)
  set(PIXEL_RELATIVE_BOUND 0.1)
  set(REFERENCE_FLOOR 0.0005)
  set(RENDER_ARGS -P ${WORKERS} -w ${SIZE} -n ${SIZE} -p 2 -c "viewsize 0.5"
    -c "orientation 0 0 0 1"
    -c "set sampling=uniform samples=${SAMPLES} light_intensity=0 environment_color=white environment_intensity=1 background_color=black max_bounces=${BOUNCES} max_glossy_bounces=${BOUNCES} max_specular_bounces=${BOUNCES} rr_min_path_length=0")
  foreach(shape IN LISTS shapes)
    if(shape STREQUAL "box")
      set(DB "${WORK}/actual.g")
      set(layers outer inner core)
      set(indices 1.5 2.4 1.1)
      set(priorities 1 2 3)
      set(axes_x 1 0.5 0.2)
      set(axes_y ${axes_x})
      set(axes_z ${axes_x})
      set(kinds box mesh)
      set(eye_x 0)
      set(camera_z 40 4)
      set(REFERENCE_OBJECT_MODEL mesh_object)
      set(REFERENCE_OBJECT_NAME geometry.cube)
      set(REFERENCE_OBJECT_PARAMETERS "<parameter name=\"filename\" value=\"art_rough_box.obj\" />")
      configure_file("${source_dir}/art_rough_box.obj" "${WORK}/art_rough_box.obj" COPYONLY)
      run("${VEXEC}" create-nested "${DB}")
    else()
      if(NOT shape STREQUAL "sphere" AND NOT shape STREQUAL "ellipsoid")
        message(FATAL_ERROR "Unsupported standard-path geometry: ${shape}")
      endif()
      create_curved_pair("${shape}")
      set(DB "${ACTUAL_DB}")
      set(layers outer inner)
      set(indices 1.5 2.4)
      set(priorities 1 2)
      set(axes_x 10 5)
      if(shape STREQUAL "sphere")
        set(axes_y ${axes_x})
        set(axes_z ${axes_x})
      else()
        set(axes_y 8 4)
        set(axes_z 6 3)
      endif()
      set(kinds curved)
      set(eye_x 1.5)
      set(camera_z 12 0)
      set(REFERENCE_OBJECT_MODEL sphere_object)
      set(REFERENCE_OBJECT_NAME geometry)
      set(REFERENCE_OBJECT_PARAMETERS "")
    endif()
    foreach(layer ior priority IN ZIP_LISTS layers indices priorities)
      run("${VEXEC}" set-named-material "${DB}" "${layer}.material" OSL art_standard_transmission)
      run("${VEXEC}" set-named-material "${DB}" "${layer}.material" OSL.medium_priority "${priority}")
      set_nested_parameter("${DB}" "${layer}" in_ior "float ${ior}")
      # A common authored frame controls variance across geometry adapters.
      # The diagonal tangent is nonparallel to every box face.
      set_nested_parameter("${DB}" "${layer}" in_tangent "vector 1 1 1")
    endforeach()
    run("${TOOL}" --pattern constant:color=0,0,0 ${SIZE}x${SIZE} 3 -o "${WORK}/black.exr")
    foreach(distribution IN ITEMS ggx beckmann)
      foreach(alpha IN ITEMS 0.12 0.4)
        foreach(mode IN ITEMS 1 2)
          foreach(layer IN LISTS layers)
            set_nested_parameter("${DB}" "${layer}" in_alpha "float ${alpha}")
            set_nested_parameter("${DB}" "${layer}" in_mode "int ${mode}")
            set_nested_parameter("${DB}" "${layer}" in_distribution "string ${distribution}")
          endforeach()
          foreach(z IN LISTS camera_z)
            set(stem "${WORK}/${shape}-${distribution}-${alpha}-${mode}-${z}")
            set(reference "${stem}-native.exr")
            art_standard_native_project("${stem}.appleseed" art_standard_transmission "${distribution}" "${alpha}" "${mode}" "${eye_x}" "${z}")
            art_standard_render_native("${stem}.appleseed" "${reference}")
            foreach(kind IN LISTS kinds)
              set(actual "${stem}-${kind}.exr")
              # Exclude the analytic receiver from these uniform-environment paths.
              set(objects)
              foreach(layer IN LISTS layers)
                list(APPEND objects "${kind}.${layer}.r")
              endforeach()
              run("${EXEC}" ${RENDER_ARGS} -c "eye_pt ${eye_x} 0 ${z}" -o "${actual}" "${DB}" ${objects})
              run("${VEXEC}" check-image "${actual}" ${SIZE} ${SIZE})
              art_standard_compare_paths("${actual}" "${reference}")
            endforeach()
            if(distribution STREQUAL "ggx" AND mode EQUAL 2)
              set(reference "${stem}-canonical.exr")
              art_standard_native_project("${stem}-canonical.appleseed" art_canonical_glass "${distribution}" "${alpha}" "${mode}" "${eye_x}" "${z}")
              art_standard_render_native("${stem}-canonical.appleseed" "${reference}")
              foreach(kind IN LISTS kinds)
                art_standard_compare_paths("${stem}-${kind}.exr" "${reference}")
              endforeach()
            endif()
          endforeach()
        endforeach()
      endforeach()
    endforeach()
  endforeach()
endfunction()
