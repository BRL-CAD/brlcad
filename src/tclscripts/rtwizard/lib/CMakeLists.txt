set(
  rtwizard_lib_TCLSCRIPTS
  DbPage.itk
  ExamplePage.itk
  FbPage.itk
  FeedbackDialog.itk
  FullColorPage.itk
  GhostPage.itk
  HelpPage.itk
  HighlightedPage.itk
  IntroPage.itk
  LinePage.itk
  MGEDpage.itk
  PictureTypeA.itcl
  PictureTypeB.itcl
  PictureTypeBase.itcl
  PictureTypeC.itcl
  PictureTypeD.itcl
  PictureTypeE.itcl
  PictureTypeF.itcl
  Wizard.itk
)
if(BRLCAD_ENABLE_TCL)
  brlcad_adddata(rtwizard_lib_TCLSCRIPTS tclscripts/rtwizard/lib)
  pkgindex_build(tclscripts/rtwizard/lib)
  tclindex_build(tclscripts/rtwizard/lib)
endif(BRLCAD_ENABLE_TCL)

cmakefiles(
  CMakeLists.txt
  ${rtwizard_lib_TCLSCRIPTS}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
