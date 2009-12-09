if ES_DOCS
lessonsDOCBOOK_ES = \
	lessons/es/mged01_crear_figuras_primitivas.xml
	
lessonsDOCBOOK_ES_IMAGES = \
	lessons/es/images/mged01_commandwindow.png \
	lessons/es/images/mged01_graphicswindow.png \
	lessons/es/images/mged01_terminal.png 

lessonsDOCBOOK_EShtmldir = $(BRLCAD_DATA)/html/lessons/es
lessonsDOCBOOK_EShtml_DATA = \
	$(BUILTLESSONS_ESHTML)

BUILTLESSONS_ESHTML = \
	${lessonsDOCBOOK_ES:xml=html}

lessonsDOCBOOK_EShtmlimagesdir = $(BRLCAD_DATA)/html/lessons/es/images
lessonsDOCBOOK_EShtmlimages_DATA = \
	$(lessonsDOCBOOK_ES_IMAGES)

if BUILD_PDF_DOCS

BUILTLESSONS_ESPDF = ${lessonsDOCBOOK_ES:xml=pdf}

endif

lessonsDOCBOOK_ESpdfdir = $(BRLCAD_DATA)/pdf/lessons/es
lessonsDOCBOOK_ESpdf_DATA = $(BUILTLESSONS_ESPDF)
endif
