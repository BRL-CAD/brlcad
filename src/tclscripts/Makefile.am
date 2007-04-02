# $Id$

SUBDIRS = \
	archer \
	geometree \
	lib \
	mged \
	nirt \
	pl-dm \
	rtwizard \
	sdialogs \
	swidgets \
	util

this_dir = tclscripts
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
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

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

EXTRA_DIST = $(scripties)

# CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library $(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library $(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
