set(LIBGCV_SOURCES
    region_end.c
    region_end_mc.c
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

BRLCAD_ADDLIB(gcv "${LIBGCV_SOURCES}" "bu librt")
SET_TARGET_PROPERTIES(gcv PROPERTIES VERSION 19.0.1 SOVERSION 19)
