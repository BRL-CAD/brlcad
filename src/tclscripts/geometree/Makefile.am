# $Id$

tcldir = $(prefix)/tclscripts/geometree

dist_tcl_SCRIPTS = \
	GeometryBrowser.tcl \
	geometree.tcl

install-data-hook:
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

uninstall-hook:
	${RM} -f $(tcldir)/tclIndex
	${RM} -f $(tcldir)/pkgIndex.tcl
