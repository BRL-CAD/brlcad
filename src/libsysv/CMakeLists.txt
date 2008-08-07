set(LIBSYSV_SOURCES
    memset.c
    strchr.c
    strdup.c
    strsep.c
    strtok.c
    tcl.c
)

include_directories(
    ../../include
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

add_library(libsysv ${LIBSYSV_SOURCES})
