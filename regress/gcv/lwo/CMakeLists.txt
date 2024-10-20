if(TARGET gcv-assetimport)
  gcv_regress_util("lwo" "tetra.lwo" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "1136")
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
  tetra.lwo
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
