
lib_LTLIBRARIES = librt.la
noinst_LTLIBRARIES = \
	librt_nil.la \
	librt_xxx.la

BREP_TEST = brep_test
BREP_CPPFLAGS = ${SSE} -DOBJ_BREP=1 ${TNT_CPPFLAGS} ${OPENNURBS_CPPFLAGS}
BREP_LIBADD = ${OPENNURBS}

brep_test_SOURCES = brep_test.cpp
# cannot set per-target CPPFLAGS until automake 1.7+
brep_test_CXXFLAGS = ${SSE}
brep_test_LDADD = \
	librt.la \
	${RT_LIBS}

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${REGEX_CPPFLAGS} \
	${TCL_CPPFLAGS} \
	${BREP_CPPFLAGS}

AM_CFLAGS = ${STRICT_FLAGS}

noinst_PROGRAMS = \
	${BREP_TEST} \
	namegen \
	nurb_example \
	parsetest \
	prcomb

# make sure example and temporary code compiles, but don't include it in librt
librt_xxx_la_SOURCES = \
	primitives/nmg/nmg_junk.c \
	primitives/xxx/xxx.c

librt_nil_la_SOURCES = \
	attributes.c \
	bbox.c \
	bezier_2d_isect.c \
	binunif/binunif.c \
	binunif/db5_bin.c \
	bool.c \
	bundle.c \
	cmd.c \
	comb/comb.c \
	comb/comb_mirror.c \
	comb/db_comb.c \
	constraint.c \
	cut.c \
	db5_alloc.c \
	db5_io.c \
	db5_scan.c \
	db5_types.c \
	db_alloc.c \
	db_anim.c \
	db_corrupt.c \
	db_flags.c \
	db_flip.c \
	db_fullpath.c \
	db_inmem.c \
	db_io.c \
	db_lookup.c \
	db_match.c \
	db_open.c \
	db_path.c \
	db_scan.c \
	db_tree.c \
	db_walk.c \
	dir.c \
	dspline.c \
	fortray.c \
	globals.c \
	htbl.c \
	many.c \
	mater.c \
	memalloc.c \
	mkbundle.c \
	opennurbs_ext.cpp \
	oslo_calc.c \
	oslo_map.c \
	pr.c \
	prep.c \
	primitives/arb8/arb8.c \
	primitives/arb8/arb8_brep.cpp \
	primitives/arb8/arb8_mirror.c \
	primitives/arbn/arbn.c \
	primitives/arbn/arbn_brep.cpp \
	primitives/arbn/arbn_mirror.c \
	primitives/ars/ars.c \
	primitives/ars/ars_mirror.c \
	primitives/bot/bot.c \
	primitives/bot/bot_brep.cpp \
	primitives/bot/bot_mirror.c \
	primitives/bot/btg.c \
	primitives/bot/btgf.c \
	primitives/brep/brep.cpp \
	primitives/brep/brep_debug.cpp \
	primitives/bspline/bspline.cpp \
	primitives/bspline/bspline_mirror.c \
	primitives/bspline/nurb_basis.c \
	primitives/bspline/nurb_bezier.c \
	primitives/bspline/nurb_bound.c \
	primitives/bspline/nurb_brep.cpp \
	primitives/bspline/nurb_c2.c \
	primitives/bspline/nurb_copy.c \
	primitives/bspline/nurb_diff.c \
	primitives/bspline/nurb_eval.c \
	primitives/bspline/nurb_flat.c \
	primitives/bspline/nurb_interp.c \
	primitives/bspline/nurb_knot.c \
	primitives/bspline/nurb_norm.c \
	primitives/bspline/nurb_plot.c \
	primitives/bspline/nurb_poly.c \
	primitives/bspline/nurb_ray.c \
	primitives/bspline/nurb_refine.c \
	primitives/bspline/nurb_reverse.c \
	primitives/bspline/nurb_solve.c \
	primitives/bspline/nurb_split.c \
	primitives/bspline/nurb_tess.c \
	primitives/bspline/nurb_trim.c \
	primitives/bspline/nurb_trim_util.c \
	primitives/bspline/nurb_util.c \
	primitives/bspline/nurb_xsplit.c \
	primitives/cline/cline.c \
	primitives/dsp/dsp.c \
	primitives/dsp/dsp_brep.cpp \
	primitives/dsp/dsp_mirror.c \
	primitives/ebm/ebm.c \
	primitives/ebm/ebm_brep.cpp \
	primitives/ebm/ebm_mirror.c \
	primitives/ehy/ehy.c \
	primitives/ehy/ehy_brep.cpp \
	primitives/ell/ell.c \
	primitives/ell/ell_brep.cpp \
	primitives/ell/ell_mirror.c \
	primitives/epa/epa.c \
	primitives/epa/epa_brep.cpp \
	primitives/epa/epa_mirror.c \
	primitives/eto/eto.c \
	primitives/eto/eto_brep.cpp \
	primitives/eto/eto_mirror.c \
	primitives/extrude/extrude.c \
	primitives/extrude/extrude_brep.cpp \
	primitives/generic.c \
	primitives/grip/grip.c \
	primitives/grip/grip_mirror.c \
	primitives/half/half.c \
	primitives/half/half_mirror.c \
	primitives/hf/hf.c \
	primitives/hyp/hyp.c \
	primitives/hyp/hyp_brep.cpp \
	primitives/hyp/hyp_mirror.c \
	primitives/metaball/metaball.c \
	primitives/metaball/metaball_tri.c \
	primitives/mirror.c \
	primitives/nmg/nmg.c \
	primitives/nmg/nmg_bool.c \
	primitives/nmg/nmg_brep.cpp \
	primitives/nmg/nmg_ck.c \
	primitives/nmg/nmg_class.c \
	primitives/nmg/nmg_copy.c \
	primitives/nmg/nmg_eval.c \
	primitives/nmg/nmg_extrude.c \
	primitives/nmg/nmg_fcut.c \
	primitives/nmg/nmg_fuse.c \
	primitives/nmg/nmg_index.c \
	primitives/nmg/nmg_info.c \
	primitives/nmg/nmg_inter.c \
	primitives/nmg/nmg_manif.c \
	primitives/nmg/nmg_mesh.c \
	primitives/nmg/nmg_mirror.c \
	primitives/nmg/nmg_misc.c \
	primitives/nmg/nmg_mk.c \
	primitives/nmg/nmg_mod.c \
	primitives/nmg/nmg_plot.c \
	primitives/nmg/nmg_pr.c \
	primitives/nmg/nmg_pt_fu.c \
	primitives/nmg/nmg_rt_isect.c \
	primitives/nmg/nmg_rt_segs.c \
	primitives/nmg/nmg_tri.c \
	primitives/nmg/nmg_tri_mc.c \
	primitives/nmg/nmg_visit.c \
	primitives/obj_adjust.c \
	primitives/obj_curve.c \
	primitives/obj_describe.c \
	primitives/obj_export.c \
	primitives/obj_free.c \
	primitives/obj_get.c \
	primitives/obj_ifree.c \
	primitives/obj_import.c \
	primitives/obj_make.c \
	primitives/obj_mirror.c \
	primitives/obj_norm.c \
	primitives/obj_params.c \
	primitives/obj_plot.c \
	primitives/obj_prep.c \
	primitives/obj_print.c \
	primitives/obj_shot.c \
	primitives/obj_tess.c \
	primitives/obj_tnurb.c \
	primitives/obj_uv.c \
	primitives/obj_vshot.c \
	primitives/obj_xform.c \
	primitives/part/part.c \
	primitives/part/part_mirror.c \
	primitives/pipe/pipe.c \
	primitives/pipe/pipe_brep.cpp \
	primitives/pipe/pipe_mirror.c \
	primitives/pnts/pnts.c \
	primitives/poly/poly.c \
	primitives/poly/poly_brep.cpp \
	primitives/poly/poly_mirror.c \
	primitives/rec/rec.c \
	primitives/revolve/revolve.c \
	primitives/revolve/revolve_brep.cpp \
	primitives/rhc/rhc.c \
	primitives/rhc/rhc_brep.cpp \
	primitives/rhc/rhc_mirror.c \
	primitives/rpc/rpc.c \
	primitives/rpc/rpc_brep.cpp \
	primitives/rpc/rpc_mirror.c \
	primitives/sketch/sketch.c \
	primitives/sketch/sketch_brep.cpp \
	primitives/sph/sph.c \
	primitives/sph/sph_brep.cpp \
	primitives/submodel/submodel.c \
	primitives/superell/superell.c \
	primitives/superell/superell_mirror.c \
	primitives/table.c \
	primitives/tgc/tgc.c \
	primitives/tgc/tgc_brep.cpp \
	primitives/tgc/tgc_mirror.c \
	primitives/tor/tor.c \
	primitives/tor/tor_brep.cpp \
	primitives/tor/tor_mirror.c \
	primitives/vol/vol.c \
	primitives/vol/vol_brep.cpp \
	primitives/vol/vol_mirror.c \
	regionfix.c \
	roots.c \
	search.c \
	shoot.c \
	spectrum.c \
	tcl.c \
	timer42.c \
	tol.c \
	transform.c \
	tree.c \
	vers.c \
	vlist.c \
	vshoot.c \
	wdb.c

