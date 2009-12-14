XSLTPROC_FLAGS=-nonet -xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

.xml.html: 
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?

.xml.1:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	mkdir -p system/man1/en/
	mv '$(@F)' system/man1/en/

.fo.pdf: .xml.fo
	$(APACHEFOP) -c fop.xconf $? -pdf $@

.xml.fo:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?

include $(srcdir)/articles/en/Makefile.am

include $(srcdir)/lessons/en/Makefile.am

include $(srcdir)/books/en/Makefile.am

include $(srcdir)/specifications/en/Makefile.am

include $(srcdir)/system/man1/en/Makefile.am

include $(srcdir)/lessons/es/Makefile.am

if BUILD_DOCS

BUILT_SOURCES = \
	$(BUILTARTICLES_ENHTML) \
	$(BUILTARTICLES_ENPDF) \
	$(BUILTLESSONS_ENHTML) \
	$(BUILTLESSONS_ENPDF) \
	$(BUILTBOOKS_ENHTML) \
	$(BUILTBOOKS_ENPDF) \
	$(BUILTSPECS_ENHTML) \
	$(BUILTSPECS_ENPDF) \
	$(BUILTMAN_ENHTML) \
	$(BUILTMAN_ENPDF) \
        $(BUILTLESSONS_ESHTML) \
        $(BUILTLESSONS_ESPDF)

BUILTMAN_EN_SOURCES = \
	${man1DOCBOOK_EN:xml=1}  

man_MANS = \
         $(BUILTMAN_EN_SOURCES)

else
BUILT_SOURCES =
BUILTMAN_EN_SOURCES =
endif


MOSTLYCLEANFILES = \
         $(BUILT_SOURCES) \
         $(BUILTMAN_EN_SOURCES)

clean-generic: mostlyclean-generic
	-rmdir -p system/man1/en

EXTRA_DIST = \
	${articlesDOCBOOK_EN_IMAGES} \
	${articlesDOCBOOK_EN} \
	${booksDOCBOOK_EN_IMAGES} \
	${booksDOCBOOK_EN} \
	${lessonsDOCBOOK_EN_IMAGES} \
	${lessonsDOCBOOK_EN} \
	${man1DOCBOOK_EN} \
	${man1INTRO_EN} \
	${specificationsDOCBOOK_EN} \
        ${lessonsDOCBOOK_ES} \
	${lessonsDOCBOOK_ES_IMAGES} \
	README \
	articles/en/images/tire_fig12.svg \
	articles/en/images/tire_fig13.svg \
	books/README \
	books/en/tutorial_series_authors.xml \
	catalog.xml \
	fop.xconf.in \
	resources \
	system/man1/README \
	system/man1/mged_cmd_template.xml

include $(top_srcdir)/misc/Makefile.defs
