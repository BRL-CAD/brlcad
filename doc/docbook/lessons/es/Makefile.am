lessonsDOCBOOK_ES = \
	lessons/es/mged01_crear_figuras_primitivas.xml \
	lessons/es/mged02_opciones_vistas.xml \
	lessons/es/mged03_utilizar_comando_in.xml \
	lessons/es/mged06_crear_una_copa.xml \
	lessons/es/mged12_taza_desde_gui.xml

lessonsDOCBOOK_ES_IMAGES = \
	lessons/es/images/mged01_commandwindow.png \
	lessons/es/images/mged01_graphicswindow.png \
	lessons/es/images/mged01_terminal.png \
	lessons/es/images/mged02_coordsys.png \
	lessons/es/images/mged02_defaultradio.png \
	lessons/es/images/mged02_flower_zooomin.png \
	lessons/es/images/mged02_flower_zooomout.png \
	lessons/es/images/mged02_multipane.png \
	lessons/es/images/mged02_radio_35_25.png \
	lessons/es/images/mged02_radio_front.png \
	lessons/es/images/mged02_radio_right.png \
	lessons/es/images/mged02_radio_top.png \
	lessons/es/images/mged02_truck_0_0_14.png \
	lessons/es/images/mged02_truck_270_-90.png \
	lessons/es/images/mged02_truck_270_90.png \
	lessons/es/images/mged02_truck_35_0.png \
	lessons/es/images/mged02_truck_35_25.png \
	lessons/es/images/mged02_truck_35_60.png \
	lessons/es/images/mged02_truck_35_90.png \
	lessons/es/images/mged02_truck_front.png \
	lessons/es/images/mged02_truck_left.png \
	lessons/es/images/mged02_truck_rear.png \
	lessons/es/images/mged02_truck_right.png \
	lessons/es/images/mged06_base_substracted_vol.png \
	lessons/es/images/mged06_goblet1stsphere.png \
	lessons/es/images/mged06_gobletbase.png \
	lessons/es/images/mged06_gobletbasestembasin.png \
	lessons/es/images/mged06_gobletstem.png \
	lessons/es/images/mged06_rtgobletaz_35_el_25.png \
	lessons/es/images/mged06_rtgoblet.png

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
