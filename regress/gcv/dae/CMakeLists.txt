if (TARGET gcv-assetimport)
  GCV_to_g_util("dae" "tetra.dae" DEPS "gcv;gcv-assetimport" INPUT_OPTIONS "--input-format=assetimport")
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
