
lib_LTLIBRARIES = libged.la
noinst_LTLIBRARIES = libged_nil.la

libged_nil_la_SOURCES = \
	adjust.c \
	ae2dir.c \
	aet.c \
	arced.c \
	arot.c \
	attr.c \
	autoview.c \
	bigE.c \
	binary.c \
	blast.c \
	bot_decimate.c \
	bot_face_sort.c \
	bot_smooth.c \
	cat.c \
	center.c \
	color.c \
	comb.c \
	comb_color.c \
	comb_std.c \
	concat.c \
	copy.c \
	copyeval.c \
	dbip.c \
	dg_obj.c \
	dir2ae.c \
	draw.c \
	dump.c \
	dup.c \
	edcomb.c \
	editit.c \
	edmater.c \
	erase.c \
	erase_all.c \
	expand.c \
	eye.c \
	eye_pos.c \
	facetize.c \
	find.c \
	form.c \
	fracture.c \
	ged.c \
	get.c \
	get_autoview.c \
	get_eyemodel.c \
	get_obj_bounds.c \
	get_type.c \
	globals.c \
	group.c \
	hide.c \
	how.c \
	illum.c \
	importFg4Section.c \
	instance.c \
	isize.c \
	item.c \
	keep.c \
	keypoint.c \
	kill.c \
	killall.c \
	killrefs.c \
	killtree.c \
	label.c \
	list.c \
	log.c \
	lookat.c \
	ls.c \
	lt.c \
	m2v_point.c \
	make.c \
	make_bb.c \
	make_name.c \
	match.c \
	mater.c \
	mirror.c \
	model2view.c \
	move.c \
	move_all.c \
	move_arb_edge.c \
	move_arb_face.c \
	mrot.c \
	nirt.c \
	nmg_collapse.c \
	nmg_simplify.c \
	ocenter.c \
	open.c \
	orient.c \
	orotate.c \
	oscale.c \
	otranslate.c \
	overlay.c \
	pathlist.c \
	pathsum.c \
	perspective.c \
	pmat.c \
	pmodel2view.c \
	pov.c \
	prcolor.c \
	prefix.c \
	push.c \
	put.c \
	qray.c \
	quat.c \
	regdef.c \
	region.c \
	remove.c \
	report.c \
	rmap.c \
	rmat.c \
	rmater.c \
	rot.c \
	rot_point.c \
	rotate_about.c \
	rotate_arb_face.c \
	rt.c \
	rtabort.c \
	rtcheck.c \
	scale.c \
	set_output_script.c \
	set_transparency.c \
	set_uplotOutputMode.c \
	setview.c \
	shaded_mode.c \
	shader.c \
	shells.c \
	showmats.c \
	size.c \
	slew.c \
	summary.c \
	title.c \
	tol.c \
	tops.c \
	tra.c \
	trace.c \
	track.c \
	tree.c \
	unhide.c \
	units.c \
	v2m_point.c \
	vdraw.c \
	version.c \
	view.c \
	view2model.c \
	view_obj.c \
	viewdir.c \
	vrot.c \
	vutil.c \
	wdb_comb_std.c \
	wdb_importFg4Section.c \
	wdb_obj.c \
	wdb_track.c \
	wdb_vdraw.c \
	whatid.c \
	which.c \
	who.c \
	wmater.c \
	xpush.c \
	ypr.c \
	zap.c \
	zoom.c

# cannot set per-target CPPFLAGS until automake 1.7+
AM_CPPFLAGS = \
	${TCL_CPPFLAGS} -DNEW_TOPS_BEHAVIOR

noinst_HEADERS = \
	ged_private.h \
	qray.h

libged_la_SOURCES =
libged_la_LDFLAGS = -version-info 19:1 -no-undefined
libged_la_LIBADD = \
	libged_nil.la \
	${GED_LIBS}

EXTRA_DIST = \
        CMakeLists.txt

DEPENDS = src/librt

include $(top_srcdir)/misc/Makefile.defs

FAST_OBJECTS = \
	$(libged_nil_la_OBJECTS) \
	$(libged_la_OBJECTS) \
	$(lib_LTLIBRARIES)
