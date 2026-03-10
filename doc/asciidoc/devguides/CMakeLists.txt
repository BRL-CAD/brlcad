set(
  devguides_ADOC
  bool_eval_development.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("HTML" devguides_ADOC "" "")
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
