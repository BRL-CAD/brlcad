SET(util_TCLSCRIPTS
	expand_int_ranges.tcl
	luniq.tcl
	)
FILE(COPY ${util_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${util_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/util)

pkgIndex_BUILD(util ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/util)
tclIndex_BUILD(util ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/util)

