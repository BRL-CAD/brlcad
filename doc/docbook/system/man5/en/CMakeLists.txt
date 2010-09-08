SET(docbook_man5_EN
	gattributes.xml
)

DOCBOOK_TO_HTML(man5 docbook_man5_EN ${BRLCAD_INSTALL_DATA_DIR}/html/man5/en)
DOCBOOK_TO_MAN(man5 docbook_man5_EN 5 5)
