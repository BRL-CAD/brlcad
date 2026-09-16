brlcad_addexec(assert_region_materials assert_region_materials.cpp "librt;libbu" TEST_USESDATA)

set(TSCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/regress-gcv-material-defaults.cmake.in")
set(EXPECTED_MATERIAL 0)
set(MIN_REGION_COUNT 1)

set(TARGET_NAME regress-gcv-stl-material-defaults)
set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../stl/tetra.stl")
set(INPUT_FORMAT "stl")
set(OUTPUT_G "${CMAKE_CURRENT_BINARY_DIR}/tetra-stl.g")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.log")
brlcad_regression_test(${TARGET_NAME} "gcv;gcv-stl;assert_region_materials" TEST_SCRIPT "${TSCRIPT}" EXEC gcv VEXEC assert_region_materials)
distclean("${OUTPUT_G}" "${LOG_FILE}")

set(TARGET_NAME regress-gcv-ply-material-defaults)
set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../ply/tetra.ply")
set(INPUT_FORMAT "ply")
set(OUTPUT_G "${CMAKE_CURRENT_BINARY_DIR}/tetra-ply.g")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.log")
brlcad_regression_test(${TARGET_NAME} "gcv;gcv-ply;assert_region_materials" TEST_SCRIPT "${TSCRIPT}" EXEC gcv VEXEC assert_region_materials)
distclean("${OUTPUT_G}" "${LOG_FILE}")

set(TARGET_NAME regress-gcv-vrml-material-defaults)
set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../vrml/vrml_example.wrl")
set(INPUT_FORMAT "vrml")
set(OUTPUT_G "${CMAKE_CURRENT_BINARY_DIR}/vrml.g")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.log")
brlcad_regression_test(${TARGET_NAME} "gcv;gcv-vrml;assert_region_materials" TEST_SCRIPT "${TSCRIPT}" EXEC gcv VEXEC assert_region_materials)
distclean("${OUTPUT_G}" "${LOG_FILE}")

set(TARGET_NAME regress-gcv-vrml1-material-defaults)
set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../vrml/vrml1_example.wrl")
set(INPUT_FORMAT "vrml")
set(OUTPUT_G "${CMAKE_CURRENT_BINARY_DIR}/vrml1.g")
set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.log")
brlcad_regression_test(${TARGET_NAME} "gcv;gcv-vrml;assert_region_materials" TEST_SCRIPT "${TSCRIPT}" EXEC gcv VEXEC assert_region_materials)
distclean("${OUTPUT_G}" "${LOG_FILE}")

if(TARGET gcv-assetimport)
  set(TARGET_NAME regress-gcv-gltf-material-defaults)
  set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../gltf/tetra-gltf2.0.gltf")
  set(INPUT_FORMAT "assetimport")
  set(OUTPUT_G "${CMAKE_CURRENT_BINARY_DIR}/tetra-gltf.g")
  set(LOG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.log")
  brlcad_regression_test(${TARGET_NAME} "gcv;gcv-assetimport;assert_region_materials" TEST_SCRIPT "${TSCRIPT}" EXEC gcv VEXEC assert_region_materials)
  distclean("${OUTPUT_G}" "${LOG_FILE}")
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
  assert_region_materials.cpp
  regress-gcv-material-defaults.cmake.in
)
