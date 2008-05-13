set(LIBGED_SOURCES
    bigE.c
    dg_obj.c
    editit.c
    edmater.c
    globals.c
    importFg4Section.c
    item.c
    log.c
    mater.c
    mirror.c
    nirt.c
    qray.c
    rmater.c
    tcl.c
    track.c
    vdraw.c
    view_obj.c
    wdb_comb_std.c
    wdb_obj.c
    wmater.c
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
