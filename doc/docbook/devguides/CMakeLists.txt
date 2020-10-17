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

set(devguides_EN
  bool_eval_development.xml
  )

set(devguides_EN_IMAGES
  images/ccx_overlap_event.png
  images/ssx_transverse_event.png
  images/ssx_tangent_event.png
  images/compare_endpoint_style.png
  images/lcurves_with_shaded_context.png
  images/ssx_10_vs_13.png
  images/curve_traversal_directions.png
  images/axis_X.png
  images/intermediate_linked_curves.png
  images/ssx_overlap_event.png
  images/evaluation_overview.png
  )

ADD_DOC(devguides_EN html/devguides)
ADD_DOC(devguides_EN_IMAGES html/devguides/images)
ADD_DOCBOOK("HTML;PHP;PDF" devguides_EN devguides/en devguides_EN_IMAGES_cp)

CMAKEFILES(
  CMakeLists.txt
  )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
