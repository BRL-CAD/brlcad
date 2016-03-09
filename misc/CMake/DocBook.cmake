#                   D O C B O O K . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by
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
  if(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in")
    message(FATAL_ERROR "Specified ${VALIDATE_EXECUTABLE} for DocBook validation, but \"${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in\" does not exist.  To use ${VALIDATE_EXECUTABLE} a ${VALIDATE_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${VALIDATE_EXECUTABLE}.cmake.in")
endif(NOT DEFINED VALIDATE_EXECUTABLE)

# Handle default exec and sanity checking for XSLT
if(NOT DEFINED XSLT_EXECUTABLE)
  set(XSLT_EXECUTABLE "xsltproc")
else(NOT DEFINED XSLT_EXECUTABLE)
  if(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in")
    message(FATAL_ERROR "Specified ${XSLT_EXECUTABLE} for DocBook processing, but \"${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in\" does not exist.  To use ${XSLT_EXECUTABLE} a ${XSLT_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${XSLT_EXECUTABLE}.cmake.in")
endif(NOT DEFINED XSLT_EXECUTABLE)

# Handle default exec and sanity checking for PDF conversion
if(NOT DEFINED PDF_CONV_EXECUTABLE)
  set(PDF_CONV_EXECUTABLE "fop")
else(NOT DEFINED PDF_CONF_EXECUTABLE)
  if(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in")
    message(FATAL_ERROR "Specified ${PDF_CONF_EXECUTABLE} for DocBook pdf conversion, but \"${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in\" does not exist.  To use ${PDF_CONF_EXECUTABLE} a ${PDF_CONF_EXECUTABLE}.cmake.in file must be defined.")
  endif(NOT EXISTS "${BRLCAD_SOURCE_DIR}/misc/CMake/${PDF_CONF_EXECUTABLE}.cmake.in")
endif(NOT DEFINED PDF_CONV_EXECUTABLE)

# Get our root path
if(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}/\${BUILD_TYPE}")
else(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

# xsltproc is finicky about slashes in names - do some
# sanity scrubbing of the full root path string in
# preparation for generating DocBook scripts
string(REGEX REPLACE "/+" "/" bin_root "${bin_root}")
string(REGEX REPLACE "/$" "" bin_root "${bin_root}")

set(OUTPUT_FORMATS)
if(BRLCAD_EXTRADOCS_HTML)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} HTML)
endif(BRLCAD_EXTRADOCS_HTML)
if(BRLCAD_EXTRADOCS_PHP)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} PHP)
endif(BRLCAD_EXTRADOCS_PHP)
if(BRLCAD_EXTRADOCS_PPT)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} PPT)
endif(BRLCAD_EXTRADOCS_PPT)
if(BRLCAD_EXTRADOCS_MAN)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} MAN1)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} MAN3)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} MAN5)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} MANN)
endif(BRLCAD_EXTRADOCS_MAN)
if(BRLCAD_EXTRADOCS_PDF)
  set(OUTPUT_FORMATS ${OUTPUT_FORMATS} PDF)
endif(BRLCAD_EXTRADOCS_PDF)

set(HTML_EXTENSION "html")
set(PHP_EXTENSION "php")
set(PPT_EXTENSION "ppt.html")
set(MAN1_EXTENSION "1")
set(MAN3_EXTENSION "3")
set(MAN5_EXTENSION "5")
set(MANN_EXTENSION "nged")
set(PDF_EXTENSION "pdf")

set(HTML_DIR "${DOC_DIR}/html/")
set(PHP_DIR "${DOC_DIR}/html/")
set(PPT_DIR "${DOC_DIR}/html/")
set(MAN1_DIR "${DOC_DIR}/../man/")
set(MAN3_DIR "${DOC_DIR}/../man/")
set(MAN5_DIR "${DOC_DIR}/../man/")
set(MANN_DIR "${DOC_DIR}/../man/")
set(PDF_DIR "${DOC_DIR}/pdf/")

string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" XSLTPROC_EXECUTABLE "${XSLTPROC_EXECUTABLE}")
string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" XMLLINT_EXECUTABLE "${XMLLINT_EXECUTABLE}")

