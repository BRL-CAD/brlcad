include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DRT_MULTISPECTRAL
)

add_library(multispectral init.c sh_temp.c)
target_link_libraries(multispectral optical librt)
INSTALL(TARGETS multispectral DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
