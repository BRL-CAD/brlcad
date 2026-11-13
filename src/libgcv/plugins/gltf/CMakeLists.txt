BRLCAD_Find_Package(TinyGLTF REQUIRED)

gcv_plugin_library(gcv-gltf SHARED gltf_data.cpp gltf_read.cpp gltf_write.cpp)
target_link_libraries(gcv-gltf libgcv libwdb librt tinygltf::tinygltf)
set_property(TARGET gcv-gltf APPEND PROPERTY COMPILE_DEFINITIONS BRLCADBUILD HAVE_CONFIG_H)
validate_style(gcv-gltf gltf_data.cpp gltf_read.cpp gltf_write.cpp)
plugin_setup(gcv-gltf gcv)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif(BUILD_TESTING)

cmakefiles(
  gltf_read.cpp
  gltf_read_util.h
  gltf_data.cpp
  gltf_data.h
  gltf_write.cpp
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
