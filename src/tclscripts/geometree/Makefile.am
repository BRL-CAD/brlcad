# $Id$

tcldir = $(prefix)/tclscripts/geometree

dist_tcl_SCRIPTS = \
	GeometryBrowser.tcl \
	geometree.tcl

TCLSH=${top_builddir}/src/other/libtcl/tclsh

install-data-hook:
	${TCLSH} $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	${TCLSH} $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

uninstall-hook:
	${RM} -f $(tcldir)/tclIndex
	${RM} -f $(tcldir)/pkgIndex.tcl
