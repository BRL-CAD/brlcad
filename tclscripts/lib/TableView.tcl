##                 T A B L E V I E W . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#	your "Statement of Terms and Conditions for the Release of
#	The BRL-CAD Package" agreement.
#
# Copyright Notice -
#	This software is Copyright (C) 1998-2004 by the United States Army
#	in all countries except the USA.  All rights reserved.
#
# Description -
#	The TableView mega-widget provides a mechanism for viewing a table
#	of data. While the number of data rows may change dynamically, either
#       by user interaction or via the programmatic interface, the number of
#       viewable rows and columns (viewable or not) is fixed at creation
#       (note - the number of data columns is always the same as the number
#       of viewable columns). This widget contains a built-in scrollbar for
#       scrolling the view.
#
# Disclaimer -
#       This mega-widget is not a spreadsheet widget and is intended to be used
#       by applications that already know what their data will look like.
#       Also, as a user I find horizontal scrolling irritating to say the least.
#       So, I did not provide this capability. If an application requires a table
#       with a large number of columns, I suggest breaking the table up into smaller
#       tables, perhaps placing them in a tabnotebook. It's my opinion that it's
#       easier to tab between tables than to scroll horizontally.
#

itk::usual TableView {
    keep -borderwidth -cursor -foreground -highlightcolor \
	 -highlightthickness -insertbackground -insertborderwidth \
	 -insertofftime -insertontime -insertwidth -justify \
	 -relief -selectbackground -selectborderwidth \
	 -selectforeground -show \
	 -entryfont -background -entrybackground
}

itcl::class TableView {
    inherit itk::Widget

    constructor {_rows _labels args} {}
    destructor {}

    public {
	# methods that change/query table values
	method getEntry {i j}
	method setEntry {i j val {allowScroll 1}}
	method getRow {i}
	method getRows {i1 i2}
	method setRow {i row}
	method getCol {j}
	method getCols {j1 j2}
	method setCol {j col}
	method getTable {}
	method setTable {t}

	# methods that operate on rows
	method appendRow {row}
	method deleteRow {i}
	method insertRow {i row}

	# methods that operate on columns
	method appendCol {col}
	method deleteCol {j}
	method insertCol {j col}

	# methods that modify the appearance/behavior of columns of entries
	method traceCol {j tcmd {ops {}} {cmd {}}}
	method bindCol {j args}
	method configCol {j args}

	method scroll {n}
	method yview {cmd args}
	method getFirstVisibleRow {}
	method getLastVisibleRow {}
	method getVisibleRows {}
	method updateRowLabels {first}
    }

    protected {
	# table of associated data
	variable table

	# percent of document visible (vertically)
	variable vshown 1

	# percent of document hidden (vertically)
	variable vhidden 0

	# column labels
	variable labels

	# number of columns (fixed at creation)
	variable cols

	# number of viewable rows (fixed at creation)
	variable rows

	# number of data rows
	variable drows
	variable invdrows

	# first visible row
	variable firstrow 1

	# last visible row
	variable lastrow

	# list of active traces
	variable traces {}

	# associative array to keep track of variables associated with each table entry
	variable evar

	method updateTable {{offset 0}}
	method updateDrows {}
	method updateVscroll {}
	method activateTraces {}
	method deactivateTraces {}
	method traceEntry {i j tcmd {ops {}} {cmd {}}}
    }

    private {
#	variable watchingTables 0

	method buildTable {}
	method watchTable {var index op}
    }
}

