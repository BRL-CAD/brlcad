if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qtglbasic_srcs
  glwidget.cpp
  logo.cpp
  main.cpp
  mainwindow.cpp
  window.cpp
  )

set(basic_moc_headers
  glwidget.h
  logo.h
  mainwindow.h
  window.h
  )

if(BRLCAD_ENABLE_QT)
  QT5_WRAP_CPP(basic_moc_srcs ${basic_moc_headers})
  BRLCAD_ADDEXEC(qtglbasic "${qtglbasic_srcs};${basic_moc_srcs}" "${OPENGL_LIBRARIES};Qt5::Core;Qt5::Widgets;Qt5::OpenGL")
endif(BRLCAD_ENABLE_QT)

set(qtglbasic_ignore_srcs
  CMakeLists.txt
  ${qtglbasic_srcs}
  ${basic_moc_headers}
  )

CMAKEFILES(${qtglbasic_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

