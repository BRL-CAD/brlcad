set(QG_DIRS
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${QTCAD_INCLUDE_DIRS}
  ${GED_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(QG_DIRS)

# TODO - we need to figure out how to use windeployqt and macdeployqt (and
# maybe something similar for Linux?) to make self-contained packages using Qt.
# Right now, qged won't work from the build dir on Windows because it can't
# find the Qt libraries, and it will have the same problem when bundled into
# the .exe and .msi installers.  Potentially helpful:
#
# https://stackoverflow.com/a/41199492/2037687
# https://github.com/probonopd/linuxdeployqt

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qged_srcs
  main_window.cpp
  app.cpp
  attributes.cpp
  event_filter.cpp
  fbserv.cpp
  import.cpp
  palettes.cpp
  )

set(qth_names
  app
  attributes
  event_filter
  fbserv
  import
  main_window
  palettes
  )
foreach(qh ${qth_names})
  set(moc_headers ${moc_headers} ${qh}.h)
endforeach(qh ${qth_names})


if (BRLCAD_ENABLE_QT)

  # Let main_window know what the platform's extension is so it knows
  # what plugin filenames to look for
  set_property(SOURCE main_window.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS "QGED_PLUGIN_SUFFIX=\"${CMAKE_SHARED_LIBRARY_SUFFIX}\"")

  if(Qt6Widgets_FOUND)
    QT6_WRAP_CPP(moc_srcs ${moc_headers})
  else()
    QT5_WRAP_CPP(moc_srcs ${moc_headers})
  endif(Qt6Widgets_FOUND)

  # https://github.com/Alexhuszagh/BreezeStyleSheets
  if(Qt6Widgets_FOUND)
    QT6_ADD_RESOURCES(theme_qrc theme/breeze.qrc)
  else()
    QT5_ADD_RESOURCES(theme_qrc theme/breeze.qrc)
  endif(Qt6Widgets_FOUND)

  add_executable(qged main.cpp ${qged_srcs} ${moc_srcs} ${theme_qrc})
  # Note - Network is needed for async embedded raytracing display
  if(Qt6Widgets_FOUND)
    target_link_libraries(qged libqtcad libged librt libbu Qt6::Widgets Qt6::Network)
  else()
    target_link_libraries(qged libqtcad libged librt libbu Qt5::Widgets Qt5::Network)
  endif(Qt6Widgets_FOUND)
  target_compile_definitions(qged PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qged
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})

  # Make sure all dm and ged plugins are accounted for when we build qged
  add_dependencies(qged dm_plugins ged_plugins)

  foreach(qh ${qth_names})
    DISTCLEAN(moc_${qh}.cpp_parameters)
  endforeach(qh ${qth_names})
  DISTCLEAN(${CMAKE_CURRENT_BINARY_DIR}/theme/breeze.qrc.depends)

endif (BRLCAD_ENABLE_QT)

# Core application is defined, now handle plugins
add_subdirectory(plugins)

set(qged_ignore_srcs
	${qged_srcs}
	${moc_headers}
	CMakeLists.txt
	TODO
	display/AxesRenderer.cpp
	display/AxesRenderer.h
	display/Camera.h
	display/Display.cpp
	display/Display.h
	display/DisplayManager.cpp
	display/DisplayManager.h
	display/GeometryRenderer.cpp
	display/GeometryRenderer.h
	display/OrthographicCamera.cpp
	display/OrthographicCamera.h
	display/README.txt
	display/Renderable.h
	fonts/Inconsolata-LICENSE.txt
	fonts/Inconsolata.otf
	main.cpp
	qdm_tests.txt
	theme/LICENSE.md
	theme/breeze.qrc
	theme/dark.qss
	theme/dark/branch_closed.svg
	theme/dark/branch_closed_hover.svg
	theme/dark/branch_end.svg
	theme/dark/branch_end_arrow.svg
	theme/dark/branch_more.svg
	theme/dark/branch_more_arrow.svg
	theme/dark/branch_open.svg
	theme/dark/branch_open_hover.svg
	theme/dark/calendar_next.svg
	theme/dark/calendar_previous.svg
	theme/dark/checkbox_checked.svg
	theme/dark/checkbox_checked_disabled.svg
	theme/dark/checkbox_indeterminate.svg
	theme/dark/checkbox_indeterminate_disabled.svg
	theme/dark/checkbox_unchecked.svg
	theme/dark/checkbox_unchecked_disabled.svg
	theme/dark/clear_text.svg
	theme/dark/close.svg
	theme/dark/close_hover.svg
	theme/dark/close_pressed.svg
	theme/dark/computer.svg
	theme/dark/desktop.svg
	theme/dark/dialog_cancel.svg
	theme/dark/dialog_close.svg
	theme/dark/dialog_discard.svg
	theme/dark/dialog_help.svg
	theme/dark/dialog_no.svg
	theme/dark/dialog_ok.svg
	theme/dark/dialog_open.svg
	theme/dark/dialog_reset.svg
	theme/dark/dialog_save.svg
	theme/dark/disc_drive.svg
	theme/dark/down_arrow.svg
	theme/dark/down_arrow_disabled.svg
	theme/dark/down_arrow_hover.svg
	theme/dark/file.svg
	theme/dark/file_dialog_contents.svg
	theme/dark/file_dialog_detailed.svg
	theme/dark/file_dialog_end.svg
	theme/dark/file_dialog_info.svg
	theme/dark/file_dialog_list.svg
	theme/dark/file_dialog_start.svg
	theme/dark/file_link.svg
	theme/dark/floppy_drive.svg
	theme/dark/folder.svg
	theme/dark/folder_link.svg
	theme/dark/folder_open.svg
	theme/dark/hard_drive.svg
	theme/dark/help.svg
	theme/dark/hmovetoolbar.svg
	theme/dark/home_directory.svg
	theme/dark/hseptoolbar.svg
	theme/dark/left_arrow.svg
	theme/dark/left_arrow_disabled.svg
	theme/dark/left_arrow_hover.svg
	theme/dark/maximize.svg
	theme/dark/menu.svg
	theme/dark/message_critical.svg
	theme/dark/message_information.svg
	theme/dark/message_question.svg
	theme/dark/message_warning.svg
	theme/dark/minimize.svg
	theme/dark/network_drive.svg
	theme/dark/radio_checked.svg
	theme/dark/radio_checked_disabled.svg
	theme/dark/radio_unchecked.svg
	theme/dark/radio_unchecked_disabled.svg
	theme/dark/restore.svg
	theme/dark/right_arrow.svg
	theme/dark/right_arrow_disabled.svg
	theme/dark/right_arrow_hover.svg
	theme/dark/shade.svg
	theme/dark/sizegrip.svg
	theme/dark/transparent.svg
	theme/dark/trash.svg
	theme/dark/undock.svg
	theme/dark/undock_hover.svg
	theme/dark/undock_hover_pressed.svg
	theme/dark/unshade.svg
	theme/dark/up_arrow.svg
	theme/dark/up_arrow_disabled.svg
	theme/dark/up_arrow_hover.svg
	theme/dark/vline.svg
	theme/dark/vmovetoolbar.svg
	theme/dark/vseptoolbar.svg
	theme/dark/window_close.svg
	)

CMAKEFILES(${qged_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


