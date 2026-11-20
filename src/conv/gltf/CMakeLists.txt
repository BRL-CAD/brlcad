BRLCAD_Find_Package(TinyGLTF REQUIRED)
set(GLTF_SCENE_SRC "${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/gltf/gltf_scene.cpp")
brlcad_addexec(gltf-g "gltf-g.cpp;${GLTF_SCENE_SRC}" "libgcv;libwdb;librt;libbg;libbn;libbu;tinygltf::tinygltf" FOLDER Conv)
target_include_directories(
  gltf-g
  BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BRLCAD_SOURCE_DIR}/src/libgcv/plugins/gltf
  ${BRLCAD_SOURCE_DIR}/src/libbu
)

if(TARGET gcv AND TARGET gcv-gltf AND TARGET gcv-stl)
  brlcad_add_test(NAME gltf_scene_cli COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_DIR=${BRLCAD_SOURCE_DIR}" "-DGLTF_G=$<TARGET_FILE:gltf-g>"
    "-DGCV=$<TARGET_FILE:gcv>" "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/scene_cli"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/scene_cli.cmake")
  distclean("${CMAKE_CURRENT_BINARY_DIR}/scene_cli")
endif()

cmakefiles(
  CMakeLists.txt
  baseColor.png
  blob.glb
  scene_cli.cmake
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
