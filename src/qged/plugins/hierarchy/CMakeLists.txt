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


# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(hierarchy_srcs
  TreeView.cpp
  Hierarchy.cpp
  )

set(hierarchy_moc_headers
  TreeView.h
  Hierarchy.h
  )

if(BRLCAD_ENABLE_QT)

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(hierarchy_moc_srcs ${hierarchy_info_moc_headers})
    QT6_ADD_RESOURCES(icon_qrc icon.qrc)
  else()
    QT5_WRAP_CPP(hierarchy_moc_srcs ${hierarchy_moc_headers})
    QT5_ADD_RESOURCES(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)

  qged_plugin(qged_hierarchy SHARED ${hierarchy_srcs} ${hierarchy_moc_srcs} ${icon_qrc})
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged_hierarchy libqtcad libged librt libbu Qt6::Widgets)
  else()
    target_link_libraries(qged_hierarchy libqtcad libged librt libbu Qt5::Widgets)
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_hierarchy APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_hierarchy TreeView.cpp)
  PLUGIN_SETUP(qged_hierarchy qged)

  DISTCLEAN(
    icon.qrc.depends
    moc_view_model.cpp_parameters
    )

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  ${hierarchy_srcs}
  ${hierarchy_moc_headers}
  icon.qrc
  info.svg
  README
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

