add_subdirectory(wfobj)

include_directories(${WFOBJ_INCLUDE_DIRS})


LIBGCV_ADD_PLUGIN(obj "obj_read.c;obj_write.c" "libwfobj")
