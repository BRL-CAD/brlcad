set(qtcad_headers
  QToolPalette.h
  QViewCtrl.h
  QgAccordion.h
  QgAppExecDialog.h
  QgColorRGB.h
  QgDockWidget.h
  QgEdit.h
  QgFlowLayout.h
  QgKeyVal.h
  QgMeasureFilter.h
  QgModel.h
  QgPolyFilter.h
  QgSelectFilter.h
  QgTreeSelectionModel.h
  QgTreeView.h
  QgUtil.h
  QgQuadView.h
  QgView.h
  QtConsole.h
  QtConsoleListener.h
  QtGL.h
  QtSW.h
  SignalFlags.h
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
