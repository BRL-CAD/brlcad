include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(QGMODEL_SRCS qgmodel.cpp)
set(QGVIEW_SRCS qgview.cpp)
set(QSKETCH_SRCS qsketch.cpp)
set(TEST_QTCAD_PLUGIN_REUSE_SRCS test_qtcad_plugin_reuse.cpp)
set(TEST_QGMODEL_MODEL_TESTER_SRCS test_qgmodel_model_tester.cpp)
set(TEST_QGMODEL_SCALE_SRCS test_qgmodel_scale.cpp)

if(TARGET Qt6::Test)
  add_executable(qgmodel qgmodel.cpp)
  target_link_libraries(
    qgmodel
    libqtcad
    libged
    librt
    libbu
    Qt6::Widgets
    Qt6::Test
    )

  add_executable(test_qgmodel_model_tester ${TEST_QGMODEL_MODEL_TESTER_SRCS})
  target_link_libraries(
    test_qgmodel_model_tester
    libqtcad
    libged
    librt
    libbu
    Qt6::Widgets
    Qt6::Test
    )

  add_executable(test_qgmodel_scale ${TEST_QGMODEL_SCALE_SRCS})
  target_link_libraries(
    test_qgmodel_scale
    libqtcad
    libged
    librt
    libbu
    Qt6::Widgets
    Qt6::Test
    )
endif(TARGET Qt6::Test)
if(TARGET Qt5::Test)
  add_executable(qgmodel ${QGMODEL_SRCS})
  target_link_libraries(
    qgmodel
    libqtcad
    libged
    librt
    libbu
    Qt5::Widgets
    Qt5::Test
    )

  add_executable(test_qgmodel_model_tester ${TEST_QGMODEL_MODEL_TESTER_SRCS})
  target_link_libraries(
    test_qgmodel_model_tester
    libqtcad
    libged
    librt
    libbu
    Qt5::Widgets
    Qt5::Test
    )

  add_executable(test_qgmodel_scale ${TEST_QGMODEL_SCALE_SRCS})
  target_link_libraries(
    test_qgmodel_scale
    libqtcad
    libged
    librt
    libbu
    Qt5::Widgets
    Qt5::Test
    )
endif(TARGET Qt5::Test)
if(TARGET qgmodel)
  target_compile_definitions(qgmodel PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgmodel RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET qgmodel)
if(TARGET test_qgmodel_model_tester)
  target_compile_definitions(test_qgmodel_model_tester PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS test_qgmodel_model_tester RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET test_qgmodel_model_tester)
if(TARGET test_qgmodel_scale)
  target_compile_definitions(test_qgmodel_scale PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS test_qgmodel_scale RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET test_qgmodel_scale)

if(TARGET Qt6::Widgets)
  add_executable(qgview ${QGVIEW_SRCS})
  target_link_libraries(
    qgview
    libqtcad
    libged
    librt
    libbu
    Qt6::Widgets
    )
endif(TARGET Qt6::Widgets)
if(TARGET Qt5::Widgets)
  add_executable(qgview ${QGVIEW_SRCS})
  target_link_libraries(
    qgview
    libqtcad
    libged
    librt
    libbu
    Qt5::Widgets
    )
endif(TARGET Qt5::Widgets)
if(TARGET qgview)
  target_compile_definitions(qgview PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgview RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET qgview)

