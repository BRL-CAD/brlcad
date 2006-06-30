
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
	bgerror.tcl \
	BotEditFrame.tcl \
	CombEditFrame.tcl \
	cursor.tcl \
	EhyEditFrame.tcl \
	EllEditFrame.tcl \
	EpaEditFrame.tcl \
	EtoEditFrame.tcl \
	ExtrudeEditFrame.tcl \
	GeometryEditFrame.tcl \
	GripEditFrame.tcl \
	HalfEditFrame.tcl \
	LoadArcherLibs.tcl \
	PartEditFrame.tcl \
	PipeEditFrame.tcl \
	Plugin.tcl \
	RhcEditFrame.tcl \
	RpcEditFrame.tcl \
	ShaderEdit.tcl \
	SketchEditFrame.tcl \
	SphereEditFrame.tcl \
	tabwindow.itk \
	TgcEditFrame.tcl \
	TorusEditFrame.tcl \
	Wizard.tcl

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripts)

EXTRA_DIST = $(scripts)

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
