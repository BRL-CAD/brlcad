brlcad_addexec(test_gcv_gltf test_gcv_gltf.cpp "libgcv;libwdb;librt;libbu;tinygltf::tinygltf" NO_INSTALL)
add_dependencies(test_gcv_gltf gcv-gltf)
brlcad_add_test(NAME gcv_gltf_joint_roundtrip COMMAND test_gcv_gltf "${CMAKE_CURRENT_BINARY_DIR}/joint-roundtrip.gltf")

set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
  "${CMAKE_CURRENT_BINARY_DIR}/joint-roundtrip.gltf"
  "${CMAKE_CURRENT_BINARY_DIR}/joint-roundtrip.gltf.transform.gltf"
)
distclean("${CMAKE_CURRENT_BINARY_DIR}/joint-roundtrip.gltf")
distclean("${CMAKE_CURRENT_BINARY_DIR}/joint-roundtrip.gltf.transform.gltf")

cmakefiles(CMakeLists.txt test_gcv_gltf.cpp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

