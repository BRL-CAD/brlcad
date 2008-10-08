XSLTPROC_FLAGS=-nonet -xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml


.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?


.xml.1:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?


.fo.pdf: .xml.fo
	$(APACHEFOP) $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?

man1DOCBOOK = \
	alias-pix.xml \
	all_sf.xml \
	bwscale.xml \
	rt.xml \
	search.xml

man1DOCBOOKhtmldir = $(BRLCAD_DATA)/html/man1/en
man1DOCBOOKhtml_DATA = $(BUILTHTML)

BUILTHTML = \
	${man1DOCBOOK:xml=html} 


if BUILD_PDF_DOCS

BUILTPDF = ${man1DOCBOOK:xml=pdf}

endif

man1DOCBOOKpdfdir = $(BRLCAD_DATA)/pdf/man1/en
man1DOCBOOKpdf_DATA = $(BUILTPDF)

BUILT_SOURCES = \
	$(BUILTHTML) \
	$(BUILTPDF)

BUILTMAN_SOURCES = \
	${man1DOCBOOK:xml=1}  

man_MANS = \
         $(BUILTMAN_SOURCES)

MOSTLYCLEANFILES = \
         $(BUILT_SOURCES) \
         $(BUILTMAN_SOURCES)

EXTRA_DIST = \
	${man1DOCBOOK}

include $(top_srcdir)/misc/Makefile.defs
