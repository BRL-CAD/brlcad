# $Id$

tcldir = $(prefix)/tclscripts/geometree

tcl_SCRIPTS = geometree.tcl GeometryBrowser.tcl
tcl_DATA = tclIndex pkgIndex.tcl
EXTRA_DIST = $(tcl_SCRIPTS) $(tcl_DATA)

pkgIndex.tcl:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ampi.tcl $(top_srcdir)/tclscripts/geometree

tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/geometree


