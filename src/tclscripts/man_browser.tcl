#               M A N _ B R O W S E R . T C L
# BRL-CAD
#
# Copyright (c) 1998-2026 United States Government as represented by
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
package require tklitehtml 0.1.0

if {[llength [info commands ::brlcad::help::manpage_search_terms]] == 0} {
    set helpcomm_path [file join [bu_dir data] "tclscripts" "helpcomm.tcl"]
    if {[file exists $helpcomm_path]} {
	source $helpcomm_path
    }
    unset helpcomm_path
}

::itcl::class ::ManBrowser {
    inherit iwidgets::Dialog

    itk_option define -useToC useToC UseToC 1
    itk_option define -defaultDir defaultDir DefaultDir ""

    public {
	variable path
	variable parentName
	variable disabledPages

	method loadPage		{pageName}
	method select		{pageName}
	method search		{query {mode full}}
    }
    protected {
	method getPages		{section}
	method loadSelectedPage	{listbox}
	method refreshPageList	{}
	method selectSection	{}
    }

    private {
	common sections {
	    all {All sections}
	    man1 {1 - Programs}
	    man3 {3 - Libraries}
	    man5 {5 - Formats}
	    mann {n - MGED commands}
	}
	# Each page records its name, section directory, and HTML path.
	variable pages {}
	variable listed_pages {}
	variable current_section
	variable search_query ""
	variable search_mode full
    }

    constructor {args} {}
}

# ------------------------------------------------------------
#                      OPERATIONS
# ------------------------------------------------------------

##
# Populate a section's man page list (man1, man3, man5 or mann).
::itcl::body ManBrowser::getPages {section} {
    set section_pages {}
    set mpath [file join $path $section]
    if {![file isdirectory $mpath]} {
	return $section_pages
    }
    foreach manFile [glob -nocomplain -types f -directory $mpath *.html] {
	set rootName [file rootname [file tail $manFile]]
	if {[lsearch -exact $disabledPages $rootName] < 0} {
	    lappend section_pages [list $rootName $section $manFile]
	}
    }
    return $section_pages
}

##
# Loads pages selected graphically or through the command line into HTML browser
#
::itcl::body ManBrowser::loadPage {pageName} {
    set viewer [$this childsite].browser.htmlview
    if {[file readable $pageName] && [file isfile $pageName]} {
	set pathname $pageName
    }
    if {![info exists pathname]} {
	foreach page $pages {
	    lassign $page name section candidate
	    if {$name eq $pageName &&
		($current_section eq "all" || $section eq $current_section)} {
		set pathname $candidate
		break
	    }
	}
    }
    if {![info exists pathname]} {
	$viewer error "Cannot read manual page '$pageName'."
	return 0
    }
    if {[catch {$viewer goto $pathname}]} {
	return 0
    }
    set title [$viewer title]
    if {$title ne ""} {
	configure -title $title
    }
    return 1
}

##
# Load the page selected in the table of contents.
#
::itcl::body ManBrowser::loadSelectedPage {listbox} {
    set selection [$listbox curselection]
    if {[llength $selection] != 1} {
	return
    }
    loadPage [lindex [lindex $listed_pages [lindex $selection 0]] 2]
}

##
# Selects page in ToC & loads into HTML browser; used for command line calls
#
::itcl::body ManBrowser::select {pageName} {
    set rootName [file rootname [file tail $pageName]]
    foreach page $pages {
	lassign $page name section pathname
	if {$name ne $rootName ||
	    ($current_section ne "all" && $section ne $current_section)} {
	    continue
	}
	if {![loadPage $pathname]} {
	    return 0
	}
	if {$itk_option(-useToC)} {
	    if {[lsearch -exact $listed_pages $page] < 0} {
		set search_query ""
	    }
	    refreshPageList
	}
	return 1
    }
    return 0
}

##
# Filter/rank the current table-of-contents list using the search box.
#
::itcl::body ManBrowser::refreshPageList {} {
    if {!$itk_option(-useToC)} {
	return
    }
    set toc $itk_component(manpagelistbox)
    $toc selection clear 0 [$toc index end]
    $toc delete 0 [$toc size]

    set query [string trim $search_query]
    set ranked {}
    foreach page $pages {
	lassign $page name section pathname
	if {$current_section ne "all" && $section ne $current_section} {
	    continue
	}
	set score 0
	if {$query ne ""} {
	    set section_label [::brlcad::help::manpage_search_section_label $section]
	    set record [::brlcad::help::manpage_search_index_file $name $section_label $pathname html]
	    if {$record eq ""} {
		continue
	    }
	    set score [::brlcad::help::manpage_search_rank_record $query $search_mode $record]
	}
	if {$score >= 0} {
	    lappend ranked [list $score $page]
	}
    }

    set listed_pages {}
    set viewer [$this childsite].browser.htmlview
    set location [$viewer location]
    set selected_path ""
    if {$location ne ""} {
	set selected_path [::tklitehtml::local_path $location]
    }
    foreach result [lsort -decreasing -integer -index 0 $ranked] {
	set page [lindex $result 1]
	lassign $page name section pathname
	lappend listed_pages $page
	$toc insert end "$name ([::brlcad::help::manpage_search_section_label $section])"
	if {[file normalize $pathname] eq $selected_path} {
	    set index [expr {[$toc size] - 1}]
	    $toc selection set $index
	    $toc activate $index
	    $toc see $index
	}
    }
}

::itcl::body ManBrowser::selectSection {} {
    set index [$itk_component(section_selector) current]
    set current_section [lindex [dict keys $sections] $index]
    refreshPageList
}

