SET(XSLTPROC_FLAGS 
	-nonet
	-xinclude
)
SET(XSL_XHTML_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl")
SET(XSL_MAN_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/manpages/docbook.xsl")
SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/fo/docbook.xsl")
SET(XML_CATALOG_FILES "${CMAKE_SOURCE_DIR}/doc/docbook/catalog.xml")

SET(XSLTPROC_EXEC "/usr/bin/xsltproc")

MACRO(DOCBOOK_TO_HTML targetname srcfile outfile targetdir)
	ADD_CUSTOM_COMMAND(
		OUTPUT ${outfile}
		COMMAND XML_CATALOG_FILES=${XML_CATALOG_FILES} ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} ${XSL_XHTML_STYLESHEET} ${srcfile}
		)
	ADD_CUSTOM_TARGET(${targetname}_HTML ALL DEPENDS ${outfile})
	INSTALL(FILES ${outfile} DESTINATION ${targetdir})
ENDMACRO(DOCBOOK_TO_HTML targetname srcfile outfile targetdir)

ADD_SUBDIRECTORY(articles/en)
