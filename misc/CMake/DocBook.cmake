#                   D O C B O O K . C M A K E
# BRL-CAD
#
# Copyright (c) 2011 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# In principle, DocBook conversion and validation can be accomplished
# with multiple programs.  BRL-CAD's CMake logic uses variables to
# hold the "active" tools for each conversion operation, but will
# set defaults if the user does not manually set them.  

# If a user wishes to use their own validation and/or conversion
# tools, they can set the following variables to their executable 
# names and create <exec_name>.cmake.in files in misc/CMake.  To work,
# the cmake.in files will need to produce the same validity "stamp"
# files and fatal errors as the default tools upon success or failure.  
# For a worked example, see rnv.cmake.in - to test it, install rnv from 
# http://sourceforge.net/projects/rnv/ and configure BRL-CAD as follows:
#
# cmake .. -DBRLCAD_EXTRADOCS_VALIDATE=ON -DVALIDATE_EXECUTABLE=rnv
#
# Note that rnv must be in the system path for this to work.


# Handle default exec and sanity checking for XML validation
IF(NOT DEFINED VALIDATE_EXECUTABLE)
	SET(VALIDATE_EXECUTABLE "xmllint")
ELSE(NOT DEFINED VALIDATE_EXECUTABLE)
	IF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in)
		MESSAGE(FATAL_ERROR "Specified ${VALIDATE_EXECUTABLE} for DocBook validation, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in does not exist.  To use ${VALIDATE_EXECUTABLE} a ${VALIDATE_EXECUTABLE}.cmake.in file must be defined.")
	ENDIF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in)
ENDIF(NOT DEFINED VALIDATE_EXECUTABLE)

# Handle default exec and sanity checking for XSLT 
IF(NOT DEFINED XSLT_EXECUTABLE)
	SET(XSLT_EXECUTABLE "xsltproc")
ELSE(NOT DEFINED XSLT_EXECUTABLE)
	IF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in)
		MESSAGE(FATAL_ERROR "Specified ${XSLT_EXECUTABLE} for DocBook processing, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in does not exist.  To use ${XSLT_EXECUTABLE} a ${XSLT_EXECUTABLE}.cmake.in file must be defined.")
	ENDIF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in)
ENDIF(NOT DEFINED XSLT_EXECUTABLE)

# Handle default exec and sanity checking for PDF conversion
IF(NOT DEFINED PDF_CONV_EXECUTABLE)
	SET(PDF_CONV_EXECUTABLE "fop")
ELSE(NOT DEFINED PDF_CONF_EXECUTABLE)
	IF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in)
		MESSAGE(FATAL_ERROR "Specified ${PDF_CONF_EXECUTABLE} for DocBook pdf conversion, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in does not exist.  To use ${PDF_CONF_EXECUTABLE} a ${PDF_CONF_EXECUTABLE}.cmake.in file must be defined.")
	ENDIF(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in)
ENDIF(NOT DEFINED PDF_CONV_EXECUTABLE)

# Macro to define individual validation build targets and generate the script files 
# used to run the validation step during build
MACRO(DB_VALIDATE_TARGET targetname filename_root)
	# Man page root names are not unique (and other documents' root names are not guaranteed to be
	# unique) - incorporate the two parent dirs into things like target names for uniqueness
	get_filename_component(root1 ${CMAKE_CURRENT_SOURCE_DIR} NAME)
	get_filename_component(path1 ${CMAKE_CURRENT_SOURCE_DIR} PATH)
	get_filename_component(root2 ${path1} NAME)
	SET(validate_target ${root2}_${root1}_${filename_root}_validate)
	SET(db_scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}_validate.cmake)
	SET(db_outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.valid)
	# If we're already set up, no need to do it twice (CMake doesn't like it anyway)
	# Handle this by getting a TARGET_NAME property on the target that we set when 
	# creating the target. If the target doesn't exist yet, the get returns NOTFOUND
	# and we know to create the target.  Otherwise, the target exists and this xml
	# file has handled - skip it.
	get_target_property(tarprop ${validate_target} TARGET_NAME)
	IF("${tarprop}" MATCHES "NOTFOUND")
		configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in ${db_scriptfile} @ONLY)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${db_outfile}
			COMMAND ${CMAKE_COMMAND} -P ${db_scriptfile}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XMLLINT_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
			COMMENT "Validating DocBook source with ${VALIDATE_EXECUTABLE}:"
			)
		ADD_CUSTOM_TARGET(${validate_target} ALL DEPENDS ${db_outfile})
		set_target_properties(${validate_target} PROPERTIES TARGET_NAME ${validate_target})
	ENDIF("${tarprop}" MATCHES "NOTFOUND")
ENDMACRO(DB_VALIDATE_TARGET)

# HTML output, the format used by BRL-CAD's graphical help systems
MACRO(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir)
	IF(BRLCAD_EXTRADOCS_HTML)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.html)
			SET(targetname ${filename_root}_${targetname_suffix}_html)
			SET(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_XHTML_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in ${scriptfile} @ONLY)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_HTML)
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir)

# This macro produces Unix-syle manual or "man" pages
MACRO(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext targetdir)
	IF(BRLCAD_EXTRADOCS_MAN)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.${manext})
			SET(targetname ${filename_root}_${targetname_suffix}_man${mannum})
			SET(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_MAN_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in ${scriptfile} @ONLY)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
			INSTALL(FILES ${outfile} DESTINATION ${MAN_DIR}/man${mannum})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_MAN)
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir)

# PDF output is generated in a two stage process - the XML file is first
# converted to an "FO" file, and the FO file is in turn translated to PDF.
MACRO(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir)
	IF(BRLCAD_EXTRADOCS_PDF)
		SET(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
		FOREACH(filename ${${xml_files}})
			STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
			SET(targetname ${filename_root}_${targetname_suffix}_pdf)
			SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
			SET(scriptfile1 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_fo.cmake)
			SET(CURRENT_XSL_STYLESHEET ${XSL_FO_STYLESHEET})
			configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in ${scriptfile1} @ONLY)
			IF(BRLCAD_EXTRADOCS_VALIDATE)
				DB_VALIDATE_TARGET(${targetname} ${filename_root})
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile1}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ELSE(BRLCAD_EXTRADOCS_VALIDATE)
				ADD_CUSTOM_COMMAND(
					OUTPUT ${outfile}
					COMMAND ${CMAKE_COMMAND} -P ${scriptfile1}
					DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}	${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
					)
			ENDIF(BRLCAD_EXTRADOCS_VALIDATE)
			SET(pdf_outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.pdf)
			SET(scriptfile2 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_pdf.cmake)
			configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONV_EXECUTABLE}.cmake.in ${scriptfile2} @ONLY)
			ADD_CUSTOM_COMMAND(
				OUTPUT ${pdf_outfile}
				COMMAND ${CMAKE_COMMAND} -P ${scriptfile2}
				DEPENDS ${outfile} ${DOCBOOK_RESOURCE_FILES}
				)
			ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${pdf_outfile})
			INSTALL(FILES ${pdf_outfile} DESTINATION ${DATA_DIR}/${targetdir})
		ENDFOREACH(filename ${${xml_files}})
	ENDIF(BRLCAD_EXTRADOCS_PDF)
	CMAKEFILES(${${xml_files}})
ENDMACRO(DOCBOOK_TO_PDF targetname_suffix srcfile outfile targetdir)


