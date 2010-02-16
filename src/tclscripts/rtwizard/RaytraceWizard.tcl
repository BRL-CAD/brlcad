#               R A Y T R A C E W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2004-2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# This is the main script for the RaytraceWizard.
#

#
# Extend Autopath
#
foreach i $auto_path {
    set wizpath [file join $i {..} tclscripts rtwizard ]
    if {  [file exists $wizpath] } {
	lappend auto_path $wizpath
	lappend auto_path [ file join $wizpath lib ]
    }
}


#
# All RaytraceWizard stuff is in the RaytraceWizard namespace
#
namespace eval RaytraceWizard {

    #
    # Required packages. There are several!
    #
    package require Itcl
    package require Itk
    package require Iwidgets

    package require Wizard 1.0
    package require DbPage
    package require FbPage
    package require HelpPage
    package require ExamplePage

    package require FullColorPage
    package require HighlightedPage
    package require GhostPage
    package require LinePage

    package require FeedbackDialog

    #
    # The supported picture types
    #
    package require PictureTypeA
    package require PictureTypeB
    package require PictureTypeC
    package require PictureTypeD
    package require PictureTypeE
    package require PictureTypeF

    set dbFile ""
    set helpFont {-family helvetica -size 12 \
		      -slant italic}


    #
    # main
    #
    # Do all that is doable
    #
    proc main { args } {

	#
	# Create the Feedback
	#
	set fb [ FeedbackDialog .\#auto \
		     -title "Loading rtwizard ..." \
		     -width 250 \
		     -height 100 ]

	$fb center
	$fb activate
	$fb center

	#
	# Create the wizard
	#
	set w [ RaytraceWizard::Wizard .\#auto ]
	$fb inform "Wizard megawidget created" 5

	#
	# Add the first set of pages to the wizard.
	# Note that we can't create the Mged-based pages until
	# the database is selected.
	#
	# XXX Fixing this will require debugging the Mged object
	#     The opendb command has a fatal bug.
	#

	# Database Page
	set ::dbp [$w add RaytraceWizard::DbPage "dbp"]
	$w enable "dbp"
	$fb inform "Database page created." 6

	# Framebuffer Page
	set ::fbp [$w add RaytraceWizard::FbPage "fbp"]
	$w enable "fbp"
	$fb inform "Frame buffer page created." 4

	$w add RaytraceWizard::IntroPage "intro"
	$w enable "intro"
	$fb inform "Intro page created." 4

	$w add RaytraceWizard::HelpPage "help"
	$w enable "help"
	$fb inform "Help page created" 4

	set ::exp [$w add RaytraceWizard::ExamplePage "exp"]
	$w enable "exp"
	$fb inform "Example page created" 4

	#
	# Load the picture types into the example page.
	# It then forwards the info to the wizard for the
	# radiobox in the "Image" menu. The "switch" for
	# the picture type is maintained by the ExamplePage
	#
	$::exp addType RaytraceWizard::PictureTypeA typeA
	$fb inform "Support for image type A loaded." 1

	$::exp addType RaytraceWizard::PictureTypeB typeB
	$fb inform "Support for image type B loaded." 1

	$::exp addType RaytraceWizard::PictureTypeC typeC
	$fb inform "Support for image type C loaded." 1

	$::exp addType RaytraceWizard::PictureTypeD typeD
	$fb inform "Support for image type D loaded." 1

	$::exp addType RaytraceWizard::PictureTypeE typeE
	$fb inform "Support for image type E loaded." 2

	$::exp addType RaytraceWizard::PictureTypeF typeF
	$fb inform "Support for image type F loaded." 2

	#
	# Pack the wizard
	#
	pack $w -expand 1 -fill both
	wm protocol . WM_DELETE_WINDOW ::exit
	$fb inform "Megawidgets packed." 5

	#
	# Get the database name. If the name was specified on the
	# command line, use it and proceed. Otherwise, spin up the
	# gui, select the database page, and wait for the database
	# file to be specified.
	#
	if { [llength $args] > 0 } {
	    set ::RaytraceWizard::dbFile [ lindex $args 0 ]
	    if { ! [file exists $::RaytraceWizard::dbFile] } {
		set ::RaytraceWizard::dbFile ""
	    }
	}

	if { [string length $::RaytraceWizard::dbFile] == 0 } {
	    #
	    # select the database page
	    #
	    $w select "dbp"

	    #
	    # Hide the Feedback
	    #
	    wm iconify $fb

	    #
	    # Start up the gui, and run until the dbFile has
	    # been specified.
	    #
	    vwait ::RaytraceWizard::dbFile

	    #
	    # Restore the Feedback
	    #
	    wm deiconify $fb
	}

	$w activateMenu image
	$w activateMenu help

	#
	# At this point, the database should be specified.
	# We load the database ourselves and hand it to the
	# pages.
	#
	set ::mgedObj [Mged .\#auto $::RaytraceWizard::dbFile]
	$::mgedObj configure -multi_pane 0
	$fb inform "MGED object instantiated." 40

	$w add RaytraceWizard::FullColorPage fullColor $::RaytraceWizard::dbFile
	$fb inform "Support for full color images loaded." 5

	$w add RaytraceWizard::HighlightedPage highlighted $::RaytraceWizard::dbFile
	$fb inform "Support for highlighted images loaded." 5

	$w add RaytraceWizard::GhostPage ghost $::RaytraceWizard::dbFile
	$fb inform "Support for ghost images loaded." 5

	$w add RaytraceWizard::LinePage lines $::RaytraceWizard::dbFile
	$w select "exp"
	$fb inform "rtwizard ready!" 5

	#
	# Pause...
	#
	after 1000
	$fb deactivate

	#
	# Go!
	#
	vwait forever
    }

}


#
# Start main
#
RaytraceWizard::main $argv


# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
