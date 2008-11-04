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


ebm_primitiveDOCBOOK = \
	ebm_primitive.xml


BUILTHTML = ${ebm_primitiveDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${ebm_primitiveDOCBOOK:xml=pdf}

endif

ebm_primitiveDOCBOOKhtmldir = $(BRLCAD_DATA)/html/articles/ebm_primitive/en
ebm_primitiveDOCBOOKhtml_DATA = $(BUILTHTML)

ebm_primitiveDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/articles/ebm_primitive/en
ebm_primitiveDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${ebm_primitiveDOCBOOK} 

include $(top_srcdir)/misc/Makefile.defs
