SET(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)

SET(XSLTPROC_FLAGS 
	-nonet
	-xinclude
)
SET(XSL_XHTML_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl")
SET(XSL_MAN_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/manpages/docbook.xsl")
SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/fo/docbook.xsl")
SET(XML_CATALOG_FILES "${CMAKE_SOURCE_DIR}/doc/docbook/catalog.xml")

MACRO(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.html)
		SET(targetname ${filename_root}_${targetname_suffix}_html)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND XML_CATALOG_FILES=${XML_CATALOG_FILES} ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} ${XSL_XHTML_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${targetdir})
	ENDFOREACH(filename ${${xml_files}})
ENDMACRO(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir)

MACRO(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.${manext})
		SET(targetname ${filename_root}_${targetname_suffix}_man${mannum})
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND XML_CATALOG_FILES=${XML_CATALOG_FILES} ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} ${XSL_MAN_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${${CMAKE_PROJECT_NAME}_INSTALL_MAN_DIR}/man${mannum})
	ENDFOREACH(filename ${${xml_files}})
ENDMACRO(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir)

MACRO(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(tmp_fo_file ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
		SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.pdf)
		SET(targetname ${filename_root}_${targetname_suffix}_pdf)
		ADD_CUSTOM_COMMAND(
		        OUTPUT ${tmp_fo_file}
			COMMAND XML_CATALOG_FILES=${XML_CATALOG_FILES} ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${tmp_fo_file} ${XSL_FO_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND ${APACHE_FOP} -c ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf ${tmp_fo_file} -pdf ${outfile}
			DEPENDS ${tmp_fo_file}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${targetdir})
	ENDFOREACH(filename ${${xml_files}})
ENDMACRO(DOCBOOK_TO_PDF targetname_suffix srcfile outfile targetdir)



ADD_SUBDIRECTORY(articles/en)
ADD_SUBDIRECTORY(books/en)
ADD_SUBDIRECTORY(lessons/en)
ADD_SUBDIRECTORY(lessons/es)
ADD_SUBDIRECTORY(specifications/en)
ADD_SUBDIRECTORY(system/man1/en)
ADD_SUBDIRECTORY(system/man3/en)
ADD_SUBDIRECTORY(system/man5/en)
ADD_SUBDIRECTORY(system/mann/en)
