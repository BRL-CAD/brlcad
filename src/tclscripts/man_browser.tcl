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
#    -Document the interface
#    -Resizing window could be improved (i.e. limited).
#    -It would be nice if clicking html text would bring you to
#     the clicked cmd's man page, like an href. May be difficult.

package require Tk
package require Itcl
package require hv3
package provide ManBrowser 1.0

::itcl::class ::ManBrowser {
    inherit itk::Toplevel

    constructor {title args} {}

    private {
	variable commands [list]
	variable data
    }

    public {	
	variable path
	variable title

	method activate		{}
	method showPage		{cmdName}
    }
}

::itcl::configbody ManBrowser::path {
    if ($path == {} || ![file isdirectory $path]) {
	set path [file join [bu_brlcad_data "html"] mann en]]
    }
}

::itcl::configbody ManBrowser::title {
    if ($title == {} || ![string is wordchar -strict $title]) {
	set title BRLCAD
    }
}

::itcl::body ManBrowser::activate {} {
    #center [namespace tail $this]
    #::update
    #$itk_component($this) activate
}

::itcl::body ManBrowser::showPage {cmdName} {
    #set help_fd [open [file join [bu_brlcad_data "html"] mann en $cmdName.html]]
    #set manData [read $help_fd]
    #close $help_fd

    #$w reset;
    #$w configure -parsemode html
    #$w parse $manData
}

::itcl::body ManBrowser::constructor {title args} {
    global env
    global manhtmlviewer
    global manhtml

    return $this ;# XXXXXXXXXXXXXXXX BROKEN BROKEN BROKEN BROKEN BROKEN BROKEN
    
    #itk_component add $this { 
    #    ::iwidgets::dialog $itk_interior.$this \
    #        -modality none \
    #        -title "$title Manual Page Browser" \
    #        -background $SystemButtonFace
    #} {}
    #$itk_component($this) hide 1
    #$itk_component($this) hide 2
    #$itk_component($this) hide 3
    #$itk_component($this) configure \
    #    -thickness 2 \
    #    -buttonboxpady 0
    #$itk_component($this) buttonconfigure 0 \
    #    -defaultring yes \
    #    -defaultringpad 3 \
    #    -borderwidth 1 \
    #    -pady 0

    $this configure -title "$title Manual Page Browser"
    
    # ITCL can be nasty
    set win [$itk_component($this) component bbox component OK component hull]
    after idle "$win configure -relief flat"

    set tlparent [$itk_component($this) childsite]

    # Table of Contents
    itk_component add ToC {
        ::tk::frame $tlparent.ToC
    } {}

    set sfcsToC $itk_component(ToC)

    itk_component add ToC_scrollbar {
        ::ttk::scrollbar $itk_component(ToC).ToC_scrollbar \
        } {}

    itk_component add mantree {
        ::tk::listbox $itk_component(ToC).mantree -bd 2 -width 16 -exportselection false -yscroll "$itk_component(ToC_scrollbar) set" -listvariable mancmds
    } {}

    $itk_component(ToC_scrollbar) configure -command "$itk_component(mantree) yview"

    grid $itk_component(mantree) $itk_component(ToC_scrollbar) -sticky nsew -in $sfcsToC

    grid columnconfigure $sfcsToC 0 -weight 1
    grid rowconfigure $sfcsToC 0 -weight 1

    if {[file exists [file join [bu_brlcad_data "html"] mann en Introduction.html]]} {

        # List of available help documents
        set cmdfiles [glob -directory [file join [bu_brlcad_data "html"] mann en] *.html ]
        foreach cmdfile $cmdfiles {
            regexp {(.+/)(.+)(.html)} $cmdfile -> url cmdrootname htmlsuffix
            if {[string compare $cmdrootname "Introduction"]} {
                set mancmds [concat $mancmds [list $cmdrootname]]
            }
        }
        set mancmds [lsort $mancmds]

        pack $itk_component(ToC) -side left -expand no -fill y

        # Main HTML window
        itk_component add browser {
            ::tk::frame $tlparent.browser
        } {}
        set sfcsman $itk_component(browser)
        pack $sfcsman -expand yes -fill both

        # HTML widget
        set manhtmlviewer [::hv3::hv3 $sfcsman.htmlview]
        set manhtml [$manhtmlviewer html]
        $manhtml configure -parsemode html
        set help_fd [lindex [list [file join [bu_brlcad_data "html"] mann en Introduction.html]] 0]
        get_html_data $help_fd
        $manhtml parse $manData

        grid $manhtmlviewer -sticky nsew -in $sfcsman

        grid columnconfigure $sfcsman 0 -weight 1
        grid rowconfigure $sfcsman 0 -weight 1

        pack $itk_component(browser) -side left -expand yes -fill both
    }
    bind $itk_component(mantree) <<ListboxSelect>> {
        Archer::get_html_man_data [%W get [%W curselection]]
        Archer::html_man_display $manhtml
    }

    wm geometry $itk_component($this) "800x600"

    eval configure $args
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