librt_nil_la_LIBADD = ${BREP_LIBADD}

librt_la_SOURCES =
librt_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
librt_la_LIBADD = \
	librt_nil.la \
	${RT_LIBS}

prcomb_SOURCES = prcomb.c
prcomb_LDADD = \
	librt.la \
	${RT_LIBS}

nurb_example_SOURCES = nurb_example.c
nurb_example_LDADD = \
	librt.la \
	${RT_LIBS}

namegen_SOURCES = namegen.c
namegen_LDADD = \
	librt.la \
	${RT_LIBS}

parsetest_SOURCES = columnparse.c
parsetest_LDADD = \
	librt.la \
	${RT_LIBS}


noinst_HEADERS = \
	librt_private.h \
	opennurbs_ext.h \
	primitives/bot/btg.h \
	primitives/bot/tieprivate.h \
	primitives/brep/brep_debug.h \
	primitives/dsp/dsp.h \
	primitives/fixpt.h \
	primitives/metaball/metaball.h \
	primitives/revolve/revolve.h \
	primitives/xxx/xxx.h

dist_man_MANS = librt.3

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	nurb_example.c \
	raydebug.tcl

# XXX need to fix these so the timers are selected by configure.ac
EXTRA_DIST = \
	$(sample_applications_DATA) \
	CMakeLists.txt \
	brep_test.cpp \
	columnparse.c \
	namegen.c \
	pmalloc.c \
	primitives/bot/g_bot_include.c \
	primitives/bot/tie.c \
	primitives/bot/tie_kdtree.c \
	search.h \
	timer-nt.c \
	timerhep.c \
	timerunix.c \
	uvpoints.cpp

if BUILD_REGEX
DEPADD = src/other/libregex
endif

DEPENDS = src/libbn src/libsysv src/other/openNURBS ${DEPADD}

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(librt_nil_la_OBJECTS) \
	$(librt_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(noinst_LTLIBRARIES) \
	$(comb_OBJECTS) \
	$(nurb_example_OBJECTS) \
	$(noinst_PROGRAMS)
