# $Id$

tcldir = $(prefix)/tclscripts/rtwizard/lib
sometclscripts = DbPage.itk ExamplePage.itk FbPage.itk FeedbackDialog.itk FrameBufferDialog.itk FullColorPage.itk GhostPage.itk HelpPage.itk HighlightedPage.itk IntroPage.itk LinePage.itk MGEDpage.itk PictureTypeA.itcl PictureTypeBase.itcl PictureTypeB.itcl PictureTypeC.itcl PictureTypeD.itcl PictureTypeE.itcl PictureTypeF.itcl Wizard.itk
tcl_SCRIPTS = $(sometclscripts) tclIndex pkgIndex.tcl
EXTRA_DIST = $(sometclscripts)

tclIndex:
	$(top_builddir)/bwish/bwish $(top_srcdir)/tclscripts/ami.tcl $(top_srcdir)/tclscripts/rtwizard/lib
 
pkgIndex.tcl:
	$(top_srcdir)/tclscripts/ampi.tcl $(top_srcdir)/tclscripts/rtwizard/lib

