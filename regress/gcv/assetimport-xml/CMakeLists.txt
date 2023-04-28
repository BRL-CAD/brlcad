if (TARGET gcv-assetimport)
  GCV_regress_util("assxml" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPECTED_FILE_SIZE "3425" EXPORTER)
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
