SET(boteditor_TCLSCRIPTS
	botEditor.tcl
	botPropertyBox.tcl
	botTools.tcl
)

INSTALL(FILES ${boteditor_TCLSCRIPTS} DESTINATION ${BRLCAD_INSTALL_DATA_DIR}/tclscripts/boteditor)
