# Style sheet for XSLT transformation to book HTML pages
if(BRLCAD_EXTRADOCS_HTML)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-presentation-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-presentation-xhtml-stylesheet.xsl)
endif(BRLCAD_EXTRADOCS_HTML)

if(BRLCAD_EXTRADOCS_PHP)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-presentation-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-presentation-xhtml-stylesheet.xsl)
endif(BRLCAD_EXTRADOCS_PHP)

if(BRLCAD_EXTRADOCS_PPT)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/presentation.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/presentation.xsl)
endif(BRLCAD_EXTRADOCS_PPT)

# Files for PDF products
if(BRLCAD_EXTRADOCS_PDF)
  # Style sheet for XSLT transformation to PDF
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-presentation-fo-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-presentation-fo-stylesheet.xsl)
endif(BRLCAD_EXTRADOCS_PDF)

# For HTML and FO (FO is an intermediate file used in the
# XML->PDF transformation) we use variables to hold the full
# stylesheet path.
set(XSL_PHP_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/wordpress.xsl")
set(XSL_HTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-presentation-xhtml-stylesheet.xsl")
set(XSL_PPT_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/presentation.xsl")
set(XSL_FO_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-presentation-fo-stylesheet.xsl")


add_subdirectory(en)
CMAKEFILES(README)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
