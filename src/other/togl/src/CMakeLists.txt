include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/Xmu
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/../include
	${OPENGL_INCLUDE_DIR_GL}
	${TCL_INCLUDE_DIRS}
	${TK_INCLUDE_PATH}
	)

# Because Togl is Yet Another Tcl/Tk package that needs
# the interal headers to build, we must include a local
# copy of the candidate headers and use them - an installed
# Tcl/Tk has no obligation to provide them.
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include/tcltk/tk${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}
	)

SET(TOGL_STUB_INIT_TXT "
/* 
 * Togl - a Tk OpenGL widget
 *
 * Copyright (C) 1996-2002  Brian Paul and Ben Bederson
 * Copyright (C) 2005-2009  Greg Couch
 * See the LICENSE file for copyright details.
 */

#include \"togl.h\"

extern const ToglStubs toglStubs;

/* !BEGIN!: Do not edit below this line. */


/* !END!: Do not edit above this line. */

")

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

FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs)
FILE(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/toglStubInit.c "${TOGL_STUB_INIT_TXT}")
FILE(WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/toglDecls.h "${TOGL_DECLS_H_TXT}")

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/STUBSDONE ${CMAKE_CURRENT_BINARY_DIR}/STUBSSYNCED ${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c ${CMAKE_CURRENT_BINARY_DIR}/toglDecls.h
	COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/genStubs.tcl ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs ${CMAKE_CURRENT_SOURCE_DIR}/togl.decls
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/toglStubInit.c ${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/toglDecls.h ${CMAKE_CURRENT_BINARY_DIR}/toglDecls.h
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/STUBSSYNCED
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/togl.decls
)

ADD_CUSTOM_TARGET(togl_stubs_gen ALL 
	DEPENDS ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/STUBSDONE
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/STUBSSYNCED
)

# If we re-run cmake, need to ensure we re-run togl_stubs_gen too
FILE(REMOVE ${CMAKE_BINARY_DIR}/CMakeTmp/toglstubs/STUBSDONE)
FILE(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/STUBSSYNCED)

IF(TOGL_WINDOWINGSYSTEM MATCHES "TOGL_X11")
	SET(TOGL_XMU_SRCS
		Xmu/CmapAlloc.c
		Xmu/CrCmap.c
		Xmu/DelCmap.c
		Xmu/LookupCmap.c
		Xmu/StdCmap.c
		)
ENDIF(TOGL_WINDOWINGSYSTEM MATCHES "TOGL_X11")

SET(TOGL_SRCS
	glew/glew.c
	${TOGL_XMU_SRCS}
	togl.c
	toglProcAddr.c
	toglStubLib.c
	${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c
	)

add_library(togl SHARED ${TOGL_SRCS})
ADD_DEPENDENCIES(togl togl_stubs_gen)
IF(WIN32)
	target_link_libraries(togl ${TCL_STUB_LIBRARIES} ${X11_LIBRARIES} ${OPENGL_LIBRARIES})
ELSE(WIN32)
	target_link_libraries(togl ${TCL_LIBRARIES} ${X11_LIBRARIES} ${OPENGL_LIBRARIES})
ENDIF(WIN32)
install(TARGETS togl DESTINATION ${LIB_DIR})

add_library(toglstub STATIC ${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c toglStubLib.c)
ADD_DEPENDENCIES(toglstub togl_stubs_gen)
install(TARGETS toglstub DESTINATION ${LIB_DIR})
MARK_AS_ADVANCED(TOGL_STUB_LIBRARIES)

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TOGL_PKGNAME} ${TOGL_PKGVERSION} [list load [file join $dir .. libtogl${CMAKE_SHARED_LIBRARY_SUFFIX}] togl]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TOGL_PKGNAME}${TOGL_PKGVERSION})

