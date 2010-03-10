set(LIBGCV_SOURCES
    obj/obj_rules_static.cc
    obj/obj_grammar_static.cc
    obj/obj_parser_static.cc
    region_end.c
    region_start_mc.c
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

add_library(libgcv ${LIBGCV_SOURCES})
