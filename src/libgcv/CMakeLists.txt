set(LIBGCV_SOURCES
    region_end.c
)

include_directories(
    ../../include
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(libgcv ${LIBGCV_SOURCES})
