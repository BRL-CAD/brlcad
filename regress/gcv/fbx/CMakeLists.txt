if (TARGET gcv-assetimport)
  GCV_regress_util("fbx" "shapes.fbx" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "168920")
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  shapes.fbx
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
