set(LIBANALYZE_SOURCES
    density.c
    overlaps.c
)

include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${TCL_INCLUDE_PATH}
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
)

BRLCAD_ADDLIB(analyze "${LIBANALYZE_SOURCES}" bu)
SET_TARGET_PROPERTIES(analyze PROPERTIES VERSION 19.0.1 SOVERSION 19)
