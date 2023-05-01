if (TARGET gcv-assetimport)
  set(dep_str DEPS "gcv;gcv-stl;gcv-assetimport" CONVERTERS "assetimport")
else ()
  set(dep_str "")
endif (TARGET gcv-assetimport)

GCV_regress_util("stl" "tetra.stl" ${dep_str} EXPECTED_FILE_SIZE "700")
GCV_regress_util("stl" "tetra.g" ${dep_str} EXPORTER)

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
