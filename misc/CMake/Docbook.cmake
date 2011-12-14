# Set up xml validation routines

SET(XMLLINT_EXECUTABLE "${CMAKE_BINARY_DIR}/bin/xmllint")

MACRO(DB_VALIDATE_TARGET targetname filename_root)
	get_filename_component(root1 ${CMAKE_CURRENT_SOURCE_DIR} NAME)
	get_filename_component(path1 ${CMAKE_CURRENT_SOURCE_DIR} PATH)
	get_filename_component(root2 ${path1} NAME)
	SET(validate_target ${root2}_${root1}_${filename_root}_validate)
	SET(db_scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}_validate.cmake)
	SET(db_outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.valid)
	# If we're already set up, no need to do it twice (CMake doesn't like it anyway)
	get_target_property(tarprop ${validate_target} TARGET_NAME)
	IF("${tarprop}" MATCHES "NOTFOUND")
		configure_file(${BRLCAD_SOURCE_DIR}/doc/docbook/xmllint.cmake.in ${db_scriptfile} @ONLY)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${db_outfile}
			COMMAND ${CMAKE_COMMAND} -P ${db_scriptfile}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XMLLINT_EXECUTABLE_TARGET}
			)
		ADD_CUSTOM_TARGET(${validate_target} ALL DEPENDS ${db_outfile})
		set_target_properties(${validate_target} PROPERTIES TARGET_NAME ${validate_target})
	ENDIF("${tarprop}" MATCHES "NOTFOUND")
ENDMACRO(DB_VALIDATE_TARGET)

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


