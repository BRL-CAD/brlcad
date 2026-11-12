set(
  qtcad_headers
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
  QgPaletteController.h
  QgPolyFilter.h
  QgPluginApi.h
  QgPluginCommands.h
  QgPluginContext.h
  QgPluginDescriptor.h
  QgPluginInterfaces.h
  QgPluginManager.h
  QgPluginVersion.h
  QgQuadView.h
  QgRoles.h
  QgSW.h
  QgSelectFilter.h
  QgSession.h
  QgSignalFlags.h
  QgSketchFilter.h
  QgToolBase.h
  QgToolPalette.h
  QgTreeSelectionModel.h
  QgTreeView.h
  QgUtil.h
  QgView.h
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
