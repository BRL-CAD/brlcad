# $Id$


lib_LTLIBRARIES = librt.la
noinst_LTLIBRARIES = librt_nil.la

if WITH_OPENNURBS

BREP_SRCS = g_brep.cpp
BREP_TEST = brep_test
BREP_CPPFLAGS = -DOBJ_BREP=1 $(OPENNURBS_CPPFLAGS)

brep_test_SOURCES = brep_test.cpp
# cannot set per-target CPPFLAGS until automake 1.7+
brep_test_CXXFLAGS = ${SSE}
brep_test_LDADD = \
	librt.la ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS}

else # !WITH_OPENNURBS

BREP_SRCS =
BREP_TEST =
BREP_CPPFLAGS =

endif # WITH_OPENNURBS

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${TCL_CPPFLAGS} \
	${BREP_CPPFLAGS}

noinst_PROGRAMS = \
	comb \
	nurb_example \
	${BREP_TEST}

librt_nil_la_SOURCES = \
	$(BREP_SRCS) \
	bezier_2d_isect.c \
	bigE.c \
	binary_obj.c \
	bomb.c \
	bool.c \
	bundle.c \
	cmd.c \
	cut.c \
	db5_alloc.c \
	db5_bin.c \
	db5_comb.c \
	db5_io.c \
	db5_scan.c \
	db5_types.c \
	db_alloc.c \
	db_anim.c \
	db_comb.c \
	db_flags.c \
	db_inmem.c \
	db_io.c \
	db_lookup.c \
	db_match.c \
	db_open.c \
	db_path.c \
	db_scan.c \
	db_tree.c \
	db_walk.c \
	dg_obj.c \
	dir.c \
	dspline.c \
	fortray.c \
	g_arb.c \
	g_arbn.c \
	g_ars.c \
	g_bot.c \
	g_cline.c \
	g_dsp.c \
	g_ebm.c \
	g_ehy.c \
	g_ell.c \
	g_epa.c \
	g_eto.c \
	g_extrude.c \
	g_grip.c \
	g_half.c \
	g_hf.c \
	g_metaball.c \
	g_nmg.c \
	g_nurb.c \
	g_part.c \
	g_pg.c \
	g_pipe.c \
	g_rec.c \
	g_rhc.c \
	g_rpc.c \
	g_sketch.c \
	g_sph.c \
	g_submodel.c \
	g_superell.c \
	g_tgc.c \
	g_torus.c \
	g_vol.c \
	global.c \
	htbl.c \
	importFg4Section.c \
	many.c \
	mater.c \
	memalloc.c \
	mirror.c \
	mkbundle.c \
	nirt.c \
	nmg_bool.c \
	nmg_ck.c \
	nmg_class.c \
	nmg_eval.c \
	nmg_extrude.c \
	nmg_fcut.c \
	nmg_fuse.c \
	nmg_index.c \
	nmg_info.c \
	nmg_inter.c \
	nmg_manif.c \
	nmg_mesh.c \
	nmg_misc.c \
	nmg_mk.c \
	nmg_mod.c \
	nmg_plot.c \
	nmg_pr.c \
	nmg_pt_fu.c \
	nmg_rt_isect.c \
	nmg_rt_segs.c \
	nmg_tri.c \
	nmg_visit.c \
	nurb_basis.c \
	nurb_bezier.c \
	nurb_bound.c \
	nurb_c2.c \
	nurb_copy.c \
	nurb_diff.c \
	nurb_eval.c \
	nurb_flat.c \
	nurb_interp.c \
	nurb_knot.c \
	nurb_norm.c \
	nurb_plot.c \
	nurb_poly.c \
	nurb_ray.c \
	nurb_refine.c \
	nurb_reverse.c \
	nurb_solve.c \
	nurb_split.c \
	nurb_tess.c \
	nurb_trim.c \
	nurb_trim_util.c \
	nurb_util.c \
	nurb_xsplit.c \
	oslo_calc.c \
	oslo_map.c \
	pr.c \
	prep.c \
	qray.c \
	regionfix.c \
	roots.c \
	shoot.c \
	spectrum.c \
	storage.c \
	table.c \
	tcl.c \
	timer42.c \
	track.c \
	transform.c \
	tree.c \
	vdraw.c \
	vers.c \
	view_obj.c \
	vlist.c \
	wdb.c \
	wdb_comb_std.c \
	wdb_obj.c

EXTRA_librt_nil_la_SOURCES = \
	g_brep.cpp

librt_la_SOURCES =
librt_la_LDFLAGS = -version-info 19:1
librt_la_LIBADD = \
	librt_nil.la

comb_SOURCES = comb.c
comb_LDADD = \
	librt.la ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

nurb_example_SOURCES = nurb_example.c
nurb_example_LDADD = \
	librt.la ${RT_LIBS} \
	${BN} ${BN_LIBS} \
	${BU} ${BU_LIBS} \
	${SYSV} ${SYSV_LIBS}

include_HEADERS = \
	bot.h \
	debug.h \
	fixpt.h \
	plane.h \
	qray.h

dist_man_MANS = librt.3

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	g_xxx.c \
	nurb_example.c \
	raydebug.tcl

# XXX need to fix these so the timers are selected by configure.ac
EXTRA_DIST = \
	$(BREP_SRCS) \
	$(sample_applications_DATA) \
	g_bot_include.c \
	librt.dsp \
	nmg_junk.c \
	parse.c \
	pmalloc.c \
	sqrt.gould.s \
	sqrt.vax.s \
	timer-nt.c \
	timer52brl.c \
	timercos.c \
	timerhep.c \
	timerunix.c \
	vshoot.c

if BUILD_OPENNURBS
DEPADD=src/other/openNURBS
else
DEPADD=
endif
DEPS=src/libbn src/libsysv $(DEPADD)

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(librt_nil_la_OBJECTS) \
	$(librt_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(comb_OBJECTS) \
	$(nurb_example_OBJECTS) \
	$(noinst_PROGRAMS)
