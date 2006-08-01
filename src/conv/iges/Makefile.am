# $Id$

bin_PROGRAMS = iges-g g-iges

man_MANS = g-iges.1 iges-g.1

noinst_HEADERS = \
	iges.h \
	iges_extern.h \
	iges_struct.h \
	iges_types.h

iges_g_srcs = \
	add_face.c \
	add_inner_shell.c \
	add_loop.c \
	b-spline.c \
	block2.c \
	brep.c \
	brlabs.c \
	check_names.c \
	cone.c \
	conv_drawings.c \
	convassem.c \
	convinst.c \
	convsolids.c \
	convsurf.c \
	convtree.c \
	cyl.c \
	do_subfigs.c \
	docolor.c \
	ell.c \
	evalxform.c \
	extrudcirc.c \
	extrudcon.c \
	extrude.c \
	findp.c \
	freetree.c \
	get_att.c \
	get_cnurb_curve.c \
	get_edge_list.c \
	get_iges_vertex.c \
	get_outer_shell.c \
	get_vertex.c \
	get_vertex_list.c \
	getcurve.c \
	iges_types.c \
	main.c \
	make_face.c \
	make_nurb_face.c \
	makedir.c \
	makegroup.c \
	makemembers.c \
	matmult.c \
	orient_loops.c \
	planar_nurb.c \
	read_att.c \
	read_edge_list.c \
	read_vertex_list.c \
	readcnv.c \
	readcols.c \
	readdbl.c \
	readflt.c \
	readglobal.c \
	readint.c \
	readmatrix.c \
	readname.c \
	readrec.c \
	readstart.c \
	readstrg.c \
	readtime.c \
	readtree.c \
	recsize.c \
	revolve.c \
	showtree.c \
	sphere.c \
	spline.c \
	splinef.c \
	stack.c \
	summary.c \
	tor.c \
	trimsurf.c \
	usage.c \
	wedge.c

iges_g_SOURCES = $(iges_g_srcs) iges-g-vers.c
iges_g_LDADD = ${RT} ${WDB}

g_iges_srcs = g-iges.c iges.c
g_iges_SOURCES = $(g_iges_srcs) g-iges-vers.c
g_iges_LDADD = ${RT}

BUILT_SOURCES = \
	iges-g-vers.c \
	g-iges-vers.c

iges-g-vers.c: $(iges_g_srcs) Makefile.am
	sh $(top_srcdir)/sh/vers.sh version "IGES-to-BRLCAD Translator" > $@

g-iges-vers.c: $(g_iges_srcs) Makefile.am
	sh $(top_srcdir)/sh/vers.sh version "BRLCAD-to-IGES Translator" > $@

EXTRA_DIST = \
	$(man_MANS) \
	arrange.c \
	bubbleup.c \
	copytree.c \
	g-iges-vers_win.c \
	ge6.igs \
	iges-g-vers_win.c \
	spl.c \
	treecheck.c \
	wing128.igs \
	woodie.igs

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(iges_g_OBJECTS) \
	$(g_iges_OBJECTS) \
	$(bin_PROGRAMS)
