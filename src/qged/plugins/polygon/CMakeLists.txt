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

set(
  polygon_create_srcs
  polygon_create_tool.cpp
  polygon_create.json
  QPolyCreate.cpp
  QPolySettings.cpp
)
set(
  polygon_modify_srcs
  polygon_modify_tool.cpp
  polygon_modify.json
  QPolyMod.cpp
  QPolySettings.cpp
)
set(polygon_create_moc_hdrs QPolyCreate.h QPolySettings.h)
set(polygon_modify_moc_hdrs QPolyMod.h QPolySettings.h)

if(BRLCAD_ENABLE_QT)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(polygon_create_moc_srcs ${polygon_create_moc_hdrs})
    qt6_wrap_cpp(polygon_modify_moc_srcs ${polygon_modify_moc_hdrs})
    qt6_add_resources(icon_qrc_create icon.qrc)
    qt6_add_resources(icon_qrc_modify icon.qrc)
    set(polygon_qt_widgets Qt6::Widgets)
  else()
    qt5_wrap_cpp(polygon_create_moc_srcs ${polygon_create_moc_hdrs})
    qt5_wrap_cpp(polygon_modify_moc_srcs ${polygon_modify_moc_hdrs})
    qt5_add_resources(icon_qrc_create icon.qrc)
    qt5_add_resources(icon_qrc_modify icon.qrc)
    set(polygon_qt_widgets Qt5::Widgets)
  endif(Qt6Widgets_FOUND)

  qged_plugin(
    qged_polygon_create
    SHARED
    ${polygon_create_srcs}
    ${polygon_create_moc_srcs}
    ${icon_qrc_create}
  )
  qged_plugin(
    qged_polygon_modify
    SHARED
    ${polygon_modify_srcs}
    ${polygon_modify_moc_srcs}
    ${icon_qrc_modify}
  )

  foreach(polygon_target qged_polygon_create qged_polygon_modify)
    set_target_properties(${polygon_target} PROPERTIES AUTOMOC ON)
    target_link_libraries(
      ${polygon_target}
      libqtcad
      libged
      librt
      libbu
      ${polygon_qt_widgets}
    )
    set_property(
      TARGET ${polygon_target}
      APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H
    )
    plugin_setup(${polygon_target} qged-qt)
  endforeach()

  validate_style(
    qged_polygon_create
    polygon_create_tool.cpp
    QPolyCreate.cpp
    QPolySettings.cpp
  )
  validate_style(
    qged_polygon_modify
    polygon_modify_tool.cpp
    QPolyMod.cpp
    QPolySettings.cpp
  )

  distclean(
    icon.qrc.depends
    moc_QPolyCreate.cpp_parameters
    moc_QPolyMod.cpp_parameters
    moc_QPolySettings.cpp_parameters
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_polygon_create.exp"
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_polygon_create.pdb"
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_polygon_modify.exp"
    "${CMAKE_BINARY_DIR}/${LIBEXEC_DIR}/qged-qt/qged_polygon_modify.pdb"
  )
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
