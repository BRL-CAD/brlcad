include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    )


LIBGCV_ADD_PLUGIN(fastgen4 "fastgen4_read.c;fastgen4_write.cpp" "")


CMAKEFILES(NOTES)
