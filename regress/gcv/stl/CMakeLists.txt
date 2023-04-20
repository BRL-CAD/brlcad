if (TARGET gcv-assetimport)
  GCV_to_g_util("stl" "tetra.stl" DEPS "gcv;gcv-assetimport" INPUT_OPTIONS "--input-format=assetimport" EXPECTED_FILE_SIZE "912")
endif (TARGET gcv-assetimport)

CMAKEFILES(
  CMakeLists.txt
  tetra.stl
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
