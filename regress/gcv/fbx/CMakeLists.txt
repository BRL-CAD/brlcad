if(TARGET gcv-assetimport)
  gcv_regress_util("fbx" "shapes.fbx" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "168920")
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
  shapes.fbx
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
