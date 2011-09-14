XSLTPROC_FLAGS=--nonet --xinclude
XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/xhtml-1_1/docbook.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/manpages/docbook.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/standard/xsl/fo/docbook.xsl

.xml.html:
	@mkdir -p $$(dirname $@)
	$(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $?

.xml.1:
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.3:
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.5:
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.nged:
	$(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $?
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.fo.pdf: .xml.fo
	$(APACHEFOP) -c fop.xconf $? -pdf $@

.xml.fo:
	@mkdir -p $$(dirname $@)
	$(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $?


include $(srcdir)/articles/en/Makefile.am
include $(srcdir)/books/en/Makefile.am
include $(srcdir)/lessons/en/Makefile.am
include $(srcdir)/lessons/es/Makefile.am
include $(srcdir)/specifications/en/Makefile.am
include $(srcdir)/system/man1/en/Makefile.am
include $(srcdir)/system/man3/en/Makefile.am
include $(srcdir)/system/man5/en/Makefile.am
include $(srcdir)/system/mann/en/Makefile.am
include $(srcdir)/presentations/en/Makefile.am

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
	$(BUILTLESSONS_ESPDF) \
	$(BUILTPRESENTATIONS_ENHTML) \
	$(BUILTPRESENTATIONS_ENPDF)

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
	${presentationsDOCBOOK_EN_IMAGES} \
	${presentationsDOCBOOK_EN} \
	${specificationsDOCBOOK_EN} \
	README \
	CMakeLists.txt \
	articles/CMakeLists.txt \
	articles/en/CMakeLists.txt \
	articles/en/Makefile.am \
	articles/en/images/tire_fig12.svg \
	articles/en/images/tire_fig13.svg \
	books/CMakeLists.txt \
	books/README \
	books/en/CMakeLists.txt \
	books/en/Makefile.am \
	books/en/tutorial_series_authors.xml \
	fop.xconf.in \
	lessons/CMakeLists.txt \
	lessons/en/CMakeLists.txt \
	lessons/en/Makefile.am \
	lessons/es/CMakeLists.txt \
	lessons/es/Makefile.am \
	log4j.properties \
	presentations/CMakeLists.txt \
	presentations/en/CMakeLists.txt \
	resources \
	specifications/CMakeLists.txt \
	specifications/en/CMakeLists.txt \
	specifications/en/Makefile.am \
	system/CMakeLists.txt \
	system/README \
	system/man1/CMakeLists.txt \
	system/man1/en/CMakeLists.txt \
	system/man1/en/Makefile.am \
	system/man3/CMakeLists.txt \
	system/man3/en/CMakeLists.txt \
	system/man3/en/Makefile.am \
	system/man5/CMakeLists.txt \
	system/man5/en/CMakeLists.txt \
	system/man5/en/Makefile.am \
	system/mann/CMakeLists.txt \
	system/mann/en/CMakeLists.txt \
	system/mann/en/Makefile.am \
	system/mann/mged_cmd_template.xml

include $(top_srcdir)/misc/Makefile.defs
