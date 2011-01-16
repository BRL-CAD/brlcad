SET(pl-dm_TCLSCRIPTS
	bindings.tcl
	help.tcl
	menu.tcl
	)

FILE(COPY ${pl-dm_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/pl-dm)
INSTALL(FILES ${pl-dm_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/pl-dm)

pkgIndex_BUILD(pl-dm tclscripts/pl-dm)
tclIndex_BUILD(pl-dm tclscripts/pl-dm)

