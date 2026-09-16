set(
  presentations_EN
  Introduction-brlcad-app-devel.adoc
  Introduction-to-tcltk.adoc
  brlcad-app-devel.adoc
  intro-to-tcltk.adoc
)

set(
  presentations_EN_IMAGES
  images/copy-sph-part-left.png
  images/brlcad_app_devel.png
  images/intro_tcl_tk.png
  images/copy-sph-part-right.png
  images/mged-defaults.png
  images/special-character-interpretation.png
  images/tk-based-gui-for-mged.png
)
if(TARGET mged OR TARGET qged OR TARGET archer)
  if(BRLCAD_EXTRADOCS)
    add_doc(presentations_EN_IMAGES html/presentations/images)
    add_asciidoc("HTML" presentations_EN presentations presentations_EN_IMAGES_cp)
  endif(BRLCAD_EXTRADOCS)
endif(TARGET mged OR TARGET qged OR TARGET archer)

cmakefiles(
  README
  CMakeLists.txt
  ${presentations_EN}
  ${presentations_EN_IMAGES}
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
