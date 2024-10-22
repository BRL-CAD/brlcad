BRLCAD_Find_Package(TinyGLTF REQUIRED)
brlcad_addexec(gltf-g gltf-g.cpp "libwdb;librt;libnmg;libbu;tinygltf::tinygltf" FOLDER Conv NO_ERROR)
target_include_directories(gltf-g BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

cmakefiles(
  CMakeLists.txt
  baseColor.png
  blob.glb
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
