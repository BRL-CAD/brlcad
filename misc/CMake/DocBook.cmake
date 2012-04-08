#                   D O C B O O K . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2012 United States Government as represented by
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
if(NOT DEFINED VALIDATE_EXECUTABLE)
  set(VALIDATE_EXECUTABLE "xmllint")
else(NOT DEFINED VALIDATE_EXECUTABLE)
  if(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in)
    message(FATAL_ERROR "Specified ${VALIDATE_EXECUTABLE} for DocBook validation, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in does not exist.  To use ${VALIDATE_EXECUTABLE} a ${VALIDATE_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in)
endif(NOT DEFINED VALIDATE_EXECUTABLE)

# Handle default exec and sanity checking for XSLT 
if(NOT DEFINED XSLT_EXECUTABLE)
  set(XSLT_EXECUTABLE "xsltproc")
else(NOT DEFINED XSLT_EXECUTABLE)
  if(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in)
    message(FATAL_ERROR "Specified ${XSLT_EXECUTABLE} for DocBook processing, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in does not exist.  To use ${XSLT_EXECUTABLE} a ${XSLT_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in)
endif(NOT DEFINED XSLT_EXECUTABLE)

# Handle default exec and sanity checking for PDF conversion
if(NOT DEFINED PDF_CONV_EXECUTABLE)
  set(PDF_CONV_EXECUTABLE "fop")
else(NOT DEFINED PDF_CONF_EXECUTABLE)
  if(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in)
    message(FATAL_ERROR "Specified ${PDF_CONF_EXECUTABLE} for DocBook pdf conversion, but ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in does not exist.  To use ${PDF_CONF_EXECUTABLE} a ${PDF_CONF_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS ${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in)
endif(NOT DEFINED PDF_CONV_EXECUTABLE)

if(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

# Handle script generation in both single and multi configuration setups.  While we're at
# it, this is a good place to make sure all the directories we'll be needing exist
# (xsltproc needs the directory to already exist when multiple docs are building in
# parallel.)  If CMAKE_CFG_INTDIR is just the current working directory, then everything
# is flat even if we are multi-config and we proceed as normal.
macro(DB_SCRIPT targetname targetdir executable)
  if(NOT CMAKE_CONFIGURATION_TYPES OR "${CMAKE_CFG_INTDIR}" STREQUAL ".")
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/${DOC_DIR}/${targetdir})
    set(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${targetname}.cmake)
    configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${executable}.cmake.in ${scriptfile} @ONLY)
  else(NOT CMAKE_CONFIGURATION_TYPES OR "${CMAKE_CFG_INTDIR}" STREQUAL ".")
    # Multi-configuration is more complex - for each configuration, the
    # standard variables must reflect the final directory (not the CMAKE_CFG_INTDIR
    # value used by the build tool) but after generating the config specific
    # script the values must be restored to the values using CMAKE_CFG_INTDIR
    # for the generation of flexible build targets.
    foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE})
      file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CFG_TYPE}/${DATA_DIR}/${DOC_DIR}/${targetdir})
      # Temporarily store flexible values
      string(TOUPPER "${executable}" exec_upper)
      set(outfile_tmp "${outfile}")
      set(exec_tmp "${${exec_upper}_EXECUTABLE}")
      # Generate final paths for configure_file
      string(REPLACE "${CMAKE_CFG_INTDIR}" "${CFG_TYPE}" outfile "${outfile}")
      string(REPLACE "${CMAKE_CFG_INTDIR}" "${CFG_TYPE}" ${exec_upper}_EXECUTABLE "${${exec_upper}_EXECUTABLE}")
      string(REPLACE "${CMAKE_CFG_INTDIR}" "${CFG_TYPE}" executable "${executable}")
      set(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${CFG_TYPE}/${targetname}.cmake)
      configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/${executable}.cmake.in ${scriptfile} @ONLY)
      # Restore flexible values
      set(outfile "${outfile_tmp}")
      set(${exec_upper}_EXECUTABLE "${exec_tmp}")
    endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    set(scriptfile ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${targetname}.cmake)
  endif(NOT CMAKE_CONFIGURATION_TYPES OR "${CMAKE_CFG_INTDIR}" STREQUAL ".")
