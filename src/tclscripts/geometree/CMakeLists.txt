SET(geometree_TCLSCRIPTS
	GeometryBrowser.tcl
	geometree.tcl
)

FILE(COPY ${geometree_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${geometree_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/geometree)

pkgIndex_BUILD(geometree ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/geometree)
tclIndex_BUILD(geometree ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/geometree)

