# $Id$
bin_PROGRAMS = mged cad_boundp cad_parea

mged_SOURCES = ged.c dodraw.c dozoom.c titles.c scroll.c usepen.c dir.c columns.c mover.c tedit.c cmd.c chgview.c chgmodel.c chgtree.c rtif.c anal.c typein.c predictor.c buttons.c menu.c adc.c edsol.c edarb.c edpipe.c edars.c polyif.c red.c mater.c plot.c overlay.c inside.c concat.c utility1.c utility2.c track.c arbs.c facedef.c set.c animedit.c solids_on_ray.c history.c vrlink.c attach.c dm-plot.c dm-ps.c muves.c doevent.c update.c dm-X.c dm-generic.c vdraw.c fbserv.c rect.c grid.c axes.c qray.c color_scheme.c vparse.c share.c bodyio.c vers.c
cad_boundp_SOURCES = cad_boundp.c
cad_parea_SOURCES = cad_parea.c

noinst_HEADERS = cmd.h comb_bool.h editobjGui.h fbserv.h ged.h htmlLibraryUi.h icreateGui.h joints.h menu.h mgdev.h mged_dm.h mged_solid.h mgedtcl.h objdir.h qray.h scroll.h sedit.h sunicon.h titles.h vgdev.h vld_std.h vmathUi.h

INCLUDES = @X_CFLAGS@ -I$(top_srcdir)/h


mged_LDADD = @DM@ @FB@ @ITCL_LIBS@ @ITK_LIBS@ @SYSV@ @TERMIO@ @TK_LIBS@ @WDB@

cad_parea_LDADD = @BU@
cad_boundp_LDADD = @BU@

man_MANS = cad_boundp.1 cad_parea.1 mged.1
EXTRA_DIST = $(man_MANS)

BUILT_SOURCES = vers.c
vers.c:
	$(top_srcdir)/sh/newvers.sh
