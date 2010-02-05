mannDOCBOOK_EN = \
	system/mann/en/apropos.xml \
	system/mann/en/attr.xml \
	system/mann/en/cat.xml \
	system/mann/en/cp.xml 

mannDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/mann/en
mannDOCBOOK_ENhtml_DATA = $(BUILTMAN_ENHTML)

BUILTMANN_ENHTML = \
	${mannDOCBOOK_EN:xml=html} \
	${mannINTRO_EN:xml=html} 

if BUILD_PDF_DOCS

BUILTMANN_ENPDF = ${man1DOCBOOK_EN:xml=pdf}

endif

mannDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/mann/en
mannDOCBOOK_ENpdf_DATA = $(BUILTMANN_ENPDF)
