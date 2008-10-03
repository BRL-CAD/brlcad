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


tireDOCBOOK = \
	tire.xml


BUILTHTML = ${tireDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${tireDOCBOOK:xml=pdf}

endif

tireDOCBOOKhtmldir = $(BRLCAD_DATA)/html/articles/tire/en
tireDOCBOOKhtml_DATA = $(BUILTHTML)

tireDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/articles/tire/en
tireDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${tireDOCBOOK} 

include $(top_srcdir)/misc/Makefile.defs
