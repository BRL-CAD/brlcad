SET(docbook_man1_EN
	bwscale.xml 
	coil.xml 
	db.xml 
	gqa.xml 
	mged.xml 
	nirt.xml 
	obj-g.xml 
	rt.xml 
	rtarea.xml 
	rtcheck.xml 
	rtedge.xml 
	tire.xml 
)

DOCBOOK_TO_HTML(man1 docbook_man1_EN ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/html/man1/en)
DOCBOOK_TO_MAN(man1 docbook_man1_EN 1 1)

IF(BRLCAD-BUILD_EXTRADOCS_PDF_MAN)
  DOCBOOK_TO_PDF(man1 docbook_man1_EN ${${CMAKE_PROJECT_NAME}_INSTALL_DATA_DIR}/pdf/man1/en)
ENDIF(BRLCAD-BUILD_EXTRADOCS_PDF_MAN)
