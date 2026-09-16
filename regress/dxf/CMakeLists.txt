if(TARGET dxf-g AND TARGET g2asc)
  brlcad_add_test(
    NAME regress-dxf-face-indices
    COMMAND ${CMAKE_COMMAND}
      "-DDXF_G=$<TARGET_FILE:dxf-g>"
      "-DG2ASC=$<TARGET_FILE:g2asc>"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/face_indices.dxf"
      "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/face_indices.cmake"
  )
  brlcad_regression_test(regress-dxf-face-indices "dxf-g;g2asc" TEST_DEFINED)
endif()

set(dxf_outputs
  "${CMAKE_CURRENT_BINARY_DIR}/face_indices.g"
  "${CMAKE_CURRENT_BINARY_DIR}/face_indices.asc"
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${dxf_outputs}")
distclean(${dxf_outputs})

cmakefiles(
  CMakeLists.txt
  face_indices.cmake
  face_indices.dxf
)
