# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
tcl_SCRIPTS = rtwizard.tcl pkgIndex.tcl tclIndex
EXTRA_DIST = rtwizard.tcl

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/rtwizard

pkgIndex.tcl:
	$(top_srcdir)/tclscripts/ampi.tcl $(top_srcdir)/tclscripts/rtwizard

