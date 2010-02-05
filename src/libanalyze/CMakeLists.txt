set(LIBANALYZE_SOURCES
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

add_library(libanalyze ${LIBANALYZE_SOURCES})
