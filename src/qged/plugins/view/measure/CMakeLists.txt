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

set(measure_srcs
  measure.cpp
  widget.cpp
  )

set(measure_moc_hdrs
  widget.h
  )

if(BRLCAD_ENABLE_QT)

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(measure_moc_srcs ${measure_moc_hdrs})
    QT6_ADD_RESOURCES(icon_qrc icon.qrc)
  else()
    QT5_WRAP_CPP(measure_moc_srcs ${measure_moc_hdrs})
    QT5_ADD_RESOURCES(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)
  qged_plugin(qged_measure SHARED ${measure_srcs} ${measure_moc_srcs} ${icon_qrc})
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged_measure libqtcad libged librt libbu Qt6::Widgets)
  else()
    target_link_libraries(qged_measure libqtcad libged librt libbu Qt5::Widgets)
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_measure APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_measure measure.cpp)
  PLUGIN_SETUP(qged_measure qged)

  DISTCLEAN(
    icon.qrc.depends
    moc_widget.cpp_parameters
    )

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${measure_srcs}
  ${measure_moc_hdrs}
  icon.qrc
  measure.svg
  README
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

