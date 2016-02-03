include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


BRLCAD_ADDLIB(gcv_stl "stl_read.c;stl_write.c" "librt;libwdb;libbu;" STATIC NO_INSTALL)
set_property(TARGET gcv_stl-static PROPERTY POSITION_INDEPENDENT_CODE ON)
