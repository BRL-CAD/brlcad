set(
  devguides_EN
  bool_eval_development.adoc
)

set(
  devguides_EN_IMAGES
  images/ccx_overlap_event.png
  images/ssx_transverse_event.png
  images/ssx_tangent_event.png
  images/compare_endpoint_style.png
  images/lcurves_with_shaded_context.png
  images/ssx_10_vs_13.png
  images/curve_traversal_directions.png
  images/axis_X.png
  images/intermediate_linked_curves.png
  images/ssx_overlap_event.png
  images/evaluation_overview.png
)

if(TARGET mged OR TARGET qged OR TARGET archer)
  if(BRLCAD_EXTRADOCS)
    add_doc(devguides_EN_IMAGES html/devguides/images)
    add_asciidoc("HTML" devguides_EN devguides devguides_EN_IMAGES_cp)
  endif(BRLCAD_EXTRADOCS)
endif(TARGET mged OR TARGET qged OR TARGET archer)

cmakefiles(
  CMakeLists.txt
  ${devguides_EN}
  ${devguides_EN_IMAGES}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
