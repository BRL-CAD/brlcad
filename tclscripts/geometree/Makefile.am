# $Id$

tcldir = $(prefix)/tclscripts/geometree

dist_tcl_SCRIPTS = geometree.tcl GeometryBrowser.tcl

install-data-hook:
	${top_builddir}/bwish/btclsh $(top_srcdir)/tclscripts/ami.tcl $(tcldir)
	${top_builddir}/bwish/btclsh $(top_srcdir)/tclscripts/ampi.tcl $(tcldir)

