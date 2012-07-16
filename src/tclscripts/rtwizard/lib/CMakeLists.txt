set(rtwizard_lib_TCLSCRIPTS
	DbPage.itk
	ExamplePage.itk
	FbPage.itk
	FeedbackDialog.itk
	FrameBufferDialog.itk
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
BRLCAD_ADDDATA(rtwizard_lib_TCLSCRIPTS tclscripts/rtwizard/lib)
pkgIndex_BUILD(rtwizard_lib tclscripts/rtwizard/lib)
tclIndex_BUILD(rtwizard_lib tclscripts/rtwizard/lib)

CMAKEFILES(Makefile.am pkgIndex.tcl tclIndex)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
