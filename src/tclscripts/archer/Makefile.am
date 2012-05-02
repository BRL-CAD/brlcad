
SUBDIRS = images

this_dir = tclscripts/archer
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripts = \
	Arb4EditFrame.tcl \
	Arb5EditFrame.tcl \
	Arb6EditFrame.tcl \
	Arb7EditFrame.tcl \
	Arb8EditFrame.tcl \
	Archer.tcl \
	ArcherCore.tcl \
	AttrGroupsDisplayUtility.tcl \
	BotUtility.tcl \
	CombEditFrame.tcl \
	DataUtils.tcl \
	EhyEditFrame.tcl \
	EllEditFrame.tcl \
	EpaEditFrame.tcl \
	EtoEditFrame.tcl \
	ExtrudeEditFrame.tcl \
	GeometryEditFrame.tcl \
	GripEditFrame.tcl \
	HalfEditFrame.tcl \
	HypEditFrame.tcl \
	LoadArcherLibs.tcl \
	PartEditFrame.tcl \
	PipeEditFrame.tcl \
	Plugin.tcl \
	RhcEditFrame.tcl \
	RpcEditFrame.tcl \
	ShaderEdit.tcl \
	SphereEditFrame.tcl \
	SuperellEditFrame.tcl \
	TgcEditFrame.tcl \
	TorusEditFrame.tcl \
	Utility.tcl \
	Wizard.tcl \
	bgerror.tcl \
	itk_redefines.tcl \
	tabwindow.itk

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

EXTRA_DIST = $(tcl_SCRIPTS) CMakeLists.txt

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
