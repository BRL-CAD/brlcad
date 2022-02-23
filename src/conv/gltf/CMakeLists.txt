set(GLTF_CONV_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BU_INCLUDE_DIRS}
  ${RT_INCLUDE_DIRS}
  )
BRLCAD_INCLUDE_DIRS(GLTF_CONV_INCLUDE_DIRS)

BRLCAD_ADDEXEC(gltf-g gltf-g.cpp "libwdb;librt;libnmg;libbu" FOLDER Conv)


CMAKEFILES(
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
