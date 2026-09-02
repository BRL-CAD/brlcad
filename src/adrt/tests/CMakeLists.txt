brlcad_addexec(adrt_annotation_test annotation.c
  "librender;libwdb;librt;libbu;${M_LIBRARY}" TEST)
target_include_directories(adrt_annotation_test PRIVATE
  ${BRLCAD_SOURCE_DIR}/src/adrt
  ${BRLCAD_SOURCE_DIR}/src/adrt/librender
)
brlcad_add_test(NAME adrt_annotation_test COMMAND adrt_annotation_test)

cmakefiles(annotation.c CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
