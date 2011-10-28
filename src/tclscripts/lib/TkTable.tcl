#                     T K T A B L E . T C L
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
# Description -
#	The TkTable class wraps tktable with common functionality.
#

::itcl::class cadwidgets::TkTable {
    inherit itk::Widget

    constructor {_datavar _headings args} {}
    destructor {}

    itk_option define -dataCallback dataCallback DataCallback ""
    itk_option define -tablePopupHandler tablePopupHandler TablePopupHandler ""
    itk_option define -validatecommand validatecommand ValidateCommand ""
    itk_option define -vclientdata vclientdata VClientData ""
    itk_option define -entercommand entercommand EnterCommand ""
    itk_option define -singleSelectCallback singleSelectCallback SingleDataCallback ""

    public {
	method handlePaste {}
	method see {_index}
	method setDataEntry {_index _val}
	method setTableCol {_col _val}
	method setTableVal {_index _val}
	method selectSingleRow {_row}
	method updateTitleCol {}
	method width {args}
    }

    protected {
	variable mNumCols 3
	variable mLastCol 2
	variable mNumRows 10
	variable mLastRow 9
	variable mTableDataVar
	variable mTableHeadings
	variable mToggleSelectMode 0
	variable mInsertMode 0
	variable mDoBreak 0
	variable mSingleSelectRow 0

	method doBreak {}
	method handleCopy {_win}
	method handleCut {_win}
	method handleEnter {_win}
	method handleKey {_win _key}
	method handleLeftRight {_win _sflag}
	method handleTablePopup {_win _x _y _X _Y}
	method handleUpDown {_win _up}
	method keyVisible {_key}
	method setInsertMode {_imode}
	method toggleSelect {_win _x _y}
	method validateTableEntry {_row _col _newval}
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

    set mNumCols [llength $_headings]
    set mLastCol [expr {$mNumCols - 1}]

    itk_component add table {
	::table $itk_interior.table \
	    -state disabled \
	    -titlecols 0 \
	    -titlerows 1 \
	    -cols $mNumCols \
	    -validatecommand [::itcl::code $this validateTableEntry %r %c %S] \
	    -variable $mTableDataVar
    } {
	keep -anchor -autoclear -background -bordercursor -borderwidth \
	    -browsecommand -cache -colorigin -colseparator \
	    -colstretchmode -coltagcommand -colwidth -command -cursor -drawmode \
	    -ellipsis -flashmode -flashtime -font -foreground \
	    -height -highlightbackground -highlightcolor -highlightthickness \
	    -insertbackground -insertborderwidth -insertofftime -insertontime \
	    -insertwidth -invertselected -ipadx -ipady -justify -maxheight \
	    -maxwidth -multiline -padx -pady -relief -resizeborders -rowheight \
	    -roworigin -rows -rowseparator -rowstretchmode -rowtagcommand \
	    -selectioncommand \
	    -takefocus -usecommand -validate \
	    -width -wrap
    }

# Hide these options from users of TkTable
#-cols
#-exportselection
#-selecttitles
#-selectmode
#-selecttype
#-sparsearray
#-state
#-titlecols
#-titlerows
#-validatecommand
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


    # Button Bindings
    bind $itk_component(table) <Button-1> "[::itcl::code $this toggleSelect %W %x %y]; if {\[[::itcl::code $this doBreak]\]} {break}"
    bind $itk_component(table) <Button-3> [::itcl::code $this handleTablePopup %W %x %y %X %Y]
    bind $itk_component(table) <B3-Motion>

    # Key Bindings
    bind $itk_component(table) <Key> "[::itcl::code $this handleKey %W %K]; if {\[[::itcl::code $this doBreak]\]} {break}"
    bind $itk_component(table) <Key-Tab> "[::itcl::code $this handleLeftRight %W 0]; break"
#    bind $itk_component(table) <Shift-Key-Tab> "[::itcl::code $this handleLeftRight %W 1]; break"
    bind $itk_component(table) <<PrevWindow>> "[::itcl::code $this handleLeftRight %W 1]; break"
    bind $itk_component(table) <Key-Left> "[::itcl::code $this handleLeftRight %W 1]; break"
    bind $itk_component(table) <Key-Right> "[::itcl::code $this handleLeftRight %W 0]; break"
    bind $itk_component(table) <Control-Key-Left> {%W icursor [expr {[%W icursor]-1}]; break}
    bind $itk_component(table) <Control-Key-Right> {%W icursor [expr {[%W icursor]+1}]; break}
    bind $itk_component(table) <Key-Up> "[::itcl::code $this handleUpDown %W 1]; break"
    bind $itk_component(table) <Key-Down> "[::itcl::code $this handleUpDown %W 0]; break"
    bind $itk_component(table) <Control-b> {%W icursor [expr {[%W icursor]-1}]; break}
    bind $itk_component(table) <Control-c> [::itcl::code $this handleCopy %W]
    bind $itk_component(table) <Control-d> {%W delete active insert; break}
    bind $itk_component(table) <Control-f> {%W icursor [expr {[%W icursor]+1}]; break}
    bind $itk_component(table) <Control-i> [::itcl::code $this setInsertMode 1]
    bind $itk_component(table) <Control-k> "[::itcl::code $this handleCut %W]; break"
    bind $itk_component(table) <Control-n> "[::itcl::code $this handleUpDown %W 0]; break"
    bind $itk_component(table) <Control-p> "[::itcl::code $this handleUpDown %W 1]; break"
    bind $itk_component(table) <Control-v> "[::itcl::code $this handlePaste]; break"
    bind $itk_component(table) <Control-y> "[::itcl::code $this handlePaste]; break"
    bind $itk_component(table) <Key-Return> "[::itcl::code $this handleEnter %W]; break"

    bind $itk_component(table) <Control-Key> "\# nothing; break"
    bind $itk_component(table) <Key-Insert> [::itcl::code $this setInsertMode 1]

    set bg [$itk_component(table) tag cget title -background]
    $itk_component(table) tag configure select_col \
	-background \#9999ff
    $itk_component(table) tag raise select_col
    $itk_component(table) tag col title 0
    $itk_component(table) tag configure title \
	-relief raised

    eval itk_initialize $args
    set mNumRows [$itk_component(table) cget -rows]
    set mLastRow [expr {$mNumRows - 1}]
}

# ------------------------------------------------------------
#                      PUBLIC METHODS
# ------------------------------------------------------------

::itcl::body cadwidgets::TkTable::handlePaste {} {
    #
    # Don't allow pasting into the title cells
    #
    if {[$itk_component(table) tag includes title active]} {
        return
    }

    set savestate [$itk_component(table) cget -state]
    setInsertMode 1

    if {[catch {::tk::table::GetSelection $itk_component(table) CLIPBOARD} data]} {
	return
    }

    set newinsert [string length $data]

    if {$savestate == "normal"} {
	set curval [$itk_component(table) curvalue]
	set insert [$itk_component(table) icursor]
	set first [string range $curval 0 $insert-1]
	set last [string range $curval $insert end]

	set index [$itk_component(table) index active]

	append first $data $last
	set data $first
	set newinsert [expr {$insert + $newinsert}]
    }

    if {$itk_option(-validatecommand) != ""} {
	set index [$itk_component(table) index active]
	set ilist [split $index ,]
	set row [lindex $ilist 0]
	set col [lindex $ilist 1]

	if {[catch {$itk_option(-validatecommand) $row $col $data $itk_option(-vclientdata)} isvalid]} {
	    set isvalid 0
	}
    } else {
	set isvalid 1
    }

    if {$isvalid} {
	$itk_component(table) set active $data
	$itk_component(table) icursor $newinsert
    }

#    focus $itk_component(table)
}

::itcl::body cadwidgets::TkTable::see {_index} {
    $itk_component(table) see $_index
}

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

	if {$_col == 0} {
	    if {$_val == "*"} {
		$itk_component(table) tag cell select_col $row,$_col
	    } else {
		$itk_component(table) tag cell {} $row,$_col
	    }
	}

	incr row
    }

    if {$itk_option(-singleSelectCallback) == ""} {
	if {$_col == 0} {
	    if {$_val == "*"} {
		set mToggleSelectMode 1
	    } else {
		set mToggleSelectMode 0
	    }
	}
    }
}

