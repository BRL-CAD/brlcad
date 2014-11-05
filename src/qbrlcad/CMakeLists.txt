include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
  ${BRLCAD_SOURCE_DIR}/include
  ${GED_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  ${BU_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  )

add_definitions(${Qt5Widgets_DEFINITIONS})

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qbrlcad_srcs
  main.cxx
  main_window.cxx
  cadapp.cxx
  cadappexec.cxx
  cadconsole.cxx
  cadimport.cxx
  cadtreemodel.cxx
  cadtreeview.cxx
  cadtreenode.cxx
  pqConsoleWidget.cxx
  )

set(moc_headers
  main_window.h
  cadapp.h
  cadappexec.h
  cadimport.h
  cadconsole.h
  cadtreemodel.h
  cadtreeview.h
  pqConsoleWidget.h
  )
QT5_WRAP_CPP(moc_srcs ${moc_headers})

QT5_ADD_RESOURCES(cad_qrc cadresources.qrc)

add_executable(qbrlcad ${qbrlcad_srcs} ${moc_srcs} ${cad_qrc})
qt5_use_modules(qbrlcad Widgets OpenGL)
target_link_libraries(qbrlcad libbu librt libged ${QT_LIBRARIES} ${OPENGL_LIBRARIES})
install(TARGETS qbrlcad
  RUNTIME DESTINATION ${BIN_DIR}
  LIBRARY DESTINATION ${LIB_DIR}
  ARCHIVE DESTINATION ${LIB_DIR})

CMAKEFILES(images)
CMAKEFILES(cadstyle.qss)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

