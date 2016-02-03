add_subdirectory(wfobj)


include_directories(
    ${BRLCAD_BINARY_DIR}/include
    ${BRLCAD_SOURCE_DIR}/include
    ${BU_INCLUDE_DIRS}
    ${RT_INCLUDE_DIRS}
    ${WFOBJ_INCLUDE_DIRS}
    )


BRLCAD_ADDLIB(gcv_obj "obj_read.c;obj_write.c" "libwfobj;librt;libwdb;libbu;" STATIC NO_INSTALL)
set_property(TARGET gcv_obj-static PROPERTY POSITION_INDEPENDENT_CODE ON)
