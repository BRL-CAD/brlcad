if(TARGET gcv-assetimport)
  gcv_regress_util("x3d" "tetra.x3d" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "792")
  gcv_regress_util("x3d_nurbs" "nurbs_patch.x3d" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "10672")
  gcv_regress_util("x3d" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1223" EXPORTER)
endif(TARGET gcv-assetimport)

if(TARGET g-x3d AND TARGET moss.g)
  brlcad_add_test(
    NAME regress-x3d-regions
    COMMAND ${CMAKE_COMMAND}
      "-DX3D_EXEC=$<TARGET_FILE:g-x3d>"
      "-DINPUT=${CMAKE_BINARY_DIR}/${DATA_DIR}/db/moss.g"
      "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/x3d_regions.cmake"
  )
  brlcad_regression_test(regress-x3d-regions "g-x3d;moss.g" TEST_DEFINED)
  set(x3d_region_outputs
    "${CMAKE_CURRENT_BINARY_DIR}/regions-group.x3d"
    "${CMAKE_CURRENT_BINARY_DIR}/regions-list.x3d"
  )
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${x3d_region_outputs}")
  distclean(${x3d_region_outputs})
endif()

cmakefiles(
  CMakeLists.txt
  nurbs_patch.x3d
  tetra.x3d
  x3d_regions.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
