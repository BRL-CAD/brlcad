set(LIBGCV_SOURCES
    region_end.c
    region_end_mc.c
)

include_directories(
    ../../include
    ../other
    ../other/libregex
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libgcv ${LIBGCV_SOURCES})
