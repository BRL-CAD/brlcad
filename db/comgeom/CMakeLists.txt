set(COMGEOM_V5_MODELS
  ADA073408.comgeom
)

foreach(comgeom_input ${COMGEOM_V5_MODELS})
  ADD_G_TARGET(${comgeom_input} "BRL-CAD Geometry Models/Sample")
endforeach(comgeom_input ${COMGEOM_V5_MODELS})

CMAKEFILES(
  ${COMGEOM_V5_MODELS}
  CMakeLists.txt
  README_ADA073408
  ADA073408_inputs.tar.gz
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
