# $Id$

tcldir = $(prefix)/tclscripts/util

dist_tcl_SCRIPTS = expand_int_ranges.tcl luniq.tcl

install-data-hook:
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)