::itcl::body cadwidgets::TkTable::setTableVal {_index _val} {
    set $mTableDataVar\($_index\) $_val
}

::itcl::body cadwidgets::TkTable::selectSingleRow {_row} {
    if {$_row == 0} {
	return
    }

    # Turn off previously selected row
    if {$mSingleSelectRow} {
	setTableVal $mSingleSelectRow,0 ""
	$itk_component(table) tag cell {} $mSingleSelectRow,0
    }

    setTableVal $_row,0 "*"
    $itk_component(table) tag cell select_col $_row,0
    set mSingleSelectRow $_row
}

::itcl::body cadwidgets::TkTable::updateTitleCol {} {
    set row 1
    while {[info exists $mTableDataVar\($row,0\)]} {
	if {[set [subst $mTableDataVar\($row,0\)]] == "*"} {
	    $itk_component(table) tag cell select_col $row,0
	} else {
	    $itk_component(table) tag cell {} $row,0
	}

	incr row
    }

    # Do the rest of the rows. Note - these have NO data
    for {} {$row < $mNumRows} {incr row} {
	$itk_component(table) tag cell {} $row,0
    }
}

::itcl::body cadwidgets::TkTable::width {args} {
    eval $itk_component(table) width $args
}

# ------------------------------------------------------------
#                      PROTECTED METHODS
# ------------------------------------------------------------

