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

set(polygon_create_srcs polygon_create_tool.cpp QPolyCreate.cpp QPolySettings.cpp)
set(polygon_modify_srcs polygon_modify_tool.cpp QPolyMod.cpp QPolySettings.cpp)

set(polygon_create_moc_hdrs QPolyCreate.h QPolySettings.h PolygonCreateFactory.h)
set(polygon_modify_moc_hdrs QPolyMod.h QPolySettings.h PolygonModifyFactory.h)

if(BRLCAD_ENABLE_QT)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(polygon_create_moc_srcs ${polygon_create_moc_hdrs})
    qt6_wrap_cpp(polygon_modify_moc_srcs ${polygon_modify_moc_hdrs})
    qt6_add_resources(icon_qrc_create icon.qrc)
    qt6_add_resources(icon_qrc_modify icon.qrc)
  else()
    qt5_wrap_cpp(polygon_create_moc_srcs ${polygon_create_moc_hdrs})
    qt5_wrap_cpp(polygon_modify_moc_srcs ${polygon_modify_moc_hdrs})
    qt5_add_resources(icon_qrc_create icon.qrc)
    qt5_add_resources(icon_qrc_modify icon.qrc)
  endif(Qt6Widgets_FOUND)

  qged_plugin(qged_polygon_create SHARED ${polygon_create_srcs} ${polygon_create_moc_srcs} ${icon_qrc_create})
  qged_plugin(qged_polygon_modify SHARED ${polygon_modify_srcs} ${polygon_modify_moc_srcs} ${icon_qrc_modify})
  if(Qt6Widgets_FOUND)
    target_link_libraries(
      qged_polygon_create
      libqtcad
      libged
      librt
      libbu
      Qt6::Widgets
    )
    target_link_libraries(
      qged_polygon_modify
      libqtcad
      libged
      librt
      libbu
      Qt6::Widgets
    )
  else()
    target_link_libraries(
      qged_polygon_create
      libqtcad
      libged
      librt
      libbu
      Qt5::Widgets
    )
    target_link_libraries(
      qged_polygon_modify
      libqtcad
      libged
      librt
      libbu
      Qt5::Widgets
    )
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_polygon_create APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  set_property(TARGET qged_polygon_modify APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  validate_style(qged_polygon_create polygon_create_tool.cpp)
  validate_style(qged_polygon_modify polygon_modify_tool.cpp)
  plugin_setup(qged_polygon_create qged)
  plugin_setup(qged_polygon_modify qged)

  distclean(moc_QPolyCreate.cpp_parameters)
  distclean(moc_QPolyMod.cpp_parameters)
  distclean(moc_QPolySettings.cpp_parameters)
  distclean(moc_PolygonCreateFactory.cpp_parameters)
  distclean(moc_PolygonModifyFactory.cpp_parameters)
  distclean(icon.qrc.depends)
endif(BRLCAD_ENABLE_QT)

set(
  ignore_srcs
  CMakeLists.txt
  ${polygon_create_srcs}
  ${polygon_modify_srcs}
  ${polygon_create_moc_hdrs}
  ${polygon_modify_moc_hdrs}
  icon.qrc
  circle.svg
  ellipse.svg
  poly_create.svg
  poly_modify.svg
  polygon_create.json
  polygon_modify.json
  polygon.svg
  rectangle.svg
  square.svg
)

cmakefiles(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
