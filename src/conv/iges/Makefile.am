
ON_IGES = iges
IGES_FLAGS = -g $(OPENNURBS_CPPFLAGS) $(TNT_CPPFLAGS)

bin_PROGRAMS = iges-g g-iges
EXTRA_PROGRAMS = iges

AM_CPPFLAGS = ${TCL_CPPFLAGS} ${IGES_FLAGS}

man_MANS = g-iges.1 iges-g.1

noinst_HEADERS = \
	PullbackCurve.h \
	brlcad_brep.hpp \
	iges.h \
	iges_extern.h \
	iges_struct.h \
	iges_types.h \
	n_iges.hpp

iges_SOURCES = \
	BrepHandler.cpp \
	brlcad_brep.cpp \
	n_iges.cpp  \
	n_main.cpp

iges_LDADD = \
	${WDB} \
	${BU} \
	${BU_LIBS} \
	${OPENNURBS}

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
	${WDB} \
	${WDB_LIBS}

g_iges_SOURCES = \
	g-iges.c \
	iges.c

g_iges_LDADD = \
	${RT} \
	${RT_LIBS}

EXTRA_DIST = \
	$(man_MANS) \
	CMakeLists.txt \
	PullbackCurve.cpp \
	add_face.cpp \
	arrange.c \
	brep.cpp \
	bubbleup.c \
	copytree.c \
	ge6.igs \
	get_outer_shell.cpp \
	spl.c \
	treecheck.c \
	wing128.igs \
	woodie.igs

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(iges_g_OBJECTS) \
	$(g_iges_OBJECTS) \
	$(bin_PROGRAMS)
