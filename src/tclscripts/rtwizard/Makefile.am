# $Id$
SUBDIRS = examples lib
tcldir = $(prefix)/tclscripts/rtwizard
dist_tcl_SCRIPTS = rtwizard.tcl

install-data-hook:
	${top_builddir}/src/other/libtcl/tclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	${top_builddir}/src/other/libtcl/tclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

uninstall-hook:
	${RM} -f $(tcldir)/tclIndex
	${RM} -f $(tcldir)/pkgIndex.tcl
