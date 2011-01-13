ADD_SUBDIRECTORY(scripts)

SET(sdialogs_TCLSCRIPTS
	sdialogs.tcl
)

FILE(COPY ${sdialogs_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${sdialogs_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/sdialogs)

pkgIndex_BUILD(sdialogs ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/sdialogs)
tclIndex_BUILD(sdialogs ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/sdialogs)

