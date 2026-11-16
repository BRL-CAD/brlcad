set(ART_TEST_SHADER_NAMES
  art_diffuse
  art_color
  art_uv
  art_normals
  art_footprints
  art_parameters
  art_texture
  art_emission
)
cmakefiles(CMakeLists.txt art_scene.cpp art_test_mesh.h regress-art.cmake.in)
foreach(shader IN LISTS ART_TEST_SHADER_NAMES)
  cmakefiles("${shader}.osl")
endforeach()

if(TARGET art AND BUILD_TESTING)
  get_target_property(renderer_prefix art ART_RENDERER_PREFIX)
  find_program(ART_OSLC_EXECUTABLE NAMES oslc
    HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH REQUIRED)
  find_path(ART_OSL_SHADER_INCLUDE_DIR stdosl.h
    HINTS "${renderer_prefix}/share/OSL/shaders" "${renderer_prefix}/shaders" NO_DEFAULT_PATH REQUIRED)
  find_program(ART_MAKETX_EXECUTABLE NAMES maketx
    HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH REQUIRED)
  set(ART_TEST_SHADERS)
  foreach(shader IN LISTS ART_TEST_SHADER_NAMES)
    set(compiled "${CMAKE_CURRENT_BINARY_DIR}/${shader}.oso")
    add_custom_command(OUTPUT "${compiled}"
      COMMAND "${ART_OSLC_EXECUTABLE}" -I "${ART_OSL_SHADER_INCLUDE_DIR}"
        -o "${compiled}" "${CMAKE_CURRENT_SOURCE_DIR}/${shader}.osl"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}.osl"
        "${ART_OSLC_EXECUTABLE}" "${ART_OSL_SHADER_INCLUDE_DIR}/stdosl.h"
      VERBATIM)
    list(APPEND ART_TEST_SHADERS "${compiled}")
  endforeach()
  add_custom_target(art_test_shaders DEPENDS ${ART_TEST_SHADERS})
  brlcad_addexec(art_scene_test art_scene.cpp "libged;libwdb;libicv" TEST)
  add_dependencies(art_scene_test art_test_shaders)
  brlcad_regression_test(regress-art "art;art_scene_test" EXEC art VEXEC art_scene_test TIMEOUT 120)
  add_test(NAME regress-art-intersections COMMAND art_intersection_test)
  brlcad_regression_test(regress-art-intersections "art_intersection_test" TEST_DEFINED TIMEOUT 120)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/output")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
