# $Id$

tcldir = $(prefix)/tclscripts/util

tcl_SCRIPTS = expand_int_ranges.tcl luniq.tcl
tcl_DATA = tclIndex
EXTRA_DIST = $(tcl_SCRIPTS) $(tcl_DATA)

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/util
