set(LIBSYSV_SOURCES
    memset.c
    strchr.c
    strdup.c
    strsep.c
    strtok.c
    tcl.c
)

include_directories(
    ${BRLCAD_SOURCE_DIR}/include
    ../other/tcl/generic
)

add_definitions(
    -DBRLCADBUILD
    -DHAVE_CONFIG_H
    -DHAVE_MEMSET
    -DHAVE_STRCHR
    -DHAVE_STRDUP
    -DHAVE_STRTOK
)

add_library(sysv ${LIBSYSV_SOURCES})
target_link_libraries(sysv ${TCL_LIBRARY})
