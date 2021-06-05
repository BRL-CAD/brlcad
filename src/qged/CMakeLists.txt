if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  find_package(OpenGL)

  include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BRLCAD_BINARY_DIR}/${INCLUDE_DIR}
    ${BRLCAD_SOURCE_DIR}/include
    ${QTCAD_INCLUDE_DIRS}
    ${GED_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${BU_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    )

endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

# NOTE: We can't use the CMake global values for automoc et. al.
# because most of BRL-CAD's targets are not Qt targets.  We don't
# want to run the automatic moc logic for all of them.

set(qged_srcs
  main.cpp
  main_window.cpp
  accordion.cpp
  app.cpp
  attributes.cpp
  cadhelp.cpp
  import.cpp
  )

set(moc_headers
  main_window.h
  accordion.h
  app.h
  attributes.h
  cadhelp.h
  import.h
  )

if(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

  if (OPENGL_opengl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_opengl_LIBRARY})
  endif (OPENGL_opengl_LIBRARY)
  if (OPENGL_gl_LIBRARY)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
  endif (OPENGL_gl_LIBRARY)

  QT5_WRAP_CPP(moc_srcs ${moc_headers})

  # https://github.com/Alexhuszagh/BreezeStyleSheets
  #QT5_ADD_RESOURCES(cad_qrc breeze.qrc)

  add_executable(qged ${qged_srcs} ${moc_srcs} ${cad_qrc})
  target_link_libraries(qged libqtcad libged librt libbu Qt5::Widgets Qt5::OpenGL ${OPENGL_LIBS})
  target_compile_definitions(qged PRIVATE BRLCADBUILD HAVE_CONFIG_H)
  install(TARGETS qged
    RUNTIME DESTINATION ${BIN_DIR}
    LIBRARY DESTINATION ${LIB_DIR}
    ARCHIVE DESTINATION ${LIB_DIR})
endif(BRLCAD_ENABLE_QT AND BRLCAD_ENABLE_OPENGL)

CMAKEFILES(${qged_srcs})
CMAKEFILES(${moc_headers})

set(qged_ignore_srcs
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
  CMakeLists.txt
  Qt_Notes.txt
  TODO
  TODO.qt
  )

CMAKEFILES(${qged_ignore_srcs})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

