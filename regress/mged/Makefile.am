.PHONY : regression regress test mged 

regression: Makefile clean mged
	@${ECHO} ---
	@${ECHO} MGED regression testing completed.

regress: regression

test: regression

mged: $(top_srcdir)/regress/mged/mged.tcl
	 $(top_builddir)/src/bwish/btclsh $(top_srcdir)/regress/mged/mged.tcl $(top_srcdir)
	@${ECHO} +++ $@ test complete.

# these are here because they go in the distribution but are not installed
EXTRA_DIST = \
	3ptarb.mged \
	arb.mged \
	build_region.mged \
	clone.mged \
	comb.mged \
	cp.mged \
	cpi.mged \
	g.mged \
	in.mged \
	make.mged \
	make_bb.mged \
	mged.tcl \
	mirror.mged \
	mv.mged \
	mvall.mged \
	prefix.mged \
	r.mged \
	regression_resources.tcl \
	run_test.tcl 

CLEANFILES = \
	$(MOSTLYCLEANFILES) \
	mged.g

MOSTLYCLEANFILES = \
	mged_test.mged \
	mged.log 

include $(top_srcdir)/misc/Makefile.defs
