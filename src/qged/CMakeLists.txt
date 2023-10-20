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
  QgEdMainWindow.cpp
  QgEdApp.cpp
  QgEdFilter.cpp
  fbserv.cpp
  QgEdPalette.cpp
  )

set(qth_names
  QgEdApp
  QgEdFilter
  fbserv
  QgEdMainWindow
  QgEdPalette
  )
foreach(qh ${qth_names})
  set(moc_headers ${moc_headers} ${qh}.h)
endforeach(qh ${qth_names})


if (BRLCAD_ENABLE_QT)

  # Let the plugin lookup know what the platform's extension is so it knows
  # what plugin filenames to look for
  set_property(SOURCE QgEdPalette.cpp APPEND PROPERTY
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

  add_executable(qged WIN32 main.cpp ${qged_srcs} ${moc_srcs} ${theme_qrc})
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

# Theme subdirectory handles file bookkeeping for CMake
add_subdirectory(theme)

set(qged_ignore_srcs
  ${qged_srcs}
  ${moc_headers}
  BUILDING_QT
  CMakeLists.txt
  TODO
  fonts/Inconsolata-LICENSE.txt
  fonts/Inconsolata.otf
  main.cpp
  )

CMAKEFILES(${qged_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8


