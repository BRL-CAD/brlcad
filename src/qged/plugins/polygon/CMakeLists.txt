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

set(polygon_srcs
  polygon_tool.cpp
  QPolyCreate.cpp
  QPolyMod.cpp
  QPolySettings.cpp
  )

set(polygon_moc_hdrs
  QPolyCreate.h
  QPolyMod.h
  QPolySettings.h
  )

if(BRLCAD_ENABLE_QT)

  QT5_WRAP_CPP(polygon_moc_srcs ${polygon_moc_hdrs})
  QT5_ADD_RESOURCES(icon_qrc icon.qrc)

  qged_plugin(qged_polygon SHARED ${polygon_srcs} ${polygon_moc_srcs} ${icon_qrc})
  target_link_libraries(qged_polygon libqtcad libged librt libbu Qt5::Widgets)
  set_property(TARGET qged_polygon APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_polygon polygon_tool.cpp polygon_control.cpp)
  PLUGIN_SETUP(qged_polygon qged)

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${polygon_srcs}
  ${polygon_moc_hdrs}
  icon.qrc
  circle.svg
  ellipse.svg
  poly_create.svg
  poly_modify.svg
  polygon.svg
  rectangle.svg
  square.svg
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

