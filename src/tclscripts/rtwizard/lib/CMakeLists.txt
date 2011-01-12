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

INSTALL(FILES ${rtwizard_lib_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/rtwizard/lib)

pkgIndex_BUILD(rtwizard_lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard/lib)
tclIndex_BUILD(rtwizard_lib ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/rtwizard/lib)

SET(rtwizard_lib_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
	)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${rtwizard_lib_TCLSCRIPT_FILES}")

