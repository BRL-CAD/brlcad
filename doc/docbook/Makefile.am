XSLTPROC_FLAGS=--nonet --xinclude

XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-xhtml-stylesheet.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-man-stylesheet.xsl
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-fo-stylesheet.xsl
# for covers
XSL_FO_COVERS_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-fo-stylesheet-covers.xsl

# redefine APACHEFOP--better error checking
FOP_OPTS = "-Djava.awt.headless=true -Dxml.catalog.files=$(XML_CATALOG)"
APACHEFOP = FOP_OPTS=$(FOP_OPTS) /usr/local/src/fop-1.0/fop

# fop hyphenation path (fop version >= 1.0)
# need v2 hphenation
FOP_HYPH=$(top_srcdir)/doc/docbook/resources/offo-hyphenation-binary-v2.0/fop-hyph.jar

# fop hyphenation path (fop version < 1.0)
#FOP_HYPH=$(top_srcdir)/doc/docbook/resources/offo-hyphenation-fop-stable-binary-v1.2/fop-hyph.jar

FOP_ENV_VARS = FOP_HYPHENATION_PATH=$(FOP_HYPH) CLASSPATH=$(top_srcdir)/doc/docbook

# two xml catalog files
XML_CATALOG=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-xml-catalog-autogen.xml
FOP_XML_CATALOG=$(top_srcdir)/doc/docbook/resources/brlcad/CatalogManage.properties
# both files as dependencies
XML_CATALOGS=$(FOP_XML_CATALOG) $(XML_CATALOG)

# the xml catalog generator creates the two catalog files
GEN_XML_CATALOGS=$(top_srcdir)/doc/docbook/create-xml-catalogs.pl

# the covers generator uses the covers template to create a cover fo
# xsl for a book
GEN_FO_COVER=$(top_srcdir)/doc/docbook/create-book-covers.pl
# Perl doc module is a cover dep
DOC_MODULE=$(top_srcdir)/doc/docbook/BRLCAD_DOC.pm
# the template
FO_COVER_TEMPLATE=$(top_srcdir)/doc/docbook/resources/brlcad/book-covers-fo-template.xsl
# the dependencies
FO_COVER_DEPS=$(GEN_FO_COVER) $(FO_COVER_TEMPLATE) $(DOC_MODULE)

# the xml catalog location must be known by xsltproc
XSLTPROC_ENV_VARS = XML_CATALOG_FILES=$(XML_CATALOG) XML_DEBUG_CATALOG=1

$(XML_CATALOGS): $(GEN_XML_CATALOGS) BRLCAD_DOC.pm
	perl $(GEN_XML_CATALOGS)

.xml.html: $(XML_CATALOGS)
	@mkdir -p $$(dirname $@)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $<

.xml.1:  $(XML_CATALOGS)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.3:  $(XML_CATALOGS)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.5:  $(XML_CATALOGS)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.nged:  $(XML_CATALOGS)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.fo.pdf: .xml.fo
	$(FOP_ENV_VARS) $(APACHEFOP) -c fop.xconf -fo $< -pdf $@

.xml.fo:  $(XML_CATALOGS)
	@mkdir -p $$(dirname $@)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_FO_STYLESHEET) $<

# special for pdf books with covers
.PHONY: cover covers clean-covers clean-fonts

# the cover alone for testing
fo_out  = covers.fo
pdf_out = covers.pdf

# a target to help test font problems
clean-fonts:
	-rm articles/en/oed.pdf
	-rm system/man1/en/gqa.pdf
	-rm system/mann/en/db.pdf
	-rm system/mann/en/em.pdf
	-rm system/mann/en/gui.pdf
	-rm system/mann/en/lm.pdf
	-rm system/mann/en/t.pdf
	-rm system/mann/en/title.pdf

# a target to test all enbook covers
clean-covers:
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeI.fo
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeII.fo
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeIII.fo
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeIV.fo
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeI.pdf
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeII.pdf
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeIII.pdf
	-rm books/en/BRL-CAD_Tutorial_Series-VolumeIV.pdf

