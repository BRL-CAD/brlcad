SET(geometree_TCLSCRIPTS
	GeometryBrowser.tcl
	geometree.tcl
)

INSTALL(FILES ${geometree_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/geometree)

pkgIndex_BUILD(geometree ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/geometree)
tclIndex_BUILD(geometree ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/geometree)

SET(geometree_TCLSCRIPT_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/pkgIndex.tcl
	${CMAKE_CURRENT_SOURCE_DIR}/tclIndex
	)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${geometree_TCLSCRIPT_FILES}")
