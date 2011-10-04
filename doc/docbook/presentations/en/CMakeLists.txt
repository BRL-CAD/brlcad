SET(docbook_presentations_EN
	intro-to-tcltk.xml
	)

SET(docbook_presentations_EN_IMAGES
	images/copy-sph-part-left.png
	images/copy-sph-part-right.png
	images/mged-defaults.png
	images/special-character-interpretation.png
	images/tk-based-gui-for-mged.png
	)

DOCBOOK_TO_HTML(presentation docbook_presentations_EN html/presentations/en)
BRLCAD_ADDDATA(docbook_presentations_EN_IMAGES html/presentations/en/images)

IF(BRLCAD_EXTRADOCS_PDF)
	DOCBOOK_TO_PDF(presentation docbook_presentations_EN pdf/presentations/en)
ENDIF(BRLCAD_EXTRADOCS_PDF)
CMAKEFILES(Makefile.am)
