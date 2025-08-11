if(TARGET gcv-assetimport)
  gcv_regress_util("off" "tetra.off" DEPS "gcv;gcv-assetimport" GCV_OPTIONS "--input-format=assetimport")
endif(TARGET gcv-assetimport)

cmakefiles(
  CMakeLists.txt
  tetra.off
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
