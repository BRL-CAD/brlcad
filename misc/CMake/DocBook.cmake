#                   D O C B O O K . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2014 United States Government as represented by
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

# Get our root path
if(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

# xsltproc is finicky about slashes in names - do some
# sanity scrubbing of the full root path string in
# preparation for generating DocBook scripts
string(REGEX REPLACE "/+" "/" bin_root "${bin_root}")
string(REGEX REPLACE "/$" "" bin_root "${bin_root}")

# Handle script generation in both single and multi configuration setups.  While we're at
# it, this is a good place to make sure all the directories we'll be needing exist
# (xsltproc needs the directory to already exist when multiple docs are building in
# parallel.)  If CMAKE_CFG_INTDIR is just the current working directory, then everything
# is flat even if we are multi-config and we proceed as normal.
macro(DB_SCRIPT targetname outdir executable)
  if(NOT CMAKE_CONFIGURATION_TYPES OR "${CMAKE_CFG_INTDIR}" STREQUAL ".")
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${outdir})
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
      file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${CFG_TYPE}/${outdir})
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

# Macro to define individual validation commands generate the script files
# used to run the validation step during build

define_property(GLOBAL PROPERTY DB_VALIDATION_FILE_LIST BRIEF_DOCS "DocBook files to validate" FULL_DOCS "List used to track which files already have a validation command set up.")

