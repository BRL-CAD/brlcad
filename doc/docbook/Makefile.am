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

.fo.pdf: .xml.fo
	$(APACHEFOP) -c fop.xconf $? -pdf $@

.xml.fo:
	@mkdir -p $$(dirname $@)
	XML_CATALOG_FILES=$(XML_CATALOG_FILES) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?

ARTICLES_EN_MAKEFILE_AM = $(srcdir)/articles/en/Makefile.am
BOOKS_EN_MAKEFILE_AM = $(srcdir)/books/en/Makefile.am
LESSONS_EN_MAKEFILE_AM = $(srcdir)/lessons/en/Makefile.am
LESSONS_ES_MAKEFILE_AM = $(srcdir)/lessons/es/Makefile.am
SPECIFICATIONS_EN_MAKEFILE_AM = $(srcdir)/specifications/en/Makefile.am
SYSTEM_MAN1_EN_MAKEFILE_AM = $(srcdir)/system/man1/en/Makefile.am
SYSTEM_MAN3_EN_MAKEFILE_AM = $(srcdir)/system/man3/en/Makefile.am

include $(srcdir)/articles/en/Makefile.am
include $(srcdir)/books/en/Makefile.am
include $(srcdir)/lessons/en/Makefile.am
include $(srcdir)/lessons/es/Makefile.am
include $(srcdir)/specifications/en/Makefile.am
include $(srcdir)/system/man1/en/Makefile.am
include $(srcdir)/system/man3/en/Makefile.am

MAKEFILE_TEMPLATES = \
	${ARTICLES_EN_MAKEFILE_AM} \
	${BOOKS_EN_MAKEFILE_AM} \
	${LESSONS_EN_MAKEFILE_AM} \
	${LESSONS_ES_MAKEFILE_AM} \
	${SPECIFICATIONS_EN_MAKEFILE_AM} \
	${SYSTEM_MAN1_EN_MAKEFILE_AM} \
	${SYSTEM_MAN3_EN_MAKEFILE_AM}

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
	$(BUILTLESSONS_ESHTML) \
	$(BUILTLESSONS_ESPDF)

BUILTMAN_EN_SOURCES = \
	${man1DOCBOOK_EN:xml=1}  \
	${man3DOCBOOK_EN:xml=3}

man_MANS = \
	$(BUILTMAN_EN_SOURCES)

else
BUILT_SOURCES =
BUILTMAN_EN_SOURCES =
endif


CLEANFILES = \
	$(BUILT_SOURCES) \
	$(BUILTMAN_EN_SOURCES)


EXTRA_DIST = \
	${MAKEFILE_TEMPLATES} \
	${articlesDOCBOOK_EN_IMAGES} \
	${articlesDOCBOOK_EN} \
	${booksDOCBOOK_EN_IMAGES} \
	${booksDOCBOOK_EN} \
	${lessonsDOCBOOK_EN_IMAGES} \
	${lessonsDOCBOOK_EN} \
	${lessonsDOCBOOK_ES_IMAGES} \
	${lessonsDOCBOOK_ES} \
	${man1DOCBOOK_EN} \
	${man1INTRO_EN} \
	${man3DOCBOOK_EN} \
	${specificationsDOCBOOK_EN} \
	README \
	articles/en/images/tire_fig12.svg \
	articles/en/images/tire_fig13.svg \
	books/README \
	books/en/tutorial_series_authors.xml \
	catalog.xml \
	fop.xconf.in \
	resources \
	system/README \
	system/man1/mged_cmd_template.xml \
	system/man3/mged_cmd_template.xml

include $(top_srcdir)/misc/Makefile.defs
