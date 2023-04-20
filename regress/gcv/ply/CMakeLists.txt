if (TARGET gcv-assetimport)
  set(run_str "ply" "tetra.ply" DEPS "gcv;gcv-ply;gcv-assetimport" CONVERTERS "assetimport")
else ()
  set(run_str "ply" "tetra.ply")
endif (TARGET gcv-assetimport)

GCV_regress_util(${run_str})

CMAKEFILES(
  CMakeLists.txt
  tetra.ply
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
