# $Id$

tcldir = $(prefix)/tclscripts/pl-dm
dist_tcl_SCRIPTS = bindings.tcl help.tcl menu.tcl

install-data-hook:
	${top_builddir}/bwish/btclsh $(top_srcdir)/tclscripts/ami.tcl $(PWD) && ${INSTALL_DATA} tclIndex $(tcldir)/tclIndex

