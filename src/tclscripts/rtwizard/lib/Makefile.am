# $Id$

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
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

include $(top_srcdir)/misc/Makefile.defs
