# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
tcl_SCRIPTS = rtwizard.tcl pkgIndex.tcl tclIndex
EXTRA_DIST = rtwizard.tcl

.PHONY : tclIndex
tclIndex:
	echo 'set auto_index(::RtWizard::main) [list source [file join $$dir rtwizard.tcl]]' > tclIndex

pkgIndex.tcl:
	$(top_srcdir)/tclscripts/ampi.tcl $(top_srcdir)/tclscripts/rtwizard

