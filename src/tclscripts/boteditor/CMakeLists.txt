SET(boteditor_TCLSCRIPTS
	botEditor.tcl
	botPropertyBox.tcl
	botTools.tcl
)

FILE(COPY ${boteditor_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${boteditor_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/boteditor)
