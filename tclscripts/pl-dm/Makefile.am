# $Id$

tcldir = $(prefix)/tclscripts/pl-dm
tclfiles = bindings.tcl help.tcl menu.tcl
tcl_SCRIPTS = $(tclfiles) tclIndex
EXTRA_DIST = $(tclfiles)

.PHONY : tclIndex
tclIndex:
	grep '^[ \t]*proc' *tcl *tk | sed 's,\([^:]*\):[ \t]*proc[ \t]*\([a-zA-Z0-9_?/:-][a-zA-Z0-9_?/:-]*\).*,set auto_index(\2) [list source [file join \$$dir \1]],' > tclIndex

