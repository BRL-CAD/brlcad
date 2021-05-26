set(qtcad_headers
  QAccordionWidget.h
  QtCADView.h
  QtConsole.h
  QtConsoleListener.h
  QtGL.h
  QtGLQuad.h
  QtSW.h
  defines.h
  )
BRLCAD_MANAGE_FILES(qtcad_headers ${INCLUDE_DIR}/brlcad/qtcad)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
