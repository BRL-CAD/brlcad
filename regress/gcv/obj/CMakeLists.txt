if(TARGET gcv-assetimport)
  set(dep_str DEPS "gcv;gcv-obj;gcv-assetimport" CONVERTERS "assetimport")

  distclean("${CMAKE_CURRENT_BINARY_DIR}/conv-tetra-assetimport.mtl") # hard to track companion files
else()
  set(dep_str "")
endif(TARGET gcv-assetimport)

gcv_regress_util("obj" "tetra.obj" ${dep_str})
gcv_regress_util("obj_unnamed_group" "tetra-unnamed-group.obj" DEPS "gcv;gcv-obj")
gcv_regress_util("obj" "tetra.g" ${dep_str} EXPORTER)
distclean("${CMAKE_CURRENT_BINARY_DIR}/conv-tetra.mtl")

cmakefiles(
  CMakeLists.txt
  tetra.obj
  tetra-unnamed-group.obj
  tetra.mtl	# material file for obj
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
