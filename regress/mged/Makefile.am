.PHONY : regression regress test mged 

regression: Makefile clean mged
	@${ECHO} ---
	@${ECHO} MGED regression testing completed.

regress: regression

test: regression

mged: $(top_srcdir)/regress/mged/mged.tcl
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/regress/mged/mged.tcl $(top_srcdir)
	@${ECHO} +++ $@ test complete.

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
	3ptarb.mged \
	B.mged \
	Z.mged \
	accept.mged \
	adc.mged \
	ae.mged \
	arb.mged \
	arced.mged \
	arot.mged \
	autoview.mged \
	build_region.mged \
	center.mged \
	clone.mged \
	comb.mged \
	copymat.mged \
	cp.mged \
	cpi.mged \
	dall.mged \
	draw.mged \
	draw_e_erase_d_who.mged \
	erase.mged \
	extrude.mged \
	eye_pt.mged \
	facedef.mged \
	g.mged \
	hide.mged \
	i.mged \
	in.mged \
	keypoint.mged \
	kill.mged \
	killall.mged \
	killtree.mged \
	knob.mged \
	loadview.mged \
	lookat.mged \
	make.mged \
	make_bb.mged \
	mged.tcl \
	mirface.mged \
	mirror.mged \
	mrot.mged \
	mv.mged \
	mvall.mged \
	oed.mged \
	orientation.mged \
	orot.mged \
	oscale.mged \
	overlay.mged \
	permute.mged \
	plot.mged \
	prefix.mged \
	ps.mged \
	push.mged \
	putmat.mged \
	qorot.mged \
	qvrot.mged \
	r.mged \
	refresh.mged \
	regression_resources.tcl \
	reject.mged \
	rm.mged \
	rot_edit.mged \
	rot_view.mged \
	rotobj.mged \
	run_test \
	saveview.mged \
	sca_edit.mged \
	sca_view.mged \
	sed.mged \
	set_perspective.mged \
	setview.mged \
	size.mged \
	status.mged \
	sv.mged \
	tra_edit.mged \
	tra_view.mged \
	translate.mged \
	unhide.mged \
	view.mged \
	vrot.mged \
	who.mged \
	xpush.mged \
	zoom.mged

CLEANFILES = \
	$(MOSTLYCLEANFILES) \
	mged.g

MOSTLYCLEANFILES = \
	mged_test.mged \
	mged.log 

include $(top_srcdir)/misc/Makefile.defs
