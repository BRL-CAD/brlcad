set(building_srcs
  building_geometry.cpp
  building_spec.cpp
  mkbuilding.cpp
  )

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/building.schema.json)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/building.schema.json building_schema_json)
configure_file(building_schema.h.in ${CMAKE_CURRENT_BINARY_DIR}/building_schema.h @ONLY)

brlcad_addexec(mkbuilding "${building_srcs}" "libwdb;${M_LIBRARY}" FOLDER Shapes)
target_include_directories(mkbuilding PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

brlcad_add_test(
  NAME mkbuilding_native_validate
  COMMAND mkbuilding --validate-only --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/apartments.json
  )
brlcad_add_test(
  NAME mkbuilding_parts_validate
  COMMAND mkbuilding --validate-only --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/complex_parts.json
  )
brlcad_add_test(
  NAME mkbuilding_osm_validate
  COMMAND mkbuilding --validate-only --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/osm_feature.geojson
  )
brlcad_add_test(
  NAME mkbuilding_cityjson_validate
  COMMAND mkbuilding --validate-only --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/cityjson_building.json
  )
brlcad_add_test(
  NAME mkbuilding_osm_generate
  COMMAND mkbuilding --force --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/osm_feature.geojson -o ${CMAKE_CURRENT_BINARY_DIR}/mkbuilding-osm-test.g
  )
brlcad_add_test(
  NAME mkbuilding_cityjson_generate
  COMMAND mkbuilding --force --spec ${CMAKE_CURRENT_SOURCE_DIR}/examples/cityjson_building.json -o ${CMAKE_CURRENT_BINARY_DIR}/mkbuilding-cityjson-test.g
  )
brlcad_add_test(
  NAME mkbuilding_generate
  COMMAND mkbuilding --force --no-auto-openings -o ${CMAKE_CURRENT_BINARY_DIR}/mkbuilding-test.g
  )
set_tests_properties(mkbuilding_generate PROPERTIES FIXTURES_SETUP mkbuilding_house_database)
brlcad_add_test(
  NAME mkbuilding_gable_closure
  COMMAND ${CMAKE_COMMAND}
    -DNIRT_EXECUTABLE=$<TARGET_FILE:nirt>
    -DBUILDING_DATABASE=${CMAKE_CURRENT_BINARY_DIR}/mkbuilding-test.g
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/gable_closure.cmake
  )
set_tests_properties(mkbuilding_gable_closure PROPERTIES FIXTURES_REQUIRED mkbuilding_house_database)
brlcad_add_test(
  NAME mkbuilding_roof_support
  COMMAND ${CMAKE_COMMAND}
    -DMKBUILDING_EXECUTABLE=$<TARGET_FILE:mkbuilding>
    -DNIRT_EXECUTABLE=$<TARGET_FILE:nirt>
    -DOUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/roof_support.cmake
  )
brlcad_add_test(
  NAME mkbuilding_demo_unique_geometry
  COMMAND mkbuilding --force --demo-file ${CMAKE_CURRENT_BINARY_DIR}/mkbuilding-demo-test.g
  )
set_tests_properties(mkbuilding_demo_unique_geometry PROPERTIES PASS_REGULAR_EXPRESSION "Generated 51 building\\(s\\)")

cmakefiles(
  CMakeLists.txt
  building.h
  building.schema.json
  building_schema.h.in
  building_geometry.cpp
  building_spec.cpp
  examples/apartments.json
  examples/cityjson_building.json
  examples/complex_parts.json
  examples/osm_feature.geojson
  mkbuilding.cpp
  tests/gable_closure.cmake
  tests/roof_support.cmake
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
