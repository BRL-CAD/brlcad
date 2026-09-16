if(TARGET nastran-g AND TARGET g2asc)
  brlcad_add_test(
    NAME regress-nastran-large-pids
    COMMAND ${CMAKE_COMMAND}
      "-DNASTRAN_G=$<TARGET_FILE:nastran-g>"
      "-DG2ASC=$<TARGET_FILE:g2asc>"
      "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/large_pids.bdf"
      "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/large_pids.cmake"
  )
  brlcad_regression_test(regress-nastran-large-pids "nastran-g;g2asc" TEST_DEFINED)
endif()

set(nastran_outputs
  "${CMAKE_CURRENT_BINARY_DIR}/large_pids.g"
  "${CMAKE_CURRENT_BINARY_DIR}/large_pids.asc"
)
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES "${nastran_outputs}")
distclean(${nastran_outputs})

cmakefiles(
  CMakeLists.txt
  large_pids.bdf
  large_pids.cmake
)
