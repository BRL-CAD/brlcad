
# We need various configuration files set up for Docbook processing tools -
# deal with those first
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/)

# Style sheet for XSLT transformation to HTML pages
IF(BRLCAD_EXTRADOCS_HTML)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl.in
		${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl)
ENDIF(BRLCAD_EXTRADOCS_HTML)

# Style sheet for XSLT transformation to manual pages
IF(BRLCAD_EXTRADOCS_MAN)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl)
ENDIF(BRLCAD_EXTRADOCS_MAN)

# If we're doing PDF output, Apache FOP needs a couple of config files set up
IF(BRLCAD_EXTRADOCS_PDF)
	SET(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
	configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/log4j.properties ${CMAKE_BINARY_DIR}/doc/docbook/log4j.properties COPYONLY)
ENDIF(BRLCAD_EXTRADOCS_PDF)

# For HTML, MAN and FO (FO is an intermediate file used in the XML->PDF
# transformation) we use variables to hold the full stylesheet path. Because
# BRL-CAD has not (thus far) needed to customize it's FO stylesheets for most
# situations, we used the standard XSL FO stylesheet.  In the case we DO need
# to customize FO stylesheets (the BRL-CAD manuals in books/en) we have
# separate CMake logic in books/en/CMakeLists.txt to handle the customization.
SET(XSL_XHTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl")
SET(XSL_MAN_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl")
SET(XSL_FO_STYLESHEET "${CMAKE_SOURCE_DIR}/doc/docbook/resources/other/standard/xsl/fo/docbook.xsl")

# Include CMake macros for Docbook.
include(${BRLCAD_SOURCE_DIR}/misc/CMake/Docbook.cmake)

MACRO(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir)
	IF(BRLCAD_EXTRADOCS_HTML)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.html)
			SET(targetname ${filename_root}_${targetname_suffix}_html)
			SET(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_XHTML_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/doc/docbook/xsltproc.cmake.in	${scriptfile} @ONLY)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${outfile}
				COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET}
				)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile} ${db_outfile})
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_HTML)
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir)

MACRO(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext targetdir)
	IF(BRLCAD_EXTRADOCS_MAN)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.${manext})
			SET(targetname ${filename_root}_${targetname_suffix}_man${mannum})
			SET(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_MAN_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/doc/docbook/xsltproc.cmake.in ${scriptfile} @ONLY)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${outfile}
				COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET}
				)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile} ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.valid)
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			INSTALL(FILES ${outfile} DESTINATION ${MAN_DIR}/man${mannum})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_MAN)
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir)

MACRO(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir)
	IF(BRLCAD_EXTRADOCS_PDF)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(targetname ${filename_root}_${targetname_suffix}_pdf)
			SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
			SET(scriptfile1 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_fo.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_FO_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/doc/docbook/xsltproc.cmake.in ${scriptfile1} @ONLY)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${outfile}
				COMMAND ${CMAKE_COMMAND} -P ${scriptfile1}
				DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}	${XSLTPROC_EXECUTABLE_TARGET}
				)
			SET(pdf_outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.pdf)
			SET(scriptfile2 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_pdf.cmake)
			configure_file(${BRLCAD_SOURCE_DIR}/doc/docbook/fop.cmake.in ${scriptfile2} @ONLY)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${pdf_outfile}
				COMMAND ${CMAKE_COMMAND} -P ${scriptfile2}
				DEPENDS ${outfile}
				)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile} ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.valid)
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${pdf_outfile})
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			INSTALL(FILES ${pdf_outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_PDF)
	CMAKEFILES(${${xml_files}})
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

CMAKEFILES(README fop.xconf.in fop.cmake.in xsltproc.cmake.in log4j.properties resources)
CMAKEFILES(README.DB_authors_notes)
CMAKEFILES(validate.pl)
CMAKEFILES(css/brlcad.css)
