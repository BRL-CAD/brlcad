# $Id$

tcldir = $(prefix)/tclscripts/rtwizard/lib
dist_tcl_SCRIPTS = DbPage.itk ExamplePage.itk FbPage.itk FeedbackDialog.itk FrameBufferDialog.itk FullColorPage.itk GhostPage.itk HelpPage.itk HighlightedPage.itk IntroPage.itk LinePage.itk MGEDpage.itk PictureTypeA.itcl PictureTypeBase.itcl PictureTypeB.itcl PictureTypeC.itcl PictureTypeD.itcl PictureTypeE.itcl PictureTypeF.itcl Wizard.itk

install-data-hook:
	${bindir}/btclsh $(top_srcdir)/tclscripts/ami.tcl $(PWD) && ${INSTALL_DATA} tclIndex $(tcldir)/tclIndex
	${bindir}/btclsh $(top_srcdir)/tclscripts/ampi.tcl $(PWD) && ${INSTALL_DATA} pkgIndex.tcl $(tcldir)/pkgIndex.tcl

