if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${QTCAD_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
  )
endif(BRLCAD_ENABLE_QT)

set(host_status_dialog_srcs
  host_status_dialog.cpp
  ../common/HostStatusCommon.cpp
)
set(host_status_dialog_moc_hdrs HostStatusDialogFactory.h)

if(BRLCAD_ENABLE_QT)
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(host_status_dialog_moc_srcs ${host_status_dialog_moc_hdrs})
  else()
    qt5_wrap_cpp(host_status_dialog_moc_srcs ${host_status_dialog_moc_hdrs})
  endif(Qt6Widgets_FOUND)

  qged_plugin(qged_host_status_dialog SHARED ${host_status_dialog_srcs} ${host_status_dialog_moc_srcs})
  if(Qt6Widgets_FOUND)
    target_link_libraries(
      qged_host_status_dialog
      libqtcad
      libged
      librt
      libbu
      Qt6::Widgets
    )
  else()
    target_link_libraries(
      qged_host_status_dialog
      libqtcad
      libged
      librt
      libbu
      Qt5::Widgets
    )
  endif(Qt6Widgets_FOUND)
  set_property(TARGET qged_host_status_dialog APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
  validate_style(qged_host_status_dialog host_status_dialog.cpp)
  plugin_setup(qged_host_status_dialog qged)

  distclean(moc_HostStatusDialogFactory.cpp_parameters)
endif(BRLCAD_ENABLE_QT)

set(
  ignore_srcs
  CMakeLists.txt
  ${host_status_dialog_srcs}
  ${host_status_dialog_moc_hdrs}
  host_status_dialog.json
)

cmakefiles(${ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
