SET(sdialogs_scripts_TCLSCRIPTS
	entrydialog.itk
	listdialog.itk
	stddlgs.tcl
)
FILE(COPY ${sdialogs_scripts_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${sdialogs_scripts_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/sdialogs/scripts)

tclIndex_BUILD(sdialogs_scripts ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/sdialogs/scripts)