::itcl::body cadwidgets::TkTable::doBreak {} {
    return $mDoBreak
}

::itcl::body cadwidgets::TkTable::handleCopy {_win} {
    set index [$_win index active]
    set ilist [split $index ,]
    set row [lindex $ilist 0]
    set col [lindex $ilist 1]

    if {$row == 0} {
	return
    }

    $_win selection clear all
    $_win activate $row,$col
    $_win selection set $row,$col
    tk_tableCopy $_win
}

::itcl::body cadwidgets::TkTable::handleCut {_win} {
    # First, grab the piece of string about to be cut
    # and stuff it into the clipboard.
    set curval [$_win curvalue]
    set begin [$_win icursor]
    clipboard clear -displayof $_win
    catch {clipboard append -displayof $_win [string range $curval $begin end]}

    $_win delete active insert end
}


::itcl::body cadwidgets::TkTable::handleEnter {_win} {
    if {$itk_option(-entercommand) != ""} {
	set index [$_win index active]
	set ilist [split $index ,]
	set row [lindex $ilist 0]
	set col [lindex $ilist 1]

	$_win selection clear all
	$_win activate $row,$col
	$_win selection set $row,$col

	catch {$itk_option(-entercommand) $row $col}
    }
}

::itcl::body cadwidgets::TkTable::handleKey {_win _key} {
    set index [$_win index active]
    set ilist [split $index ,]
    set row [lindex $ilist 0]
    set col [lindex $ilist 1]

    if {$row == 0} {
	set mDoBreak 1
	return
    }

    # Don't need to check for Left/Right/Down/Up
    # (i.e. these get swallowed up by more specific bindings).
    if {$col != 0 && !$mInsertMode} {
	set mDoBreak 1

	# Overwrite what's in the cell
	if {[keyVisible $_key]} {
	    setInsertMode 1

	    if {$itk_option(-validatecommand) != ""} {
		if {[catch {$itk_option(-validatecommand) $row $col $_key $itk_option(-vclientdata)} isvalid]} {
		    set isvalid 0
		}
	    } else {
		set isvalid 1
	    }

	    if {$isvalid} {
		setTableVal $index $_key
	    }
	} else {
	    setInsertMode 0
	    return
	}
    } else {
	if {$col == 0} {
	    set mDoBreak 1

	    if {![info exists $mTableDataVar\($index\)] || $_key == "Shift_L" || $_key == "Shift_R"} {
		return
	    }

	    if {[set [subst $mTableDataVar\($index\)]] == "*"} {
		setTableVal $index ""
		$itk_component(table) tag cell {} $index
	    } else {
		setTableVal $index "*"
		$itk_component(table) tag cell select_col $index
	    }
	} else {
	    set mDoBreak 0
	}
    }
}

::itcl::body cadwidgets::TkTable::handleLeftRight {_win _sflag} {
    set index [$_win index active]
    set ilist [split $index ,]
    set row [lindex $ilist 0]
    set col [lindex $ilist 1]

    if {$row == 0} {
	return
    }

    # This is a <<PrevWindow>> or <Shift-Tab>
    if {$_sflag} {
	incr col -1
	if {$col < 0} {
	    if {$row > 1} {
		# Advance to last column of the previous row
		set col $mLastCol
		incr row -1
	    } else {
		# Stop traversing
		set col 0
	    }
	}
    } else {
	incr col
	if {$col >= $mNumCols} {
	    if {$row < $mNumRows} {
		# Advance to first column of the next row
		set col 0
		incr row
	    } else {
		# Stop traversing
		incr col -1
	    }
	}
    }

    $_win selection clear all
    $_win activate $row,$col
    $_win selection set $row,$col

    if {$mInsertMode} {
	setInsertMode 0
    }
}

