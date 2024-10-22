BRLCAD_Find_Package(TinyGLTF REQUIRED)

gcv_plugin_library(gcv-gltf SHARED gltf_read.cpp)
target_link_libraries(gcv-gltf libgcv libwdb librt tinygltf::tinygltf)
set_property(TARGET gcv-gltf APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-gltf gltf_read.cpp)
plugin_setup(gcv-gltf gcv)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
