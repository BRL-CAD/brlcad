set(LIBGED_SOURCES
    dg_obj.c
    view_obj.c
    wdb_obj.c
)

include_directories(
    ../../include
    ../other/libz
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libged ${LIBGED_SOURCES})
