if (TARGET gcv-assetimport)
  set(run_str "obj" "tetra.obj" DEPS "gcv;gcv-obj;gcv-assetimport" CONVERTERS "assetimport")
else ()
  set(run_str "obj" "tetra.obj")
endif (TARGET gcv-assetimport)

GCV_to_g_util(${run_str})

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
