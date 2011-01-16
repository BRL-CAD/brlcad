SET(swidgets_scripts_TCLSCRIPTS
	selectlists.itk
	tkgetdir.itk
	togglearrow.itk
	tooltip.itk
	tree.itk
	)

FILE(COPY ${swidgets_scripts_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/swidgets/scripts)
INSTALL(FILES ${swidgets_scripts_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/swidgets/scripts)

tclIndex_BUILD(swidgets_scripts tclscripts/swidgets/scripts)

