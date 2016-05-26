set(implementation_EN
  bool_eval_development.xml
  )

set(implementation_EN_IMAGES
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

ADD_DOC(implementation_EN html/implementation/en)
ADD_DOC(implementation_EN_IMAGES html/implementation/en/images)
ADD_DOCBOOK("HTML;PHP;PDF" implementation_EN implementation/en "")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
