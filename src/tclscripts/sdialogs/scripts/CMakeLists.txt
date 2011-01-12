SET(sdialogs_scripts_TCLSCRIPTS
	entrydialog.itk
	listdialog.itk
	stddlgs.tcl
)
INSTALL(FILES ${sdialogs_scripts_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/sdialogs/scripts)

tclIndex_BUILD(sdialogs_scripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/sdialogs/scripts)

SET(sdialogs_scripts_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
	)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${sdialogs_scripts_TCLSCRIPT_FILES}")

