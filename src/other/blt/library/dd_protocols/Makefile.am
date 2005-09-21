
this_dir = other/blt/library/dd_protocols
brlcadscriptdir = @BLT_LIBRARY@/dd_protocols

brlcadscript_SCRIPTS = \
	dd-color.tcl \
	dd-file.tcl \
	dd-number.tcl \
	dd-text.tcl

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
