set(
  books_ADOC
  BRL-CAD_Tutorial_Series-VolumeI.adoc
  BRL-CAD_Tutorial_Series-VolumeII.adoc
  BRL-CAD_Tutorial_Series-VolumeIII.adoc
  BRL-CAD_Tutorial_Series-VolumeIV.adoc
  HACKING_BRL-CAD.adoc
  tutorial_series_authors.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("HTML" books_ADOC "" "")
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
