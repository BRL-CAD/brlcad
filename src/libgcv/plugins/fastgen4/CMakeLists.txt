include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


BRLCAD_ADDLIB(gcv_fastgen4 "fastgen4_read.c;fastgen4_write.cpp" "librt;libwdb;libbu;" STATIC NO_INSTALL)
set_property(TARGET gcv_fastgen4-static PROPERTY POSITION_INDEPENDENT_CODE ON)


CMAKEFILES(NOTES)
