SET(swidgets_scripts_TCLSCRIPTS
	selectlists.itk
	tkgetdir.itk
	togglearrow.itk
	tooltip.itk
	tree.itk
	)

FILE(COPY ${swidgets_scripts_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${swidgets_scripts_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/swidgets/scripts)

tclIndex_BUILD(swidgets_scripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/swidgets/scripts)

