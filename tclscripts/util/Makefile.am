# $Id$

tcldir = $(prefix)/tclscripts/util

dist_tcl_SCRIPTS = expand_int_ranges.tcl luniq.tcl

install-data-hook:
	${top_builddir}/bwish/btclsh $(top_srcdir)/tclscripts/ami.tcl $(tcldir)