macro(DB_VALIDATE_TARGET targetdir filename filename_root)

  # Regardless of whether the command is defined or not, we'll need xml_valid_stamp set
  set(xml_valid_stamp ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${filename_root}.valid)
  # If we have already added a validation command for this file, don't add another one.  Otherwise,
  # proceed to add a new custom command
  get_property(DB_VALIDATION_FILE_LIST GLOBAL PROPERTY DB_VALIDATION_FILE_LIST)
  set(IN_LIST)
  if (EXISTS ${filename})
    set(full_path_filename ${filename})
  else (EXISTS ${filename})
    set(full_path_filename ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
  endif (EXISTS ${filename})
  list(FIND DB_VALIDATION_FILE_LIST "${full_path_filename}" IN_LIST)
  if("${IN_LIST}" STREQUAL "-1")
    set_property(GLOBAL APPEND PROPERTY DB_VALIDATION_FILE_LIST "${full_path_filename}")

    DB_SCRIPT("${filename_root}_validate" "${DOC_DIR}/${targetdir}" "${VALIDATE_EXECUTABLE}")
    add_custom_command(
      OUTPUT ${xml_valid_stamp}
      COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
      DEPENDS ${full_path_filename} ${XMLLINT_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES}
      COMMENT "Validating DocBook source with ${VALIDATE_EXECUTABLE}:"
      )
  endif("${IN_LIST}" STREQUAL "-1")
endmacro(DB_VALIDATE_TARGET)

# HTML output, the format used by BRL-CAD's graphical help systems
macro(DOCBOOK_TO_HTML targetname_suffix xml_files targetdir deps_list)
  set(xml_valid_stamp)
  if(BRLCAD_EXTRADOCS_HTML)
    foreach(filename ${${xml_files}})
      get_filename_component(filename_root "${filename}" NAME_WE)
      set(outfile ${bin_root}/${DOC_DIR}/${targetdir}/${filename_root}.html)
      set(targetname ${filename_root}_${targetname_suffix}_html)
      set(CURRENT_XSL_STYLESHEET ${XSL_XHTML_STYLESHEET})
      if (EXISTS ${filename})
	set(full_path_filename ${filename})
      else (EXISTS ${filename})
	set(full_path_filename ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
      endif (EXISTS ${filename})

      # If we have extra outputs, need to handle them now
      set(EXTRAS)
      get_property(EXTRA_OUTPUTS SOURCE ${filename} PROPERTY EXTRA_HTML_OUTPUTS)
      foreach(extra_out ${EXTRA_OUTPUTS})
	set(EXTRAS ${EXTRAS} ${bin_root}/${DOC_DIR}/${targetdir}/${extra_out})
      endforeach(extra_out ${EXTRA_OUTPUTS})

      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetdir} "${filename}" ${filename_root})
      endif(BRLCAD_EXTRADOCS_VALIDATE)

      # Generate the script that will be used to run the XSLT executable
      DB_SCRIPT("${targetname}" "${DOC_DIR}/${targetdir}" "${XSLT_EXECUTABLE}")

      add_custom_command(
	OUTPUT ${outfile} ${EXTRAS}
	COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	DEPENDS ${full_path_filename} ${xml_valid_stamp} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_XHTML_STYLESHEET} ${deps_list}
	)
      add_custom_target(${targetname} ALL DEPENDS ${outfile})

      # CMAKE_CFG_INTDIR can't be used in installation rules:
      # http://www.cmake.org/Bug/view.php?id=5747
      if(CMAKE_CONFIGURATION_TYPES)
        string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
	if(EXTRAS)
	  string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" EXTRAS "${EXTRAS}")
	endif(EXTRAS)
      endif(CMAKE_CONFIGURATION_TYPES)
      install(FILES ${outfile} ${EXTRAS} DESTINATION ${DOC_DIR}/${targetdir})

      get_property(BRLCAD_EXTRADOCS_HTML_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_HTML_TARGETS)
      set(BRLCAD_EXTRADOCS_HTML_TARGETS ${BRLCAD_EXTRADOCS_HTML_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_HTML_TARGETS "${BRLCAD_EXTRADOCS_HTML_TARGETS}")

      # If multiple outputs are being generated from the same file, make sure
      # one of them is run to completion before the others to avoid multiple
      # triggerings of the XML validation command.
      get_property(1ST_TARGET SOURCE ${filename} PROPERTY 1ST_TARGET)
      if(1ST_TARGET)
	add_dependencies(${targetname} ${1ST_TARGET})
      else(1ST_TARGET)
	set_property(SOURCE ${filename} PROPERTY 1ST_TARGET "${targetname}")
      endif(1ST_TARGET)

    endforeach(filename ${${xml_files}})
  endif(BRLCAD_EXTRADOCS_HTML)

  CMAKEFILES(${${xml_files}})
endmacro(DOCBOOK_TO_HTML targetname_suffix srcfile outfile targetdir deps_list)

# This macro produces Unix-style manual or "man" pages
macro(DOCBOOK_TO_MAN targetname_suffix xml_files mannum manext targetdir deps_list)
  set(xml_valid_stamp)
  if(BRLCAD_EXTRADOCS_MAN)
    foreach(filename ${${xml_files}})
      get_filename_component(filename_root "${filename}" NAME_WE)
      set(outfile ${bin_root}/${MAN_DIR}/${targetdir}/${filename_root}.${manext})
      set(targetname ${filename_root}_${targetname_suffix}_man)
      set(CURRENT_XSL_STYLESHEET ${XSL_MAN_STYLESHEET})
      if (EXISTS ${filename})
	set(full_path_filename ${filename})
      else (EXISTS ${filename})
	set(full_path_filename ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
      endif (EXISTS ${filename})

      # If we have more outputs than the default, they need to be handled here.
      set(EXTRAS)
      get_property(EXTRA_OUTPUTS SOURCE ${filename} PROPERTY EXTRA_MAN_OUTPUTS)
      foreach(extra_out ${EXTRA_OUTPUTS})
	set(EXTRAS ${EXTRAS} ${bin_root}/${MAN_DIR}/${targetdir}/${extra_out})
      endforeach(extra_out ${EXTRA_OUTPUTS})

      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetdir} "${filename}" ${filename_root})
      endif(BRLCAD_EXTRADOCS_VALIDATE)

      # Generate the script that will be used to run the XSLT executable
      DB_SCRIPT("${targetname}" "${MAN_DIR}/${targetdir}" "${XSLT_EXECUTABLE}")

      add_custom_command(
	OUTPUT ${outfile} ${EXTRAS}
	COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	DEPENDS ${full_path_filename} ${xml_valid_stamp} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_MAN_STYLESHEET} ${deps_list}
	)
      add_custom_target(${targetname} ALL DEPENDS ${outfile})

      # CMAKE_CFG_INTDIR can't be used in installation rules:
      # http://www.cmake.org/Bug/view.php?id=5747
      if(CMAKE_CONFIGURATION_TYPES)
        string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
	if(EXTRAS)
	  string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" EXTRAS "${EXTRAS}")
	endif(EXTRAS)
      endif(CMAKE_CONFIGURATION_TYPES)
      install(FILES ${outfile} ${EXTRAS} DESTINATION ${MAN_DIR}/man${mannum})

      # Add the current build target to the list of man page targets
      get_property(BRLCAD_EXTRADOCS_MAN_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_MAN_TARGETS)
      set(BRLCAD_EXTRADOCS_MAN_TARGETS ${BRLCAD_EXTRADOCS_MAN_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_MAN_TARGETS "${BRLCAD_EXTRADOCS_MAN_TARGETS}")

      # If multiple outputs are being generated from the same file, make sure
      # one of them is run to completion before the others to avoid multiple
      # triggerings of the XML validation command.
      get_property(1ST_TARGET SOURCE ${filename} PROPERTY 1ST_TARGET)
      if(1ST_TARGET)
	add_dependencies(${targetname} ${1ST_TARGET})
      else(1ST_TARGET)
	set_property(SOURCE ${filename} PROPERTY 1ST_TARGET "${targetname}")
      endif(1ST_TARGET)

    endforeach(filename ${${xml_files}})
  endif(BRLCAD_EXTRADOCS_MAN)
  # Mark files for distcheck
  CMAKEFILES(${${xml_files}})
endmacro(DOCBOOK_TO_MAN targetname_suffix srcfile outfile targetdir deps_list)

# PDF output is generated in a two stage process - the XML file is first
# converted to an "FO" file, and the FO file is in turn translated to PDF.
macro(DOCBOOK_TO_PDF targetname_suffix xml_files targetdir deps_list)
  set(xml_valid_stamp)
  if(BRLCAD_EXTRADOCS_PDF)
    foreach(filename ${${xml_files}})
      get_filename_component(filename_root "${filename}" NAME_WE)
      if (EXISTS ${filename})
	set(full_path_filename ${filename})
      else (EXISTS ${filename})
	set(full_path_filename ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
      endif (EXISTS ${filename})

      set(targetname ${filename_root}_${targetname_suffix}_fo)
      if(CMAKE_CONFIGURATION_TYPES)
	file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})
	set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${filename_root}.fo)
      else(CMAKE_CONFIGURATION_TYPES)
        set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
      endif(CMAKE_CONFIGURATION_TYPES)
      set(fo_outfile ${outfile})
      set(CURRENT_XSL_STYLESHEET ${XSL_FO_STYLESHEET})
      if(BRLCAD_EXTRADOCS_VALIDATE)
	DB_VALIDATE_TARGET(${targetdir} "${filename}" ${filename_root})
      endif(BRLCAD_EXTRADOCS_VALIDATE)
      DB_SCRIPT("${targetname}" "${DOC_DIR}/${targetdir}" "${XSLT_EXECUTABLE}")
      add_custom_command(
	OUTPUT ${outfile}
	COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
	DEPENDS ${full_path_filename} ${xml_valid_stamp} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${XSL_FO_STYLESHEET} ${deps_list}
	)
      set(targetname ${filename_root}_${targetname_suffix}_pdf)
      set(outfile ${bin_root}/${DOC_DIR}/${targetdir}/${filename_root}.pdf)
      DB_SCRIPT("${targetname}_pdf" "${DOC_DIR}/${targetdir}" "${PDF_CONV_EXECUTABLE}")
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
      install(FILES ${outfile} DESTINATION ${DOC_DIR}/${targetdir})
      get_property(BRLCAD_EXTRADOCS_PDF_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS)
      set(BRLCAD_EXTRADOCS_PDF_TARGETS ${BRLCAD_EXTRADOCS_PDF_TARGETS} ${targetname})
      set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS "${BRLCAD_EXTRADOCS_PDF_TARGETS}")

      # If multiple outputs are being generated from the same file, make sure
      # one of them is run to completion before the others to avoid multiple
      # triggerings of the XML validation command.
      get_property(1ST_TARGET SOURCE ${filename} PROPERTY 1ST_TARGET)
      if(1ST_TARGET)
	add_dependencies(${targetname} ${1ST_TARGET})
      else(1ST_TARGET)
	set_property(SOURCE ${filename} PROPERTY 1ST_TARGET "${targetname}")
      endif(1ST_TARGET)

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
