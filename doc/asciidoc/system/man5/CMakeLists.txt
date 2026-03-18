if(BRLCAD_EXTRADOCS)
  add_asciidoc("MAN5" STEP.adoc            man5 "" REQUIRED "step-g")
  add_asciidoc("MAN5" attributes.adoc      man5 "" REQUIRED "librt")
  add_asciidoc("MAN5" benchmark.adoc       man5 "" REQUIRED "benchmark")
  add_asciidoc("MAN5" bw.adoc              man5 "" REQUIRED "libicv")
  add_asciidoc("MAN5" cmap.adoc            man5 "" REQUIRED "libdm")
  add_asciidoc("MAN5" dsp.adoc             man5 "" REQUIRED "librt")
  add_asciidoc("MAN5" idents.adoc          man5 "" REQUIRED "libged")
  add_asciidoc("MAN5" nirt_formatting.adoc man5 "" REQUIRED "libanalyze")
  add_asciidoc("MAN5" pix.adoc             man5 "" REQUIRED "libicv")
  add_asciidoc("MAN5" plot3.adoc           man5 "" REQUIRED "libbg")
endif(BRLCAD_EXTRADOCS)

cmakefiles(
  CMakeLists.txt
  STEP.adoc
  attributes.adoc
  benchmark.adoc
  bw.adoc
  cmap.adoc
  dsp.adoc
  idents.adoc
  nirt_formatting.adoc
  pix.adoc
  plot3.adoc
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
