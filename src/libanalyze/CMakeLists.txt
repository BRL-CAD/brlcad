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

add_library(analyze ${LIBANALYZE_SOURCES})
target_link_libraries(analyze bu)
INSTALL(TARGETS analyze DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