macro(ADD_DOCBOOK fmts in_xml_files outdir deps_list)

  # If we got the name of a list or an explicit list,
  # translate into the form we need.
  list(GET ${in_xml_files} 0 xml_files)
  if("${xml_files}" MATCHES "NOTFOUND")
    set(xml_files ${in_xml_files})
  else("${xml_files}" MATCHES "NOTFOUND")
    set(xml_files ${${in_xml_files}})
  endif("${xml_files}" MATCHES "NOTFOUND")

  # Mark files for distcheck
  CMAKEFILES(${xml_files})

  # Each file gets its own target, which handles all the outputs
  # to be produced from that file
  foreach(fname ${xml_files})
    get_filename_component(fname_root "${fname}" NAME_WE)
    get_filename_component(filename "${fname}" ABSOLUTE)

    # Find out which outputs we're actually going to produce, between
    # what's currently enabled and what the command says the target
    # *can* produce
    set(CURRENT_OUTPUT_FORMATS)
    foreach(fmt ${fmts})
      list(FIND OUTPUT_FORMATS "${fmt}" IN_LIST)
      if(NOT "${IN_LIST}" STREQUAL "-1")
	set(CURRENT_OUTPUT_FORMATS ${CURRENT_OUTPUT_FORMATS} ${fmt})
      endif(NOT "${IN_LIST}" STREQUAL "-1")
    endforeach(fmt ${fmts})

    # Now that we know our formats, prepare output paths
    set(outputs)
    foreach(fmt ${fmts})
      list(FIND OUTPUT_FORMATS "${fmt}" IN_LIST)
      if(NOT "${IN_LIST}" STREQUAL "-1")
	set(${fmt}_OUTFILE ${bin_root}/${${fmt}_DIR}${outdir}/${fname_root}.${${fmt}_EXTENSION})
	set(outputs ${outputs} ${${fmt}_OUTFILE})
      endif(NOT "${IN_LIST}" STREQUAL "-1")
    endforeach(fmt ${OUTPUT_FORMATS})
    # If we have more outputs than the default, they need to be handled here.
    foreach(fmt ${fmts})
      list(FIND OUTPUT_FORMATS "${fmt}" IN_LIST)
      if(NOT "${IN_LIST}" STREQUAL "-1")
	set(${fmt}_EXTRAS)
	get_property(EXTRA_OUTPUTS SOURCE ${fname} PROPERTY EXTRA_${fmt}_OUTPUTS)
	foreach(extra_out ${EXTRA_OUTPUTS})
	  set(${fmt}_EXTRAS ${${fmt}_EXTRAS} ${bin_root}/${${fmt}_DIR}${outdir}/${extra_out})
	  set(outputs ${outputs} ${${fmt}_OUTFILE})
	endforeach(extra_out ${EXTRA_OUTPUTS})
      endif(NOT "${IN_LIST}" STREQUAL "-1")
    endforeach(fmt ${fmts})

    # As long as we're outputting *something*, we have a target to produce
    if(NOT "${outputs}" STREQUAL "")
      string(MD5 path_md5 "${CMAKE_CURRENT_SOURCE_DIR}/${fname}")
      configure_file(${BRLCAD_CMAKE_DIR}/docbook.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/dbp_${fname_root}-${path_md5}.cmake @ONLY)
      add_custom_command(
	OUTPUT ${outputs}
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/dbp_${fname_root}-${path_md5}.cmake
	DEPENDS ${fname} ${XMLLINT_EXECUTABLE_TARGET} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} ${deps_list}
	)
      add_custom_target(docbook-${fname_root}-${path_md5} ALL DEPENDS ${outputs})
      set_target_properties(docbook-${fname_root}-${path_md5} PROPERTIES FOLDER "DocBook")

      # Now, set up the install rules in such a way that they will pull output from
      # the correct directory in single or multi configuration build situations.
      # Note that CMAKE_CFG_INTDIR can't be used in installation rules (known limitation):
      # http://www.cmake.org/Bug/view.php?id=5747
      foreach(fmt ${fmts})
	list(FIND OUTPUT_FORMATS "${fmt}" IN_LIST)
	if(NOT "${IN_LIST}" STREQUAL "-1")
	  set(outfile ${bin_root}/${${fmt}_DIR}${outdir}/${fname_root}.${${fmt}_EXTENSION})
	  install(FILES ${outfile} DESTINATION ${${fmt}_DIR}${outdir})
	  get_property(EXTRA_OUTPUTS SOURCE ${fname} PROPERTY EXTRA_${fmt}_OUTPUTS)
	  foreach(extra_out ${EXTRA_OUTPUTS})
	    set(outfile ${bin_root}/${${fmt}_DIR}${outdir}/${extra_out})
	    install(FILES ${outfile} DESTINATION ${${fmt}_DIR}${outdir})
	  endforeach(extra_out ${EXTRA_OUTPUTS})
	endif(NOT "${IN_LIST}" STREQUAL "-1")
      endforeach(fmt ${fmts})
    endif(NOT "${outputs}" STREQUAL "")

  endforeach(fname ${xml_files})

endmacro(ADD_DOCBOOK)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
