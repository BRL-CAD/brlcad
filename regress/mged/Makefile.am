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
	accept.mged \
	arb.mged \
	arced.mged \
	arot.mged \
	B.mged \
	build_region.mged \
	clone.mged \
	comb.mged \
	copymat.mged \
	cp.mged \
	cpi.mged \
	dall.mged \
	draw_e_erase_d_who.mged \
	draw.mged \
	erase.mged \
	extrude.mged \
	facedef.mged \
	g.mged \
	hide.mged \
	i.mged \
	in.mged \
	keypoint.mged \
	kill.mged \
	killall.mged \
	killtree.mged \
	make.mged \
	make_bb.mged \
	mged.tcl \
	mirface.mged \
	mirror.mged \
	mv.mged \
	mvall.mged \
	oed.mged \
	orot.mged \
	oscale.mged \
	permute.mged \
	prefix.mged \
	push.mged \
	putmat.mged \
	qorot.mged \
	r.mged \
	regression_resources.tcl \
	reject.mged \
	rm.mged \
	rot_edit.mged \
	rotobj.mged \
	run_test \
	sca_edit.mged \
	sed.mged \
	status.mged \
	tra_edit.mged \
	translate.mged \
	unhide.mged \
	who.mged \
	xpush.mged \
	Z.mged 

CLEANFILES = \
	$(MOSTLYCLEANFILES) \
	mged.g

MOSTLYCLEANFILES = \
	mged_test.mged \
	mged.log 

include $(top_srcdir)/misc/Makefile.defs
