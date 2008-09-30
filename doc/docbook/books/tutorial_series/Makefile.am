XSLTPROC_FLAGS=-nonet -xinclude
XSL_SINGLE_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_CHUNKED_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/chunk.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_SINGLE_XHTML_STYLESHEET) $?

.fo.pdf: .xml.fo
	$(APACHEFOP) $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?


tutorial_seriesDOCBOOK = \
	VolumeII.xml\
	VolumeIII.xml

BUILTHTML = ${tutorial_seriesDOCBOOK:xml=html} 
BUILTPDF = ${tutorial_seriesDOCBOOK:xml=pdf}

tutorial_seriesDOCBOOKhtmldir = $(BRLCAD_DATA)/html/tutorial_series
tutorial_seriesDOCBOOKhtml_DATA = $(BUILTHTML)

tutorial_seriesDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/tutorial_series
tutorial_seriesDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${tutorial_seriesDOCBOOK}

include $(top_srcdir)/misc/Makefile.defs
