add_subdirectory(wfobj)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/wfobj
  ${CMAKE_CURRENT_BINARY_DIR}/wfobj
  )

LIBGCV_ADD_PLUGIN(obj "obj_read.c;obj_write.c;tri_face.c" "librt;libwfobj")

CMAKEFILES(tri_face.h)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

