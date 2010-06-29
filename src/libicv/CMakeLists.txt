set(LIBICV_SOURCES
    rot.c
)

include_directories(
    ../../include
    ../other
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libicv ${LIBICV_SOURCES})
