# $Id$
bin_PROGRAMS = mged cad_boundp cad_parea

mged_SOURCES = \
	adc.c \
	anal.c \
	animedit.c \
	arbs.c \
	attach.c \
	axes.c \
	bodyio.c \
	buttons.c \
	chgmodel.c \
	chgtree.c \
	chgview.c \
	cmd.c \
	color_scheme.c \
	columns.c \
	concat.c \
	dir.c \
	dm-X.c \
	dm-generic.c \
	dm-ogl.c \
	dm-plot.c \
	dm-ps.c \
	dodraw.c \
	doevent.c \
	dozoom.c \
	edarb.c \
	edars.c \
	edpipe.c \
	edsol.c \
	facedef.c \
	fbserv.c \
	ged.c \
	grid.c \
	history.c \
	inside.c \
	mater.c \
	menu.c \
	mover.c \
	muves.c \
	overlay.c \
	plot.c \
	polyif.c \
	predictor.c \
	qray.c \
	rect.c \
	red.c \
	rtif.c \
	scroll.c \
	set.c \
	share.c \
	solids_on_ray.c \
	tedit.c \
	titles.c \
	track.c \
	typein.c \
	update.c \
	usepen.c \
	utility1.c \
	utility2.c \
	vdraw.c \
	vers.c \
	vparse.c \
	vrlink.c

cad_boundp_SOURCES = cad_boundp.c
cad_parea_SOURCES = cad_parea.c

noinst_HEADERS = \
	cmd.h \
	comb_bool.h \
	editobjGui.h \
	fbserv.h \
	ged.h \
	htmlLibraryUi.h \
	icreateGui.h \
	joints.h \
	menu.h \
	mgdev.h \
	mged_dm.h \
	mged_solid.h \
	mgedtcl.h \
	objdir.h \
	qray.h \
	scroll.h \
	sedit.h \
	sunicon.h \
	titles.h \
	vgdev.h \
	vld_std.h \
	vmathUi.h

INCLUDES = @GL_CFLAGS@ @X_CFLAGS@ -I$(top_srcdir)/h

mged_LDADD = @DM@ @FB@ @ITCL_LIBS@ @ITK_LIBS@ @SYSV@ @TERMIO@ @TK_LIBS@ @WDB@ @GL_LIBS@ @PNG_LIBS@

cad_parea_LDADD = @BU@
cad_boundp_LDADD = @BU@

man_MANS = cad_boundp.1 cad_parea.1 mged.1
EXTRA_DIST = $(man_MANS)

BUILT_SOURCES = vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh
