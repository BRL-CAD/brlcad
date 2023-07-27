if (TARGET gcv-assetimport)
  set(dep_str DEPS "gcv;gcv-obj;gcv-assetimport" CONVERTERS "assetimport")

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/conv-tetra-assetimport.mtl")   # hard to track companion files
else()
  set(dep_str "")
endif (TARGET gcv-assetimport)

GCV_regress_util("obj" "tetra.obj" ${dep_str})
GCV_regress_util("obj" "tetra.g" ${dep_str} EXPORTER)

CMAKEFILES(
  CMakeLists.txt
  tetra.obj
  tetra.mtl	# material file for obj
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
