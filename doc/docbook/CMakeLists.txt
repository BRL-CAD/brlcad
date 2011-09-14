SET(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/log4j.properties ${CMAKE_BINARY_DIR}/doc/docbook/log4j.properties COPYONLY)

SET(XSLTPROC_FLAGS
	-nonet
	-xinclude
)
SET(XSL_XHTML_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl")
SET(XSL_MAN_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/manpages/docbook.xsl")
SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/standard/xsl/fo/docbook.xsl")

MACRO(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir)
	FOREACH(filename ${${xml_files}})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.html)
		SET(targetname ${filename_root}_${targetname_suffix}_html)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} ${XSL_XHTML_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
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
			COMMAND ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${outfile} ${XSL_MAN_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${MAN_DIR}/man${mannum})
	ENDFOREACH(filename ${${xml_files}})
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir)

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
				COMMAND ${XSLTPROC_EXEC} ${XSLTPROC_FLAGS} -o ${tmp_fo_file} ${XSL_FO_STYLESHEET} ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
				)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${outfile}
				COMMAND ${APACHE_FOP} -c ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf ${tmp_fo_file} -pdf ${outfile}
				DEPENDS ${tmp_fo_file}
				)
			ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_PDF)
ENDMACRO(DOCBOOK_TO_PDF targetname_suffix srcfile outfile targetdir)


ADD_SUBDIRECTORY(articles)
ADD_SUBDIRECTORY(books)
ADD_SUBDIRECTORY(lessons)
ADD_SUBDIRECTORY(presentations)
ADD_SUBDIRECTORY(specifications)
ADD_SUBDIRECTORY(system)

CMAKEFILES(Makefile.am README fop.xconf.in log4j.properties resources)
