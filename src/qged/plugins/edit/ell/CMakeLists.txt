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

set(ell_srcs
  QEll.cpp
  ell_tool.cpp
  )

set(qth_names
  QEll
  )
foreach(qh ${qth_names})
  set(ell_moc_hdrs ${ell_moc_hdrs} ${qh}.h)
endforeach(qh ${qth_names})

if(BRLCAD_ENABLE_QT)

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(ell_moc_srcs ${ell_moc_hdrs})
    QT6_ADD_RESOURCES(icon_qrc icon.qrc)
  else()
    QT5_WRAP_CPP(ell_moc_srcs ${ell_moc_hdrs})
    QT5_ADD_RESOURCES(icon_qrc icon.qrc)
  endif(Qt6Widgets_FOUND)

  qged_plugin(qged_ell SHARED ${ell_srcs} ${ell_moc_srcs} ${icon_qrc})
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged_ell libqtcad libged librt libbu Qt6::Widgets)
  else()
    target_link_libraries(qged_ell libqtcad libged librt libbu Qt5::Widgets)
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_ell APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  VALIDATE_STYLE(qged_ell ell_tool.cpp)
  PLUGIN_SETUP(qged_ell qged)

  foreach(qh ${qth_names})
    DISTCLEAN(moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})
  DISTCLEAN(icon.qrc.depends)

endif(BRLCAD_ENABLE_QT)

set(ignore_srcs
  CMakeLists.txt
  README
  ${ell_srcs}
  ${ell_moc_hdrs}
  icon.qrc
  ell.svg
  )

CMAKEFILES(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

