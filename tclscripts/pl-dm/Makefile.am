# $Id$

tcldir = $(prefix)/tclscripts/pl-dm
tclfiles = bindings.tcl help.tcl menu.tcl
tcl_SCRIPTS = $(tclfiles) tclIndex
EXTRA_DIST = $(tclfiles)

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/pl-dm
