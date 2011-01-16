SET(swidgets_image_DATA
	closed.gif
	leaf.gif
	minus.xbm
	open.gif
	plus.xbm
	tip.gif
)
FILE(COPY ${swidgets_image_DATA} DESTINATION ${CMAKE_BINARY_DIR}/${DATA_DIR}/tclscripts/swidgets/images)
INSTALL(FILES ${swidgets_image_DATA} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/tclscripts/swidgets/images)
