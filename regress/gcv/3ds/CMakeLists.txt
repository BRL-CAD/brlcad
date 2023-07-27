if (TARGET gcv-assetimport)
  GCV_regress_util("3ds" "tetra.3ds" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
  GCV_regress_util("3ds" "tetra.g" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--output-format=assetimport" EXPORTER)
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.3ds
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
