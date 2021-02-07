#               M A N _ B R O W S E R . T C L
# BRL-CAD
#
# Copyright (c) 1998-2021 United States Government as represented by
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
# Description:
#    Man page browser
#
# To-do/ideas:
#    -Add ability to add >1 path (like {{mann/en/} {man/en/archer}}
#    -Add method for retrieving the list of commands displayed
#    -Resizing window could be improved (i.e. limited).
#    -It would be nice if clicking html text would bring you to
#     the clicked cmd's man page, like an href. May be difficult.
#    -Add support for args like "*make*", returning >1 results in browser pane
#    -Make a parent class; then a HelpBrowser sibling (removes much duplication)

package provide ManBrowser 1.0
package require Tk
package require Itcl
package require hv3
package require cadwidgets::Accordian 1.0

::itcl::class ::ManBrowser {
    inherit iwidgets::Dialog

    itk_option define -useToC useToC UseToC 1
    itk_option define -defaultDir defaultDir DefaultDir 1

    public {
	variable path
	variable parentName
	variable disabledPages

	method setManType	{}
	method loadPage		{pageName}
	method select		{pageName}
    }
    protected {
	method getPages		{section}
	method accordianCallback {_item _state}
    }

    # Lists of pages for loading into ToC listbox
    private {
	common pages
	variable current_section
	variable mAccordianCallbackActive 0
    }

    constructor {args} {}
}

# ------------------------------------------------------------
#                      OPERATIONS
# ------------------------------------------------------------

##
# Populate a section's man page list (man1, man3, man5 or mann).
::itcl::body ManBrowser::getPages {section} {
    set mpath [file join $path $section]
    if {[file exists $mpath]} {
	set manFiles [glob -nocomplain -directory $mpath *.html ]

	set mlist [list]
	foreach manFile $manFiles {
	    set rootName [file rootname [file tail $manFile]]
	    # If the page is not disabled, add it
	    if {![expr [lsearch -sorted -exact $disabledPages $rootName] != -1]} {
		lappend mlist $rootName
	    }
	}
	set pages($section) [lsort $mlist]
    }
}

##
# Loads pages selected graphically or through the command line into HTML browser
#
::itcl::body ManBrowser::loadPage {pageName} {
# Get page
    if {[file exists $pageName] && ![file isdirectory $pageName] && ![file executable $pageName]} {set pathname $pageName}
    if {![info exists pathname]} {
	if {[file exists [file join $path $current_section $pageName.html]]} {
	    set pathname [file join $path $current_section $pageName.html]
	}
    }
    if {[info exists pathname]} {
	set htmlFile [open $pathname]
	set pageData [read $htmlFile]
	close $htmlFile

	# Display page
	set htmlview [[$this childsite].browser.htmlview html]
	$htmlview reset
	$htmlview configure -parsemode html
	$htmlview parse $pageData
    }
}

##
# Selects page in ToC & loads into HTML browser; used for command line calls
#
::itcl::body ManBrowser::select {pageName} {
    set result False
    if {[info exists pages]} {
    # Select the requested man page
	set rootName [file rootname [file tail $pageName]]
	set idx [lsearch -sorted -exact $pages($current_section) $rootName]

	if {$idx != -1} {
	    set result True
	    set toc $itk_component(manpagelistbox)

	    # Deselect previous selection
	    $toc selection clear 0 [$toc index end]

	    # Select pageName in table of contents
	    $toc selection set $idx
	    $toc activate $idx
	    $toc see $idx

	    loadPage $pageName
	}
    }
    return $result
}

# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------
::itcl::body ManBrowser::constructor {args} {
    eval itk_initialize $args

    # Path to HTML man page directories
    if {![info exists path] || ![file isdirectory $path]} {
	set path [file join [bu_dir doc] html]
    }

    configure -title "BRL-CAD Manual Page Browser"

    # We don't list Introduction page as a man page.  Right
    # now that's the only one we filter out - any additional
    # pages that shouldn't be listed should be added here.
    set disabledPages [list Introduction]

    # Build the lists of files
    getPages "man1"
    getPages "man3"
    getPages "man5"
    getPages "mann"

    # unless we know better, set the current section to man1
    set current_section man$itk_option(-defaultDir)
    if {$itk_option(-defaultDir) == ""} {
	set current_section "man1"
    }

    $this hide 1
    $this hide 2
    $this hide 3
    $this configure \
	-modality none \
	-thickness 2 \
	-buttonboxpady 0
    $this buttonconfigure 0 \
	-defaultring yes \
	-defaultringpad 3 \
	-borderwidth 1 \
	-pady 0

	# ITCL can be nasty
    set win [$this component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set parent [$this childsite]

    # Table of Contents
    if {$itk_option(-useToC)} {

	itk_component add treeAccordian {
	    ::cadwidgets::Accordian $parent.treeAccordian
	} {}
	$itk_component(treeAccordian) addTogglePanelCallback [::itcl::code $this accordianCallback]
	$itk_component(treeAccordian) insert 0 "MGED (mann)"
	$itk_component(treeAccordian) insert 0 "Conventions (man5)"
	$itk_component(treeAccordian) insert 0 "Libraries (man3)"
	$itk_component(treeAccordian) insert 0 "Programs (man1)"

	itk_component add toc_scrollbar {
	    ::ttk::scrollbar $itk_component(treeAccordian).toc_scrollbar
	} {}

	itk_component add manpagelistbox {
	    ::tk::listbox $itk_component(treeAccordian).manpagelistbox -bd 2 \
		-width 16 \
		-exportselection false \
		-yscroll "$itk_component(treeAccordian).toc_scrollbar set"
	} {}

	$itk_component(treeAccordian).toc_scrollbar configure -command "$itk_component(treeAccordian).manpagelistbox yview"

	if {$current_section == "man1"} {$itk_component(treeAccordian) togglePanel "Programs (man1)"}
	if {$current_section == "man3"} {$itk_component(treeAccordian) togglePanel "Libraries (man3)"}
	if {$current_section == "man5"} {$itk_component(treeAccordian) togglePanel "Conventions (man5)"}
	if {$current_section == "mann"} {$itk_component(treeAccordian) togglePanel "MGED (mann)"}

	pack $itk_component(treeAccordian) -side left -expand no -fill y
    }

    # Main HTML window
    itk_component add browser {
	::tk::frame $parent.browser
    } {}
    set sfcsman $itk_component(browser)
    pack $sfcsman -expand yes -fill both

    # HTML widget
    set manhtmlviewer [::hv3::hv3 $sfcsman.htmlview]
    set manhtml [$manhtmlviewer html]

    grid $manhtmlviewer -sticky nsew -in $sfcsman

    grid columnconfigure $sfcsman 0 -weight 1
    grid rowconfigure $sfcsman 0 -weight 1

    pack $itk_component(browser) -side left -expand yes -fill both

    # Load Introduction.html if it's there, otherwise load first page
    if {[file exists [file join $path Introduction.html]]} {
	loadPage Introduction
    } else {
	if {[info exists pages]} {
	    loadPage [lindex $pages($current_section) 0]
	}
    }

    if {$itk_option(-useToC)} {
	bind $itk_component(manpagelistbox) <<ListboxSelect>> {
	    set mb [itcl_info objects -class ManBrowser]
	    $mb loadPage [%W get [%W curselection]]
	}
    }

    configure -height 600 -width 800
    return $this
}

::itcl::body ManBrowser::accordianCallback {_item _state} {
    if {$mAccordianCallbackActive} { return }
    set mAccordianCallbackActive 1

    #puts "got callback: $_item, $_state"

    if {$_item == "Programs (man1)"} {
	set current_section "man1"
	configure -title "BRL-CAD Manual Page Browser (man1)"
    }
    if {$_item == "Libraries (man3)"} {
	set current_section "man3"
	configure -title "BRL-CAD Manual Page Browser (man3)"
    }
    if {$_item == "Conventions (man5)"} {
	set current_section "man5"
	configure -title "BRL-CAD Manual Page Browser (man5)"
    }
    if {$_item == "MGED (mann)"} {
	set current_section "mann"
	configure -title "BRL-CAD Manual Page Browser (mann)"
    }

    set childsite [$itk_component(treeAccordian) itemChildsite $_item]

    $itk_component(manpagelistbox) selection clear 0 [$itk_component(manpagelistbox) index end]
    $itk_component(manpagelistbox) delete 0 [$itk_component(manpagelistbox) size]
    if {[info exists pages]} {
	foreach pg $pages($current_section) {
	    $itk_component(manpagelistbox) insert end $pg
	}
    }
    grid $itk_component(manpagelistbox) $itk_component(toc_scrollbar) -sticky nsew -in $childsite

    grid columnconfigure $childsite 0 -weight 1
    grid rowconfigure $childsite 0 -weight 1

    raise $itk_component(manpagelistbox)
    raise $itk_component(toc_scrollbar)

    set mAccordianCallbackActive 0
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
