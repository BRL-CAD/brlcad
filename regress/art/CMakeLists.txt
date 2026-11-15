if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.22)
  project(art_installed_regressions NONE)
endif()

set(ART_TEST_SHADER_NAMES
  art_diffuse
  art_alpha
  art_attributes
  art_trace_queries
  art_color
  art_uv
  art_normals
  art_footprints
  art_parameters
  art_texture
  art_emission
  art_directional_emission
  art_emission_medium
  art_embedded_surface
  art_shared_layer
  art_npr
  art_sampling
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
  art_mapped_glass_field
  art_mapped_glass_reference
  art_glass_footprint_field
  art_glass_footprint_reference
  art_nested_reference
  art_curved_reference
  art_rough_glass_reference
  art_rough_furnace_reference
  art_glass_lighting_reference
  art_photon_reference
  art_area_exit_reference
  art_mirror_mixture
  art_mirror_node
  art_surface_reference
  art_coat_source
  art_coat_reference
  art_instance_materials
)
set(ART_REGRESSION_TIMEOUT 180)
# The energy cases use 32 million photons to bound estimator error at 1 percent.
set(ART_PHOTON_REGRESSION_TIMEOUT 720)
# Inside-light cases compare clear and absorbing transport in separate batches.
set(ART_PHOTON_INSIDE_REGRESSION_TIMEOUT 1560)
# Spectral conservation uses 12.6 million samples and permits concurrent checks.
set(ART_VOLUME_ENERGY_REGRESSION_TIMEOUT 720)
# Absorbing random-walk comparisons use 131072 samples per scattering view.
set(ART_VOLUME_WALK_REGRESSION_TIMEOUT 1200)
# Mapped transmission compares 78 complete nested paths with analytic references.
set(ART_MAPPED_GLASS_REGRESSION_TIMEOUT 600)
# Screen-space checks compare 204 paths at two footprint sizes.
set(ART_GLASS_FOOTPRINT_REGRESSION_TIMEOUT 1200)
# Two area emitters each qualify constant and angular photon emission.
set(ART_DIRECTIONAL_EMISSION_REGRESSION_TIMEOUT 1200)
# Each dielectric exit case checks clear and absorbing 16-million-photon batches.
set(ART_AREA_EXIT_REGRESSION_TIMEOUT 900)
function(art_regression_timeout name result)
  if(name MATCHES "^regress-art-photon-inside-(csg|bot)-")
    set(${result} ${ART_PHOTON_INSIDE_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name MATCHES "^regress-art-area-exit-(csg|bot)-")
    set(${result} ${ART_AREA_EXIT_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name STREQUAL "regress-art-directional-emission")
    set(${result} ${ART_DIRECTIONAL_EMISSION_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name MATCHES "^regress-art-volume-walk(-overlap)?$")
    set(${result} ${ART_VOLUME_WALK_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name STREQUAL "regress-art-glass-footprints")
    set(${result} ${ART_GLASS_FOOTPRINT_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name STREQUAL "regress-art-mapped-glass")
    set(${result} ${ART_MAPPED_GLASS_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name STREQUAL "regress-art-volume-energy-spectral")
    set(${result} ${ART_VOLUME_ENERGY_REGRESSION_TIMEOUT} PARENT_SCOPE)
  elseif(name MATCHES "^regress-art-photon-(csg|bot)-")
    set(${result} ${ART_PHOTON_REGRESSION_TIMEOUT} PARENT_SCOPE)
  else()
    set(${result} ${ART_REGRESSION_TIMEOUT} PARENT_SCOPE)
  endif()
endfunction()
set(ART_REGRESSION_TEST_NAMES
  regress-art
  regress-art-bump
  regress-art-normal
  regress-art-texture
  regress-art-trace
  regress-art-attributes
  regress-art-emission
  regress-art-directional-emission
  regress-art-area-exit-csg-15
  regress-art-area-exit-csg-20
  regress-art-area-exit-bot-15
  regress-art-area-exit-bot-20
  regress-art-emission-medium
  regress-art-nee-context
  regress-art-color
  regress-art-color-metadata
  regress-art-image-maps
  regress-art-resources
  regress-art-transmission
  regress-art-absorption
  regress-art-initial-media
  regress-art-volume
  regress-art-volume-materials
  regress-art-volume-scattering
  regress-art-volume-energy
  regress-art-volume-energy-spectral
  regress-art-volume-walk
  regress-art-volume-walk-overlap
  regress-art-volume-overlap
  regress-art-volume-glass
  regress-art-medium-priority
  regress-art-shared-media
  regress-art-transparent-contact
  regress-art-curved-media
  regress-art-curved-contact
  regress-art-rough-glass
  regress-art-rough-energy
  regress-art-rough-paths
  regress-art-caustics-clear
  regress-art-caustics-absorption
  regress-art-caustics-state
  regress-art-caustics-rough
  regress-art-photon-csg-15
  regress-art-photon-csg-20
  regress-art-photon-bot-15
  regress-art-photon-bot-20
  regress-art-photon-inside-csg-15
  regress-art-photon-inside-csg-20
  regress-art-photon-inside-bot-15
  regress-art-photon-inside-bot-20
  regress-art-photon-controls
  regress-art-refraction
  regress-art-glass-sides
  regress-art-mapped-glass
  regress-art-glass-footprints
  regress-art-nested
  regress-art-mixture
  regress-art-surface
  regress-art-metal-energy
  regress-art-coat
  regress-art-materials
  regress-art-inspect
  regress-art-region-colors
  regress-art-view
  regress-art-frames
  regress-art-selection
  regress-art-lighting
  regress-art-output
  regress-art-settings
  regress-art-environment
  regress-art-environment-map
  regress-art-framebuffer
  regress-art-progress
  regress-art-sampling
  regress-art-bounces
  regress-art-aovs
  regress-art-npr
  regress-art-exposure
)
if(UNIX)
  list(APPEND ART_REGRESSION_TEST_NAMES regress-art-cancel regress-art-photon-cancel)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include("${CMAKE_CURRENT_SOURCE_DIR}/art_installed_tests.cmake")
  return()
endif()

cmakefiles(CMakeLists.txt art_test_setup.cmake art_installed_tests.cmake
  art_nested_test_commands.cmake art_curved_test_commands.cmake
  art_caustic_test_commands.cmake.in art_photon_test_commands.cmake.in
  art_area_exit_test_commands.cmake.in
  regress-art-area-exit-csg-15.cmake.in
  regress-art-area-exit-csg-20.cmake.in
  regress-art-area-exit-bot-15.cmake.in
  regress-art-area-exit-bot-20.cmake.in
  art_rough_box.appleseed.in art_rough_box.obj art_lit_glass.appleseed.in
  regress-art-color-metadata.cmake.in
  art_initial_volume.appleseed.in regress-art-volume.cmake.in
  regress-art-volume-materials.cmake.in regress-art-volume-scattering.cmake.in
  art_volume_test_commands.cmake.in art_volume_energy_test_commands.cmake.in
  art_volume_walk_test_commands.cmake.in art_volume_reference.json
  regress-art-volume-walk.cmake.in regress-art-volume-walk-overlap.cmake.in
  regress-art-volume-energy.cmake.in regress-art-volume-energy-spectral.cmake.in
  regress-art-volume-overlap.cmake.in regress-art-volume-glass.cmake.in
  art_installed_runtime.cmake.in art-installed-material.cmake.in
  art-installed-plate.cmake.in art-installed-view.cmake.in
  regress-art-photon-csg-15.cmake.in
  regress-art-photon-csg-20.cmake.in
  regress-art-photon-bot-15.cmake.in
  regress-art-photon-bot-20.cmake.in
  regress-art-photon-inside-csg-15.cmake.in
  regress-art-photon-inside-csg-20.cmake.in
  regress-art-photon-inside-bot-15.cmake.in
  regress-art-photon-inside-bot-20.cmake.in
  regress-art-photon-controls.cmake.in
  regress-art-photon-cancel.cmake.in
  art_scene.cpp art_test_mesh.h art_glass_reference.h art_test_commands.cmake
  regress-art.cmake.in regress-art-bump.cmake.in regress-art-normal.cmake.in
  regress-art-trace.cmake.in
  regress-art-attributes.cmake.in
  regress-art-emission.cmake.in regress-art-directional-emission.cmake.in
  regress-art-emission-medium.cmake.in regress-art-nee-context.cmake.in
  regress-art-texture.cmake.in regress-art-color.cmake.in
  regress-art-image-maps.cmake.in regress-art-resources.cmake.in
  regress-art-transmission.cmake.in regress-art-absorption.cmake.in regress-art-initial-media.cmake.in regress-art-medium-priority.cmake.in
  regress-art-shared-media.cmake.in regress-art-transparent-contact.cmake.in regress-art-curved-media.cmake.in
  regress-art-curved-contact.cmake.in regress-art-rough-glass.cmake.in
  regress-art-rough-energy.cmake.in regress-art-rough-paths.cmake.in
  regress-art-caustics-clear.cmake.in regress-art-caustics-absorption.cmake.in regress-art-caustics-state.cmake.in regress-art-caustics-rough.cmake.in
  regress-art-refraction.cmake.in
  regress-art-glass-sides.cmake.in regress-art-mapped-glass.cmake.in
  regress-art-glass-footprints.cmake.in
  regress-art-mixture.cmake.in regress-art-surface.cmake.in regress-art-nested.cmake.in
  regress-art-metal-energy.cmake.in regress-art-coat.cmake.in
  regress-art-materials.cmake.in regress-art-inspect.cmake.in
  regress-art-region-colors.cmake.in regress-art-view.cmake.in
  regress-art-frames.cmake.in regress-art-selection.cmake.in
  regress-art-lighting.cmake.in regress-art-output.cmake.in regress-art-settings.cmake.in
  regress-art-environment.cmake.in regress-art-environment-map.cmake.in
  regress-art-framebuffer.cmake.in regress-art-progress.cmake.in regress-art-cancel.cmake.in
  regress-art-sampling.cmake.in regress-art-bounces.cmake.in regress-art-aovs.cmake.in regress-art-npr.cmake.in regress-art-exposure.cmake.in)
foreach(shader IN LISTS ART_TEST_SHADER_NAMES)
  cmakefiles("${shader}.osl")
endforeach()

if(TARGET art AND BUILD_TESTING)
  get_target_property(renderer_prefix art ART_RENDERER_PREFIX)
  include("${CMAKE_CURRENT_SOURCE_DIR}/art_test_setup.cmake")
  brlcad_addexec(art_scene_test art_scene.cpp "libged;libwdb;libicv" TEST)
  brlcad_addexec(art_volume_reference art_volume_reference.cpp libbu TEST)
  add_dependencies(art_scene_test art_test_shaders)
  foreach(test IN LISTS ART_REGRESSION_TEST_NAMES)
    art_regression_timeout("${test}" timeout)
    brlcad_regression_test(${test} "art;art_scene_test" EXEC art VEXEC art_scene_test TIMEOUT ${timeout})
  endforeach()
  add_test(NAME regress-art-render-control COMMAND art_render_control_test)
  brlcad_regression_test(regress-art-render-control "art_render_control_test" TEST_DEFINED TIMEOUT 30)
  add_test(NAME regress-art-intersections COMMAND art_intersection_test)
  brlcad_regression_test(regress-art-intersections "art_intersection_test" TEST_DEFINED TIMEOUT 120)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/output" "${CMAKE_CURRENT_BINARY_DIR}/bump-output"
    "${CMAKE_CURRENT_BINARY_DIR}/normal-output" "${CMAKE_CURRENT_BINARY_DIR}/texture-output"
    "${CMAKE_CURRENT_BINARY_DIR}/attributes-output"
    "${CMAKE_CURRENT_BINARY_DIR}/emission-output"
    "${CMAKE_CURRENT_BINARY_DIR}/directional-emission-output"
    "${CMAKE_CURRENT_BINARY_DIR}/area-exit-csg-15-output"
    "${CMAKE_CURRENT_BINARY_DIR}/area-exit-csg-20-output"
    "${CMAKE_CURRENT_BINARY_DIR}/area-exit-bot-15-output"
    "${CMAKE_CURRENT_BINARY_DIR}/area-exit-bot-20-output"
    "${CMAKE_CURRENT_BINARY_DIR}/emission-medium-output"
    "${CMAKE_CURRENT_BINARY_DIR}/nee-context-output"
    "${CMAKE_CURRENT_BINARY_DIR}/trace-output"
    "${CMAKE_CURRENT_BINARY_DIR}/color-output"
    "${CMAKE_CURRENT_BINARY_DIR}/color-metadata-output"
    "${CMAKE_CURRENT_BINARY_DIR}/image-maps-output"
    "${CMAKE_CURRENT_BINARY_DIR}/resources-output"
    "${CMAKE_CURRENT_BINARY_DIR}/transmission-output"
    "${CMAKE_CURRENT_BINARY_DIR}/absorption-output" "${CMAKE_CURRENT_BINARY_DIR}/initial-media-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-materials-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-scattering-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-energy-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-energy-spectral-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-overlap-output" "${CMAKE_CURRENT_BINARY_DIR}/volume-glass-output" "${CMAKE_CURRENT_BINARY_DIR}/medium-priority-output" "${CMAKE_CURRENT_BINARY_DIR}/shared-media-output" "${CMAKE_CURRENT_BINARY_DIR}/transparent-contact-output"
    "${CMAKE_CURRENT_BINARY_DIR}/volume-walk-output"
    "${CMAKE_CURRENT_BINARY_DIR}/volume-walk-overlap-output"
    "${CMAKE_CURRENT_BINARY_DIR}/curved-media-output" "${CMAKE_CURRENT_BINARY_DIR}/curved-contact-output"
    "${CMAKE_CURRENT_BINARY_DIR}/rough-glass-output"
    "${CMAKE_CURRENT_BINARY_DIR}/rough-energy-output"
    "${CMAKE_CURRENT_BINARY_DIR}/rough-paths-output"
    "${CMAKE_CURRENT_BINARY_DIR}/caustics-clear-output"
    "${CMAKE_CURRENT_BINARY_DIR}/caustics-absorption-output"
    "${CMAKE_CURRENT_BINARY_DIR}/caustics-state-output"
    "${CMAKE_CURRENT_BINARY_DIR}/caustics-rough-output"
    "${CMAKE_CURRENT_BINARY_DIR}/regress-art-photon-csg-15-output"
    "${CMAKE_CURRENT_BINARY_DIR}/regress-art-photon-csg-20-output"
    "${CMAKE_CURRENT_BINARY_DIR}/regress-art-photon-bot-15-output"
    "${CMAKE_CURRENT_BINARY_DIR}/regress-art-photon-bot-20-output"
    "${CMAKE_CURRENT_BINARY_DIR}/photon-controls-output"
    "${CMAKE_CURRENT_BINARY_DIR}/photon-cancel-output"
    "${CMAKE_CURRENT_BINARY_DIR}/refraction-output"
    "${CMAKE_CURRENT_BINARY_DIR}/glass-sides-output"
    "${CMAKE_CURRENT_BINARY_DIR}/mapped-glass-output"
    "${CMAKE_CURRENT_BINARY_DIR}/glass-footprints-output"
    "${CMAKE_CURRENT_BINARY_DIR}/nested-output"
    "${CMAKE_CURRENT_BINARY_DIR}/mixture-output"
    "${CMAKE_CURRENT_BINARY_DIR}/surface-output"
    "${CMAKE_CURRENT_BINARY_DIR}/metal-energy-output"
    "${CMAKE_CURRENT_BINARY_DIR}/coat-output"
    "${CMAKE_CURRENT_BINARY_DIR}/materials-output"
    "${CMAKE_CURRENT_BINARY_DIR}/inspect-output"
    "${CMAKE_CURRENT_BINARY_DIR}/region-colors-output"
    "${CMAKE_CURRENT_BINARY_DIR}/view-output"
    "${CMAKE_CURRENT_BINARY_DIR}/frames-output" "${CMAKE_CURRENT_BINARY_DIR}/selection-output"
    "${CMAKE_CURRENT_BINARY_DIR}/lighting-output" "${CMAKE_CURRENT_BINARY_DIR}/image-output" "${CMAKE_CURRENT_BINARY_DIR}/settings-output"
    "${CMAKE_CURRENT_BINARY_DIR}/environment-output"
    "${CMAKE_CURRENT_BINARY_DIR}/environment-map-output"
    "${CMAKE_CURRENT_BINARY_DIR}/framebuffer-output"
    "${CMAKE_CURRENT_BINARY_DIR}/progress-output" "${CMAKE_CURRENT_BINARY_DIR}/cancel-output"
    "${CMAKE_CURRENT_BINARY_DIR}/sampling-output" "${CMAKE_CURRENT_BINARY_DIR}/bounces-output"
    "${CMAKE_CURRENT_BINARY_DIR}/aovs-output" "${CMAKE_CURRENT_BINARY_DIR}/npr-output" "${CMAKE_CURRENT_BINARY_DIR}/exposure-output")
endif()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
