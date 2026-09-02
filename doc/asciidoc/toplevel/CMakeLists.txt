set(
  toplevel_NAV
  main_menu.adoc
  tutorials.adoc
  guides.adoc
  developer.adoc
  command_reference.adoc
  man1_index.adoc
  mann_index.adoc
  man3_index.adoc
  formats.adoc
)
if(TARGET mged OR TARGET qged OR TARGET archer)
  if(BRLCAD_EXTRADOCS)
    add_asciidoc("HTML" toplevel_NAV "" "")
  endif(BRLCAD_EXTRADOCS)
endif(TARGET mged OR TARGET qged OR TARGET archer)

cmakefiles(
  CMakeLists.txt
  ${toplevel_NAV}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
