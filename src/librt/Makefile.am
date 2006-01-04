# $Id$

lib_LTLIBRARIES = librt.la

SRCS = \
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
	rt_dspline.c \
	shoot.c \
	spectrum.c \
	storage.c \
	table.c \
	tcl.c \
	timer42.c \
	track.c \
	tree.c \
	vdraw.c \
	view_obj.c \
	vlist.c \
	wdb.c \
	wdb_comb_std.c \
	wdb_obj.c

librt_la_SOURCES = \
	$(SRCS) \
	vers.c

librt_la_LDFLAGS = -version-info 19:1
librt_la_LIBADD = \
	${BN} \
	${BU} \
	${TCL} \
	${REGEX} \
	${LIBM}

include_HEADERS = \
	bot.h \
	debug.h \
	fixpt.h \
	plane.h \
	qray.h

dist_man_MANS = librt.3

noinst_PROGRAMS = \
	comb \
	nurb_example

comb_LDADD = librt.la ${BU}
comb_SOURCES = comb.c

nurb_example_LDADD = librt.la ${BU}
nurb_example_SOURCES = nurb_example.c

sample_applicationsdir = $(SAMPLE_APPLICATIONS_DIR)

sample_applications_DATA = \
	g_xxx.c \
	nurb_example.c \
	raydebug.tcl

# XXX need to fix these so the timers are selected by configure.ac
EXTRA_DIST = \
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
	vers_win.c \
	vshoot.c

BUILT_SOURCES = vers.c

vers.c: $(SRCS)
	@SH@ $(top_srcdir)/sh/vers.sh rt_version "The BRL-CAD Ray-Tracing Library" > $@

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = version
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(librt_la_OBJECTS) \
	$(lib_LTLIBRARIES) \
	$(comb_OBJECTS) \
	$(nurb_example_OBJECTS) \
	$(noinst_PROGRAMS)
