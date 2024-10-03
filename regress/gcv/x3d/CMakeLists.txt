if(TARGET gcv-assetimport)
  gcv_regress_util("x3d" "tetra.x3d" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "792")
  gcv_regress_util("x3d" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1223" EXPORTER)
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
  tetra.x3d
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
