# $Id$
SUBDIRS=geometree lib mged nirt pl-dm rtwizard util

tcldir = $(prefix)/tclscripts

# should it be _DATA ? +x or not?)
dist_tcl_SCRIPTS= ami.tcl ampi.tcl cad_clrpick.tcl cad_dialog.tcl chkexterns.tcl fs_dialog.tk helpcomm.tcl helplib.tcl hoc.tcl html_library.tcl libdm.tcl menu_override.tcl mouse.tcl vmath.tcl
tcl_DATA = tclIndex

.PHONY : tclIndex
tclIndex: 
	grep '^[ \t]*proc' *tcl *itk | sed 's,\([^:]*\):[ \t]*proc[ \t]*\([a-zA-Z0-9_?/:-][a-zA-Z0-9_?/:-]*\).*,set auto_index(\2) [list source [file join \$$dir \1]],' > tclIndex
