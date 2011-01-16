ADD_SUBDIRECTORY(scripts)

SET(sdialogs_TCLSCRIPTS
	sdialogs.tcl
)

FILE(COPY ${sdialogs_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/sdialogs)
INSTALL(FILES ${sdialogs_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/sdialogs)

pkgIndex_BUILD(sdialogs tclscripts/sdialogs)
tclIndex_BUILD(sdialogs tclscripts/sdialogs)

