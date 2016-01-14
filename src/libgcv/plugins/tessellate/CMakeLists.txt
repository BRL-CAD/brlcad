include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


BRLCAD_ADDLIB(gcv_tessellate "tessellate.c" "libbu;librt" STATIC NO_INSTALL)
set_property(TARGET gcv_tessellate-static PROPERTY POSITION_INDEPENDENT_CODE ON)
