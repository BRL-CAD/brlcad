if (TARGET gcv-assetimport)
  GCV_regress_util("assbin" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1490" EXPORTER)
  GCV_regress_util("assbin" "tetra.assbin" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.assbin
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
