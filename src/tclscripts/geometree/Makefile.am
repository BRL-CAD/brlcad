# $Id$

tcldir = $(prefix)/tclscripts/geometree

dist_tcl_SCRIPTS = geometree.tcl GeometryBrowser.tcl

install-data-hook:
	$(abs_top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	$(abs_top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

