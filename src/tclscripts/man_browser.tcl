#               M A N _ B R O W S E R . T C L
# BRL-CAD
#
# Copyright (c) 1998-2011 United States Government as represented by
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
# To do:
#    -Add ability to add >1 path (like {{mann/en/} {man/en/archer}}
#    -Add method for retrieving the list of commands
#    -Document the interface
#    -Resizing window could be improved (i.e. limited).
#    -It would be nice if clicking html text would bring you to
#     the clicked cmd's man page, like an href. May be difficult.

package require Tk
package require Itcl
package require hv3
package provide ManBrowser 1.0

::itcl::class ::ManBrowser {
    inherit iwidgets::Dialog

    public {	
	variable path
	variable parentName
	variable disabledPages
	variable enabledPages

        method setPageNames	{}
	method loadPage		{pageName}
	method select		{pageName}

	proc getBrowser		{}
    }
 
    private {
	common pages [list]
	variable pageData
    }

    constructor {args} {}
}

::itcl::configbody ManBrowser::path {
    if {![info exists path] || ![file isdirectory $path]} {
	set path [file join [bu_brlcad_data "html"] mann en]
    }
}

::itcl::configbody ManBrowser::parentName {
    if {![info exists parentName] || ![string is print -strict $parentName]} {
	set parentName BRLCAD
    }
    configure -title "[string trim $parentName] Manual Page Browser"
}

::itcl::configbody ManBrowser::disabledPages {
    # Page names added to this list are always disabled
    set disabledByDefault [list Introduction]

    if {![info exists disabledPages] || ![string is list $disabledPages]} {
    	set disabledPages $disabledByDefault
    } else {
	lappend disabledPages $disabledByDefault
    }
    set disabledPages [lsort $disabledPages]

    # Reset pages list
    if {[info exists pages] && $pages != {}} {
	setPageNames
    }
}

::itcl::configbody ManBrowser::enabledPages {
    if {![info exists enabledPages] || ![string is list $enabledPages]} {
    	set enabledPages [list]
    }
    set enabledPages [lsort $enabledPages]

    # Reset pages list
    if {[info exists pages] && $pages != {}} {
	setPageNames
    }
}

::itcl::body ManBrowser::setPageNames {} {
    set manFiles [glob -directory $path *.html ]

    set pages [list]
    foreach manFile $manFiles {
	set rootName [file rootname [file tail $manFile]]

	# If the page exists in disabledPages, disable it 
	set isDisabled [expr [lsearch -sorted -exact \
			      $disabledPages $rootName] != -1]

	# If enabledPages is defined and the page exists, enable it
	if {$enabledPages != {}} {
	    set isEnabled [expr [lsearch -sorted -exact \
				 $enabledPages $rootName] != -1]
	} else {
	    set isEnabled 1
	}

	# Obviously, if the page is both disabled/enabled, it will be disabled
	if {!$isDisabled && $isEnabled} {
	    lappend pages $rootName
	}
    }
    set pages [lsort $pages]
}

::itcl::body ManBrowser::select {pageName} {
    # Select the requested man page 
    set idx [lsearch -sorted -exact $pages $pageName]

    if {$idx != -1} {
        set result True
	set toc $itk_component(toc_listbox)

	# Deselect previous selection
	$toc selection clear 0 [$toc index end]

        # Select pageName in table of contents
	$toc selection set $idx
	$toc activate $idx
	$toc see $idx

	loadPage $pageName
    } else {
	set result False
    }
    
    return $result
}

::itcl::body ManBrowser::getBrowser {} {
    return [itcl_info objects -class ManBrowser]
}

::itcl::body ManBrowser::loadPage {pageName} {
    # Get page
    set pathname [file join $path $pageName.html]
    set htmlFile [open $pathname]
    set pageData [read $htmlFile]
    close $htmlFile

    # Display page
    set htmlview [[$this childsite].browser.htmlview html]
    $htmlview reset
    $htmlview configure -parsemode html
    $htmlview parse $pageData
}

::itcl::body ManBrowser::constructor {args} {
    eval itk_initialize $args

    # Trigger configbody defaults if user didn't pass them
    set opts {{path} {parentName} {disabledPages} {enabledPages}}
    foreach o $opts {
	if {![info exists $o]} {eval itk_initialize {-$o {}}}
    }

    setPageNames
    
    $this hide 1
    $this hide 2
    $this hide 3
    $this configure \
        -modality none \
        -thickness 2 \
        -buttonboxpady 0
       #-background $SystemButtonFace
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
    itk_component add toc {
        ::tk::frame $parent.toc
    } {}

    set toc $itk_component(toc)

    itk_component add toc_scrollbar {
        ::ttk::scrollbar $toc.toc_scrollbar
    } {}

    itk_component add toc_listbox {
        ::tk::listbox $toc.toc_listbox -bd 2 \
 				       -width 16 \
                                       -exportselection false \
	                               -yscroll "$toc.toc_scrollbar set" \
				       -listvariable [scope pages]
    } {}

    $toc.toc_scrollbar configure -command "$toc.toc_listbox yview"

    grid $toc.toc_listbox $toc.toc_scrollbar -sticky nsew -in $toc

    grid columnconfigure $toc 0 -weight 1
    grid rowconfigure $toc 0 -weight 1

    pack $toc -side left -expand no -fill y

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
        loadPage [lindex $pages 0]
    }

    bind $toc.toc_listbox <<ListboxSelect>> {
	set mb [ManBrowser::getBrowser]
	$mb loadPage [%W get [%W curselection]]
    }

    #center [namespace tail $this]
    #::update

    configure -height 600 -width 800
    return $this
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
