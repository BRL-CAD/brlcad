set(LIBGCV_SOURCES
    region_end.c
    region_end_mc.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ../other/tcl/generic
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(gcv ${LIBGCV_SOURCES})
target_link_libraries(gcv bu rt)
