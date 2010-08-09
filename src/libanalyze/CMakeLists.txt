set(LIBANALYZE_SOURCES
    density.c
    overlaps.c
)

include_directories(
    ../../include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(analyze ${LIBANALYZE_SOURCES})
target_link_libraries(analyze bu)