##
# Programmatic search entry point used by MGED menu items.
#
::itcl::body ManBrowser::search {query {mode full}} {
    set search_query $query
    if {$mode == "short"} {
	set search_mode short
    } else {
	set search_mode full
    }
    refreshPageList
    if {!$itk_option(-useToC) || [string trim $query] eq ""} {
	return
    }

    set toc $itk_component(manpagelistbox)
    if {[$toc size] > 0} {
	$toc selection clear 0 end
	$toc selection set 0
	$toc activate 0
	$toc see 0
	loadSelectedPage $toc
    }
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

    foreach section [dict keys $sections] {
	if {$section ne "all"} {
	    lappend pages {*}[getPages $section]
	}
    }
    set pages [lsort -dictionary -index 0 $pages]

    set current_section all
    if {$itk_option(-defaultDir) ne ""} {
	set current_section [::brlcad::help::manpage_search_section_dir $itk_option(-defaultDir)]
    }
    if {![dict exists $sections $current_section]} {
	error "Manual section must be 1, 3, 5, or n"
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
	set index_width 16

	itk_component add toc_panel {
	    ::tk::frame $parent.toc_panel
	} {}

	itk_component add section_selector {
	    ::ttk::combobox $itk_component(toc_panel).section_selector \
		-state readonly -width $index_width -values [dict values $sections]
	} {}
	$itk_component(section_selector) current \
	    [lsearch -exact [dict keys $sections] $current_section]
	bind $itk_component(section_selector) <<ComboboxSelected>> \
	    [::itcl::code $this selectSection]
	pack $itk_component(section_selector) -side top -fill x

	itk_component add search_frame {
	    ::tk::frame $itk_component(toc_panel).search_frame
	} {}
	set search_frame $itk_component(search_frame)
	label $search_frame.label -text "Search:"
	entry $search_frame.entry \
	    -textvariable [::itcl::scope search_query] \
	    -width $index_width
	frame $search_frame.controls
	checkbutton $search_frame.controls.full \
	    -text "Full text" \
	    -variable [::itcl::scope search_mode] \
	    -onvalue full \
	    -offvalue short \
	    -command [::itcl::code $this refreshPageList]
	button $search_frame.controls.clear \
	    -text "Clear" \
	    -command [::itcl::code $this search "" short]
	# A second row keeps the secondary controls from widening the sidebar.
	grid $search_frame.label -row 0 -column 0 -sticky w
	grid $search_frame.entry -row 0 -column 1 -sticky ew
	grid $search_frame.controls -row 1 -column 0 -columnspan 2 -sticky ew
	grid columnconfigure $search_frame 1 -weight 1
	pack $search_frame.controls.full -side left
	pack $search_frame.controls.clear -side right
	bind $search_frame.entry <KeyRelease> [::itcl::code $this refreshPageList]
	bind $search_frame.entry <Return> [::itcl::code $this refreshPageList]
	pack $search_frame -side top -fill x

	itk_component add index_frame {
	    ::tk::frame $itk_component(toc_panel).index_frame
	} {}

	itk_component add toc_scrollbar {
	    ::ttk::scrollbar $itk_component(index_frame).toc_scrollbar
	} {}

	itk_component add manpagelistbox {
	    ::tk::listbox $itk_component(index_frame).manpagelistbox -bd 2 \
		-width $index_width \
		-exportselection false \
		-yscrollcommand [list $itk_component(toc_scrollbar) set]
	} {}

	$itk_component(toc_scrollbar) configure \
	    -command [list $itk_component(manpagelistbox) yview]
	grid $itk_component(manpagelistbox) -row 0 -column 0 -sticky nsew
	grid $itk_component(toc_scrollbar) -row 0 -column 1 -sticky ns
	grid columnconfigure $itk_component(index_frame) 0 -weight 1
	grid rowconfigure $itk_component(index_frame) 0 -weight 1
	pack $itk_component(index_frame) -side top -expand yes -fill both
	pack $itk_component(toc_panel) -side left -expand no -fill y
    }

    # Main HTML window
    itk_component add browser {
	::tk::frame $parent.browser
    } {}
    set browser_frame $itk_component(browser)

    set manhtmlviewer [::tklitehtml::browser $browser_frame.htmlview]

    grid $manhtmlviewer -sticky nsew -in $browser_frame

    grid columnconfigure $browser_frame 0 -weight 1
    grid rowconfigure $browser_frame 0 -weight 1

    pack $itk_component(browser) -side left -expand yes -fill both

    set first_page ""
    foreach page $pages {
	lassign $page name section pathname
	if {$current_section eq "all" || $section eq $current_section} {
	    set first_page $pathname
	    break
	}
    }
    # Load Introduction.html if it is readable.  Otherwise, load the
    # first page in the selected section.
    set introduction [file join $path Introduction.html]
    if {[file readable $introduction] && [file isfile $introduction]} {
	loadPage $introduction
    } elseif {$first_page ne ""} {
	loadPage $first_page
    } else {
	$manhtmlviewer error \
	    "No readable manual pages were found in '$path'."
    }

    if {$itk_option(-useToC)} {
	refreshPageList
	bind $itk_component(manpagelistbox) <<ListboxSelect>> \
	    [::itcl::code $this loadSelectedPage %W]
    }

    # Give the manual room to read while respecting smaller displays.
    set default_width 1200
    set default_height 800
    lassign [wm maxsize $itk_component(hull)] max_width max_height
    configure -width [expr {min($default_width, $max_width)}] \
	-height [expr {min($default_height, $max_height)}]
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
