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

set(view_settings_srcs view_settings.cpp CADViewSettings.cpp view_settings.json)

set(view_settings_moc_hdrs CADViewSettings.h)

if(BRLCAD_ENABLE_QT)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(view_settings_moc_srcs ${view_settings_moc_hdrs})
    qt6_add_resources(icon_qrc icon.qrc)
  else()
    qt5_wrap_cpp(view_settings_moc_srcs ${view_settings_moc_hdrs})
    qt5_add_resources(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)
  qged_plugin(qged_view_settings SHARED ${view_settings_srcs} ${view_settings_moc_srcs} ${icon_qrc})
  set_target_properties(qged_view_settings PROPERTIES AUTOMOC ON)
  if(Qt6Widgets_FOUND)
    target_link_libraries(
      qged_view_settings
      libqtcad
      libged
      librt
      libbu
      Qt6::Widgets
    )
  else()
    target_link_libraries(
      qged_view_settings
      libqtcad
      libged
      librt
      libbu
      Qt5::Widgets
    )
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_view_settings APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  validate_style(qged_view_settings view_settings.cpp)
  plugin_setup(qged_view_settings qged-qt)

  distclean(
    icon.qrc.depends
    moc_CADViewSettings.cpp_parameters
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_view_settings.exp"
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_view_settings.pdb"
  )
endif(BRLCAD_ENABLE_QT)

set(
  ignore_srcs
  CMakeLists.txt
  ${view_settings_srcs}
  ${view_settings_moc_hdrs}
  icon.qrc
  settings.svg
  view_settings.json
  README
)

cmakefiles(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
