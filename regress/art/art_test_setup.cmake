unset(ART_OSLC_EXECUTABLE CACHE)
find_program(ART_OSLC_EXECUTABLE NAMES oslc
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
unset(ART_OSL_SHADER_INCLUDE_DIR CACHE)
find_path(ART_OSL_SHADER_INCLUDE_DIR stdosl.h
  HINTS "${renderer_prefix}/share/OSL/shaders" "${renderer_prefix}/shaders" NO_DEFAULT_PATH NO_CACHE REQUIRED)
unset(ART_OSL_HELPER_INCLUDE_DIR CACHE)
find_path(ART_OSL_HELPER_INCLUDE_DIR NAMES appleseed/as_osl_extensions.h
  HINTS "${renderer_prefix}/shaders/include" NO_DEFAULT_PATH NO_CACHE REQUIRED)
unset(ART_MAKETX_EXECUTABLE CACHE)
find_program(ART_MAKETX_EXECUTABLE NAMES maketx
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
unset(ART_APPLESEED_CLI_EXECUTABLE CACHE)
find_program(ART_APPLESEED_CLI_EXECUTABLE NAMES appleseed.cli
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
unset(ART_OIIOTOOL_EXECUTABLE CACHE)
find_program(ART_OIIOTOOL_EXECUTABLE NAMES oiiotool
  HINTS "${renderer_prefix}/bin" NO_DEFAULT_PATH NO_CACHE REQUIRED)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_caustic_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_caustic_test_commands.cmake" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_photon_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_photon_test_commands.cmake" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_area_exit_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_area_exit_test_commands.cmake" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_volume_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_volume_test_commands.cmake" @ONLY)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_volume_energy_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_volume_energy_test_commands.cmake" @ONLY)

# SDK helper includes are outside the source tree; track their updates too.
file(GLOB_RECURSE ART_OSL_HELPER_HEADERS CONFIGURE_DEPENDS
  "${ART_OSL_HELPER_INCLUDE_DIR}/appleseed/*.h")
set(ART_TEST_SHADERS)
foreach(shader IN LISTS ART_TEST_SHADER_NAMES)
  set(compiled "${CMAKE_CURRENT_BINARY_DIR}/${shader}.oso")
  add_custom_command(OUTPUT "${compiled}"
    COMMAND ${ART_TEST_TOOL_ENV} "${ART_OSLC_EXECUTABLE}" "-I${ART_OSL_SHADER_INCLUDE_DIR}"
      "-I${ART_OSL_HELPER_INCLUDE_DIR}"
      -o "${compiled}" "${CMAKE_CURRENT_SOURCE_DIR}/${shader}.osl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}.osl"
      "${ART_OSLC_EXECUTABLE}" "${ART_OSL_SHADER_INCLUDE_DIR}/stdosl.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/art_glass_reference.h"
      "${CMAKE_CURRENT_SOURCE_DIR}/art_beckmann_reference.h"
      ${ART_OSL_HELPER_HEADERS}
    VERBATIM)
  list(APPEND ART_TEST_SHADERS "${compiled}")
endforeach()
add_custom_target(art_test_shaders DEPENDS ${ART_TEST_SHADERS})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/art_volume_walk_test_commands.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/art_volume_walk_test_commands.cmake" @ONLY)
