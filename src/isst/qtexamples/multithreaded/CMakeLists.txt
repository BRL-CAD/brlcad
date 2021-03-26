if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    )
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qtmulti_srcs
  glwidget.cpp
  main.cpp
  mainwindow.cpp
  )

set(mt_moc_headers
  glwidget.h
  mainwindow.h
  )

if(BRLCAD_ENABLE_QT)
  QT5_WRAP_CPP(mt_moc_srcs ${mt_moc_headers})
  BRLCAD_ADDEXEC(qtmulti "${qtmulti_srcs};${mt_moc_srcs}" "${OPENGL_LIBRARIES};Qt5::Core;Qt5::Widgets;Qt5::OpenGL")
endif(BRLCAD_ENABLE_QT)

set(qtmulti_ignore_srcs
  CMakeLists.txt
  ${qtmulti_srcs}
  ${mt_moc_headers}
  )

CMAKEFILES(${qtmulti_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

