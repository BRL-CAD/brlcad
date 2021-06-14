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

set(polygon_circle_srcs
  polygon_circle.cpp
  polygon_circle_control.cpp
  )

set(polygon_circle_moc_hdrs
  polygon_circle_control.h
  )

if(BRLCAD_ENABLE_QT)

  QT5_WRAP_CPP(polygon_circle_moc_srcs ${polygon_circle_moc_hdrs})
  QT5_ADD_RESOURCES(icon_qrc icon.qrc)

  qged_plugin(qged_polygon_circle SHARED ${polygon_circle_srcs} ${polygon_circle_moc_srcs} ${icon_qrc})
  target_link_libraries(qged_polygon_circle libqtcad libged librt libbu Qt5::Widgets)
  set_property(TARGET qged_polygon_circle APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_polygon_circle polygon_circle.cpp)
  PLUGIN_SETUP(qged_polygon_circle qged)

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${polygon_circle_srcs}
  ${polygon_circle_moc_hdrs}
  icon.qrc
  circle.svg
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

