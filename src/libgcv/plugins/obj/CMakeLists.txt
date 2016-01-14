add_subdirectory(wfobj)


include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${WFOBJ_INCLUDE_DIRS}
    )


LIBGCV_ADD_PLUGIN(obj "obj_read.c;obj_write.c" "libwfobj")
