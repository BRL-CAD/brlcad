# $Id$

this_dir = tclscripts/mged
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	accel.tcl \
	adc.tcl \
	anim.tcl \
	apply.tcl \
	apply_mat.tcl \
	asc2g.tcl \
	attr_edit.tcl \
	bindings.tcl \
	bot_face_select.tcl \
	build_region.tcl \
	calipers.tcl \
	callbacks.tcl \
	collaborate.tcl \
	color.tcl \
	color_scheme.tcl \
	comb.tcl \
	combmenu.tcl \
	cycle.tcl \
	dbfindtree.tcl \
	dbupgrade.tcl \
	e_id.tcl \
	edit_menu.tcl \
	edit_solid.tcl \
	edit_solid_int.tcl \
	editmenu.tcl \
	editobj.tcl \
	eobjmenu.tcl \
	extract.tcl \
	font.tcl \
	g2asc.tcl \
	garbage_collect.tcl \
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
	tree.tcl

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
	mike-tux.ppm

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

EXTRA_DIST = \
	$(dist_tcl_DATA) \
	$(scripties)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
