
this_dir = tclscripts/mged
tcldir = $(BRLCAD_DATA)/$(this_dir)

if BUILD_DOCS
doc_scripts = man.tcl
else
doc_scripts =
endif

scripties = \
	$(doc_scripts) \
	accel.tcl \
	adc.tcl \
	anim.tcl \
	apply.tcl \
	apply_mat.tcl \
	asc2g.tcl \
	attr_edit.tcl \
	bindings.tcl \
	bot_face_select.tcl \
	botedit.tcl \
	bots.tcl \
	build_region.tcl \
	calipers.tcl \
	callbacks.tcl \
	clear.tcl \
	collaborate.tcl \
	color.tcl \
	color_scheme.tcl \
	comb.tcl \
	combmenu.tcl \
	cycle.tcl \
	dbfindtree.tcl \
	dbupgrade.tcl \
	dmtype.tcl \
	e_id.tcl \
	edit_menu.tcl \
	edit_solid.tcl \
	edit_solid_int.tcl \
	editmenu.tcl \
	editobj.tcl \
	eobjmenu.tcl \
	expand_comb.tcl \
	extract.tcl \
	facetize_all_regions.tcl \
	font.tcl \
	g2asc.tcl \
	garbage_collect.tcl \
	get_regions.tcl \
	grid.tcl \
	help.tcl \
	helpdevel.tcl \
	icreate.tcl \
	illum.tcl \
	lgt_mat.tcl \
	list.tcl \
	make_solid.tcl \
	menu.tcl \
	mged.tcl \
	mgedrc.tcl \
	mike.tcl \
	mouse.tcl \
	muves.tcl \
	mview.tcl \
	openw.tcl \
	overlap.tcl \
	pattern.tcl \
	pattern_gui.tcl \
	plot.tcl \
	points.tcl \
	prj_add.tcl \
	ps.tcl \
	qray.tcl \
	ray.tcl \
	raypick.tcl \
	reid.tcl \
	remap_mater.tcl \
	remat.tcl \
	rt.tcl \
	rt_script.tcl \
	sample.tcl \
	shaders.tcl \
	skt_ed.tcl \
	solclick.tcl \
	solcreate.tcl \
	solid.tcl \
	text.tcl \
	titles.tcl \
	tree.tcl \
	xclone.tcl

dist_tcl_DATA = \
	l_i0_v0_s0.gif \
	l_i0_v0_s1.gif \
	l_i0_v0_s2.gif \
	l_i0_v0_s3.gif \
	l_i0_v0_s4.gif \
	l_i0_v0_s5.gif \
	l_i0_v0_s6.gif \
	l_i0_v0_s7.gif \
	l_i0_v0_s8.gif \
	l_i0_v0_s9.gif \
	l_i0_v1_s0.gif \
	l_i0_v1_s1.gif \
	l_i0_v1_s2.gif \
	l_i0_v1_s3.gif \
	l_i0_v1_s4.gif \
	l_i0_v1_s5.gif \
	l_i0_v1_s6.gif \
	l_i0_v1_s7.gif \
	l_i0_v1_s8.gif \
	l_i0_v1_s9.gif \
	l_i1_v0_s0.gif \
	l_i1_v0_s1.gif \
	l_i1_v0_s2.gif \
	l_i1_v0_s3.gif \
	l_i1_v0_s4.gif \
	l_i1_v0_s5.gif \
	l_i1_v0_s6.gif \
	l_i1_v0_s7.gif \
	l_i1_v0_s8.gif \
	l_i1_v0_s9.gif \
	l_i1_v1_s0.gif \
	l_i1_v1_s1.gif \
	l_i1_v1_s2.gif \
	l_i1_v1_s3.gif \
	l_i1_v1_s4.gif \
	l_i1_v1_s5.gif \
	l_i1_v1_s6.gif \
	l_i1_v1_s7.gif \
	l_i1_v1_s8.gif \
	l_i1_v1_s9.gif \
	mike-dedication.txt \
	mike-tux.ppm \
	mike-tux.png

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties) \
	man.tcl

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

EXTRA_DIST = \
	$(dist_tcl_DATA) \
	rrmb.tcl \
	$(tcl_SCRIPTS) \
	CMakeLists.txt

# CLEANFILES = $(BUILT_SOURCES)

pkgIndex.tcl:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
