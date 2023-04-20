if (TARGET gcv-assetimport)
  set(run_str DEPS "gcv;gcv-obj;gcv-assetimport" CONVERTERS "assetimport;obj")

  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/conv-tetra.mtl")   # hard to track companion files
else()
  set(run_str "")
endif (TARGET gcv-assetimport)

GCV_regress_util("obj" "tetra.obj" ${run_str})
GCV_regress_util("obj" "tetra.g" ${run_str} EXPORTER)

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
