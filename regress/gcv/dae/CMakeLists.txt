if (TARGET gcv-assetimport)
  GCV_regress_util("dae" "tetra.dae" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
  GCV_regress_util("dae" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "4117" EXPORTER)
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.dae
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
