SET(docbook_presentations_EN
	intro-to-tcltk.xml
)

SET(docbook_presentations_EN_IMAGES
        copy-sph-part-left.png
        copy-sph-part-right.png
        mged-defaults.png
        special-character-interpretation.png
        tk-based-gui-for-mged.png
)

DOCBOOK_TO_HTML(presentation docbook_presentations_EN html/presentations/en)
BRLCAD_ADDDATA(docbook_presentations_EN_IMAGES html/presentations/en/images)

IF(BRLCAD_BUILD_EXTRADOCS_PDF)
  DOCBOOK_TO_PDF(presentation docbook_presentations_EN pdf/presentations/en)
ENDIF(BRLCAD_BUILD_EXTRADOCS_PDF)
