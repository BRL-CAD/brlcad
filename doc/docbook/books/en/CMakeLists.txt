

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
BRLCAD_ADDDATA(docbook_books_EN_IMAGES html/books/en/images)

set(docbook_books_EN
	BRL-CAD_Tutorial_Series-VolumeI.xml
	BRL-CAD_Tutorial_Series-VolumeII.xml
	BRL-CAD_Tutorial_Series-VolumeIII.xml
	BRL-CAD_Tutorial_Series-VolumeIV.xml
)
DOCBOOK_TO_HTML(book docbook_books_EN html/books/en)

# The PDFs need some special logic to generate custom
# covers.

set(COVER_COLOR "cc9966")
set(CURRENT_VOLUME "I")
set(CURRENT_TITLE "An Overview of BRL-CAD")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl @ONLY)

set(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI.xsl @ONLY)

set(COVER_COLOR "cc6666")
set(CURRENT_VOLUME "II")
set(CURRENT_TITLE "Introduction to MGED")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl @ONLY)

set(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII.xsl @ONLY)

set(COVER_COLOR "669966")
set(CURRENT_VOLUME "III")
set(CURRENT_TITLE "Principles of Effective Modeling")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl @ONLY)

set(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII.xsl @ONLY)

set(COVER_COLOR "6699cc")
set(CURRENT_VOLUME "IV")
set(CURRENT_TITLE "Converting Geometry Between BRL-CAD and Other Formats")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl @ONLY)

set(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV.xsl @ONLY)


if(BRLCAD_EXTRADOCS_PDF)
	set(targetdir pdf/books/en)
	set(mk_out_dir ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir})
	foreach(filename ${docbook_books_EN})
		string(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
		set(targetname ${filename_root}_book_pdf)
		set(scriptfile1 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_fo.cmake)
		set(CURRENT_XSL_STYLESHEET ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.xsl)
		configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/xsltproc.cmake.in	${scriptfile1} @ONLY)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND ${CMAKE_COMMAND} -P ${scriptfile1}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename} ${XSLTPROC_EXECUTABLE_TARGET} ${DOCBOOK_RESOURCE_FILES} 
			)
		set(pdf_outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/${targetdir}/${filename_root}.pdf)
		set(scriptfile2 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_pdf.cmake)
		configure_file(${BRLCAD_SOURCE_DIR}/misc/CMake/fop.cmake.in ${scriptfile2} @ONLY)
		ADD_CUSTOM_COMMAND(
			OUTPUT ${pdf_outfile}
			COMMAND ${CMAKE_COMMAND} -P ${scriptfile2}
			DEPENDS ${outfile} ${DOCBOOK_RESOURCE_FILES}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${pdf_outfile})
		install(FILES ${pdf_outfile} DESTINATION ${DATA_DIR}/pdf/books/en)
	endforeach(filename ${docbook_books_EN})
endif(BRLCAD_EXTRADOCS_PDF)

CMAKEFILES(tutorial_series_authors.xml)
