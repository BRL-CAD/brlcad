set(LIBORLE_SOURCES
    rle.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BRLCAD_UTAHRLE_INCLUDE_DIR}
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

add_library(orle ${LIBORLE_SOURCES})
