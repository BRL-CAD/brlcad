ADD_SUBDIRECTORY(images)
ADD_SUBDIRECTORY(scripts)

SET(swidgets_TCLSCRIPTS
	swidgets.tcl
)

FILE(COPY ${swidgets_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/swidgets)
INSTALL(FILES ${swidgets_TCLSCRIPTS} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/swidgets)

pkgIndex_BUILD(swidgets tclscripts/swidgets)
tclIndex_BUILD(swidgets tclscripts/swidgets)

