presentationsDOCBOOK_EN = \
	presentations/en/intro-to-tcltk.xml

presentationsDOCBOOK_EN_IMAGES = \
        presentations/en/images/copy-sph-part-left.png \
        presentations/en/images/copy-sph-part-right.png \
        presentations/en/images/mged-defaults.png \
        presentations/en/images/special-character-interpretation.png \
        presentations/en/images/tk-based-gui-for-mged.png

presentationsDOCBOOK_ENhtmldir = $(BRLCAD_DATA)/html/presentations/en
presentationsDOCBOOK_ENhtml_DATA = \
	$(BUILTPRESENTATIONS_ENHTML)

BUILTPRESENTATIONS_ENHTML = \
	${presentationsDOCBOOK_EN:xml=html}

presentationsDOCBOOK_ENhtmlimagesdir = $(BRLCAD_DATA)/html/presentations/en/images
presentationsDOCBOOK_ENhtmlimages_DATA = \
	$(presentationsDOCBOOK_EN_IMAGES)

if BUILD_PDF_DOCS

BUILTPRESENTATIONS_ENPDF = ${presentationsDOCBOOK_EN:xml=pdf}

endif

presentationsDOCBOOK_ENpdfdir = $(BRLCAD_DATA)/pdf/presentations/en
presentationsDOCBOOK_ENpdf_DATA = $(BUILTPRESENTATIONS_ENPDF)
