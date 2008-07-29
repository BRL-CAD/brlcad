set(LIBGED_SOURCES
    arced.c
    bigE.c
    comb_color.c
    dg_obj.c
    editit.c
    edcomb.c
    edmater.c
    globals.c
    importFg4Section.c
    item.c
    log.c
    make.c
    make_name.c
    mater.c
    mirror.c
    nirt.c
    qray.c
    rmater.c
    shader.c
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
