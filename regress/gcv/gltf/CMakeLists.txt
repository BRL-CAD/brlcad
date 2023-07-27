if (TARGET gcv-assetimport)
  GCV_regress_util("gltf" "tetra-gltf2.0.gltf;tetra-glb2.0.glb" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
  GCV_regress_util("glb" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1476" EXPORTER)
  GCV_regress_util("gltf" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "3356" EXPORTER)
DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/conv-tetra.bin")
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra-gltf2.0.gltf
  tetra-glb2.0.glb
  tetra.bin # support file for glb
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