# Qt / QgSW headless integration test (Phase 4 BSG + dm-swrast via Qt widget path).
# Uses Qt's built-in offscreen platform — no X11 display required.
if(TARGET Qt6::Widgets)
  set(GED_TEST_QGED_SWRAST_DATA_DIR "${CMAKE_SOURCE_DIR}/src/libged/tests/draw")
  set(QGMODEL_TEST_DATA_G "${CMAKE_SOURCE_DIR}/src/libged/tests/draw/moss.g")
  add_executable(ged_test_qged_swrast ged_test_qged_swrast.cpp)
  target_include_directories(ged_test_qged_swrast PRIVATE
    "${CMAKE_SOURCE_DIR}/src/libged"
    )
  target_link_libraries(
    ged_test_qged_swrast
    libqtcad
    libged
    libdm
    libbsg
    librt
    libbu
    Qt6::Widgets
    )
  # qsketch - standalone .g sketch editing demo
  # qsketch.cpp defines QSketchEditWindow inline and uses #include "qsketch.moc"
  # at the bottom, so we need AUTOMOC ON rather than qt6_wrap_cpp.
  add_executable(qsketch ${QSKETCH_SRCS})
  set_target_properties(qsketch PROPERTIES AUTOMOC ON)
  target_link_libraries(
    qsketch
    libqtcad
    librt
    libbu
    Qt6::Widgets
    )

  target_compile_definitions(ged_test_qged_swrast PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  if(TARGET dm-swrast)
    add_dependencies(ged_test_qged_swrast dm-swrast)
  endif()
  brlcad_add_test(NAME ged_test_qged_swrast
    COMMAND ged_test_qged_swrast -c "${GED_TEST_QGED_SWRAST_DATA_DIR}")
  set_property(TEST ged_test_qged_swrast
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
  set_property(TEST ged_test_qged_swrast
    APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
  set_property(TEST ged_test_qged_swrast
    APPEND PROPERTY LABELS drawing_baseline)
  distclean(${CMAKE_CURRENT_BINARY_DIR}/moss_qgswrast_tmp.g)
  distclean(${CMAKE_CURRENT_BINARY_DIR}/qgswrast_cache)

  # Phase 8: standalone libqtcad plugin reuse test.
  # Deliberately links only libqtcad + Qt6::Widgets (NOT libged/qged) to
  # prove the plugin framework is host-agnostic.
  add_executable(test_qtcad_plugin_reuse ${TEST_QTCAD_PLUGIN_REUSE_SRCS})
  target_link_libraries(
    test_qtcad_plugin_reuse
    libqtcad
    libbu
    Qt6::Widgets
    )
  target_compile_definitions(test_qtcad_plugin_reuse PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  # The test scans the qged plugin directory at runtime; add dependencies on
  # the Phase 7 extension plugins so they are built before the test runs.
  if(TARGET qged_host_status_command)
    add_dependencies(test_qtcad_plugin_reuse
      qged_host_status_command
      qged_host_status_panel
      qged_host_status_dialog
    )
  endif()
  if(TARGET qged_ell)
    add_dependencies(test_qtcad_plugin_reuse qged_ell)
  endif()
  brlcad_add_test(NAME test_qtcad_plugin_reuse
    COMMAND test_qtcad_plugin_reuse "${CMAKE_BINARY_DIR}/libexec/qged")
  set_property(TEST test_qtcad_plugin_reuse
    APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
  set_property(TEST test_qtcad_plugin_reuse
    APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
  set_property(TEST test_qtcad_plugin_reuse
    APPEND PROPERTY LABELS drawing_baseline)

  if(TARGET test_qgmodel_model_tester)
    brlcad_add_test(NAME test_qgmodel_model_tester
      COMMAND test_qgmodel_model_tester "${QGMODEL_TEST_DATA_G}")
    set_property(TEST test_qgmodel_model_tester
      APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
    set_property(TEST test_qgmodel_model_tester
      APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
    set_property(TEST test_qgmodel_model_tester
      APPEND PROPERTY LABELS drawing_baseline)
  endif()
  if(TARGET test_qgmodel_scale)
    brlcad_add_test(NAME test_qgmodel_scale
      COMMAND test_qgmodel_scale "${QGMODEL_TEST_DATA_G}")
    set_property(TEST test_qgmodel_scale
      APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
    set_property(TEST test_qgmodel_scale
      APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
    set_property(TEST test_qgmodel_scale
      APPEND PROPERTY LABELS drawing_baseline)
    brlcad_add_test(NAME slow-test_qgmodel_scale_16384
      COMMAND test_qgmodel_scale "${QGMODEL_TEST_DATA_G}")
    set_property(TEST slow-test_qgmodel_scale_16384
      PROPERTY SKIP_RETURN_CODE 123)
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY ENVIRONMENT BRLCAD_RUN_SLOW_TESTS=1)
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY ENVIRONMENT BRLCAD_QGMODEL_SCALE_CHILDREN=16384)
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY ENVIRONMENT BRLCAD_QGMODEL_DRAW_CHILDREN=64)
    set_property(TEST slow-test_qgmodel_scale_16384
      APPEND PROPERTY LABELS drawing_baseline)
  endif()
endif(TARGET Qt6::Widgets)

# Phase B: dm-swrast vs dm-qtgl backend benchmark.
# Reports per-frame timing and fps for each backend.  The dm-qtgl section
# requires hardware OpenGL and is skipped gracefully in headless environments.
# Exit status is always 0 — this is a benchmark, not a pass/fail test.
if(TARGET Qt6::Widgets)
# set(GED_TEST_DM_BACKEND_BENCH_DATA_DIR "${CMAKE_SOURCE_DIR}/src/libged/tests/draw")
# add_executable(ged_test_dm_backend_bench ged_test_dm_backend_bench.cpp)
# target_include_directories(ged_test_dm_backend_bench PRIVATE
#   "${CMAKE_SOURCE_DIR}/src/libged"
#   )
# set(_bench_libs libqtcad libged libdm libbsg librt libbu Qt6::Widgets)
# if(TARGET dm-swrast)
#   list(APPEND _bench_libs dm-swrast)
# endif()
# if(TARGET dm-qtgl)
#   list(APPEND _bench_libs dm-qtgl)
# endif()
# target_link_libraries(ged_test_dm_backend_bench ${_bench_libs})
# target_compile_definitions(ged_test_dm_backend_bench PRIVATE BRLCADBUILD HAVE_CONFIG_H)
# brlcad_add_test(NAME ged_test_dm_backend_bench
#   COMMAND ged_test_dm_backend_bench "${GED_TEST_DM_BACKEND_BENCH_DATA_DIR}")
# set_property(TEST ged_test_dm_backend_bench
#   APPEND PROPERTY ENVIRONMENT BRLCAD_ROOT=${CMAKE_BINARY_DIR})
# set_property(TEST ged_test_dm_backend_bench
#   APPEND PROPERTY ENVIRONMENT QT_QPA_PLATFORM=offscreen)
# distclean(${CMAKE_CURRENT_BINARY_DIR}/dmbench_swrast_tmp.g)
# distclean(${CMAKE_CURRENT_BINARY_DIR}/dmbench_qtgl_tmp.g)
# distclean(${CMAKE_CURRENT_BINARY_DIR}/dmbench_cache)
endif(TARGET Qt6::Widgets)

if(TARGET Qt5::Widgets AND NOT TARGET Qt6::Widgets)
  add_executable(qsketch ${QSKETCH_SRCS})
  set_target_properties(qsketch PROPERTIES AUTOMOC ON)
  target_link_libraries(
    qsketch
    libqtcad
    librt
    libbu
    Qt5::Widgets
    )
endif(TARGET Qt5::Widgets AND NOT TARGET Qt6::Widgets)
if(TARGET qsketch)
  target_compile_definitions(qsketch PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qsketch RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET qsketch)

cmakefiles(
  CMakeLists.txt
  ged_test_dm_backend_bench.cpp
  ged_test_qged_swrast.cpp
  ${QGMODEL_SRCS}
  ${QGVIEW_SRCS}
  ${QSKETCH_SRCS}
  ${TEST_QTCAD_PLUGIN_REUSE_SRCS}
  ${TEST_QGMODEL_MODEL_TESTER_SRCS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