itcl::body TableView::constructor {_rows _labels args} {
    if {$_rows < 1} {
	error "TableView::constructor: the number of rows must be greater than 0"
    }

    set cols [llength $_labels]
    if {$cols < 1} {
	error "TableView::constructor: the number of column labels must be greater than 0"
    }

    set rows $_rows
    set labels "{} $_labels"
    set table [Table #auto]

    # pad row with empty values (note - table already has 1 row and 1 column)
    for {set j 1} {$j < $cols} {incr j} {
	$table appendCol {{}}
    }

    # build vertical scrollbar
    itk_component add vscroll {
	::scrollbar $itk_interior.vscroll -command [code $this yview]
    } {}
    grid $itk_component(vscroll) -row 1 -col [expr {$cols + 1}] -rowspan $rows -sticky ns

    buildTable
    scroll 0

    # process options
    eval itk_initialize $args
}

itcl::body TableView::destructor {} {
}

################################ Public Methods ################################

itcl::body TableView::getEntry {i j} {
    $table getEntry $i $j
}

itcl::body TableView::setEntry {i j val {allowScroll 1}} {
    $table setEntry $i $j $val

    if {![string is digit $allowScroll]} {
	error "TableView::setEntry: bad allowScroll value - $allowScroll, must be a digit"
    }

    # if row is viewable update entries
    if {$allowScroll && $firstrow <= $i && $i <= $lastrow} {
	scroll 0
    }
}

itcl::body TableView::getRow {i} {
    $table getRow $i
}

itcl::body TableView::getRows {i1 i2} {
    $table getRows $i1 $i2
}

itcl::body TableView::setRow {i row} {
    $table setRow $i $row

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    }
}

itcl::body TableView::getCol {j} {
    $table getCol $j
}

itcl::body TableView::getCols {j1 j2} {
    $table getCols $j1 $j2
}

itcl::body TableView::setCol {j col} {
    $table setCol $j $col
    scroll 0
}

itcl::body TableView::getTable {} {
    $table cget -table
}

## - setTable
#
# This method sets the table to $t. Before doing this
# it checks to make sure that $t contains rows with
# the expected number of elements. It also pads the
# table so that it contains atleast as many rows as
# are viewable.
#
itcl::body TableView::setTable {t} {
    set tlen [llength $t]

    if {$tlen} {
	# if table is not empty, the rows better be the expected length
	foreach row $t {
	    if {[llength $row] != $cols} {
		error "TableView::setTable: expected $cols entries in row, got [llength $row]"
	    }
	}
    }

    # pad the table with empty rows
    if {$tlen < $rows} {
	# first, build empty row
	set row {}
	for {set j 0} {$j < $cols} {incr j} {
	    lappend row {}
	}

	# second, pad table with empty rows
	for {set i $tlen} {$i < $rows} {incr i} {
	    lappend t $row
	}
    }

    $table configure -table $t
    scroll 0
}

itcl::body TableView::appendRow {row} {
    $table appendRow $row
    updateDrows
    updateVscroll
}

itcl::body TableView::deleteRow {i} {
    $table deleteRow $i

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    } else {
	updateDrows
	updateVscroll
    }
}

itcl::body TableView::insertRow {i row} {
    $table insertRow $i $row

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    } else {
	updateDrows
	updateVscroll
    }
}

itcl::body TableView::traceCol {j tcmd {ops {}} {cmd {}}} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "TableView::traceCol: bad column value - $j, must be in the range 1-$cols"
    }

    if {$tcmd != "variable" && $tcmd != "vdelete"} {
	error "TableView::traceCol: bad command - $tcmd, should be \"variable\" or \"vdelete\""
    }

    switch -- $tcmd {
	variable {
	    for {set i 1} {$i <= $rows} {incr i} {
		trace variable evar($i,$j) $ops $cmd
		lappend traces [list evar($i,$j) $ops $cmd]
	    }
	}
	vdelete {
	    for {set i 1} {$i <= $rows} {incr i} {
		trace vdelete evar($i,$j) $ops $cmd
		set i [lsearch -exact $traces [list evar($i,$j) $ops $cmd]]
		if {$i != -1} {
		    set traces [lreplace $traces $i $i]
		}
	    }
	}
    }
}

itcl::body TableView::bindCol {j args} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "TableView::bindCol: bad column value - $j, must be in the range 1-$cols"
    }

    for {set i 1} {$i <= $rows} {incr i} {
	eval bind $itk_component(e$i,$j) $args
    }
}

itcl::body TableView::configCol {j args} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "TableView::configCol: bad column value - $j, must be in the range 1-$cols"
    }

    for {set i 1} {$i <= $rows} {incr i} {
	eval $itk_component(e$i,$j) configure $args
    }
}

