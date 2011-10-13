SET(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/log4j.properties ${CMAKE_BINARY_DIR}/doc/docbook/log4j.properties COPYONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-xml-catalog.xml.in ${CMAKE_CURRENT_BINARY_DIR}/brlcad-xml-catalog.xml)

SET(XSLTPROC_FLAGS -nonet -xinclude)
SET(XSL_XHTML_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/brlcad-xhtml-stylesheet.xsl")
SET(XSL_MAN_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/brlcad-man-stylesheet.xsl")
# The brlcad/brlcad-fo-stylesheet.xsl stylesheet is for docs with covers
SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/other/standard/xsl/fo/docbook.xsl")
#SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/brlcad-fo-stylesheet.xsl")

MACRO(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.html)
		SET(targetname ${filename_root}_${targetname_suffix}_html)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND
			XML_CATALOG_FILES=${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml
			XML_DEBUG_CATALOG=1
			${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} 
			${XSL_XHTML_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/${targetdir})
	ENDFOREACH(filename ${${xml_files}})
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir)

MACRO(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext targetdir)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.${manext})
		SET(targetname ${filename_root}_${targetname_suffix}_man${mannum})
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND 
			XML_CATALOG_FILES=${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml
			XML_DEBUG_CATALOG=1
			${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} 
			${XSL_MAN_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${MAN_DIR}/man${mannum})
	ENDFOREACH(filename ${${xml_files}})
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir)

# fop hyphenation path (fop version >= 1.0)
# need v2 hphenation
IF(APACHE_FOP_VERSION VERSION_LESS 1.0)
	#SET(FOP_HYPH ${CMAKE_CURRENT_SOURCE_DIR}/resources/other/offo-old/binary/fop-hyph.jar)
ELSE(APACHE_FOP_VERSION VERSION_LESS 1.0)
	SET(FOP_HYPH ${CMAKE_SOURCE_DIR}/doc/docbook/resources/other/offo/binary/fop-hyph.jar)
ENDIF(APACHE_FOP_VERSION VERSION_LESS 1.0)

SET(FOP_OPTS "-Djava.awt.headless=true -Dxml.catalog.files=${CMAKE_CURRENT_BINARY_DIR}/brlcad-xml-catalog.xml")

MACRO(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir)
	IF(BRLCAD_EXTRADOCS_PDF)
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(tmp_fo_file ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
			SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.pdf)
			SET(targetname ${filename_root}_${targetname_suffix}_pdf)
			FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
			ADD_CUSTOM_COMMAND(
				OUTPUT ${tmp_fo_file}
				COMMAND
				XML_CATALOG_FILES=${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml
				XML_DEBUG_CATALOG=1
				${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${tmp_fo_file} 
				${XSL_FO_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
				)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${outfile}
				COMMAND 
				FOP_HYPHENATION_PATH=${FOP_HYPH}
				CLASSPATH=${CMAKE_SOURCE_DIR}/doc/docbook
				FOP_OPTS=\"-Djava.awt.headless=true	-Dxml.catalog.files=${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml\"
				${APACHE_FOP} -c ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf ${tmp_fo_file} -pdf ${outfile}
				DEPENDS ${tmp_fo_file}
				)
			ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_PDF)
ENDMACRO(DOCBOOK_TO_PDF targetname_suffix srcfile outfile targetdir)


# For the html files, we need brlcad.css
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/html/css)
configure_file(css/brlcad.css ${CMAKE_BINARY_DIR}/${DATA_DIR}/html/css/brlcad.css)
install(FILES css/brlcad.css DESTINATION ${DATA_DIR}/html/css)

ADD_SUBDIRECTORY(articles)
ADD_SUBDIRECTORY(books)
ADD_SUBDIRECTORY(lessons)
ADD_SUBDIRECTORY(presentations)
ADD_SUBDIRECTORY(specifications)
ADD_SUBDIRECTORY(system)

CMAKEFILES(Makefile.am README fop.xconf.in log4j.properties resources)
CMAKEFILES(BRLCAD_DB_VALIDATION.pm BRLCAD_DOC.pm DBPATH.pm.in README.DB_authors_notes)
CMAKEFILES(create-book-covers.pl create-index.pl create-xml-catalogs.pl	dummy.xml find-db-files.pl make-svg.sh validate.pl)
CMAKEFILES(css/brlcad.css)
