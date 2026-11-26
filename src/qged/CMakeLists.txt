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
  QgHistoryDialog.cpp
  QgEdApp.cpp
  QgEdFilter.cpp
  fbserv.cpp
  QgEdPalette.cpp
)

set(
  qth_names
  QgEdApp
  QgEdFilter
  fbserv
  QgEdMainWindow
  QgEdPalette
)
foreach(qh ${qth_names})
  set(moc_headers ${moc_headers} ${qh}.h)
endforeach(qh ${qth_names})

if(BRLCAD_ENABLE_QT)
  # Let the plugin lookup know what the platform's extension is so it knows
  # what plugin filenames to look for
  set_property(
    SOURCE QgEdPalette.cpp
    APPEND
    PROPERTY COMPILE_DEFINITIONS "QGED_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\""
  )

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

  add_library(qged_ui OBJECT ${qged_srcs} ${moc_srcs} ${theme_qrc})
  set_property(TARGET qged_ui PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  target_compile_definitions(qged_ui PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  if(Qt6Widgets_FOUND)
    set(qged_qt Qt6)
  else()
    set(qged_qt Qt5)
  endif()
  # Network is needed for the asynchronous raytracing display.
  target_link_libraries(qged_ui PUBLIC libqtcad libged librt libbu ${qged_qt}::Widgets ${qged_qt}::Network)

  add_executable(qged WIN32 main.cpp)
  set_property(TARGET qged PROPERTY POSITION_INDEPENDENT_CODE TRUE)
  if(QGED_LINK_PIE_SUPPORTED)
    target_link_options(qged PRIVATE "-pie")
  endif()
  target_link_libraries(qged PRIVATE qged_ui)
  target_compile_definitions(qged PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qged RUNTIME DESTINATION ${BIN_DIR} LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})

  # Make sure all dm and ged plugins are accounted for when we build qged
  add_dependencies(qged dm_plugins ged_plugins)

  if(BUILD_TESTING AND TARGET dm-swrast)
    brlcad_addexec(qged_history_test history_test.cpp qged_ui TEST)
    add_dependencies(qged_history_test dm_plugins ged_plugins)
    brlcad_add_test(NAME qged_history COMMAND qged_history_test)
    set_tests_properties(qged_history PROPERTIES
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;BRLCAD_ROOT=${CMAKE_BINARY_DIR}"
      TIMEOUT 30
    )
  endif()

  foreach(qh ${qth_names})
    distclean(moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})
  distclean(${CMAKE_CURRENT_BINARY_DIR}/theme/breeze.qrc.depends)

  # On some platforms distclean has additional cleanup to do
  distclean("${CMAKE_BINARY_DIR}/${BIN_DIR}/qged.pdb")

endif(BRLCAD_ENABLE_QT)

# Core application is defined, now handle plugins
add_subdirectory(plugins)

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
  history_test.cpp
  qged_signals.eps
  qged_signals.dia
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
