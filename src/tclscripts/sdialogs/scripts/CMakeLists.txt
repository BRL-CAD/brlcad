SET(sdialogs_scripts_TCLSCRIPTS
	entrydialog.itk
	listdialog.itk
	stddlgs.tcl
)
FILE(COPY ${sdialogs_scripts_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/sdialogs/scripts)
INSTALL(FILES ${sdialogs_scripts_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/sdialogs/scripts)

tclIndex_BUILD(sdialogs_scripts tclscripts/sdialogs/scripts)

