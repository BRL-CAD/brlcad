# $Id$

this_dir = tclscripts/rtwizard/lib
tcldir = $(prefix)/$(this_dir)

dist_tcl_SCRIPTS = \
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

pkgIndex.tcl:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(top_srcdir)/src/$(this_dir)

tclIndex:
	$(top_builddir)/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(top_srcdir)/src/$(this_dir)

EXTRA_DIST = $(dist_tcl_SCRIPTS)

DISTCLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
