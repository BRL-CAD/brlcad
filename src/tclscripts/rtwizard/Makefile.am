# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
dist_tcl_SCRIPTS = rtwizard.tcl

install-data-hook:
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

