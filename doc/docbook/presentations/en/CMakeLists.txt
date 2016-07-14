set(presentations_EN
  intro-to-tcltk.xml
  brlcad-app-devel.xml
  Introduction-to-tcltk.xml
  Introduction-brlcad-app-devel.xml
  )

set(presentations_EN_IMAGES
  images/copy-sph-part-left.png
  images/brlcad_app_devel.png
  images/intro_tcl_tk.png
  images/copy-sph-part-right.png
  images/mged-defaults.png
  images/special-character-interpretation.png
  images/tk-based-gui-for-mged.png
  )
ADD_DOC(presentations_EN_IMAGES html/presentations/en/images)
ADD_DOCBOOK("HTML;PHP;PPT;PDF" presentations_EN presentations/en presentations_EN_IMAGES_cp)

CMAKEFILES(README.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
