# $Id$

tcldir = $(prefix)/tclscripts/pl-dm
dist_tcl_SCRIPTS = bindings.tcl help.tcl menu.tcl

install-data-hook:
	${top_builddir}/src/other/libtcl/tclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)

uninstall-hook:
	${RM} -f $(tcldir)/tclIndex
