if(TARGET gcv-assetimport)
  gcv_regress_util("gltf" "tetra-gltf2.0.gltf;tetra-glb2.0.glb" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
  gcv_regress_util("glb" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1476" EXPORTER)
  gcv_regress_util("gltf" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "3356" EXPORTER)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/conv-regress-g-gltf-tetra.bin")
endif(TARGET gcv-assetimport)

if(TARGET gcv-gltf)
  gcv_regress_util("gltf_native" "tetra-gltf2.0.gltf;tetra-glb2.0.glb" DEPS "gcv;gcv-gltf" GCV_OPTIONS "--input-format=gltf")
endif(TARGET gcv-gltf)

if(TARGET gltf-g AND TARGET gcv AND TARGET gcv-gltf AND TARGET mged)
  brlcad_add_test(
    NAME regress-gltf-scene
    COMMAND ${CMAKE_COMMAND}
      "-DGLTF_G=$<TARGET_FILE:gltf-g>"
      "-DGCV=$<TARGET_FILE:gcv>"
      "-DMGED=$<TARGET_FILE:mged>"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/instanced-scene.gltf"
      "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/gltf-scene"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_scene.cmake"
  )
  brlcad_regression_test(regress-gltf-scene "gltf-g;gcv;gcv-gltf;mged" TEST_DEFINED)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/gltf-scene")
endif()

if(TARGET gcv AND TARGET gcv-gltf AND TARGET rt)
  brlcad_add_test(
    NAME regress-gltf-texture
    COMMAND ${CMAKE_COMMAND}
      "-DGCV=$<TARGET_FILE:gcv>"
      "-DRT=$<TARGET_FILE:rt>"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/textured-quad.gltf"
      "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/gltf-texture"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_texture.cmake"
  )
  brlcad_regression_test(regress-gltf-texture "gcv;gcv-gltf;rt" TEST_DEFINED)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/gltf-texture")
endif()

cmakefiles(
  CMakeLists.txt
  check_scene.cmake
  check_texture.cmake
  instanced-scene.gltf
  textured-quad.gltf
  tetra-gltf2.0.gltf
  tetra-glb2.0.glb
  tetra.bin # support file for glb
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
