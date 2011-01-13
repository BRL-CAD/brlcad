SET(rtwizard_lib_TCLSCRIPTS
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

FILE(COPY ${rtwizard_lib_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${rtwizard_lib_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/lib)

pkgIndex_BUILD(rtwizard_lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard/lib)
tclIndex_BUILD(rtwizard_lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard/lib)

