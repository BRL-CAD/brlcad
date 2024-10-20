brlcad_addexec(gltf-g gltf-g.cpp "libwdb;librt;libnmg;libbu" FOLDER Conv NO_ERROR)
target_include_directories(gltf-g BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/tinygltf.dist" tinygltf_files REGEX "^.*$")

cmakefiles(
  ${tinygltf_files}
  CMakeLists.txt
  baseColor.png
  blob.glb
  tinygltf.dist
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
