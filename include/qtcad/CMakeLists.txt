set(qtcad_headers
  QgAccordion.h
  QgAppExecDialog.h
  QgAttributesModel.h
  QgColorRGB.h
  QgConsole.h
  QgConsoleListener.h
  QgDockWidget.h
  QgEdit.h
  QgFlowLayout.h
  QgGL.h
  QgGeomImport.h
  QgKeyVal.h
  QgMeasureFilter.h
  QgModel.h
  QgPolyFilter.h
  QgQuadView.h
  QgSW.h
  QgSelectFilter.h
  QgSignalFlags.h
  QgToolPalette.h
  QgTreeSelectionModel.h
  QgTreeView.h
  QgUtil.h
  QgView.h
  QgViewCtrl.h
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
