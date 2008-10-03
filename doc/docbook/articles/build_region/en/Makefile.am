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


build_regionDOCBOOK = \
	build_region.xml


BUILTHTML = ${build_regionDOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${build_regionDOCBOOK:xml=pdf}

endif

build_regionDOCBOOKhtmldir = $(BRLCAD_DATA)/html/articles/build_region/en
build_regionDOCBOOKhtml_DATA = $(BUILTHTML)

build_regionDOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/articles/build_region/en
build_regionDOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES)

EXTRA_DIST = \
	${build_regionDOCBOOK} 

include $(top_srcdir)/misc/Makefile.defs
