set(en_pres_ignore_files
  README.txt
  )
CMAKEFILES(${en_pres_ignore_files})

set(docbook_presentations_EN
  intro-to-tcltk.xml
  brlcad-app-devel.xml
  )

set(docbook_presentations_EN_IMAGES
  images/copy-sph-part-left.png
  images/copy-sph-part-right.png
  images/mged-defaults.png
  images/special-character-interpretation.png
  images/tk-based-gui-for-mged.png
  )
ADD_DOC(docbook_presentations_EN_IMAGES html/presentations/en/images)

DOCBOOK_TO_HTML(presentation docbook_presentations_EN html/presentations/en docbook_presentations_EN_IMAGES_cp)
DOCBOOK_TO_PDF(presentation docbook_presentations_EN pdf/presentations/en docbook_presentations_EN_IMAGES_cp)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
