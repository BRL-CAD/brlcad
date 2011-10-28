man1DOCBOOK_EN = \
	system/man1/en/bwscale.xml \
	system/man1/en/coil.xml \
	system/man1/en/db.xml \
	system/man1/en/gqa.xml \
	system/man1/en/mged.xml \
	system/man1/en/nirt.xml \
	system/man1/en/obj-g.xml \
	system/man1/en/rt.xml \
	system/man1/en/rtarea.xml \
	system/man1/en/rtcheck.xml \
	system/man1/en/rtedge.xml \
	system/man1/en/tire.xml

man1DOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/man1/en
man1DOCBOOK_ENhtml_DATA = $(BUILTMAN1_ENHTML)

BUILTMAN1_ENHTML = \
	${man1DOCBOOK_EN:xml=html}

if BUILD_PDF_DOCS

BUILTMAN1_ENPDF = ${man1DOCBOOK_EN:xml=pdf}

endif

man1DOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/man1/en
man1DOCBOOK_ENpdf_DATA = $(BUILTMAN1_ENPDF)
