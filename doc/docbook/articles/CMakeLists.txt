# Style sheet for XSLT transformation to article HTML pages
if(BRLCAD_EXTRADOCS_HTML)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_HTML)

if(BRLCAD_EXTRADOCS_PHP)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PHP)

# Files for PDF products
if(BRLCAD_EXTRADOCS_PDF)
  # Style sheet for XSLT transformation to PDF
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../resources/brlcad/brlcad-article-fo-stylesheet.xsl.in
    ${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-fo-stylesheet.xsl)
  DISTCLEAN("${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-fo-stylesheet.xsl")
endif(BRLCAD_EXTRADOCS_PDF)

# For HTML and FO (FO is an intermediate file used in the
# XML->PDF transformation) we use variables to hold the full
# stylesheet path.
set(XSL_PHP_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/wordpress.xsl")
set(XSL_HTML_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-xhtml-stylesheet.xsl")
set(XSL_FO_STYLESHEET "${CMAKE_CURRENT_BINARY_DIR}/../resources/brlcad/brlcad-article-fo-stylesheet.xsl")

set(articles_EN
  TEMPLATE.xml
  about.xml
  build_pattern.xml
  build_region.xml
  camo_shader.xml
  ebm_primitive.xml
  gcv.xml
  ged.xml
  mgedrc.xml
  nirt.xml
  oed.xml
  pipes.xml
  projection_shader.xml
  tire.xml
  mged.xml
  main_menu.xml
  overlap_tool.xml
  )

set(articles_EN_IMAGES
  images/Lee_tux.gif
  images/build_pattern_fig01.png
  images/build_pattern_fig02.png
  images/build_pattern_fig03.png
  images/build_pattern_fig04.png
  images/build_pattern_fig05.png
  images/build_pattern_fig06.png
  images/build_pattern_fig07.png
  images/build_region_fig01.png
  images/build_region_fig02.png
  images/build_region_fig03.png
  images/build_region_fig04.png
  images/ebm_primitive_fig01.png
  images/ebm_primitive_fig02.png
  images/ebm_primitive_fig03.png
  images/gcv_architecture.png
  images/gcv_unsupported_tessellation.png
  images/gcv_using_gdiff.png
  images/mgedrc_fig01.png
  images/mgedrc_fig02.png
  images/mgedrc_fig03.png
  images/moss_ssm.gif
  images/nirt_fig01.png
  images/nirt_fig02.png
  images/nirt_fig03.png
  images/nirt_fig04.png
  images/nirt_fig05.png
  images/nirt_fig06.png
  images/nirt_fig07.png
  images/nirt_fig08.png
  images/nirt_fig09.png
  images/nirt_fig10.png
  images/nirt_fig11.png
  images/nirt_fig12.png
  images/nirt_fig13.png
  images/nirt_fig14.png
  images/nirt_fig15.png
  images/nirt_fig16.png
  images/nirt_fig17.png
  images/nirt_fig18.png
  images/nirt_fig19.png
  images/nirt_fig20.png
  images/nirt_fig21.png
  images/nirt_fig22.png
  images/nirt_fig23.png
  images/nirt_fig24.png
  images/oed_fig01.png
  images/oed_fig02.png
  images/oed_fig03.png
  images/oed_fig04.png
  images/oed_fig05.png
  images/oed_fig06.png
  images/oed_fig07.png
  images/oed_fig08.png
  images/oed_fig09.png
  images/oed_fig10.png
  images/oed_fig11.png
  images/oed_fig12.png
  images/oed_fig13.png
  images/overlap_tool_fig01.gif
  images/overlap_tool_fig02.gif
  images/overlap_tool_fig03.png
  images/pipes_fig01.png
  images/pipes_fig02.png
  images/pipes_fig03.png
  images/pipes_fig04.png
  images/pipes_fig05.png
  images/pipes_fig06.png
  images/pipes_fig07.png
  images/pipes_fig08.png
  images/pipes_fig09.png
  images/pipes_fig10.png
  images/pipes_fig11.png
  images/pipes_fig12.png
  images/pipes_fig13.png
  images/projection_shader_fig01.png
  images/projection_shader_fig02.png
  images/projection_shader_fig03.png
  images/projection_shader_fig04.png
  images/projection_shader_fig05.png
  images/projection_shader_fig06.png
  images/projection_shader_fig07.png
  images/tire_fig01.png
  images/tire_fig02.png
  images/tire_fig03.png
  images/tire_fig04.png
  images/tire_fig05.png
  images/tire_fig06.png
  images/tire_fig07.png
  images/tire_fig08.png
  images/tire_fig09.png
  images/tire_fig10.png
  images/tire_fig11.png
  images/tire_fig12.png
  images/tire_fig13.png
  images/tire_fig14.png
  images/base.gif
  images/bool.gif
  images/coord-axes.gif
  images/cup_and_mug.gif
  images/cup_and_mug_small.gif
  images/cup_out+in.gif
  images/cup_outside.gif
  images/cup_w_handle.gif
  images/faceplate.gif
  images/faceplate_menu.gif
  images/faceplate_menu_sm.gif
  images/faceplate_sm.gif
  images/fillet.gif
  images/fillet.jpg
  images/handle.gif
  images/handle.jpg
  images/mug_camo.gif
  images/mug_camo.jpg
  images/mug_green.gif
  images/mug_green.jpg
  images/prims.gif
  images/rim.gif
  images/rim.jpg
  )
if(BRLCAD_EXTRADOCS)
  ADD_DOC(articles_EN_IMAGES html/articles/images)
else(BRLCAD_EXTRADOCS)
  CMAKEFILES(${articles_EN_IMAGES})
endif(BRLCAD_EXTRADOCS)

ADD_DOCBOOK("HTML;PHP;PDF" articles_EN articles articles_EN_IMAGES_cp)

CMAKEFILES(
  CMakeLists.txt
  images/tire_fig12.svg
  images/tire_fig13.svg
  )

add_subdirectory(hy)
add_subdirectory(it)
add_subdirectory(ru)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
