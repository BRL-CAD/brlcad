set(LIBGED_SOURCES
    bigE.c
    dg_obj.c
    globals.c
    nirt.c
    qray.c
    tcl.c
    vdraw.c
    view_obj.c
    wdb_obj.c
)

include_directories(
    ../../include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libged ${LIBGED_SOURCES})