itcl::body TableView::scroll {n} {
    if {![string is int $n]} {
	error "TableView::scroll: bad scroll value - $n"
    }

    # just in case table has been modified
    updateDrows

    # this also sets firstrow and lastrow
    updateRowLabels [expr {$firstrow + $n}]
    updateVscroll

    # add empty row(s) to table
    if {$drows < $lastrow} {
	# generate empty row
	set row {}
	for {set j 0} {$j < $cols} {incr j} {
	    lappend row {}
	}

	# calculate number of rows to add
	set n [expr {$lastrow - $drows}]
	for {set i 0} {$i < $n} {incr i} {
	    $table appendRow $row
	}

	updateDrows
	updateVscroll
    }

    deactivateTraces
    updateTable [expr {$firstrow - 1}]
    activateTraces
}

itcl::body TableView::yview {cmd args} {
    switch -- $cmd {
	moveto {
	    if {$args < 0} {
		set args 0
	    } elseif {$vhidden < $args} {
		set args $vhidden
	    }
	    scroll [expr {int($args * $drows) - $firstrow + 1}]
	}
	scroll {
	    set i [lindex $args 0]
	    set op [lindex $args 1]
	    switch -- $op {
		units {
		    scroll $i
		}
		pages {
		    scroll [expr $i * $rows]
		}
		default {
		    error "TableView::yview: unrecognized scroll command - $op"
		}
	    }
	}
	default {
	    error "TableView::yview: unrecognized command - $cmd"
	}
    }
}

itcl::body TableView::getFirstVisibleRow {} {
    return $firstrow
}

itcl::body TableView::getLastVisibleRow {} {
    return $lastrow
}

itcl::body TableView::getVisibleRows {} {
    return "$firstrow $lastrow"
}

## - updateRowLabels
#
# Updates the tables row labels.
#
itcl::body TableView::updateRowLabels {first} {
    if {![string is int $first]} {
	error "TableView::updateRowLabels: bad value - $first"
    }

    set firstrow $first
    set lastrow [expr {$first + $rows - 1}]
    if {$lastrow < $rows} {
	set firstrow 1
	set lastrow $rows
    }

    # update row labels
    for {set i 1; set li $firstrow} {$i <= $rows} {incr i; incr li} {
	$itk_component(rl$i) configure -text $li
    }
}


################################ Protected Methods ################################

## - updateTable
#
# Updates the table entries with data from the associated data list
# found in table. The "roffset" parameter is used as the row offset
# into table.
#
itcl::body TableView::updateTable {{roffset 0}} {
    if {![string is digit $roffset]} {
	error "TableView::updateTable: bad row offset - $roffset"
    }

    set i1 [expr {$roffset + 1}]
    set i2 [expr {$roffset + $rows}]
    set vals [$table getRows $i1 $i2]

    if {0} {
	set nrows [llength $vals]
	if {$rows < $nrows} {
	    # truncate rows
	    set vals [lrange $vals 0 "end-[expr {$nrows - $rows}]"]
	} elseif {$nrows < $rows} {
	    # add rows
	    for {set i [expr {$rows - $nrows}]} {0 < $i} {incr i -1} {
		lappend vals {}
	    }
	}
    }

    set i 1
    foreach row $vals {
	if {0} {
	    set ncols [llength $row]
	    if {$cols < $ncols} {
		# truncate the row
		set row [lrange $row 0 "end-[expr {$ncols - $cols}]"]
	    } elseif {$ncols < $cols} {
		# pad the row
		for {set j [expr {$cols - $ncols}]} {0 < $j} {incr j -1} {
		    lappend row ""
		}
	    }
	}

	set j 1
	foreach val $row {
	    set evar($i,$j) $val
	    incr j
	}

	incr i
    }
}

## - updateDrows
#
# Updates four variables used for adjusting/setting the vertical scrollbar.
#
itcl::body TableView::updateDrows {} {
    set drows [$table cget -rows]

    if {$rows < $drows} {
	set invdrows [expr {1.0 / $drows}]
    } else {
	set invdrows 0
    }

    if {$rows < $drows} {
	set vshown [expr {double($rows) / double($drows)}]
    } else {
	set vshown 1
    }

    set vhidden [expr {1.0 - $vshown}]
}

## - updateVscroll
#
# Update/adjust the vertical scroll bar.
#
itcl::body TableView::updateVscroll {} {
    # adjust vscroll
    set y1 [expr {($firstrow - 1) * $invdrows}]
    if {$y1 < 0} {
	set y1 0
    }
    set y2 [expr {$y1 + $vshown}]
    $itk_component(vscroll) set $y1 $y2
}

