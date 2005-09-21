
SUBDIRS = \
	dd_protocols

this_dir = other/blt/library
brlcadscriptdir = $(BLT_LIBRARY)

brlcadscript_SCRIPTS = \
	dnd.tcl \
	dragdrop.tcl \
	graph.tcl \
	hierbox.tcl \
	hiertable.tcl \
	tabnotebook.tcl \
	tabset.tcl \
	treeview.tcl \
	ZoomStack.itcl \
	treeview_m.xbm \
	treeview.xbm

EXTRA_DIST = \
	$(brlcadscript_SCRIPTS)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
