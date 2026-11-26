if(TARGET gcv-assetimport)
  gcv_regress_util("dae" "tetra.dae" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
  gcv_regress_util("dae" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "4117" EXPORTER)
endif(TARGET gcv-assetimport)

if(TARGET gcv AND TARGET gcv-assetimport AND TARGET mged)
  brlcad_add_test(
    NAME regress-assetimport-scene
    COMMAND ${CMAKE_COMMAND}
      "-DGCV=$<TARGET_FILE:gcv>"
      "-DMGED=$<TARGET_FILE:mged>"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/nested-scene.dae"
      "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/assetimport-scene.g"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/check_scene.cmake"
  )
  brlcad_regression_test(regress-assetimport-scene "gcv;gcv-assetimport;mged" TEST_DEFINED)
  distclean("${CMAKE_CURRENT_BINARY_DIR}/assetimport-scene.g")
endif()

cmakefiles(
  CMakeLists.txt
  check_scene.cmake
  nested-scene.dae
  tetra.dae
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
