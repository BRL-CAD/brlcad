# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
dist_tcl_SCRIPTS = rtwizard.tcl

install-data-hook:
	${bindir}/btclsh $(top_srcdir)/tclscripts/ami.tcl $(PWD) && ${INSTALL_DATA} tclIndex $(tcldir)/tclIndex
	${bindir}/btclsh $(top_srcdir)/tclscripts/ampi.tcl $(PWD) && ${INSTALL_DATA} pkgIndex.tcl $(tcldir)/pkgIndex.tcl

