# $Id$

bin_PROGRAMS = iges-g g-iges

noinst_HEADERS = iges_extern.h iges.h iges_struct.h iges_types.h


iges_g_srcs = main.c usage.c readstart.c recsize.c readrec.c readglobal.c readstrg.c readint.c readflt.c readtime.c makedir.c readcols.c findp.c summary.c stack.c readtree.c showtree.c convtree.c freetree.c makemembers.c convsolids.c readname.c convassem.c matmult.c readcnv.c evalxform.c readmatrix.c makegroup.c block2.c sphere.c wedge.c tor.c cyl.c cone.c ell.c convinst.c extrude.c extrudcirc.c extrudcon.c getcurve.c revolve.c splinef.c readdbl.c brlabs.c docolor.c spline.c convsurf.c b-spline.c brep.c get_outer_shell.c add_inner_shell.c make_face.c add_face.c add_loop.c get_edge_list.c read_edge_list.c get_vertex_list.c read_vertex_list.c get_vertex.c get_att.c read_att.c check_names.c planar_nurb.c conv_drawings.c orient_loops.c trimsurf.c iges_types.c make_nurb_face.c get_iges_vertex.c get_cnurb_curve.c do_subfigs.c
iges_g_SOURCES = $(iges_g_srcs) iges-g-vers.c
iges_g_LDADD = @RT@ @WDB@

g_iges_srcs = g-iges.c iges.c
g_iges_SOURCES = $(g_iges_srcs) g-iges-vers.c
g_iges_LDADD = @RT@

iges-g-vers.c: $(iges_g_srcs) Makefile.am
	sh $(top_srcdir)/sh/newvers2.sh version "IGES-to-BRLCAD Translator" > $@

g-iges-vers.c: $(g_iges_srcs) Makefile.am
	sh $(top_srcdir)/sh/newvers2.sh version "BRLCAD-to-IGES Translator" > $@

BUILT_SOURCES = iges-g-vers.c g-iges-vers.c

man_MANS = g-iges.1 iges-g.1
EXTRA_DIST = $(man_MANS)
