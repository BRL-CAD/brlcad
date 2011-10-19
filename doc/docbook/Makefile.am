XSLTPROC_FLAGS=--nonet --xinclude

XSL_XHTML_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-xhtml-stylesheet.xsl
XSL_MAN_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-man-stylesheet.xsl
# not used for docs without covers:
XSL_FO_STYLESHEET=$(top_srcdir)/doc/docbook/resources/brlcad/brlcad-fo-stylesheet.xsl

# redefine APACHEFOP--better error checking
FOP_OPTS = "-Djava.awt.headless=true -Dxml.catalog.files=$(XML_CATALOG)"
APACHEFOP = FOP_OPTS=$(FOP_OPTS) /usr/local/src/fop-1.0/fop

# fop hyphenation path (fop version >= 1.0)
# need v2 hphenation
FOP_HYPH=$(top_srcdir)/doc/docbook/resources/other/offo/binary/fop-hyph.jar

# fop hyphenation path (fop version < 1.0)
#FOP_HYPH=$(top_srcdir)/doc/docbook/resources/offo-hyphenation-fop-stable-binary-v1.2/fop-hyph.jar

FOP_ENV_VARS = FOP_HYPHENATION_PATH=$(FOP_HYPH) CLASSPATH=$(top_srcdir)/doc/docbook

# two xml catalog files
XML_CATALOG=resources/brlcad/brlcad-xml-catalog-autogen.xml
FOP_XML_CATALOG=$(top_srcdir)/doc/docbook/resources/brlcad/CatalogManager.properties
# both files as dependencies
XML_CATALOGS= $(FOP_XML_CATALOG) $(XML_CATALOG)

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

$(XML_CATALOG): $(GEN_XML_CATALOGS) BRLCAD_DOC.pm
	perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS)

XSLTPROC_DEPS=$(XML_CATALOGS) $(XSL_FO_STYLESHEET) $(XSL_XHTML_STYLESHEET) $(XSL_MAN_STYLESHEET)

# define extra dependencies for some double suffix rules:
.html:  resources/brlcad/brlcad-xhtml-stylesheet.xsl $(XSLTPROC_DEPS)
.1:     resources/brlcad/brlcad-xhtml-stylesheet.xsl $(XSLTPROC_DEPS)
.3:     resources/brlcad/brlcad-xhtml-stylesheet.xsl $(XSLTPROC_DEPS)
.5:     resources/brlcad/brlcad-xhtml-stylesheet.xsl $(XSLTPROC_DEPS)
.nged:  resources/brlcad/brlcad-xhtml-stylesheet.xsl $(XSLTPROC_DEPS)
.fo:    $(XSLTPROC_DEPS)

# the xml_catalog dependency doesn't seem to work--hard wire it then
.xml.html:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
	@mkdir -p $$(dirname $@)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ $(XSL_XHTML_STYLESHEET) $<

.xml.1:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.3:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.5:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.xml.nged:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) $(XSL_MAN_STYLESHEET) $<
	@dir="$$(dirname $@)" && mkdir -p $$dir && mv '$(@F)' $$dir

.fo.pdf:
	$(FOP_ENV_VARS) $(APACHEFOP) -c fop.xconf -fo $< -pdf $@

# this rule must not be applied to books
.xml.fo:
	if test ! -f $(XML_CATALOG) ; then perl -I $(top_srcdir)/doc/docbook $(GEN_XML_CATALOGS) ; fi
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
covers.fo:  dummy.xml $(FO_COVER_DEPS) $(XML_CATALOG)
	perl -I $(top_srcdir)/doc/docbook create-book-covers.pl $<
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

# the books with covers
enbooks = \
	books/en/BRL-CAD_Tutorial_Series-VolumeI.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeII.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeIII.pdf \
	books/en/BRL-CAD_Tutorial_Series-VolumeIV.pdf

# books need special cover stylesheets, and one for each book due to
# customized colors

foss=doc/docbook/resources/brlcad/brlcad-fo-stylesheet-covers
covers_ss_v1=$(top_srcdir)/$(foss)-v1.xsl
covers_ss_v2=$(top_srcdir)/$(foss)-v2.xsl
covers_ss_v3=$(top_srcdir)/$(foss)-v3.xsl
covers_ss_v4=$(top_srcdir)/$(foss)-v4.xsl

# the actual cover ss
autofoss=doc/docbook/resources/brlcad/book-covers-fo-autogen
autogen_covers_ss_v1=$(top_srcdir)/$(autofoss)-v1.xsl
autogen_covers_ss_v2=$(top_srcdir)/$(autofoss)-v2.xsl
autogen_covers_ss_v3=$(top_srcdir)/$(autofoss)-v3.xsl
autogen_covers_ss_v4=$(top_srcdir)/$(autofoss)-v4.xsl

