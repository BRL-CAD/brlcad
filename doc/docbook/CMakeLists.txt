# We need various configuration files set up for DocBook processing tools
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/)

# First, define the commands to expand the third party components we will need to handle
# DocBook processing from their archives into the build directory
include(${CMAKE_CURRENT_SOURCE_DIR}/resources/other/expand.cmake)

# Style sheet for XSLT transformation to HTML pages
if(BRLCAD_EXTRADOCS_HTML)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl.in
		${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl)
endif(BRLCAD_EXTRADOCS_HTML)

# Style sheet for XSLT transformation to manual pages
if(BRLCAD_EXTRADOCS_MAN)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl)
endif(BRLCAD_EXTRADOCS_MAN)

# If we're doing PDF output, Apache FOP needs a couple of config files set up
if(BRLCAD_EXTRADOCS_PDF)
	set(srcdir ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/fop.xconf.in ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf)
	configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/log4j.properties ${CMAKE_BINARY_DIR}/doc/docbook/log4j.properties COPYONLY)
endif(BRLCAD_EXTRADOCS_PDF)

# For HTML, MAN and FO (FO is an intermediate file used in the XML->PDF
# transformation) we use variables to hold the full stylesheet path. Because
# BRL-CAD has not (thus far) needed to customize it's FO stylesheets for most
# situations, we used the standard XSL FO stylesheet.  In the case we DO need
# to customize FO stylesheets (the BRL-CAD manuals in books/en) we have
# separate CMake logic in books/en/CMakeLists.txt to handle the customization.
set(XSL_XHTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-xhtml-stylesheet.xsl")
set(XSL_MAN_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/resources/brlcad/brlcad-man-stylesheet.xsl")
set(XSL_FO_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/resources/other/standard/xsl/fo/docbook.xsl")

# Include CMake macros for DocBook.
include(${BRLCAD_SOURCE_DIR}/misc/CMake/DocBook.cmake)

# For the html files, we need brlcad.css
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/html/css)
configure_file(css/brlcad.css ${CMAKE_BINARY_DIR}/${DATA_DIR}/html/css/brlcad.css)
install(FILES css/brlcad.css DESTINATION ${DATA_DIR}/html/css)

add_subdirectory(articles)
add_subdirectory(books)
add_subdirectory(lessons)
add_subdirectory(presentations)
add_subdirectory(specifications)
add_subdirectory(system)

CMAKEFILES(README fop.xconf.in log4j.properties resources)
CMAKEFILES(README.DB_authors_notes)
CMAKEFILES(css/brlcad.css)
