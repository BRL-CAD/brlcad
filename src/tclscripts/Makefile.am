# $Id$
SUBDIRS=geometree lib mged nirt pl-dm rtwizard util

this_dir = tclscripts
tcldir = $(prefix)/$(this_dir)

dist_tcl_SCRIPTS= \
	$(BUILT_SOURCES) \
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

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

EXTRA_DIST = $(dist_tcl_SCRIPTS)

DISTCLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
