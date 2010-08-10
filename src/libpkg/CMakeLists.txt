set(LIBPKG_SOURCES
    pkg.c
    tcl.c
    vers.c
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

add_library(pkg ${LIBPKG_SOURCES})
target_link_libraries(pkg bu)