booksen1 = books/en/BRL-CAD_Tutorial_Series-VolumeI
booksen2 = books/en/BRL-CAD_Tutorial_Series-VolumeII
booksen3 = books/en/BRL-CAD_Tutorial_Series-VolumeIII
booksen4 = books/en/BRL-CAD_Tutorial_Series-VolumeIV

$(autogen_covers_ss_v1): $(booksen1).xml $(FOP_COVER_DEPS)
	./create-book-covers.pl $<

$(autogen_covers_ss_v2): $(booksen2).xml $(FOP_COVER_DEPS)
	./create-book-covers.pl $<

$(autogen_covers_ss_v3): $(booksen3).xml $(FOP_COVER_DEPS)
	./create-book-covers.pl $<

$(autogen_covers_ss_v4): $(booksen4).xml $(FOP_COVER_DEPS)
	./create-book-covers.pl $<

# en book fo
$(booksen1).fo : $(booksen1).xml $(FO_COVER_DEPS) $(XML_CATALOG) $(autogen_covers_ss_v1)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ \
          $(covers_ss_v1) $<

$(booksen2).fo : $(booksen2).xml $(FO_COVER_DEPS) $(XML_CATALOG) $(autogen_covers_ss_v2)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ \
          $(covers_ss_v2) $<

$(booksen3).fo : $(booksen3).xml $(FO_COVER_DEPS) $(XML_CATALOG) $(autogen_covers_ss_v3)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ \
          $(covers_ss_v3) $<

$(booksen4).fo : $(booksen4).xml $(FO_COVER_DEPS) $(XML_CATALOG) $(autogen_covers_ss_v4)
	$(XSLTPROC_ENV_VARS) $(XSLTPROC) $(XSLTPROC_FLAGS) -o $@ \
          $(covers_ss_v4) $<

# en book pdf
#	  && if test -f $< ; then rm $< ; fi
# for fo debugging, comment out the lines that remove the fo files
$(booksen1).pdf : $(booksen1).fo $(XML_CATALOG)
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $< -pdf $@ \
	  && if test -f $< ; then rm $< ; fi

$(booksen2).pdf : $(booksen2).fo $(XML_CATALOG)
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $< -pdf $@ \
	  && if test -f $< ; then rm $< ; fi

$(booksen3).pdf : $(booksen3).fo $(XML_CATALOG)
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $< -pdf $@ \
	  && if test -f $< ; then rm $< ; fi

$(booksen4).pdf : $(booksen4).fo $(XML_CATALOG)
	$(FOP_ENV_VARS) $(APACHEFOP) $(fopdebug) -c fop.xconf -fo $< -pdf $@ \
	  && if test -f $< ; then rm $< ; fi


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
	$(BUILTSPECS_ENPDF) \
	$(XML_CATALOGS)

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

# source and dest dir for BRLCAD html css stylesheets
cssBRLCADdir   = $(BRLCAD_DATA)/html/css
cssBRLCAD_DATA = css/brlcad.css

EXTRA_DIST = \
	${articlesDOCBOOK_EN_IMAGES} \
	${articlesDOCBOOK_EN} \
	${booksDOCBOOK_EN_IMAGES} \
	${booksDOCBOOK_EN} \
	${cssBRLCAD_DATA} \
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
	resources/README \
	resources/brlcad/book-covers-fo-template.xsl \
	resources/brlcad/brlcad-common.xsl \
	resources/brlcad/brlcad-fo-stylesheet-covers-v1.xsl \
	resources/brlcad/brlcad-fo-stylesheet-covers-v2.xsl \
	resources/brlcad/brlcad-fo-stylesheet-covers-v3.xsl \
	resources/brlcad/brlcad-fo-stylesheet-covers-v4.xsl \
	resources/brlcad/brlcad-fo-stylesheet.xsl \
	resources/brlcad/brlcad-fonts.xsl \
	resources/brlcad/brlcad-gendata.xsl \
	resources/brlcad/brlcad-man-stylesheet.xsl \
	resources/brlcad/brlcad-xhtml-header-navigation.xsl \
	resources/brlcad/brlcad-xhtml-stylesheet.xsl \
	resources/brlcad/brlcad-xml-catalog.xml.in \
	resources/brlcad/center-table-print.xsl \
	resources/brlcad/images \
	resources/brlcad/tutorial-cover-template.xsl.in \
	resources/brlcad/tutorial-template.xsl.in \
	resources/other \
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