endmacro(DB_SCRIPT) 

# Macro to define individual validation build targets and generate the script files 
# used to run the validation step during build
macro(DB_VALIDATE_TARGET targetname filename_root)
  # Man page root names are not unique (and other documents' root names are not guaranteed to be
  # unique) - incorporate the two parent dirs into things like target names for uniqueness
  get_filename_component(root1 ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  get_filename_component(path1 ${CMAKE_CURRENT_SOURCE_DIR} PATH)
  get_filename_component(root2 ${path1} NAME)
  set(validate_target ${root2}_${root1}_${filename_root}_validate)
  set(db_outfile ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${filename_root}.valid)
  # If we're already set up, no need to do it twice (CMake doesn't like it anyway)
  # Handle this by getting a TARGET_NAME property on the target that we set when 
  # creating the target. If the target doesn't exist yet, the get returns NOTFOUND
  # and we know to create the target.  Otherwise, the target exists and this xml
  # file has handled - skip it.
  get_target_property(tarprop ${validate_target} TARGET_NAME)
  if("${tarprop}" MATCHES "NOTFOUND")
    DB_SCRIPT("${validate_target}" "${VALIDATE_EXECUTABLE}")
    add_custom_command(
      OUTPUT ${db_outfile}
      COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XMLLINT_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
      COMMENT "Validating DocBook source with ${VALIDATE_EXECUTABLE}:"
      )
    add_custom_target(${validate_target} ALL DEPENDS ${db_outfile})
    set_target_properties(${validate_target} PROPERTIES TARGET_NAME ${validate_target})
  endif("${tarprop}" MATCHES "NOTFOUND")
endmacro(DB_VALIDATE_TARGET)

# HTML output, the format used by BRL-CAD's graphical help systems
macro(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir deps_list)
  if(BRLCAD_EXTRADOCS_HTML)
    foreach(filename ${${xml_files}})
      string(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
      set(outfile ${bin_root}/${DATA_DIR}/${DOC_DIR}/${targetdir}/${filename_root}.html)
      set(targetname ${filename_root}_${targetname_suffix}_html)
      set(CURRENT_XSL_STYLESHEET ${XSL_XHTML_STYLESHEET})
      DB_SCRIPT("${targetname}" "${targetdir}" "${XSLT_EXECUTABLE}")
      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetname} ${filename_root})
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_XHTML_STYLESHEET} ${deps_list}
	  )
      else(BRLCAD_EXTRADOCS_VALIDATE)
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_XHTML_STYLESHEET} ${deps_list}
	  )
      endif(BRLCAD_EXTRADOCS_VALIDATE)
      add_custom_target(${targetname} ALL DEPENDS ${outfile})
      # CMAKE_CFG_INTDIR can't be used in installation rules:
      # http://www.cmake.org/Bug/view.php?id=5747
      if(CMAKE_CONFIGURATION_TYPES)
        string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
      endif(CMAKE_CONFIGURATION_TYPES)
      install(FILES ${outfile} DESTINATION ${DATA_DIR}/${DOC_DIR}/${targetdir})
      get_property(BRLCAD_EXTRADOCS_HTML_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_HTML_TARGETS)
      set(BRLCAD_EXTRADOCS_HTML_TARGETS ${BRLCAD_EXTRADOCS_HTML_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_HTML_TARGETS "${BRLCAD_EXTRADOCS_HTML_TARGETS}")
    endforeach(filename ${${xml_files}})
  endif(BRLCAD_EXTRADOCS_HTML)
  CMAKEFILES(${${xml_files}})
endmacro(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir deps_list)

# This macro produces Unix-syle manual or "man" pages
macro(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext targetdir deps_list)
  if(BRLCAD_EXTRADOCS_MAN)
    foreach(filename ${${xml_files}})
      string(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
      set(outfile ${bin_root}/${DATA_DIR}/${targetdir}/${filename_root}.${manext})
      set(targetname ${filename_root}_${targetname_suffix}_man)
      set(CURRENT_XSL_STYLESHEET ${XSL_MAN_STYLESHEET})
      DB_SCRIPT("${targetname}" "${targetdir}" "${XSLT_EXECUTABLE}")
      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetname} ${filename_root})
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_MAN_STYLESHEET} ${deps_list}
	  )
      else(BRLCAD_EXTRADOCS_VALIDATE)
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_MAN_STYLESHEET} ${deps_list}
	  )
      endif(BRLCAD_EXTRADOCS_VALIDATE)
      add_custom_target(${targetname} ALL DEPENDS ${outfile})
      # CMAKE_CFG_INTDIR can't be used in installation rules:
      # http://www.cmake.org/Bug/view.php?id=5747
      if(CMAKE_CONFIGURATION_TYPES)
        string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
      endif(CMAKE_CONFIGURATION_TYPES)
      install(FILES ${outfile} DESTINATION ${MAN_DIR}/man${mannum})
      get_property(BRLCAD_EXTRADOCS_MAN_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_MAN_TARGETS)
      set(BRLCAD_EXTRADOCS_MAN_TARGETS ${BRLCAD_EXTRADOCS_MAN_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_MAN_TARGETS "${BRLCAD_EXTRADOCS_MAN_TARGETS}")
    endforeach(filename ${${xml_files}})
  endif(BRLCAD_EXTRADOCS_MAN)
  CMAKEFILES(${${xml_files}})
endmacro(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir deps_list)

# PDF output is generated in a two stage process - the XML file is first
# converted to an "FO" file, and the FO file is in turn translated to PDF.
macro(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir deps_list)
  if(BRLCAD_EXTRADOCS_PDF)
    foreach(filename ${${xml_files}})
      string(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
      set(targetname ${filename_root}_${targetname_suffix}_fo)
      if(CMAKE_CONFIGURATION_TYPES)
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})
	set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${filename_root}.fo)
      else(CMAKE_CONFIGURATION_TYPES)
        set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
      endif(CMAKE_CONFIGURATION_TYPES)
      set(fo_outfile ${outfile})
      set(CURRENT_XSL_STYLESHEET ${XSL_FO_STYLESHEET})
      DB_SCRIPT("${targetname}" "${targetdir}" "${XSLT_EXECUTABLE}")
      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetname} ${filename_root})
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${db_outfile} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_FO_STYLESHEET} ${deps_list}
	  )
      else(BRLCAD_EXTRADOCS_VALIDATE)
	add_custom_command(
	  OUTPUT ${outfile}
	  COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_FO_STYLESHEET} ${deps_list}
	  )
      endif(BRLCAD_EXTRADOCS_VALIDATE)
      set(targetname ${filename_root}_${targetname_suffix}_pdf)
      set(outfile ${bin_root}/${DATA_DIR}/${DOC_DIR}/${targetdir}/${filename_root}.pdf)
      DB_SCRIPT("${targetname}_pdf" "${targetdir}" "${PDF_CONV_EXECUTABLE}")
      add_custom_command(
	OUTPUT ${outfile}
	COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	DEPENDS ${fo_outfile} ${DOCBOOK_RESOURCE_FILES} ${deps_list}
	)
      add_custom_target(${targetname} ALL DEPENDS ${outfile})
      # CMAKE_CFG_INTDIR can't be used in installation rules:
      # http://www.cmake.org/Bug/view.php?id=5747
      if(CMAKE_CONFIGURATION_TYPES)
        string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
      endif(CMAKE_CONFIGURATION_TYPES)
      install(FILES ${outfile} DESTINATION ${DATA_DIR}/${DOC_DIR}/${targetdir})
      get_property(BRLCAD_EXTRADOCS_PDF_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS)
      set(BRLCAD_EXTRADOCS_PDF_TARGETS ${BRLCAD_EXTRADOCS_PDF_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS "${BRLCAD_EXTRADOCS_PDF_TARGETS}") 
    endforeach(filename ${${xml_files}})
  endif(BRLCAD_EXTRADOCS_PDF)
  CMAKEFILES(${${xml_files}})
endmacro(DOCBOOK_TO_PDF targetname_suffix srcfile outfile targetdir deps_list)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
