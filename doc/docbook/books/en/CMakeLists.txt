

set(docbook_books_EN_IMAGES
  images/tutorial_series_volI_fig01.png
  images/tutorial_series_volI_fig02.png
  images/tutorial_series_volI_fig03.png
  images/tutorial_series_volI_fig04.png
  images/tutorial_series_volI_fig05.png
  images/tutorial_series_volI_fig06.png
  images/tutorial_series_volI_fig07.png
  images/tutorial_series_volIII_fig01.png
  images/tutorial_series_volIII_fig02.png
  images/tutorial_series_volIII_fig03.png
  images/tutorial_series_volIII_fig04.png
  images/tutorial_series_volIII_fig05.png
  images/tutorial_series_volIII_fig06.png
  images/tutorial_series_volIII_fig07.png
  images/tutorial_series_volIII_fig08.png
  images/tutorial_series_volIII_fig09.png
  images/tutorial_series_volIII_fig10.png
  images/tutorial_series_volIII_fig11.png
  images/tutorial_series_volIII_fig12.png
  images/tutorial_series_volIII_fig13.png
  images/tutorial_series_volIII_fig14.png
  images/tutorial_series_volIII_fig15.png
  images/tutorial_series_volIII_fig16.png
  images/tutorial_series_volIII_fig17.png
  images/tutorial_series_volIII_fig18.png
  images/tutorial_series_volIII_fig19.png
  images/tutorial_series_volIII_fig20.png
  images/tutorial_series_volIII_fig21.png
  images/tutorial_series_volIII_fig22.png
  images/tutorial_series_volIII_fig23.png
  images/tutorial_series_volIII_fig24.png
  images/tutorial_series_volIII_fig25.png
  images/tutorial_series_volIII_fig26.png
  images/tutorial_series_volIII_fig27.png
  images/tutorial_series_volIII_fig28.png
  images/tutorial_series_volIII_fig29.png
  images/tutorial_series_volIII_fig30.png
  images/tutorial_series_volIII_table_image_1.png
  images/v4-image001.jpg
  images/v4-image002.jpg
  images/v4-image003.gif
  images/v4-image004.jpg
  images/v4-image005.jpg
  images/v4-image006.jpg
  images/v4-image007.jpg
  images/v4-image008.gif
  images/v4-image009.jpg
  images/v4-image010.jpg
  images/v4-image011.jpg
  )
ADD_DOC(docbook_books_EN_IMAGES html/books/en/images)

set(docbook_books_EN
  BRL-CAD_Tutorial_Series-VolumeI.xml
  BRL-CAD_Tutorial_Series-VolumeII.xml
  BRL-CAD_Tutorial_Series-VolumeIII.xml
  BRL-CAD_Tutorial_Series-VolumeIV.xml
  )
DOCBOOK_TO_HTML(book docbook_books_EN html/books/en docbook_books_EN_IMAGES_cp)

# The PDFs need some special logic to generate custom
# covers.

set(COVER_COLOR "cc9966")
set(CURRENT_VOLUME "I")
set(CURRENT_TITLE "An Overview of BRL-CAD")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl @ONLY)

string(REPLACE " " "%20" TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI.xsl @ONLY)

set(COVER_COLOR "cc6666")
set(CURRENT_VOLUME "II")
set(CURRENT_TITLE "Introduction to MGED")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl @ONLY)

string(REPLACE " " "%20" TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII.xsl @ONLY)

set(COVER_COLOR "669966")
set(CURRENT_VOLUME "III")
set(CURRENT_TITLE "Principles of Effective Modeling")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl @ONLY)

string(REPLACE " " "%20" TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII.xsl @ONLY)

set(COVER_COLOR "6699cc")
set(CURRENT_VOLUME "IV")
set(CURRENT_TITLE "Converting Geometry Between BRL-CAD and Other Formats")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl @ONLY)

string(REPLACE " " "%20" TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
  ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV.xsl @ONLY)

