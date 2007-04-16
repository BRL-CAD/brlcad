# $Id$

bin_PROGRAMS = iges-g g-iges

AM_CPPFLAGS = ${TCL_CPPFLAGS}

man_MANS = g-iges.1 iges-g.1

noinst_HEADERS = \
	iges.h \
	iges_extern.h \
	iges_struct.h \
	iges_types.h

iges_g_SOURCES = \
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

iges_g_LDADD = \
	${WDB} ${WDB_LIBS} \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

g_iges_SOURCES = \
	g-iges.c \
	iges.c

g_iges_LDADD = \
	${RT} ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

EXTRA_DIST = \
	$(man_MANS) \
	arrange.c \
	bubbleup.c \
	copytree.c \
	ge6.igs \
	spl.c \
	treecheck.c \
	wing128.igs \
	woodie.igs

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(iges_g_OBJECTS) \
	$(g_iges_OBJECTS) \
	$(bin_PROGRAMS)
