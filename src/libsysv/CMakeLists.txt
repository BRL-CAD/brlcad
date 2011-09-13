include_directories(
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

IF(BRLCAD_BUILD_LOCAL_TCL)
	ADD_DEPENDENCIES(libsysv tcl)
ENDIF(BRLCAD_BUILD_LOCAL_TCL)
CMAKEFILES(Makefile.am)
