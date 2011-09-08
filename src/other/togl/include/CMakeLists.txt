SET(TOGL_HDRS
	togl.h
	${CMAKE_CURRENT_BINARY_DIR}/togl_config.h
	${CMAKE_CURRENT_BINARY_DIR}/../src/toglDecls.h
	)
install(FILES ${TOGL_HDRS} DESTINATION include/togl)
install(FILES GL/glew.h DESTINATION include/GL)
