set(
  presentations_ADOC
  Introduction-brlcad-app-devel.adoc
  Introduction-to-tcltk.adoc
  brlcad-app-devel.adoc
  intro-to-tcltk.adoc
)

if(BRLCAD_EXTRADOCS)
  ADD_ASCIIDOC("HTML" presentations_ADOC "" "")
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
