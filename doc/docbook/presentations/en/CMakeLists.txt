set(docbook_presentations_EN
  intro-to-tcltk.xml
  )

set(docbook_presentations_EN_IMAGES
  images/copy-sph-part-left.png
  images/copy-sph-part-right.png
  images/mged-defaults.png
  images/special-character-interpretation.png
  images/tk-based-gui-for-mged.png
  )
BRLCAD_ADDDATA(docbook_presentations_EN_IMAGES ${DOC_DIR}/html/presentations/en/images)

DOCBOOK_TO_HTML(presentation docbook_presentations_EN html/presentations/en docbook_presentations_EN_IMAGES_cp)
DOCBOOK_TO_PDF(presentation docbook_presentations_EN pdf/presentations/en docbook_presentations_EN_IMAGES_cp)
