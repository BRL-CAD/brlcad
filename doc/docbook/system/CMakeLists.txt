# Style sheet for XSLT transformation to HTML pages
if(BRLCAD_EXTRADOCS_HTML)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_HTML)

if(BRLCAD_EXTRADOCS_PHP)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PHP)

# Style sheet for XSLT transformation to manual pages
if(BRLCAD_EXTRADOCS_MAN)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-man-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_MAN)

# Files for PDF
if(BRLCAD_EXTRADOCS_PDF)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-man-fo-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-fo-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-fo-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PDF)

# For HTML, MAN and FO (FO is an intermediate file used in the
# XML->PDF transformation) we use variables to hold the full
# stylesheet path. In the case we need to further
# customize FO stylesheets we can have separate CMake logic in
# appropriate directories to handle the customization (e.g., the
# BRL-CAD manuals in books/en/CMakeLists.txt).
set(XSL_PHP_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/wordpress.xsl")
set(XSL_HTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-xhtml-stylesheet.xsl")
set(XSL_MAN_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-stylesheet.xsl")
set(XSL_FO_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-man-fo-stylesheet.xsl")

add_subdirectory(man1)
add_subdirectory(man3)
add_subdirectory(man5)
add_subdirectory(mann)

CMAKEFILES(README)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