::itcl::body cadwidgets::TkTable::handleTablePopup {_win _x _y _X _Y} {
    if {$itk_option(-tablePopupHandler) == ""} {
	return
    }

    set index [$_win index @$_x,$_y]
    catch {$itk_option(-tablePopupHandler) $index $_X $_Y}
}

::itcl::body cadwidgets::TkTable::handleUpDown {_win _up} {
    set index [$_win index active]
    set ilist [split $index ,]
    set row [lindex $ilist 0]
    set col [lindex $ilist 1]

    if {$row == 0} {
	return
    }

    if {$_up} {
	incr row -1
	if {$row < 1} {
	    set row 1
	}
    } else {
	incr row
	if {$row > $mNumRows} {
	    set row $mLastRow
	}
    }

    $_win selection clear all
    $_win activate $row,$col
    $_win selection set $row,$col

    if {$mInsertMode} {
	setInsertMode 0
    }
}

::itcl::body cadwidgets::TkTable::keyVisible {_key} {
    if {[string length $_key] == 1} {
	return 1
    }

    return 0
}

::itcl::body cadwidgets::TkTable::setInsertMode {_imode} {
    set mInsertMode $_imode

    if {$mInsertMode} {
	$itk_component(table) configure -state normal
	bind $itk_component(table) <Key-Left> {%W icursor [expr {[%W icursor]-1}]; break}
	bind $itk_component(table) <Key-Right> {%W icursor [expr {[%W icursor]+1}]; break}
    } else {
	$itk_component(table) configure -state disabled
	bind $itk_component(table) <Key-Right> "[::itcl::code $this handleLeftRight %W 0]; break"
	bind $itk_component(table) <Key-Left> "[::itcl::code $this handleLeftRight %W 1]; break"
    }
}

::itcl::body cadwidgets::TkTable::toggleSelect {_win _x _y} {
    set index [$_win index @$_x,$_y]
    set ilist [split $index ,]
    set row [lindex $ilist 0] 
    set col [lindex $ilist 1] 

    if {$col != 0} {
	set mDoBreak 0

	if {![catch {$_win index active} aindex]} {
	    set ailist [split $aindex ,]
	    set arow [lindex $ailist 0]
	    set acol [lindex $ailist 1]

	    if {$row == $arow && $col == $acol} {
		setInsertMode 1
	    } else {
		# This line causes the cell data to be
		# retained even after the state is disable
		# by the call to "setInsertMode 0" below.
		$itk_component(table) activate active

		setInsertMode 0
	    }
	}

	return
    }

    if {![info exists $mTableDataVar\($row,$col\)]} {
	if {$itk_option(-singleSelectCallback) != ""} {
	    set mDoBreak 1
	}
	return
    }

    if {$itk_option(-singleSelectCallback) != ""} {
	set mDoBreak 1
	selectSingleRow $row
	catch {$itk_option(-singleSelectCallback) $row}
    } else {
	set mDoBreak 0

	if {$row != 0} {
	    # The outer subst doesn't seem to work with Itcl instance variables
	    # from some class other than the current one.
	    # if {[subst $[subst $mTableDataVar\($index\)]] == "*"}
	    # Using "set" instead.
	    if {[set [subst $mTableDataVar\($index\)]] == "*"} {
		setTableVal $index ""
		$_win tag cell {} $index
	    } else {
		setTableVal $index "*"
		$_win tag cell select_col $index
	    }
	} else {
	    set mDoBreak 1
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
}

::itcl::body cadwidgets::TkTable::validateTableEntry {_row _col _newval} {
    if {$itk_option(-validatecommand) != ""} {
	if {[catch {$itk_option(-validatecommand) $_row $_col $_newval $itk_option(-vclientdata)} isvalid]} {
	    # Always invalid
	    return 0
	}

	# Validity depends on -validatecommand
	return $isvalid
    }

    # Always valid
    return 1
}



# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
