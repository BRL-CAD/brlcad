# $Id$
SUBDIRS=geometree lib mged nirt pl-dm rtwizard util

tcldir = $(prefix)/tclscripts

# should it be _DATA ? +x or not?)
tcl_SCRIPTS= ami.tcl ampi.tcl cad_clrpick.tcl cad_dialog.tcl chkexterns.tcl fs_dialog.tk helpcomm.tcl helplib.tcl hoc.tcl html_library.tcl libdm.tcl menu_override.tcl mouse.tcl vmath.tcl
tcl_DATA = tclIndex

.PHONY : tclIndex
tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts
