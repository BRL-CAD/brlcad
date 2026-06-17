set(
  qtcad_headers
  QgAccordion.h
  QgAppExecDialog.h
  QgAttributesModel.h
  QgCanvasBase.h
  QgColorRGB.h
  QgConsole.h
  QgConsoleListener.h
  QgDockWidget.h
  QgFlowLayout.h
  QgGedEventBatch.h
  QgGL.h
  QgGeomImport.h
  QgKeyVal.h
  QgMeasureFilter.h
  QgModel.h
  QgPaletteController.h
  QgPluginApi.h
  QgPluginCommands.h
  QgPluginContext.h
  QgPluginDescriptor.h
  QgPluginInterfaces.h
  QgPluginManager.h
  QgPolyFilter.h
  QgQuadView.h
  QgRoles.h
  QgSW.h
  QgSelectFilter.h
  QgSignalFlags.h
  QgSketchFilter.h
  QgToolBase.h
  QgToolPalette.h
  QgTreeSelectionModel.h
  QgTreeView.h
  QgTypes.h
  QgUtil.h
  QgView.h
  QgViewFilter.h
  QgViewCtrl.h
  defines.h
)
brlcad_manage_files(qtcad_headers ${INCLUDE_DIR}/brlcad/qtcad REQUIRED libqtcad)

cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
