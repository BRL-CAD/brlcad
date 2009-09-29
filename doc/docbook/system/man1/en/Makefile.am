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
	3ptarb.xml \
	B.xml \
	E.xml \
	M.xml \
	Z.xml \
	adc.xml \
	ae.xml \
	alias-pix.xml \
	all_sf.xml \
	analyze.xml \
	anim_cascade.xml \
	anim_fly.xml \
	anim_hardtrack.xml \
	anim_keyread.xml \
	anim_lookat.xml \
	anim_offset.xml \
	anim_orient.xml \
	animmate.xml \
	apropos.xml \
	aproposdevel.xml \
	aproposlib.xml \
	arb.xml \
	arced.xml \
	area.xml \
	arot.xml \
	attach.xml \
	attr.xml \
	autoview.xml \
	bb.xml \
	bev.xml \
	bo.xml \
	bolt.xml \
	bot_condense.xml \
	bot_decimate.xml \
	bot_face_fuse.xml \
	bot_face_sort.xml \
	bot_vertex_fuse.xml \
	build_region.xml \
	bwscale.xml \
	c.xml \
	cat.xml \
	center.xml \
	coil.xml \
	color.xml \
	comb_color.xml \
	copyeval.xml \
	copymat.xml \
	cp.xml \
	cpi.xml \
	d.xml \
	dall.xml \
	db.xml \
	db_glob.xml \
	dbconcat.xml \
	debugbu.xml \
	debugdir.xml \
	debuglib.xml \
	debugmem.xml \
	debugnmg.xml \
	decompose.xml \
	delay.xml \
	dm.xml \
	gastank.xml \
	gqa.xml \
	handle.xml \
	mged.xml \
	nirt.xml \
	picket_fence.xml \
	rt.xml \
	rtarea.xml \
	rtcheck.xml \
	rtedge.xml \
	search.xml \
	tire.xml \
	window.xml \
	window_frame.xml \
	wire.xml

man1INTRO = \
	Introduction.xml

man1DOCBOOKhtmldir = $(BRLCAD_DATA)/html/man1/en
man1DOCBOOKhtml_DATA = $(BUILTHTML)

BUILTHTML = \
	${man1DOCBOOK:xml=html} \
	${man1INTRO:xml=html} 

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
	${man1DOCBOOK} \
	${man1INTRO}

include $(top_srcdir)/misc/Makefile.defs
