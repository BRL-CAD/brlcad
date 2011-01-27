include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include
	${CMAKE_CURRENT_SOURCE_DIR}/Xmu
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/../include
	${OPENGL_INCLUDE_DIR_GL}
	${TCL_INCLUDE_PATH}
	${TK_INCLUDE_PATH}
	)

# Because Togl is Yet Another Tcl/Tk package that needs
# the interal headers to build, we must include a local
# copy of the candidate headers and use them - an installed
# Tcl/Tk has no obligation to provide them.
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../include/tcltk/tk${TCL_VERSION_MAJOR}.${TCL_VERSION_MINOR}
	)

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
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/toglDecls.h "${TOGL_DECLS_H_TXT}")

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
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c "${TOGL_STUB_INIT_TXT}")

ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/STUBSDONE
	COMMAND ${TCL_TCLSH} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/genStubs.tcl	${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/togl.decls
)

ADD_CUSTOM_TARGET(togl_stubs_gen ALL DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/STUBSDONE
)

# If we re-run cmake, need to ensure we re-run togl_stubs_gen too
FILE(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/STUBSDONE)

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
	${TOGL_XMU_SRCS}
	togl.c
	toglProcAddr.c
	toglStubLib.c
	${CMAKE_CURRENT_BINARY_DIR}/toglStubInit.c
	)

add_library(togl SHARED ${TOGL_SRCS})
ADD_DEPENDENCIES(togl togl_stubs_gen)
target_link_libraries(togl ${TCL_LIBRARY} ${TK_LIBRARY} ${X11_LIBRARIES} ${OPENGL_LIBRARIES})
install(TARGETS togl DESTINATION lib)

# Create the pkgIndex.tcl file.
FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl "package ifneeded ${TOGL_PKGNAME} ${TOGL_PKGVERSION} [list load [file join $dir .. libtogl${CMAKE_SHARED_LIBRARY_SUFFIX}] togl]")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pkgIndex.tcl DESTINATION lib/${TOGL_PKGNAME}${TOGL_PKGVERSION})

