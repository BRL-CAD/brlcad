SET(geometree_TCLSCRIPTS
	GeometryBrowser.tcl
	geometree.tcl
)

FILE(COPY ${geometree_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/geometree)
INSTALL(FILES ${geometree_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/geometree)

pkgIndex_BUILD(geometree tclscripts/geometree)
tclIndex_BUILD(geometree tclscripts/geometree)

