# $Id$


lib_LTLIBRARIES = librt.la
noinst_LTLIBRARIES = \
	librt_nil.la \
	librt_xxx.la

BREP_TEST = brep_test
BREP_CPPFLAGS = @SSE@ -DOBJ_BREP=1 ${TNT_CPPFLAGS} ${OPENNURBS_CPPFLAGS}
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

noinst_PROGRAMS = \
	comb \
	nurb_example \
	${BREP_TEST}

# make sure the example compiles, but don't include it in librt
librt_xxx_la_SOURCES = primitives/g_xxx.c

librt_nil_la_SOURCES = \
	bezier_2d_isect.c \
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
	dir.c \
	dspline.c \
	fortray.c \
	globals.c \
	htbl.c \
	many.c \
	mater.c \
	memalloc.c \
	mirror.c \
	mkbundle.c \
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
	opennurbs_ext.cpp \
	oslo_calc.c \
	oslo_map.c \
	pr.c \
	prep.c \
	primitives/g_arb.c \
	primitives/g_arbn.c \
	primitives/g_ars.c \
	primitives/g_bot.c \
	primitives/g_brep.cpp \
	primitives/g_cline.c \
	primitives/g_dsp.c \
	primitives/g_ebm.c \
	primitives/g_ehy.c \
	primitives/g_ell.c \
	primitives/g_epa.c \
	primitives/g_eto.c \
	primitives/g_extrude.c \
	primitives/g_grip.c \
	primitives/g_half.c \
	primitives/g_hf.c \
	primitives/g_hyp.c \
	primitives/g_metaball.c \
	primitives/g_nmg.c \
	primitives/g_nurb.c \
	primitives/g_part.c \
	primitives/g_pg.c \
	primitives/g_pipe.c \
	primitives/g_rec.c \
	primitives/g_rhc.c \
	primitives/g_rpc.c \
	primitives/g_sketch.c \
	primitives/g_sph.c \
	primitives/g_submodel.c \
	primitives/g_superell.c \
	primitives/g_tgc.c \
	primitives/g_torus.c \
	primitives/g_vol.c \
	primitives/nurb/nurb_basis.c \
	primitives/nurb/nurb_bezier.c \
	primitives/nurb/nurb_bound.c \
	primitives/nurb/nurb_c2.c \
	primitives/nurb/nurb_copy.c \
	primitives/nurb/nurb_diff.c \
	primitives/nurb/nurb_eval.c \
	primitives/nurb/nurb_flat.c \
	primitives/nurb/nurb_interp.c \
	primitives/nurb/nurb_knot.c \
	primitives/nurb/nurb_norm.c \
	primitives/nurb/nurb_plot.c \
	primitives/nurb/nurb_poly.c \
	primitives/nurb/nurb_ray.c \
	primitives/nurb/nurb_refine.c \
	primitives/nurb/nurb_reverse.c \
	primitives/nurb/nurb_solve.c \
	primitives/nurb/nurb_split.c \
	primitives/nurb/nurb_tess.c \
	primitives/nurb/nurb_trim.c \
	primitives/nurb/nurb_trim_util.c \
	primitives/nurb/nurb_util.c \
	primitives/nurb/nurb_xsplit.c \
	regionfix.c \
	roots.c \
	shoot.c \
	spectrum.c \
	storage.c \
	table.c \
	tcl.c \
	timer42.c \
	transform.c \
	tree.c \
	vers.c \
	vlist.c \
	wdb.c

librt_nil_la_LIBADD = ${BREP_LIBADD}

librt_la_SOURCES =
librt_la_LDFLAGS = -version-info 19:1 -no-undefined -Wc,${FEXCEPTIONS}
librt_la_LIBADD = \
	librt_nil.la \
	${RT_LIBS}

comb_SOURCES = comb.c
comb_LDADD = \
	librt.la \
	${RT_LIBS}

nurb_example_SOURCES = nurb_example.c
nurb_example_LDADD = \
	librt.la \
	${RT_LIBS}

# XXX this should be noinst or moved to include
include_HEADERS = \
	primitives/fixpt.h

dist_man_MANS = librt.3

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	nurb_example.c \
	primitives/g_xxx.c \
	raydebug.tcl

# XXX need to fix these so the timers are selected by configure.ac
EXTRA_DIST = \
	$(sample_applications_DATA) \
        CMakeLists.txt \
	brep_test.cpp \
	nmg_junk.c \
	parse.c \
	pmalloc.c \
	primitives/g_bot_include.c \
	timer-nt.c \
	timer52brl.c \
	timerhep.c \
	timerunix.c \
	vshoot.c

if BUILD_REGEX
DEPADD = src/other/libregex
endif

DEPENDS = src/libbn src/libsysv src/other/openNURBS ${DEPADD}

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(librt_nil_la_OBJECTS) \
	$(librt_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(comb_OBJECTS) \
	$(nurb_example_OBJECTS) \
	$(noinst_PROGRAMS)
