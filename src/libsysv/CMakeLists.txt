set(LIBSYSV_SOURCES
    memset.c
    strchr.c
    strdup.c
    strsep.c
    strtok.c
    tcl.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TCL_INCLUDE_PATH}
)

add_definitions(
    -DBRLCADBUILD
    -DHAVE_CONFIG_H
    -DHAVE_MEMSET
    -DHAVE_STRCHR
    -DHAVE_STRDUP
    -DHAVE_STRTOK
)

BRLCAD_ADDLIB(sysv "${LIBSYSV_SOURCES}" "${TCL_LIBRARY}")
SET_TARGET_PROPERTIES(sysv PROPERTIES VERSION 19.0.1 SOVERSION 19)
