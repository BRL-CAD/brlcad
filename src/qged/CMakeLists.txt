if(BRLCAD_ENABLE_QT)
  find_package_qt(REQUIRED)
endif(BRLCAD_ENABLE_QT)

# TODO - need this now for code in lower level directories,
# need to do some cleanup.
include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(
  qged_srcs
  QgEdMainWindow.cpp
  QgEdApp.cpp
  QgEdCategories.h
  QgEdFilter.cpp
  fbserv.cpp
)

set(
  qth_names
  QgEdApp
  QgEdFilter
  fbserv
  QgEdMainWindow
)
foreach(qh ${qth_names})
  set(moc_headers ${moc_headers} ${qh}.h)
endforeach(qh ${qth_names})

if(BRLCAD_ENABLE_QT)
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-pie" QGED_LINK_PIE_SUPPORTED)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(moc_srcs ${moc_headers})
  else()
    qt5_wrap_cpp(moc_srcs ${moc_headers})
  endif(Qt6Widgets_FOUND)

  # https://github.com/Alexhuszagh/BreezeStyleSheets
  if(Qt6Widgets_FOUND)
    qt6_add_resources(theme_qrc theme/breeze.qrc)
  else()
    qt5_add_resources(theme_qrc theme/breeze.qrc)
  endif(Qt6Widgets_FOUND)

  add_executable(qged WIN32 main.cpp ${qged_srcs} ${moc_srcs} ${theme_qrc})
  # Note - Network is needed for async embedded raytracing display
  set_property(TARGET qged PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  if(QGED_LINK_PIE_SUPPORTED)
    target_link_options(qged PRIVATE "-pie")
  endif()
  if(Qt6Widgets_FOUND)
    target_link_libraries(
      qged
      libqtcad
      libged
      librt
      libbu
      Qt6::Widgets
      Qt6::Network
    )
  else()
    target_link_libraries(
      qged
      libqtcad
      libged
      librt
      libbu
      Qt5::Widgets
      Qt5::Network
    )
  endif(Qt6Widgets_FOUND)
  target_compile_definitions(qged PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qged RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})

  # Make sure all dm and ged plugins are accounted for when we build qged
  add_dependencies(qged dm_plugins ged_plugins)

  foreach(qh ${qth_names})
    distclean(moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})
  distclean(${CMAKE_CURRENT_BINARY_DIR}/theme/breeze.qrc.depends)

  # On some platforms distclean has additional cleanup to do
  distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/qged.pdb")

endif(BRLCAD_ENABLE_QT)

# Core application is defined, now handle plugins
add_subdirectory(plugins)

if(BRLCAD_ENABLE_QT AND BUILD_TESTING)
  if(TARGET Qt6::Test)
    set(QGED_TEST_QT_TEST Qt6::Test)
    set(QGED_TEST_QT_WIDGETS Qt6::Widgets)
  elseif(TARGET Qt5::Test)
    set(QGED_TEST_QT_TEST Qt5::Test)
    set(QGED_TEST_QT_WIDGETS Qt5::Widgets)
  endif()
  if(QGED_TEST_QT_TEST)
    add_executable(test_qged_native_plugins tests/test_qged_native_plugins.cpp)
    set_target_properties(test_qged_native_plugins PROPERTIES AUTOMOC ON)
    target_compile_definitions(test_qged_native_plugins
      PRIVATE BRLCADBUILD HAVE_CONFIG_H)
    target_link_libraries(test_qged_native_plugins
      libqtcad
      libbu
      ${QGED_TEST_QT_TEST}
      ${QGED_TEST_QT_WIDGETS}
    )
    add_dependencies(test_qged_native_plugins
      qged_view_info
      qged_view_settings
      qged_select
      qged_measure
      qged_polygon_create
      qged_polygon_modify
    )
    brlcad_add_test(NAME test_qged_native_plugins
      COMMAND test_qged_native_plugins)
    set_property(TEST test_qged_native_plugins APPEND PROPERTY ENVIRONMENT
      QGED_TEST_PLUGIN_DIR=${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt
      BRLCAD_ROOT=${CMAKE_BINARY_DIR}
      QT_QPA_PLATFORM=offscreen
    )
  endif()
endif()

# Theme subdirectory handles file bookkeeping for CMake
add_subdirectory(theme)

set(
  qged_ignore_srcs
  ${qged_srcs}
  ${moc_headers}
  BUILDING_QT
  CMakeLists.txt
  TODO
  fonts/Inconsolata-LICENSE.txt
  fonts/Inconsolata.otf
  main.cpp
  qged_signals.eps
  qged_signals.dia
  tests/test_qged_native_plugins.cpp
  view_update_cycle.odg
  view_update_cycle.pdf
)

cmakefiles(${qged_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
