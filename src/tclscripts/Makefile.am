# $Id$
SUBDIRS=geometree lib mged nirt pl-dm rtwizard util

tcldir = $(prefix)/tclscripts

# should it be _DATA ? +x or not?)
dist_tcl_SCRIPTS= \
	ami.tcl \
	ampi.tcl \
	cad_clrpick.tcl \
	cad_dialog.tcl \
	chkexterns.tcl \
	fs_dialog.tk \
	helpcomm.tcl \
	helplib.tcl \
	hoc.tcl \
	html_library.tcl \
	libdm.tcl \
	menu_override.tcl \
	mouse.tcl \
	vmath.tcl

install-data-hook:
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)

uninstall-hook:
	${RM} -f $(tcldir)/tclIndex
