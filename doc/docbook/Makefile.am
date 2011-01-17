XSLTPROC_FLAGS=-nonet -xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl
XML_CATALOG_FILES=$(top_srcdir)/doc/docbook/catalog.xml

.xml.html:
	@mkdir -p $$(dirname $@)
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?

.xml.1:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.3:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.5:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.nged:
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.fo.pdf: .xml.fo
	$(APACHEFOP) -c fop.xconf $? -pdf $@

.xml.fo:
	@mkdir -p $$(dirname $@)
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?


include $(srcdir)/articles/en/Makefile.am
include $(srcdir)/books/en/Makefile.am
include $(srcdir)/lessons/en/Makefile.am
include $(srcdir)/lessons/es/Makefile.am
include $(srcdir)/specifications/en/Makefile.am
include $(srcdir)/system/man1/en/Makefile.am
include $(srcdir)/system/man3/en/Makefile.am
include $(srcdir)/system/man5/en/Makefile.am
include $(srcdir)/system/mann/en/Makefile.am

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
	$(BUILTMAN3_ENHTML) \
	$(BUILTMAN3_ENPDF) \
	$(BUILTMAN5_ENHTML) \
	$(BUILTMAN5_ENPDF) \
	$(BUILTMANN_ENHTML) \
	$(BUILTMANN_ENPDF) \
	$(BUILTLESSONS_ESHTML) \
	$(BUILTLESSONS_ESPDF)

BUILTMAN_EN_SOURCES = \
	${man1DOCBOOK_EN:xml=1}  \
	${man3DOCBOOK_EN:xml=3}  \
	${man5DOCBOOK_EN:xml=5}  \
	${mannDOCBOOK_EN:xml=nged}

man_MANS = \
	$(BUILTMAN_EN_SOURCES)

else
BUILT_SOURCES =
BUILTMAN_EN_SOURCES =
endif


CLEANFILES = \
	$(BUILTMAN_EN_SOURCES) \
	$(BUILT_SOURCES)


EXTRA_DIST = \
	${articlesDOCBOOK_EN_IMAGES} \
	${articlesDOCBOOK_EN} \
	${booksDOCBOOK_EN_IMAGES} \
	${booksDOCBOOK_EN} \
	${lessonsDOCBOOK_EN_IMAGES} \
	${lessonsDOCBOOK_EN} \
	${lessonsDOCBOOK_ES_IMAGES} \
	${lessonsDOCBOOK_ES} \
	${man1DOCBOOK_EN} \
	${man3DOCBOOK_EN} \
	${man5DOCBOOK_EN} \
	${mannDOCBOOK_EN} \
	${mannINTRO_EN} \
	${specificationsDOCBOOK_EN} \
	README \
	articles/en/Makefile.am \
	articles/en/images/tire_fig12.svg \
	articles/en/images/tire_fig13.svg \
	books/README \
	books/en/Makefile.am \
	books/en/tutorial_series_authors.xml \
	catalog.xml \
	fop.xconf.in \
	lessons/en/Makefile.am \
	lessons/es/Makefile.am \
	log4j.properties \
	resources \
	specifications/en/Makefile.am \
	system/README \
	system/man1/en/Makefile.am \
	system/man1/mged_cmd_template.xml \
	system/man3/en/Makefile.am \
	system/man3/mged_cmd_template.xml \
	system/man5/en/Makefile.am \
	system/mann/en/Makefile.am

include $(top_srcdir)/misc/Makefile.defs
