SET(util_TCLSCRIPTS
	expand_int_ranges.tcl
	luniq.tcl
	)
FILE(COPY ${util_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/util)
INSTALL(FILES ${util_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/util)

pkgIndex_BUILD(util tclscripts/util)
tclIndex_BUILD(util tclscripts/util)

