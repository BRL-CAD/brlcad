if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.22)
  project(art_installed_regressions NONE)
endif()

set(ART_TEST_SHADER_NAMES
  art_diffuse
  art_color
  art_uv
  art_normals
  art_footprints
  art_parameters
  art_texture
  art_emission
  art_graph_value
  art_bump_field
  art_bump_normal
  art_sdk_helpers
  art_normal_reference
  art_color_check
  art_transfer
  art_image_field
  art_array_values
  art_absorption_reference
  art_refraction_reference
  art_mirror_mixture
  art_mirror_node
  art_surface_reference
  art_coat_source
  art_coat_reference
  art_instance_materials
)
set(ART_REGRESSION_TIMEOUT 180)
set(ART_REGRESSION_TEST_NAMES
  regress-art
  regress-art-bump
  regress-art-normal
  regress-art-texture
  regress-art-color
  regress-art-image-maps
  regress-art-resources
  regress-art-transmission
  regress-art-refraction
  regress-art-mixture
  regress-art-surface
  regress-art-metal-energy
  regress-art-coat
  regress-art-materials
  regress-art-inspect
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include("${CMAKE_CURRENT_SOURCE_DIR}/art_installed_tests.cmake")
  return()
endif()

cmakefiles(CMakeLists.txt art_test_setup.cmake art_installed_tests.cmake
  art_installed_runtime.cmake.in art_scene.cpp art_test_mesh.h art_test_commands.cmake
  regress-art.cmake.in regress-art-bump.cmake.in regress-art-normal.cmake.in
  regress-art-texture.cmake.in regress-art-color.cmake.in
  regress-art-image-maps.cmake.in regress-art-resources.cmake.in
  regress-art-transmission.cmake.in regress-art-refraction.cmake.in
  regress-art-mixture.cmake.in regress-art-surface.cmake.in
  regress-art-metal-energy.cmake.in regress-art-coat.cmake.in
  regress-art-materials.cmake.in regress-art-inspect.cmake.in)
foreach(shader IN LISTS ART_TEST_SHADER_NAMES)
  cmakefiles("${shader}.osl")
endforeach()

if(TARGET art AND BUILD_TESTING)
  get_target_property(renderer_prefix art ART_RENDERER_PREFIX)
  include("${CMAKE_CURRENT_SOURCE_DIR}/art_test_setup.cmake")
  brlcad_addexec(art_scene_test art_scene.cpp "libged;libwdb;libicv" TEST)
  add_dependencies(art_scene_test art_test_shaders)
  foreach(test IN LISTS ART_REGRESSION_TEST_NAMES)
    brlcad_regression_test(${test} "art;art_scene_test" EXEC art VEXEC art_scene_test TIMEOUT ${ART_REGRESSION_TIMEOUT})
  endforeach()
  add_test(NAME regress-art-intersections COMMAND art_intersection_test)
  brlcad_regression_test(regress-art-intersections "art_intersection_test" TEST_DEFINED TIMEOUT 120)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/output" "${CMAKE_CURRENT_BINARY_DIR}/bump-output"
    "${CMAKE_CURRENT_BINARY_DIR}/normal-output" "${CMAKE_CURRENT_BINARY_DIR}/texture-output"
    "${CMAKE_CURRENT_BINARY_DIR}/color-output"
    "${CMAKE_CURRENT_BINARY_DIR}/image-maps-output"
    "${CMAKE_CURRENT_BINARY_DIR}/resources-output"
    "${CMAKE_CURRENT_BINARY_DIR}/transmission-output"
    "${CMAKE_CURRENT_BINARY_DIR}/refraction-output"
    "${CMAKE_CURRENT_BINARY_DIR}/mixture-output"
    "${CMAKE_CURRENT_BINARY_DIR}/surface-output"
    "${CMAKE_CURRENT_BINARY_DIR}/metal-energy-output"
    "${CMAKE_CURRENT_BINARY_DIR}/coat-output"
    "${CMAKE_CURRENT_BINARY_DIR}/materials-output"
    "${CMAKE_CURRENT_BINARY_DIR}/inspect-output")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
