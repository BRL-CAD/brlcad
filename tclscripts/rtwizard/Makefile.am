# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
tcl_SCRIPTS = rtwizard.tcl
EXTRA_DIST = $(tcl_SCRIPTS)

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/rtwizard