if(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
else(CMAKE_CONFIGURATION_TYPES)
  set(bin_root "${CMAKE_BINARY_DIR}")
endif(CMAKE_CONFIGURATION_TYPES)

if(BRLCAD_EXTRADOCS_PDF)
  set(targetdir pdf/books/en)
  foreach(filename ${docbook_books_EN})
    if (EXISTS ${filename})
      set(full_path_filename ${filename})
    else (EXISTS ${filename})
      set(full_path_filename ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
    endif (EXISTS ${filename})
    string(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
    if(CMAKE_CONFIGURATION_TYPES)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})
      set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${filename_root}.fo)
    else(CMAKE_CONFIGURATION_TYPES)
      set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
    endif(CMAKE_CONFIGURATION_TYPES)
    set(fo_outfile ${outfile})
    set(CURRENT_XSL_STYLESHEET ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.xsl)
    DB_SCRIPT("${filename_root}_book_fo" "${targetdir}" "${XSLT_EXECUTABLE}")
    add_custom_command(
      OUTPUT ${outfile}
      COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} docbook_books_EN_IMAGES_cp
      )
    set(outfile ${bin_root}/${DOC_DIR}/${targetdir}/${filename_root}.pdf)
    set(targetname "${filename_root}_book_pdf")
    DB_SCRIPT("${targetname}" "${targetdir}" "${PDF_CONV_EXECUTABLE}")
    add_custom_command(
      OUTPUT ${outfile}
      COMMAND ${CMAKE_COMMAND} -P ${scriptfile}
      DEPENDS ${fo_outfile} ${DOCBOOK_RESOURCE_FILES} docbook_books_EN_IMAGES_cp
      )
    add_custom_target(${targetname} ALL DEPENDS ${outfile})
    # CMAKE_CFG_INTDIR can't be used in installation rules:
    # http://www.cmake.org/Bug/view.php?id=5747
    if(CMAKE_CONFIGURATION_TYPES)
      string(REPLACE "${CMAKE_CFG_INTDIR}" "\${BUILD_TYPE}" outfile "${outfile}")
    endif(CMAKE_CONFIGURATION_TYPES)
    install(FILES ${outfile} DESTINATION ${DOC_DIR}/pdf/books/en)
    get_property(BRLCAD_EXTRADOCS_PDF_TARGETS GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS)
    set(BRLCAD_EXTRADOCS_PDF_TARGETS ${BRLCAD_EXTRADOCS_PDF_TARGETS} ${targetname})
    set_property(GLOBAL PROPERTY BRLCAD_EXTRADOCS_PDF_TARGETS "${BRLCAD_EXTRADOCS_PDF_TARGETS}")
  endforeach(filename ${docbook_books_EN})
endif(BRLCAD_EXTRADOCS_PDF)

CMAKEFILES(tutorial_series_authors.xml)

set(hacking_brlcad_images
   images/hacking_brlcad_article_tire_pdf.png
   images/hacking_brlcad_bearing_cc.png
   images/hacking_brlcad_book_vol3_pdf.png
   images/hacking_brlcad_brlcad_intro_hy.png
   images/hacking_brlcad_brlcad-wiki.png
   images/hacking_brlcad_csg.png
   images/hacking_brlcad_cubit.png
   images/hacking_brlcad_docbook_CMakeLists.png
   images/hacking_brlcad_docbook_working_session.png
   images/hacking_brlcad_doxygen_output.png
   images/hacking_brlcad_emacs-nxml.png
   images/hacking_brlcad_example_docbook_default_output.png
   images/hacking_brlcad_g_spec_pdf.png
   images/hacking_brlcad_havoc_c.png
   images/hacking_brlcad_havoc_views_c.png
   images/hacking_brlcad_M1A1_9x10_300dpi_cc.png
   images/hacking_brlcad_man_page_search.png
   images/hacking_brlcad_sflake2_cc.png
   images/hacking_brlcad_Sgi_cube_cc.png
   images/hacking_brlcad_stryker_slat.png
   images/hacking_brlcad_tcl_tk_presentation.png
   images/hacking_brlcad_teapot.png
   images/hacking_brlcad_tracked_mine.png
   images/hacking_brlcad_tutorial_1_pdf.png
   images/hacking_brlcad_xyz.png
   )
CMAKEFILES(${hacking_brlcad_images})
CMAKEFILES(HACKING_BRL-CAD.xml)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
