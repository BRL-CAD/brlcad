include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

set(QGMODEL_SRCS qgmodel.cpp)
set(QGVIEW_SRCS qgview.cpp)
set(QSKETCH_SRCS qsketch.cpp)
set(TEST_QTCAD_SESSION_SRCS test_qtcad_session.cpp)
set(TEST_QTCAD_CONSOLE_SRCS test_qtcad_console.cpp)
set(TEST_QTCAD_PLUGIN_MANAGER_SRCS test_qtcad_plugin_manager.cpp)
set(TEST_QTCAD_PLUGIN_BASE_HDRS test_qtcad_plugin_base.h)
set(TEST_QTCAD_PLUGIN_VALID_SRCS test_qtcad_plugin_valid.cpp)
set(TEST_QTCAD_PLUGIN_MISSING_API_SRCS test_qtcad_plugin_missing_api.cpp)
set(TEST_QTCAD_PLUGIN_METADATA
  test_qtcad_plugin_valid.json
  test_qtcad_plugin_missing_api.json
)

set(QTCAD_TEST_PLUGIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/plugins)
if(TARGET Qt6::Test)
  set(QTCAD_TEST_QT_CORE Qt6::Core)
  set(QTCAD_TEST_QT_TEST Qt6::Test)
elseif(TARGET Qt5::Test)
  set(QTCAD_TEST_QT_CORE Qt5::Core)
  set(QTCAD_TEST_QT_TEST Qt5::Test)
endif()

if(QTCAD_TEST_QT_TEST)
  add_library(qtcad_test_plugin_valid MODULE
    ${TEST_QTCAD_PLUGIN_VALID_SRCS}
    ${TEST_QTCAD_PLUGIN_BASE_HDRS}
  )
  add_library(qtcad_test_plugin_missing_api MODULE
    ${TEST_QTCAD_PLUGIN_MISSING_API_SRCS}
    ${TEST_QTCAD_PLUGIN_BASE_HDRS}
  )
  foreach(plugin_target qtcad_test_plugin_valid qtcad_test_plugin_missing_api)
    set_target_properties(${plugin_target} PROPERTIES
      AUTOMOC ON
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY ${QTCAD_TEST_PLUGIN_DIR}
      RUNTIME_OUTPUT_DIRECTORY ${QTCAD_TEST_PLUGIN_DIR}
    )
    target_compile_definitions(${plugin_target} PRIVATE BRLCADBUILD HAVE_CONFIG_H)
    target_link_libraries(${plugin_target} libqtcad ${QTCAD_TEST_QT_CORE})
  endforeach()

  add_executable(test_qtcad_plugin_manager ${TEST_QTCAD_PLUGIN_MANAGER_SRCS})
  set_target_properties(test_qtcad_plugin_manager PROPERTIES AUTOMOC ON)
  target_compile_definitions(test_qtcad_plugin_manager PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  target_link_libraries(test_qtcad_plugin_manager
    libqtcad
    ${QTCAD_TEST_QT_CORE}
    ${QTCAD_TEST_QT_TEST}
  )
  add_dependencies(test_qtcad_plugin_manager
    qtcad_test_plugin_valid
    qtcad_test_plugin_missing_api
  )
  brlcad_add_test(NAME test_qtcad_plugin_manager COMMAND test_qtcad_plugin_manager)
  set_property(TEST test_qtcad_plugin_manager APPEND PROPERTY ENVIRONMENT
    QTCAD_TEST_PLUGIN_DIR=${QTCAD_TEST_PLUGIN_DIR}
  )
endif()

if(TARGET Qt6::Test)
  add_executable(test_qtcad_console ${TEST_QTCAD_CONSOLE_SRCS})
  target_link_libraries(test_qtcad_console libqtcad Qt6::Test Qt6::Widgets)
elseif(TARGET Qt5::Test)
  add_executable(test_qtcad_console ${TEST_QTCAD_CONSOLE_SRCS})
  target_link_libraries(test_qtcad_console libqtcad Qt5::Test Qt5::Widgets)
endif()
if(TARGET test_qtcad_console)
  set_target_properties(test_qtcad_console PROPERTIES AUTOMOC ON)
  target_compile_definitions(test_qtcad_console PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  add_dependencies(test_qtcad_console ged_plugins)
  brlcad_add_test(NAME test_qtcad_console COMMAND test_qtcad_console)
endif()

if(TARGET Qt6::Test)
  add_executable(test_qtcad_session ${TEST_QTCAD_SESSION_SRCS})
  target_link_libraries(test_qtcad_session libqtcad Qt6::Test)
elseif(TARGET Qt5::Test)
  add_executable(test_qtcad_session ${TEST_QTCAD_SESSION_SRCS})
  target_link_libraries(test_qtcad_session libqtcad Qt5::Test)
endif()
if(TARGET test_qtcad_session)
  set_target_properties(test_qtcad_session PROPERTIES AUTOMOC ON)
  target_compile_definitions(test_qtcad_session PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  add_dependencies(test_qtcad_session ged_plugins)
  brlcad_add_test(NAME test_qtcad_session COMMAND test_qtcad_session)
endif()

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
endif(TARGET Qt5::Test)
if(TARGET qgmodel)
  target_compile_definitions(qgmodel PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qgmodel RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif(TARGET qgmodel)

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

# qsketch - standalone .g sketch editing demo
# qsketch.cpp defines QSketchEditWindow inline and uses #include "qsketch.moc"
# at the bottom, so we need AUTOMOC ON rather than qt6_wrap_cpp.
if(TARGET Qt6::Widgets)
  add_executable(qsketch ${QSKETCH_SRCS})
  set_target_properties(qsketch PROPERTIES AUTOMOC ON)
  target_link_libraries(
    qsketch
    libqtcad
    librt
    libbu
    Qt6::Widgets
  )
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

if(TARGET libqtcad)
  brlcad_addexec(qgmodel_db_update qgmodel_db_update.cpp "libqtcad" TEST)
  if(TARGET qgmodel_db_update)
    add_dependencies(qgmodel_db_update ged_plugins)
  endif()
  brlcad_add_test(NAME qgmodel_db_update COMMAND qgmodel_db_update)
endif()

# On some platforms distclean has additional cleanup to do
distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/qgmodel.pdb")
distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/qgview.pdb")
distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/qsketch.pdb")

cmakefiles(
  CMakeLists.txt
  qgmodel_db_update.cpp
  ${QGMODEL_SRCS}
  ${QGVIEW_SRCS}
  ${QSKETCH_SRCS}
  ${TEST_QTCAD_SESSION_SRCS}
  ${TEST_QTCAD_CONSOLE_SRCS}
  ${TEST_QTCAD_PLUGIN_MANAGER_SRCS}
  ${TEST_QTCAD_PLUGIN_BASE_HDRS}
  ${TEST_QTCAD_PLUGIN_VALID_SRCS}
  ${TEST_QTCAD_PLUGIN_MISSING_API_SRCS}
  ${TEST_QTCAD_PLUGIN_METADATA}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

