set(rtwizard_lib_TCLSCRIPTS
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
if (BRLCAD_ENABLE_TCL)
  BRLCAD_ADDDATA(rtwizard_lib_TCLSCRIPTS tclscripts/rtwizard/lib)
  pkgIndex_BUILD(tclscripts/rtwizard/lib)
  tclIndex_BUILD(tclscripts/rtwizard/lib)
endif (BRLCAD_ENABLE_TCL)

CMAKEFILES(
  CMakeLists.txt
  ${rtwizard_lib_TCLSCRIPTS}
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
