if(BRLCAD_ENABLE_QT)
  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${GED_INCLUDE_DIRS}
    ${DM_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR_GL}
    )
endif(BRLCAD_ENABLE_QT)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(dm_srcs
  main.cpp
  main_window.cpp
  dmapp.cpp
  )

set(dm_moc_headers
  main_window.h
  dmapp.h
  )

if(BRLCAD_ENABLE_QT)

  if (OPENGL_opengl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
  endif (OPENGL_opengl_LIBRARY)
  if (OPENGL_gl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
  endif (OPENGL_gl_LIBRARY)

  QT5_WRAP_CPP(dm_moc_srcs ${dm_moc_headers})
  BRLCAD_ADDEXEC(qdm "${dm_srcs};${dm_moc_srcs}" "libqtcad;libged;libdm;librt;libbu;Qt5::Core;Qt5::Widgets;Qt5::OpenGL;${OPENGL_LIBS}")
  add_dependencies(qdm dm-qtgl)
endif(BRLCAD_ENABLE_QT)

set(dm_ignore_srcs
  CMakeLists.txt
  NOTES
  qdm_tests.txt
  ${dm_srcs}
  ${dm_moc_headers}
  )

CMAKEFILES(${dm_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

