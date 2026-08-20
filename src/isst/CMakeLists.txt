set(
  isst_srcs
  gfile.cpp
  main.cpp
  main_window.cpp
  isstapp.cpp
  isstgl.cpp
)

set(
  isst_headers
  gfile.h
  isstapp.h
  isstgl.h
  main_window.h
)

if(BRLCAD_ENABLE_QT AND OPENGL_TARGETS)

  find_package_qt(REQUIRED)

  set(
    QISST_DIRS
    ${QISST_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt/librender
  )

  set(QISST_LIBS librender libgcv librt libbu)

  # NOTE: We can't use the CMake global values for automoc et. al.
  # because most of BRL-CAD's targets are not Qt targets.  We don't
  # want to run the automatic moc logic for all of them.
  if(Qt6Widgets_FOUND)
    qt6_wrap_cpp(moc_srcs ${isst_headers})
    set(
      QISST_LIBS
      ${QISST_LIBS}
      Qt6::Core
      Qt6::Widgets
      Qt6::OpenGL
      Qt6::OpenGLWidgets
      ${OPENGL_TARGETS}
    )
  else(Qt6Widgets_FOUND)
    qt5_wrap_cpp(moc_srcs ${isst_headers})
    set(
      QISST_LIBS
      ${QISST_LIBS}
      Qt5::Core
      Qt5::Widgets
      Qt5::OpenGL
      ${OPENGL_TARGETS}
    )
  endif(Qt6Widgets_FOUND)

  brlcad_addexec(qisst "${isst_srcs};${moc_srcs}" "${QISST_LIBS}")
  target_include_directories(qisst BEFORE PRIVATE ${QISST_DIRS})

  distclean(
    moc_gfile.cpp_parameters
    moc_isstapp.cpp_parameters
    moc_isstgl.cpp_parameters
    moc_main_window.cpp_parameters
  )
endif()

cmakefiles(
  CMakeLists.txt
  ${isst_srcs}
  ${isst_headers}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