itcl::body TableView::activateTraces {} {
    foreach trace $traces {
	eval trace variable $trace
    }
}

itcl::body TableView::deactivateTraces {} {
    foreach trace $traces {
	eval trace vdelete $trace
    }
}

itcl::body TableView::traceEntry {i j tcmd {ops {}} {cmd {}}} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "TableView::traceEntry: bad row value - $i, must be in the range 1-$rows"
    }

    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "TableView::traceEntry: bad column value - $j, must be in the range 1-$cols"
    }

    if {$tcmd != "variable" && $tcmd != "vdelete"} {
	error "TableView::traceEntry: bad command - $tcmd, should be \"variable\" or \"vdelete\""
    }

    switch -- $tcmd {
	variable {
	    trace variable evar($i,$j) $ops $cmd
	    lappend traces [list evar($i,$j) $ops $cmd]
	}
	vdelete {
	    trace vdelete evar($i,$j) $ops $cmd
	    set i [lsearch -exact $traces [list evar($i,$j) $ops $cmd]]
	    if {$i != -1} {
		set traces [lreplace $traces $i $i]
	    }
	}
    }
}


################################ Private Methods ################################

itcl::body TableView::buildTable {} {
    # create labels
    for {set i 1} {$i <= $rows} {incr i} {
	itk_component add rl$i {
	    ::label $itk_interior.rl$i -text $i
	} {
	    rename -width -rlwidth rlwidth Rlwidth
	}
    }

    for {set j 1} {$j <= $cols} {incr j} {
	itk_component add cl$j {
	    ::label $itk_interior.cl$j -text [lindex $labels $j]
	} {}
    }

    # create entries
    for {set i 1} {$i <= $rows} {incr i} {
	for {set j 1} {$j <= $cols} {incr j} {
	    itk_component add e$i,$j {
		# activate the text variable
		set evar($i,$j) ""

		::entry $itk_interior.e$i,$j -textvariable [scope evar($i,$j)]
	    } {
		keep -borderwidth -cursor -foreground -highlightcolor \
		     -highlightthickness -insertbackground -insertborderwidth \
		     -insertofftime -insertontime -insertwidth -justify \
		     -relief -selectbackground -selectborderwidth \
		     -selectforeground -show

		rename -font -entryfont entryFont Font
		rename -highlightbackground -background background Background
		rename -background -entrybackground entryBackground Background
	    }
	}
    }

    # pack-up labels
    for {set i 1} {$i <= $rows} {incr i} {
	grid $itk_component(rl$i) -row $i -col 0 -sticky nsew
    }

    for {set j 1} {$j <= $cols} {incr j} {
	grid $itk_component(cl$j) -row 0 -col $j -sticky nsew
    }

    # pack-up entries
    for {set i 1} {$i <= $rows} {incr i} {
	for {set j 1} {$j <= $cols} {incr j} {
	    grid $itk_component(e$i,$j) -row $i -col $j -sticky nsew
	}
    }

    # configure rows
    for {set i 1} {$i <= $rows} {incr i} {
	grid rowconfigure $itk_interior $i -weight 1
    }

    # configure columns
    for {set j 1} {$j <= $cols} {incr j} {
	grid columnconfigure $itk_interior $j -weight 1
    }

    # Set trace on write for entire array.
    # Note - this will be called before any traces
    #        on individual array elements.
    trace variable evar w [code $this watchTable]
    lappend traces [list evar w [code $this watchTable]]
}

## - watchTable
#
# Generic routine for watching TableView variables. It simply
# updates the corresponding table with a value from a TableView entry.
# The TableView entry is specified by var and index.
#
# Note - this is a private method that knows that $var is this
#        classes evar variable.
#
itcl::body TableView::watchTable {var index op} {
#puts "watchTable: var - $var, index - $index"
#    if {$watchingTables} {
#	return
#    }

#    set watchingTables 1

    set tindex [split $index ,]
    set ri [expr {[lindex $tindex 0] + $firstrow - 1}]
    set ci [lindex $tindex 1]

    # update row ri in table
    $table setEntry $ri $ci $evar($index)

#    set watchingTables 0
}
