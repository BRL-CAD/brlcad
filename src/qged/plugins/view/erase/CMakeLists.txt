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

set(erase_srcs
  erase.cpp
  widget.cpp
  )

set(erase_moc_hdrs
  widget.h
  )

if(BRLCAD_ENABLE_QT)

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(erase_moc_srcs ${erase_moc_hdrs})
    QT6_ADD_RESOURCES(icon_qrc icon.qrc)
  else()
    QT5_WRAP_CPP(erase_moc_srcs ${erase_moc_hdrs})
    QT5_ADD_RESOURCES(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)
  qged_plugin(qged_erase SHARED ${erase_srcs} ${erase_moc_srcs} ${icon_qrc})
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged_erase libqtcad libged librt libbu Qt6::Widgets)
  else()
    target_link_libraries(qged_erase libqtcad libged librt libbu Qt5::Widgets)
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_erase APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_erase erase.cpp)
  PLUGIN_SETUP(qged_erase qged)

  DISTCLEAN(
    icon.qrc.depends
    moc_widget.cpp_parameters
    )

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${erase_srcs}
  ${erase_moc_hdrs}
  icon.qrc
  eraser.svg
  README
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

