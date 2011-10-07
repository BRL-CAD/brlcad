
this_dir = tclscripts/hv3
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	combobox.tcl \
	hv3.tcl \
	hv3_encodings.tcl \
	hv3_form.tcl \
	hv3_request.tcl \
	hv3_util.tcl \
	snit.tcl

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

documentationdir = $(BRLCAD_DATA)/doc

documentation_DATA = \
	hv3.man

dist_man_MANS = \
	tkhtml.n

EXTRA_DIST = \
	$(tcl_SCRIPTS) \
	$(documentation_DATA) \
	$(dist_man_MANS) \
	license.txt \
	license_snit.txt \
	demo/index.html \
	demo/small-eagleCAD.gif \
	demo/viewpage.tcl \
	CMakeLists.txt

# CLEANFILES = $(BUILT_SOURCES)

pkgIndex.tcl:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	TCL_LIBRARY=$(top_srcdir)/src/other/tcl/library \
	LD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_builddir)/src/other/tk/unix:$$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=$(top_builddir)/src/other/tcl/unix:$(top_buildir)/src/other/tk/unix:$$DYLD_LIBRARY_PATH \
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
