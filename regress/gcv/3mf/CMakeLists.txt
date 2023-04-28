if (TARGET gcv-assetimport)
  GCV_regress_util("3mf" "tetra.3mf" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "768")
  GCV_regress_util("3mf" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "1052" EXPORTER)
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.3mf
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
