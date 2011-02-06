SET(docbook_books_EN 
	BRL-CAD_Tutorial_Series-VolumeI.xml 
	BRL-CAD_Tutorial_Series-VolumeII.xml 
	BRL-CAD_Tutorial_Series-VolumeIII.xml
	BRL-CAD_Tutorial_Series-VolumeIV.xml
)

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
)

DOCBOOK_TO_HTML(book docbook_books_EN html/books/en)
BRLCAD_ADDDATA(docbook_books_EN_IMAGES html/books/en/images)

IF(BRLCAD-BUILD_EXTRADOCS_PDF)
  DOCBOOK_TO_PDF(book docbook_books_EN pdf/books/en)
ENDIF(BRLCAD-BUILD_EXTRADOCS_PDF)

CMAKEFILES(tutorial_series_authors.xml)
