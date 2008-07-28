set(LIBSYSV_SOURCES
    strsep.c
    tcl.c
)

include_directories(
    ../../include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libsysv ${LIBSYSV_SOURCES})
