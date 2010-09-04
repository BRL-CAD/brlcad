set(LIBICV_SOURCES
    rot.c
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

add_library(libicv ${LIBICV_SOURCES})
