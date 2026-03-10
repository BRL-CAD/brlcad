set(
  system_man5_ADOC
  STEP.adoc
  benchmark.adoc
  bw.adoc
  cmap.adoc
  dsp.adoc
  idents.adoc
  nirt_formatting.adoc
  pix.adoc
  plot3.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("MAN5" system_man5_ADOC "" "")
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
