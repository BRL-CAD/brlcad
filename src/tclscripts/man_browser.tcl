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
#    -Get it working!
#    -Add method for disabling a list of commands passed in
#    -While we're at it, add method for enabling only commands passed in
#    -Add method for retrieving the list of commands
#    -Add ability to add >1 path (like {{mann/en/} {man/en/archer}}
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
	variable selection

	method get		{option}        
        method setCmdNames	{}
	proc loadPage		{pageName w} ;# For binding & internal use
    }
 
    private {
	common commands [list]
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

::itcl::body ManBrowser::get {option} {
    switch -- $option {
	path {if {[info exists path]} {return $path}}
    }
    error "bad option \"$option\""
}

::itcl::body ManBrowser::setCmdNames {} {
    set manFiles [glob -directory $path *.html ]
    foreach manFile $manFiles {
        # FIXME: should use [file rootname], etc rather than regexp
        # FIXME: string comparison with Introduction should be removed in favor
        #        of a mechanism for excluding a list of rootnames (commands)
	regexp {(.+/)(.+)(.html)} $manFile -> url rootName htmlSuffix
	if {[string compare $rootName Introduction]} {
	    set commands [concat $commands [list $rootName]]
	}
    }
    set commands [lsort $commands]
}

::itcl::configbody ManBrowser::selection {
    #set toc $itk_component(toc_listbox)
#    $toc selection set $idx
#    $toc activate $idx
#    $toc see $idx
    #set path 
    #puts [$this cget -path]
    
    #ManBrowser::loadPage $selection $this

}

proc ManBrowser::loadPage {pageName w} {
    # Get page
    set path [file join [$w get path] $pageName.html]
    set htmlFile [open $path]
    set pageData [read $htmlFile]
    close $htmlFile

    # Display page
    set htmlview [[$w childsite].browser.htmlview html]
    $htmlview reset
    $htmlview configure -parsemode html
    $htmlview parse $pageData
}

::itcl::body ManBrowser::constructor {args} {

    # Set default path if user didn't pass one
    set path [file join [bu_brlcad_data "html"] mann en]
    #if {![info exists path]} {configure -path {}}

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

    setCmdNames
    itk_component add toc_listbox {
        ::tk::listbox $toc.toc_listbox -bd 2 -width 16 -exportselection false -yscroll "$toc.toc_scrollbar set" -listvariable [scope commands]
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

    # Load Introduction.html if it's there, otherwise load first command
    if {[file exists [file join $path Introduction.html]]} {
        loadPage Introduction $this
    } else {
        #loadPage [lindex $commands 0] $this
    }

    bind $toc.toc_listbox <<ListboxSelect>> {
	ManBrowser::loadPage [%W get [%W curselection]] %W
    }

    #center [namespace tail $this]
    #::update

    eval itk_initialize $args
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
