# $Id$

tcldir = $(prefix)/tclscripts/geometree

tcl_SCRIPTS = geometree.tcl GeometryBrowser.tcl
tcl_DATA = tclIndex pkgIndex.tcl
EXTRA_DIST = $(tcl_SCRIPTS) $(tcl_DATA)

pkgIndex.tcl:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ampi.tcl $(top_srcdir)/tclscripts/geometree

.PHONY : tclIndex
tclIndex:
	grep '^[ \t]*proc' *tcl *tk | sed 's,\([^:]*\):[ \t]*proc[ \t]*\([a-zA-Z0-9_?/:-][a-zA-Z0-9_?/:-]*\).*,set auto_index(\2) [list source [file join \$$dir \1]],' > tclIndex

