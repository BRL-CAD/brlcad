# Include directories needed by libsysv users
SET(SYSV_INCLUDE_DIRS
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${TCL_INCLUDE_DIRS}
	)
BRLCAD_INCLUDE_DIRS(SYSV_INCLUDE_DIRS)

include_directories(
	${BRLCAD_BINARY_DIR}/include
	${BRLCAD_SOURCE_DIR}/include
	${TCL_INCLUDE_DIRS}
)

set(LIBSYSV_SOURCES
    memset.c
    strchr.c
    strdup.c
    strsep.c
    strtok.c
    tcl.c
)

add_definitions(
    -DHAVE_MEMSET
    -DHAVE_STRCHR
    -DHAVE_STRDUP
    -DHAVE_STRTOK
)

BRLCAD_ADDLIB(libsysv "${LIBSYSV_SOURCES}" "${TCL_LIBRARY}")
SET_TARGET_PROPERTIES(libsysv PROPERTIES VERSION 19.0.1 SOVERSION 19)

CMAKEFILES(Makefile.am)