cover:  covers
covers: covers.pdf
#verbose = --verbose
#fobdebug = -d
covers.fo:  dummy.xml $(FO_COVER_DEPS)
	perl create-book-covers.pl $<
	@echo ============ making fo... =============
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(verbose) -o $(fo_out) \
          $(XSL_FO_COVERS_STYLESHEET) $<
	@echo Indenting fo output file \"$(fo_out)\".
	mv $(fo_out) t.fo
	xmllint --format t.fo > $(fo_out)
	-rm t.fo
	@echo See fo output file \"$(fo_out)\".

covers.pdf: covers.fo
	@echo ============ making pdf... =============
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $(fo_out) -pdf $(pdf_out)
	@echo ============ finished making $(pdf_out) =============

# we cannot get good covers at the moment if the make -j option is
# used so use the special target to prevent it from taking effect
.NOTPARALLEL:

# the books with covers
enbooks = \
	books/en/BRL-CAD_Tutorial_Series-VolumeI.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeII.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeIII.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeIV.pdf

# books need special cover stylesheets, and one for each book due to
# customized colors
fo_book = tmp.fo
$(enbooks): %.pdf: %.xml $(FO_COVER_DEPS)
	perl create-book-covers.pl $<
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $(fo_book) \
          $(XSL_FO_COVERS_STYLESHEET) $<
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $(fo_book) -pdf $@ \
	  && if test -f $(fo_book) ; then rm $(fo_book) ; fi

# new index product
#indexDOCBOOK_EN = \
#	index.html

indexDOCBOOK_ENhtmldir = $(BRLCAD_DATA)
indexDOCBOOK_ENhtml_DATA = \
	$(BUILTINDEX_ENHTML)

BUILTINDEX_ENHTML = \
	${indexDOCBOOK_EN}

include $(srcdir)/articles/en/Makefile.am
include $(srcdir)/books/en/Makefile.am
include $(srcdir)/lessons/en/Makefile.am
include $(srcdir)/lessons/es/Makefile.am
include $(srcdir)/presentations/en/Makefile.am
include $(srcdir)/specifications/en/Makefile.am
include $(srcdir)/system/man1/en/Makefile.am
include $(srcdir)/system/man3/en/Makefile.am
include $(srcdir)/system/man5/en/Makefile.am
include $(srcdir)/system/mann/en/Makefile.am



if BUILD_DOCS

BUILT_SOURCES = \
	$(BUILTARTICLES_ENHTML) \
	$(BUILTARTICLES_ENPDF) \
	$(BUILTBOOKS_ENHTML) \
	$(BUILTBOOKS_ENPDF) \
	$(BUILTINDEX_ENHTML) \
	$(BUILTLESSONS_ENHTML) \
	$(BUILTLESSONS_ENPDF) \
	$(BUILTLESSONS_ESHTML) \
	$(BUILTLESSONS_ESPDF) \
	$(BUILTMAN1_ENHTML) \
	$(BUILTMAN1_ENPDF) \
	$(BUILTMAN3_ENHTML) \
	$(BUILTMAN3_ENPDF) \
	$(BUILTMAN5_ENHTML) \
	$(BUILTMAN5_ENPDF) \
	$(BUILTMANN_ENHTML) \
	$(BUILTMANN_ENPDF) \
	$(BUILTPRESENTATIONS_ENHTML) \
	$(BUILTPRESENTATIONS_ENPDF) \
	$(BUILTSPECS_ENHTML) \
	$(BUILTSPECS_ENPDF)

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
	BRLCAD_DB_VALIDATION.pm \
	BRLCAD_DOC.pm \
	CMakeLists.txt \
	DBPATH.pm.in \
	README \
	README.DB_authors_notes \
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
	create-book-covers.pl \
	create-index.pl \
	create-xml-catalogs.pl \
	find-db-files.pl \
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
	system/mann/mged_cmd_template.xml \
	validate.pl

include $(top_srcdir)/misc/Makefile.defs
