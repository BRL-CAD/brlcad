#                     T K T A B L E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2010 United States Government as represented by
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
# Description -
#	The TkTable class wraps tktable with common functionality.
#

::itcl::class cadwidgets::TkTable {
    inherit itk::Widget

    constructor {_datavar _headings args} {}
    destructor {}

    itk_option define -dataCallback dataCallback DataCallback ""
    itk_option define -tablePopupHandler tablePopupHandler TablePopupHandler ""

    public {
	method setDataEntry {_index _val}
	method setTableCol {_col _val}
	method setTableVal {_index _val}
	method width {args}
    }

    protected {
	variable mTableDataVar
	variable mTableHeadings
	variable mToggleSelectMode 0

	method handleTablePopup {_win _x _y _X _Y}
	method toggleSelect {_win _x _y}
    }

    private {}
}

# ------------------------------------------------------------
#                        OPTIONS
# ------------------------------------------------------------


# ------------------------------------------------------------
#                      CONSTRUCTOR
# ------------------------------------------------------------

::itcl::body cadwidgets::TkTable::constructor {_datavar _headings args} {
    set mTableDataVar $_datavar
    set mTableHeadings $_headings

    set numcols [llength $_headings]

    itk_component add table {
	::table $itk_interior.table \
	    -cols $numcols \
	    -variable $mTableDataVar
    } {
	keep -anchor -autoclear -background -bordercursor -borderwidth \
	    -browsecommand -cache -colorigin -colseparator \
	    -colstretchmode -coltagcommand -colwidth -command -cursor -drawmode \
	    -ellipsis -exportselection -flashmode -flashtime -font -foreground \
	    -height -highlightbackground -highlightcolor -highlightthickness \
	    -insertbackground -insertborderwidth -insertofftime -insertontime \
	    -insertwidth -invertselected -ipadx -ipady -justify -maxheight \
	    -maxwidth -multiline -padx -pady -relief -resizeborders -rowheight \
	    -roworigin -rows -rowseparator -rowstretchmode -rowtagcommand \
	    -selectioncommand -selectmode -selecttitles -selecttype -sparsearray \
	    -state -takefocus -titlecols -titlerows -usecommand -validate \
	    -validatecommand -width -wrap
    }

# Hide these options from users of TkTable
#-cols
#-variable
#-xscrollcommand -yscrollcommand

    # Set table headings
    set col 0
    foreach heading $mTableHeadings {
	set $mTableDataVar\(0,$col\) $heading
	incr col
    }

    # Create scrollbars
    itk_component add tableHScroll {
	::ttk::scrollbar $itk_interior.tableHScroll \
	    -orient horizontal
    } {}

    itk_component add tableVScroll {
	::ttk::scrollbar $itk_interior.tableVScroll \
	    -orient vertical
    } {}

    # Hook up scrollbars
    $itk_component(table) configure -xscrollcommand "$itk_component(tableHScroll) set"
    $itk_component(table) configure -yscrollcommand "$itk_component(tableVScroll) set"
    $itk_component(tableHScroll) configure -command "$itk_component(table) xview"
    $itk_component(tableVScroll) configure -command "$itk_component(table) yview"

    grid $itk_component(table) $itk_component(tableVScroll) -sticky nsew
    grid $itk_component(tableHScroll) - -sticky nsew

    grid columnconfigure $itk_interior 0 -weight 1
    grid rowconfigure $itk_interior 0 -weight 1


    bind $itk_component(table) <Button-1> [::itcl::code $this toggleSelect %W %x %y]
    bind $itk_component(table) <Button-3> [::itcl::code $this handleTablePopup %W %x %y %X %Y]
    bind $itk_component(table) <B3-Motion>

    $itk_component(table) tag col select_col 0
    $itk_component(table) tag configure select_col \
	-relief raised
    $itk_component(table) tag configure title \
	-relief raised

    eval itk_initialize $args
}

# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body cadwidgets::TkTable::setDataEntry {_index _val} {
    set $mTableDataVar\($_index\) $_val
    if {$itk_option(-dataCallback) != ""} {
	catch {$itk_option(-dataCallback)}
    }
}

::itcl::body cadwidgets::TkTable::setTableCol {_col _val} {
    set row 1
    while {[info exists $mTableDataVar\($row,$_col\)]} {
	set $mTableDataVar\($row,$_col\) $_val
	incr row
    }

    if {$_col == 0} {
	if {$_val == "*"} {
	    set mToggleSelectMode 1
	} else {
	    set mToggleSelectMode 0
	}
    }
}

::itcl::body cadwidgets::TkTable::setTableVal {_index _val} {
    set $mTableDataVar\($_index\) $_val
}


::itcl::body cadwidgets::TkTable::width {args} {
    eval $itk_component(table) width $args
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body cadwidgets::TkTable::handleTablePopup {_win _x _y _X _Y} {
    if {$itk_option(-tablePopupHandler) == ""} {
	return
    }

    set index [$_win index @$_x,$_y]
    catch {$itk_option(-tablePopupHandler) $index $_X $_Y}
}

::itcl::body cadwidgets::TkTable::toggleSelect {_win _x _y} {
    set index [$_win index @$_x,$_y]
    set ilist [split $index ,]
    set col [lindex $ilist 1] 

    if {$col != 0} {
	return
    }

    set row [lindex $ilist 0] 
    if {![info exists $mTableDataVar\($row,$col\)]} {
	return
    }

    if {$row != 0} {
	# The outer subst doesn't seem to work with Itcl instance variables
	# from some class other than the current one.
	# if {[subst $[subst $mTableDataVar\($index\)]] == "*"}
	# Using "set" instead.
	if {[set [subst $mTableDataVar\($index\)]] == "*"} {
	    setTableVal $index ""
	} else {
	    setTableVal $index "*"
	}
    } else {
	if {$mToggleSelectMode} {
	    set mToggleSelectMode 0
	    setTableCol 0 ""
	} else {
	    set mToggleSelectMode 1
	    setTableCol 0 "*"
	}
    }

    if {$itk_option(-dataCallback) != ""} {
	catch {$itk_option(-dataCallback)}
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
