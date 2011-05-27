
SET(TOGL_DECLS_H_TXT "
#ifndef ToglDecls_H
#  define ToglDecls_H

/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

/* !BEGIN!: Do not edit below this line. */

/* !END!: Do not edit above this line. */

#endif

")
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/../src/toglDecls.h "${TOGL_DECLS_H_TXT}")

SET(TOGL_HDRS
	togl.h
	${CMAKE_CURRENT_BINARY_DIR}/togl_config.h
	${CMAKE_CURRENT_BINARY_DIR}/../src/toglDecls.h
	)
install(FILES ${TOGL_HDRS} DESTINATION include/togl)
install(FILES GL/glew.h DESTINATION include/GL)
