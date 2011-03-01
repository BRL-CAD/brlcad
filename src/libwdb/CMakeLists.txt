set(LIBWDB_SOURCES
    brep.cpp
    arbn.c
    ars.c
    bot.c
    cline.c
    constraint.c
    dsp.c
    dynamic_geometry.c
    ebm.c
    export.c
    extr.c
    id.c
    mater.c
    nmg.c
    nurb.c
    pipe.c
    reg.c
    skt.c
    submodel.c
    units.c
    vol.c
    wdb.c
)

include_directories(
    ../../include
    ../other/libz
    ../other/openNURBS
    ../other/libregex
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DON_DLL_IMPORTS
)

add_library(libwdb ${LIBWDB_SOURCES})
