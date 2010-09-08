SET(docbook_specifications_EN
	BRL_CAD_g_format_V5.xml
)

DOCBOOK_TO_HTML(specification docbook_specifications_EN ${BRLCAD_INSTALL_DATA_DIR}/html/specifications/en)
