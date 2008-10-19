XSLTPROC_FLAGS=-nonet -xinclude
XSL_SINGLE_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_CHUNKED_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/chunk.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/articles/nirt/fo_custom.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_SINGLE_XHTML_STYLESHEET) $?

.fo.pdf: .xml.fo
	$(APACHEFOP) $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?


nirtDOCBOOK = \
	nirt.xml


BUILTHTML = ${nirtDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${nirtDOCBOOK:xml=pdf}

endif

nirtDOCBOOKhtmldir = $(BRLCAD_DATA)/html/articles/nirt/en
nirtDOCBOOKhtml_DATA = $(BUILTHTML)

nirtDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/articles/nirt/en
nirtDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${nirtDOCBOOK} 

include $(top_srcdir)/misc/Makefile.defs
