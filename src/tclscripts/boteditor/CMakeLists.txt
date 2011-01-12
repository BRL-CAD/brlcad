SET(boteditor_TCLSCRIPTS
	botEditor.tcl
	botPropertyBox.tcl
	botTools.tcl
)

INSTALL(FILES ${boteditor_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/boteditor)
