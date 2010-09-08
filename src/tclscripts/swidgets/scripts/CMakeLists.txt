SET(swidgets_scripts_TCLSCRIPTS
	selectlists.itk
	tkgetdir.itk
	togglearrow.itki
	tooltip.itk
	tree.itk
	)

INSTALL(FILES ${swidgets_scripts_TCLSCRIPTS} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/swidgets/scripts)

tclIndex_BUILD(swidgets_scripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/swidgets/scripts)

SET(swidgets_scripts_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
	)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${swidgets_scripts_TCLSCRIPT_FILES}")

