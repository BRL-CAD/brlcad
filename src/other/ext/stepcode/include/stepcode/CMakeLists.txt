add_subdirectory(core)
add_subdirectory(dai)
add_subdirectory(editor)
add_subdirectory(utils)
add_subdirectory(exppp)
add_subdirectory(express)

install(FILES export.h
	DESTINATION ${INCLUDE_DIR}/stepcode)

