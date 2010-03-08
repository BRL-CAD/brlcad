set(LIBGCV_SOURCES
    obj/obj_rules.cc
    obj/obj_grammar.cc
    obj/obj_parser.cc
    region_end.c
    region_start_mc.c
)

include_directories(
    ../../include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libgcv ${LIBGCV_SOURCES})
