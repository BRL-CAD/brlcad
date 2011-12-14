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
