if(BRLCAD_ENABLE_QT)

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

endif(BRLCAD_ENABLE_QT)

set(view_settings_srcs
  view_settings.cpp
  view_widget.cpp
  )

set(view_settings_moc_hdrs
  view_widget.h
  )

if(BRLCAD_ENABLE_QT)

  QT5_WRAP_CPP(view_settings_moc_srcs ${view_settings_moc_hdrs})
  QT5_ADD_RESOURCES(icon_qrc icon.qrc)

  qged_plugin(qged_view_settings SHARED ${view_settings_srcs} ${view_settings_moc_srcs} ${icon_qrc})
  target_link_libraries(qged_view_settings libqtcad libged librt libbu Qt5::Widgets)
  set_property(TARGET qged_view_settings APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_view_settings view_settings.cpp)
  PLUGIN_SETUP(qged_view_settings qged)

  DISTCLEAN(
    icon.qrc.depends
    moc_view_widget.cpp_parameters
    )

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${view_settings_srcs}
  ${view_settings_moc_hdrs}
  icon.qrc
  settings.svg
  README
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

