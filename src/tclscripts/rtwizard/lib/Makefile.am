# $Id$

tcldir = $(prefix)/tclscripts/rtwizard/lib
dist_tcl_SCRIPTS = DbPage.itk ExamplePage.itk FbPage.itk FeedbackDialog.itk FrameBufferDialog.itk FullColorPage.itk GhostPage.itk HelpPage.itk HighlightedPage.itk IntroPage.itk LinePage.itk MGEDpage.itk PictureTypeA.itcl PictureTypeBase.itcl PictureTypeB.itcl PictureTypeC.itcl PictureTypeD.itcl PictureTypeE.itcl PictureTypeF.itcl Wizard.itk

install-data-hook:
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ami.tcl $(tcldir)
	${top_builddir}/src/bwish/btclsh $(top_srcdir)/src/tclscripts/ampi.tcl $(tcldir)

