# We need various configuration files set up for DocBook processing tools
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/)

# First, define the commands to expand the third party components we will need to handle
# DocBook processing from their archives into the build directory
include(${CMAKE_CURRENT_SOURCE_DIR}/resources/other/expand.cmake)

# Rework CMake binary and source paths for DocBook templates - if we
# have spaces in the paths, they will cause a problem.
string(REPLACE " " "%20" DOCBOOK_BINARY_DIR "${CMAKE_BINARY_DIR}")
string(REPLACE " " "%20" DOCBOOK_SOURCE_DIR "${CMAKE_SOURCE_DIR}")

# Common stylesheets for all DocBook sources
if(BRLCAD_EXTRADOCS_HTML OR BRLCAD_EXTRADOCS_PDF OR BRLCAD_EXTRADOCS_MAN OR BRLCAD_EXTRADOCS_PHP OR BRLCAD_EXTRADOCS_PPT)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-common.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-common.xsl)
  # Fonts definition
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-fonts.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-fonts.xsl)
  # Wordpress
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/wordpress.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/wordpress.xsl)
endif(BRLCAD_EXTRADOCS_HTML OR BRLCAD_EXTRADOCS_PDF OR BRLCAD_EXTRADOCS_MAN OR BRLCAD_EXTRADOCS_PHP OR BRLCAD_EXTRADOCS_PPT)

# Files for PDF products
if(BRLCAD_EXTRADOCS_PDF)
  # Apache FOP needs a couple of config files set up
  set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
  configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
endif(BRLCAD_EXTRADOCS_PDF)

# Include CMake macros for DocBook.
include(${BRLCAD_SOURCE_DIR}/misc/CMake/DocBook.cmake)

# For the html files, we need brlcad.css
if(NOT CMAKE_CONFIGURATION_TYPES)
  configure_file(css/brlcad.css ${CMAKE_BINARY_DIR}/${DOC_DIR}/html/css/brlcad.css)
else(NOT CMAKE_CONFIGURATION_TYPES)
  foreach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${CFG_TYPE}" CFG_TYPE_UPPER)
    configure_file(css/brlcad.css ${CMAKE_BINARY_DIR_${CFG_TYPE_UPPER}}/${DOC_DIR}/html/css/brlcad.css)
  endforeach(CFG_TYPE ${CMAKE_CONFIGURATION_TYPES})
endif(NOT CMAKE_CONFIGURATION_TYPES)
install(FILES css/brlcad.css DESTINATION ${DOC_DIR}/html/css)

add_subdirectory(articles)
add_subdirectory(books)
add_subdirectory(lessons)
add_subdirectory(presentations)
add_subdirectory(specifications)
add_subdirectory(system)

# Now that we have all the individual targets listed by the DocBook.cmake
# macros, define convenience targets
if(BRLCAD_EXTRADOCS_HTML)
  get_property(BRLCAD_EXTRADOCS_HTML_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_HTML_TARGETS)
  add_custom_target(doc_html DEPENDS ${BRLCAD_EXTRADOCS_HTML_TARGETS})
  set_target_properties(doc_html PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_HTML)
if(BRLCAD_EXTRADOCS_PHP)
  get_property(BRLCAD_EXTRADOCS_PHP_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PHP_TARGETS)
  add_custom_target(doc_php DEPENDS ${BRLCAD_EXTRADOCS_PHP_TARGETS})
  set_target_properties(doc_php PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_PHP)
if(BRLCAD_EXTRADOCS_PPT)
  get_property(BRLCAD_EXTRADOCS_PPT_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PPT_TARGETS)
  add_custom_target(doc_ppt DEPENDS ${BRLCAD_EXTRADOCS_PPT_TARGETS})
  set_target_properties(doc_ppt PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_PPT)
if(BRLCAD_EXTRADOCS_MAN)
  get_property(BRLCAD_EXTRADOCS_MAN_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_MAN_TARGETS)
  add_custom_target(doc_man DEPENDS ${BRLCAD_EXTRADOCS_MAN_TARGETS})
  set_target_properties(doc_man PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_MAN)
if(BRLCAD_EXTRADOCS_PDF)
  get_property(BRLCAD_EXTRADOCS_PDF_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS)
  add_custom_target(doc_pdf DEPENDS ${BRLCAD_EXTRADOCS_PDF_TARGETS})
  set_target_properties(doc_pdf PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_PDF)
if(BRLCAD_EXTRADOCS_HTML OR BRLCAD_EXTRADOCS_MAN OR BRLCAD_EXTRADOCS_PDF OR BRLCAD_EXTRADOCS_PHP OR BRLCAD_EXTRADOCS_PPT )
  add_custom_target(doc_all DEPENDS
	${BRLCAD_EXTRADOCS_HTML_TARGETS}
	 ${BRLCAD_EXTRADOCS_PHP_TARGETS}
         ${BRLCAD_EXTRADOCS_PPT_TARGETS}
  ${BRLCAD_EXTRADOCS_MAN_TARGETS}
	${BRLCAD_EXTRADOCS_PDF_TARGETS})
  set_target_properties(doc_all PROPERTIES FOLDER "DocBook")
endif(BRLCAD_EXTRADOCS_HTML OR BRLCAD_EXTRADOCS_MAN OR BRLCAD_EXTRADOCS_PDF OR BRLCAD_EXTRADOCS_PHP OR BRLCAD_EXTRADOCS_PPT)

CMAKEFILES(README fop.xconf.in log4j.properties resources)
CMAKEFILES(README.DB_authors_notes)
CMAKEFILES(css/brlcad.css)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
