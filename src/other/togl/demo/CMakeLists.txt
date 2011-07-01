include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_BINARY_DIR}/../src
	${CMAKE_CURRENT_BINARY_DIR}/../include
	${OPENGL_INCLUDE_DIR_GL}
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
	)

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
IF(NOT WIN32)
	add_library(gears SHARED gears.c)
	target_link_libraries(gears togl)
	set_target_properties(gears PROPERTIES PREFIX "")
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gears.tcl ${CMAKE_CURRENT_BINARY_DIR}/gears.tcl COPYONLY)
ENDIF(NOT WIN32)
