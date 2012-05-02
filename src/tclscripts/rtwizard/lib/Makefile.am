
this_dir = tclscripts/rtwizard/lib
tcldir = $(BRLCAD_DATA)/$(this_dir)

scripties = \
	DbPage.itk \
	ExamplePage.itk \
	FbPage.itk \
	FeedbackDialog.itk \
	FrameBufferDialog.itk \
	FullColorPage.itk \
	GhostPage.itk \
	HelpPage.itk \
	HighlightedPage.itk \
	IntroPage.itk \
	LinePage.itk \
	MGEDpage.itk \
	PictureTypeA.itcl \
	PictureTypeB.itcl \
	PictureTypeBase.itcl \
	PictureTypeC.itcl \
	PictureTypeD.itcl \
	PictureTypeE.itcl \
	PictureTypeF.itcl \
	Wizard.itk

BUILT_SOURCES = \
	pkgIndex.tcl \
	tclIndex

tcl_SCRIPTS = \
	$(BUILT_SOURCES) \
	$(scripties)

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
