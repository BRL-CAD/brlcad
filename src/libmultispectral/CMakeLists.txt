include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
)

add_definitions(
    -DHAVE_CONFIG_H
    -DBRLCADBUILD
    -DRT_MULTISPECTRAL
)

BRLCAD_ADDLIB(multispectral "init.c sh_temp.c" "optical librt")
SET_TARGET_PROPERTIES(multispectral PROPERTIES VERSION 19.0.1 SOVERSION 19)
