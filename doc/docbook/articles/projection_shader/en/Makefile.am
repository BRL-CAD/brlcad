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


projection_shaderDOCBOOK = \
	projection_shader.xml


BUILTHTML = ${projection_shaderDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${projection_shaderDOCBOOK:xml=pdf}

endif

projection_shaderDOCBOOKhtmldir = $(BRLCAD_DATA)/html/articles/projection_shader/en
projection_shaderDOCBOOKhtml_DATA = $(BUILTHTML)

projection_shaderDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/articles/projection_shader/en
projection_shaderDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${projection_shaderDOCBOOK} 

include $(top_srcdir)/misc/Makefile.defs
