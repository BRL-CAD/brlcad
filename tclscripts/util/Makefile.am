# $Id$

tcldir = $(prefix)/tclscripts/util

dist_tcl_SCRIPTS = expand_int_ranges.tcl luniq.tcl

install-data-hook:
	${bindir}/btclsh $(top_srcdir)/tclscripts/ami.tcl $(PWD) && ${INSTALL_DATA} tclIndex $(tcldir)/tclIndex

