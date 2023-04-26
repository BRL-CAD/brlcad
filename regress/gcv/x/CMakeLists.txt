if (TARGET gcv-assetimport)
  GCV_regress_util("x" "tetra.x" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "1136")
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.x
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
