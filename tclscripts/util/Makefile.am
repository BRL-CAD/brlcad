# $Id$

tcldir = $(prefix)/tclscripts/util

dist_tcl_SCRIPTS = expand_int_ranges.tcl luniq.tcl
tcl_DATA = tclIndex

.PHONY : tclIndex
tclIndex:
	grep '^[ \t]*proc' *tcl *tk | sed 's,\([^:]*\):[ \t]*proc[ \t]*\([a-zA-Z0-9_?/:-][a-zA-Z0-9_?/:-]*\).*,set auto_index(\2) [list source [file join \$$dir \1]],' > tclIndex

