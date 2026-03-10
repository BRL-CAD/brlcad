set(
  articles_ADOC
  TEMPLATE.adoc
  about.adoc
  build_pattern.adoc
  build_region.adoc
  camo_shader.adoc
  ebm_primitive.adoc
  gcv.adoc
  ged.adoc
  main_menu.adoc
  mged.adoc
  mgedrc.adoc
  nirt.adoc
  oed.adoc
  overlap_tool.adoc
  pipes.adoc
  projection_shader.adoc
  tire.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("HTML" articles_ADOC "" "")
endif(BRLCAD_EXTRADOCS)

cmakefiles(
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
