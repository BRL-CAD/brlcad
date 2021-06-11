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

set(polygon_ellipse_srcs
  polygon_ellipse.cpp
  )

if(BRLCAD_ENABLE_QT)

  QT5_ADD_RESOURCES(icon_qrc icon.qrc)

  qged_plugin(qged_polygon_ellipse SHARED ${polygon_ellipse_srcs} ${icon_qrc})
  target_link_libraries(qged_polygon_ellipse libqtcad libged librt libbu Qt5::Widgets)
  set_property(TARGET qged_polygon_ellipse APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_polygon_ellipse polygon_ellipse.cpp)
  PLUGIN_SETUP(qged_polygon_ellipse qged)

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${polygon_ellipse_srcs}
  icon.qrc
  ellipse.svg
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

