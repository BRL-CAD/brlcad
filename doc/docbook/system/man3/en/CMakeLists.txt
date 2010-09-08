SET(docbook_man3_EN
	libfb.xml
)

DOCBOOK_TO_HTML(man3 docbook_man3_EN ${BRLCAD_INSTALL_DATA_DIR}/html/man3/en)
DOCBOOK_TO_MAN(man3 docbook_man3_EN 3 3)

