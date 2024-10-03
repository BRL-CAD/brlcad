if(TARGET gcv-assetimport)
  set(dep_str DEPS "gcv;gcv-ply;gcv-assetimport" CONVERTERS "assetimport")
else()
  set(dep_str "")
endif(TARGET gcv-assetimport)

gcv_regress_util("ply" "tetra.ply" ${dep_str})
gcv_regress_util("ply" "tetra.g" ${dep_str} EXPORTER)

cmakefiles(
  CMakeLists.txt
  tetra.ply
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
