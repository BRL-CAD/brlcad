if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  find_package(OpenGL)

  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt
    ${CMAKE_CURRENT_SOURCE_DIR}/../adrt/librender
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    )
endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(isst_srcs
  gfile.cpp
  main.cpp
  main_window.cpp
  isstapp.cpp
  isstgl.cpp
  )

set(moc_headers
  gfile.h
  main_window.h
  isstapp.h
  isstgl.h
  )

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  if (OPENGL_opengl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
  endif (OPENGL_opengl_LIBRARY)
  if (OPENGL_gl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
  endif (OPENGL_gl_LIBRARY)

  QT5_WRAP_CPP(moc_srcs ${moc_headers})
  BRLCAD_ADDEXEC(qisst "${isst_srcs};${moc_srcs}" "libbu;librt;libgcv;librender;Qt5::Core;Qt5::Widgets;Qt5::OpenGL;${OPENGL_LIBS}")
endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

set(isst_ignore_srcs
  CMakeLists.txt
  ${isst_srcs}
  ${moc_headers}
  )

CMAKEFILES(${isst_ignore_srcs})

add_subdirectory(dmtest)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

