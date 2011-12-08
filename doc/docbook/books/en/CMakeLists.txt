

SET(docbook_books_EN_IMAGES
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

SET(docbook_books_EN
	BRL-CAD_Tutorial_Series-VolumeI.xml
	BRL-CAD_Tutorial_Series-VolumeII.xml
	BRL-CAD_Tutorial_Series-VolumeIII.xml
	BRL-CAD_Tutorial_Series-VolumeIV.xml
)
DOCBOOK_TO_HTML(book docbook_books_EN html/books/en)

# The PDFs need some special logic to generate custom
# covers.

SET(COVER_COLOR "cc9966")
SET(CURRENT_VOLUME "I")
SET(CURRENT_TITLE "An Overview of BRL-CAD")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl @ONLY)

SET(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeI.xsl @ONLY)

SET(COVER_COLOR "cc6666")
SET(CURRENT_VOLUME "II")
SET(CURRENT_TITLE "Introduction to MGED")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl @ONLY)

SET(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeII.xsl @ONLY)

SET(COVER_COLOR "669966")
SET(CURRENT_VOLUME "III")
SET(CURRENT_TITLE "Principles of Effective Modeling")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl @ONLY)

SET(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIII.xsl @ONLY)

SET(COVER_COLOR "6699cc")
SET(CURRENT_VOLUME "IV")
SET(CURRENT_TITLE "Converting Geometry Between BRL-CAD and Other Formats")
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-cover-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl @ONLY)

SET(TUTORIAL_COVER_XSL ${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV-cover.xsl)
configure_file(${CMAKE_SOURCE_DIR}/doc/docbook/resources/brlcad/tutorial-template.xsl.in
	${CMAKE_CURRENT_BINARY_DIR}/BRL-CAD_Tutorial_Series-VolumeIV.xsl @ONLY)


IF(BRLCAD_EXTRADOCS_PDF)
	FOREACH(filename ${docbook_books_EN})
		STRING(REGEX REPLACE "([0-9a-z_-]*).xml" "\\1" filename_root "${filename}")
		SET(tmp_fo_file ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.fo)
		SET(xsl_file ${CMAKE_CURRENT_BINARY_DIR}/${filename_root}.xsl)
		SET(outfile ${CMAKE_BINARY_DIR}/${DATA_DIR}/pdf/books/en/${filename_root}.pdf)
		SET(targetname ${filename_root}_book_pdf)
		SET(scriptfile1 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_fo.cmake)
		FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/${DATA_DIR}/pdf/books/en)
		FILE(WRITE ${scriptfile1} "SET(ENV{XML_CATALOG_FILES} \"${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml\")\n")
		FILE(APPEND ${scriptfile1} "SET(ENV{XML_DEBUG_CATALOG} 1)\n")
		FILE(APPEND ${scriptfile1} "execute_process(COMMAND \"${XSLTPROC_EXECUTABLE}\" ${XSLTPROC_FLAGS} -o ${tmp_fo_file} ${xsl_file} ${CMAKE_CURRENT_SOURCE_DIR}/${filename})\n")
		ADD_CUSTOM_COMMAND(
			OUTPUT ${tmp_fo_file}
			COMMAND ${CMAKE_COMMAND} -P ${scriptfile1}
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${filename}
			)
		SET(scriptfile2 ${CMAKE_CURRENT_BINARY_DIR}/${targetname}_pdf.cmake)
		FILE(WRITE ${scriptfile2} "SET(ENV{FOP_HYPHENATION_PATH} ${FOP_HYP})\n")
		FILE(APPEND ${scriptfile2} "SET(ENV{CLASSPATH} ${CMAKE_SOURCE_DIR}/doc/docbook)\n")
		FILE(APPEND ${scriptfile2} "SET(ENV{FOP_OPTS} \"-Djava.awt.headless=true -Dxml.catalog.files=${CMAKE_BINARY_DIR}/doc/docbook/brlcad-xml-catalog.xml\")\n")
		FILE(APPEND ${scriptfile2} "execute_process(COMMAND \"${APACHE_FOP}\" -c ${CMAKE_BINARY_DIR}/doc/docbook/fop.xconf ${tmp_fo_file} -pdf ${outfile})\n")
		ADD_CUSTOM_COMMAND(
			OUTPUT ${outfile}
			COMMAND ${CMAKE_COMMAND} -P ${scriptfile2}
			DEPENDS ${tmp_fo_file}
			)
		ADD_CUSTOM_TARGET(${targetname} ALL DEPENDS ${outfile})
		INSTALL(FILES ${outfile} DESTINATION ${DATA_DIR}/pdf/books/en)
	ENDFOREACH(filename ${docbook_books_EN})
ENDIF(BRLCAD_EXTRADOCS_PDF)

CMAKEFILES(Makefile.am tutorial_series_authors.xml)
