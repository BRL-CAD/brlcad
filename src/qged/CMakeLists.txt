include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${QTCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qged_srcs
  main_window.cpp
  app.cpp
  attributes.cpp
  event_filter.cpp
  fbserv.cpp
  import.cpp
  palettes.cpp
  )

set(qth_names
  app
  attributes
  event_filter
  fbserv
  import
  main_window
  palettes
  )
foreach(qh ${qth_names})
  set(moc_headers ${moc_headers} ${qh}.h)
endforeach(qh ${qth_names})


if (BRLCAD_ENABLE_QT)

  # Let main_window know what the platform's extension is so it knows
  # what plugin filenames to look for
  set_property(SOURCE main_window.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS "QGED_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(moc_srcs ${moc_headers})
  else()
    QT5_WRAP_CPP(moc_srcs ${moc_headers})
  endif(Qt6Widgets_FOUND)

  # https://github.com/Alexhuszagh/BreezeStyleSheets
  if(Qt6Widgets_FOUND)
    QT6_ADD_RESOURCES(theme_qrc theme/breeze.qrc)
  else()
    QT5_ADD_RESOURCES(theme_qrc theme/breeze.qrc)
  endif(Qt6Widgets_FOUND)

  add_executable(qged main.cpp ${qged_srcs} ${moc_srcs} ${theme_qrc})
  # Note - Network is needed for async embedded raytracing display
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged libqtcad libged librt libbu Qt6::Widgets Qt6::Network)
  else()
    target_link_libraries(qged libqtcad libged librt libbu Qt5::Widgets Qt5::Network)
  endif(Qt6Widgets_FOUND)
  target_compile_definitions(qged PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qged
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  # Make sure all dm and ged plugins are accounted for when we build qged
  add_dependencies(qged dm_plugins ged_plugins)

  foreach(qh ${qth_names})
    DISTCLEAN(moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/theme/breeze.qrc.depends)

endif (BRLCAD_ENABLE_QT)

# Core application is defined, now handle plugins
add_subdirectory(plugins)

set(qged_ignore_srcs
  ${qged_srcs}
  ${moc_headers}
  CMakeLists.txt
  TODO
  display/AxesRenderer.cpp
  display/AxesRenderer.h
  display/Camera.h
  display/Display.cpp
  display/Display.h
  display/DisplayManager.cpp
  display/DisplayManager.h
  display/GeometryRenderer.cpp
  display/GeometryRenderer.h
  display/OrthographicCamera.cpp
  display/OrthographicCamera.h
  display/README.txt
  display/Renderable.h
  fonts/Inconsolata-LICENSE.txt
  fonts/Inconsolata.otf
  main.cpp
  qdm_tests.txt
  theme/LICENSE.md
  theme/breeze.qrc
  theme/dark.qss
  theme/dark/branch_closed-on.svg
  theme/dark/branch_closed.svg
  theme/dark/branch_open-on.svg
  theme/dark/branch_open.svg
  theme/dark/checkbox_checked.svg
  theme/dark/checkbox_checked_disabled.svg
  theme/dark/checkbox_indeterminate.svg
  theme/dark/checkbox_indeterminate_disabled.svg
  theme/dark/checkbox_unchecked.svg
  theme/dark/checkbox_unchecked_disabled.svg
  theme/dark/close-hover.svg
  theme/dark/close-pressed.svg
  theme/dark/close.svg
  theme/dark/down_arrow-hover.svg
  theme/dark/down_arrow.svg
  theme/dark/down_arrow_disabled.svg
  theme/dark/hmovetoolbar.svg
  theme/dark/hsepartoolbar.svg
  theme/dark/left_arrow.svg
  theme/dark/left_arrow_disabled.svg
  theme/dark/radio_checked.svg
  theme/dark/radio_checked_disabled.svg
  theme/dark/radio_unchecked.svg
  theme/dark/radio_unchecked_disabled.svg
  theme/dark/right_arrow.svg
  theme/dark/right_arrow_disabled.svg
  theme/dark/sizegrip.svg
  theme/dark/spinup_disabled.svg
  theme/dark/stylesheet-branch-end-closed.svg
  theme/dark/stylesheet-branch-end-open.svg
  theme/dark/stylesheet-branch-end.svg
  theme/dark/stylesheet-branch-more.svg
  theme/dark/stylesheet-vline.svg
  theme/dark/transparent.svg
  theme/dark/undock-hover.svg
  theme/dark/undock.svg
  theme/dark/up_arrow-hover.svg
  theme/dark/up_arrow.svg
  theme/dark/up_arrow_disabled.svg
  theme/dark/vmovetoolbar.svg
  theme/dark/vsepartoolbars.svg
  )

CMAKEFILES(${qged_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

