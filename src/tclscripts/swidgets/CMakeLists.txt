ADD_SUBDIRECTORY(images)
ADD_SUBDIRECTORY(scripts)

SET(swidgets_TCLSCRIPTS
	swidgets.tcl
)

FILE(COPY ${swidgets_TCLSCRIPTS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
INSTALL(FILES ${swidgets_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/swidgets)

pkgIndex_BUILD(swidgets ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/swidgets)
tclIndex_BUILD(swidgets ${CMAKE_CURRENT_SOURCE_DIR} tclscripts/swidgets)

