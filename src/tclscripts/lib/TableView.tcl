##                 T A B L E V I E W . T C L
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#       Survice Engineering Co. (www.survice.com)
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
#	of data. The number of data rows may change dynamically, either
#       by user interaction or via the programmatic interface. The number of
#       viewable rows also changes dynamically when the viewing height changes
#       in order to maintain a constant size row height. This widget contains
#       a built-in scrollbar for scrolling the view.
#

::itk::usual TableView {
    keep -borderwidth -cursor -foreground -highlightcolor \
	 -highlightthickness -insertbackground -insertborderwidth \
	 -insertofftime -insertontime -insertwidth -justify \
	 -relief -selectbackground -selectborderwidth \
	 -selectforeground -show \
	 -colfont -rowfont -background -entrybackground \
	 -rlborderwidth
}

::itcl::class TableView {
    inherit itk::Widget

    constructor {_labels args} {}
    destructor {}

    itk_option define -colfont colfont Font "Helvetica 12"
    itk_option define -rowfont rowfont Font "Helvetica 12"

    public {
	# methods that change/query table values
	method getEntry {i j}
	method setEntry {i j val {allowScroll 1}}
	method getRow {i}
	method getRows {i1 i2}
	method setRow {i rdata}
	method getCol {j}
	method getCols {j1 j2}
	method setCol {j cdata}
	method getTable {}
	method setTable {t}

	# methods that operate on rows
	method appendRow {rdata}
	method appendDefaultRow {{def {}}}
	method deleteRow {i}
	method insertRow {i rdata}
	method insertDefaultRow {i {def {}}}
	method moveRow {src dest}

	# methods that operate on columns
	method appendCol {label cdata}
	method appendDefaultCol {label {def {}}}
	method deleteCol {j}
	method insertCol {j label cdata}
	method insertDefaultCol {j label {def {}}}
	method initCol {j ival}
	method initColRange {j i1 i2 ival}

	# methods that operate on separators
	method appendSep {text}
	method deleteSep {i}
	method insertSep {i text}
	method getSepText {i}
	method setSepText {i text}
	method bindSep {args}
	method configSep {args}

	# methods that modify the appearance/behavior of columns of entries
	method traceCol {j tcmd {ops {}} {cmd {}}}
	method makeRowTraces {i}
	method deleteRowTraces {i}
	method shiftColTracesLeft {j}
	method shiftColTracesRight {j}
	method bindCol {j args}
	method configCol {j args}

	# methods that modify the appearance/behavior of
	# row or column labels
	method bindRowLabel {i args}
	method configRowLabel {i args}
	method bindColLabel {j args}
	method configColLabel {j args}

	# methods that operate on column labels
	method getColLabel {j}
	method setColLabel {j label}

	method scroll {n}
	method xview {cmd args}
	method yview {cmd args}
	method getFirstVisibleRow {}
	method getNumVisibleSep {}
	method getLastVisibleRow {}
	method getVisibleRows {}
	method getNumRows {}
	method getNumCols {}
	method updateRowLabels {first}

	method getRowIndex {w}
	method getDataRowIndex {w}
	method getColIndex {w}

	method getFirstEmptyRow {j}
	method getLastRow {}
	method getLastCol {}

	method getDefaultRow {{def {}}}
	method getDefaultSepRow {{def {}}}
	method getDefaultCol {{def {}}}

	method handleConfigure {}
	method growCol {j weight}
	method highlightRow {i}
    }

    protected {
	# string marking this row as a separator
	common SEP_MARK "XXX_SEP"

	# table of associated data
	variable table

	# percent of document visible (vertically)
	variable vshown 1

	# percent of document hidden (vertically)
	variable vhidden 0

	# column labels
	variable labels

	# number of columns
	variable vcols

	# number of viewable rows (fixed at creation)
	variable vrows 3

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

	# associative array to keep track of variables associated with each row label entry
	variable rlvar

	# keeps track of separator
	variable prevFirstHasSep 0
	variable sepIsFirst 0

	# keeps track of data row that gets highlighted
	variable highlightRow 0

	# keeps track of column colors (i.e background and foreground)
	variable colColors

	# entry options
	variable entryOptionList {-bg -cursor -exportselection -fg -font 
	    -highlightbackground -highlightcolor -highlightthickness
	    -insertbackground -insertborderwidth -insertofftime 
	    -insertontime -insertwidth -invcmd -justify -relief
	    -selectbackground -selectborderwidth -selectforeground
	    -show -state -validate -vcmd -width -xscrollcommand}

	# label options
	variable labelOptionList {-activebackground -activeforeground -anchor
	    -bd -bg -cursor -fg -font 
	    -highlightbackground -highlightcolor -highlightthickness
	    -justify -padx -pady -relief -state}

	method updateTable {{offset 0}}
	method updateDrows {}
	method updateVscroll {}
	method activateTraces {}
	method deactivateTraces {}
	method traceEntry {i j tcmd {ops {}} {cmd {}}}

	method initColColors {}

	method hscroll {first last}
	method configureCanvas {} 
	method configureTable {} 
    }

    private {
#	variable watchingTables 0

	method packRow {i}
	method packAll {}
	method packSepAll {}
	method deleteColLabel {j}
	method buildColLabel {j}
	method deleteEntry {i j}
	method buildEntry {i j}
	method buildRow {i}
	method destroyRow {i}
	method buildTable {}
	method watchTable {var index op}
    }
}

::itcl::configbody TableView::colfont {
#    after idle [::itcl::code $this handleConfigure]
    after idle [::itcl::code catch [list $this handleConfigure]]
}

::itcl::configbody TableView::rowfont {
#    after idle [::itcl::code $this handleConfigure]
    after idle [::itcl::code catch [list $this handleConfigure]]
}

::itcl::body TableView::constructor {_labels args} {
    set vcols [llength $_labels]
    if {$vcols < 1} {
	error "TableView::constructor: the number of column labels must be greater than 0"
    }

    set labels "{} $_labels"
    set table [Table \#auto]

    # pad row with empty values (note - table already has 1 row and 1 column)
    for {set j 1} {$j < $vcols} {incr j} {
	$table appendCol {{}}
    }

    # build vertical scrollbar
    itk_component add vscroll {
	::scrollbar $itk_interior.vscroll -orient vertical \
		-command [::itcl::code $this yview]
    } {}
    grid $itk_component(vscroll) -row 0 -column 2 -sticky ns

    # build horizontal scrollbar
    itk_component add hscroll {
	::scrollbar $itk_interior.hscroll -orient horizontal \
		-command [::itcl::code $this xview]
    } {}
    grid $itk_component(hscroll) -row 1 -column 1 -sticky ew

    buildTable
    initColColors

    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 1 -weight 1

    bind [namespace tail $this] <Configure> [::itcl::code $this handleConfigure]

    # process options
    eval itk_initialize $args
}

::itcl::body TableView::destructor {} {
    delete object $table
}

################################ Public Methods ################################

::itcl::body TableView::getEntry {i j} {
    $table getEntry $i $j
}

::itcl::body TableView::setEntry {i j val {allowScroll 1}} {
    $table setEntry $i $j $val

    if {![string is digit $allowScroll]} {
	error "TableView::setEntry: bad allowScroll value - $allowScroll, must be a digit"
    }

    # if row is viewable update entries
    if {$allowScroll && $firstrow <= $i && $i <= $lastrow} {
	scroll 0
    }
}

::itcl::body TableView::getRow {i} {
    $table getRow $i
}

::itcl::body TableView::getRows {i1 i2} {
    $table getRows $i1 $i2
}

::itcl::body TableView::setRow {i rdata} {
    $table setRow $i $rdata

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    }
}

::itcl::body TableView::getCol {j} {
    $table getCol $j
}

::itcl::body TableView::getCols {j1 j2} {
    $table getCols $j1 $j2
}

::itcl::body TableView::setCol {j cdata} {
    $table setCol $j $cdata
    scroll 0
}

::itcl::body TableView::getTable {} {
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
::itcl::body TableView::setTable {t} {
    set tlen [llength $t]

    if {$tlen} {
	# if table is not empty, the rows better be the expected length
	foreach row $t {
	    if {[llength $row] != $vcols} {
		error "TableView::setTable: expected $vcols entries in row, got [llength $row]"
	    }
	}
    }

    # pad the table with empty rows
    if {$tlen < $vrows} {
	# first, build empty row
	set row {}
	for {set j 0} {$j < $vcols} {incr j} {
	    lappend row {}
	}

	# second, pad table with empty rows
	for {set i $tlen} {$i < $vrows} {incr i} {
	    lappend t $row
	}
    }

    $table configure -table $t
    scroll 0
}

::itcl::body TableView::appendRow {rdata} {
    $table appendRow $rdata
    updateDrows
    updateVscroll
}

::itcl::body TableView::appendDefaultRow {{def {}}} {
    appendRow [getDefaultRow $def]
}

::itcl::body TableView::deleteRow {i} {
    $table deleteRow $i

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    } else {
	updateDrows
	updateVscroll
    }
}

::itcl::body TableView::insertRow {i rdata} {
    $table insertRow $i $rdata

    # if row is viewable update entries
    if {$firstrow <= $i && $i <= $lastrow} {
	scroll 0
    } else {
	updateDrows
	updateVscroll
    }
}

::itcl::body TableView::insertDefaultRow {i {def {}}} {
    insertRow $i [getDefaultRow $def]
}

::itcl::body TableView::moveRow {src dest} {
    if {![$table isValidRow $src]} {
	error "moveRow: invalid source index ($src)"
	return
    }

    if {![$table isValidRow $dest]} {
	error "moveRow: invalid destination index ($dest)"
	return
    }

    # nothing to do
    if {$src == $dest} {
	return
    }

    # since the table will be modified after
    # the delete, we need to modify the destination
    if {$src < $dest} {
	incr dest -1
    }

    set row [$table getRow $src]
    $table deleteRow $src
    insertRow $dest $row
}

#
# Note - Dynamic columns always work on the last column. Afterwards,
#        simple adjustments are made to correct the view.
#

::itcl::body TableView::appendCol {label cdata} {
    incr vcols

    # update data
    $table appendCol $cdata

    # update labels
    lappend labels $label
    buildColLabel $vcols
    grid $itk_component(cl$vcols) -row 0 -column $vcols -sticky nsew

    # update entries
    deactivateTraces
    for {set i 1} {$i <= $vrows} {incr i} {
	buildEntry $i $vcols
	if {[grid info $itk_component(rl$i)] != ""} {
	    grid $itk_component(e$i,$vcols) \
		-row $i -column $vcols -sticky nsew
	}
    }
    activateTraces

    #grid columnconfigure $itk_interior $vcols -weight 1

    # repack the vertical bar
    #grid $itk_component(vscroll) -row 1 -column 2 -rowspan $vrows -sticky ns
    #grid columnconfigure $itk_interior [expr {$vcols + 1}] -weight 0

    initColColors
    packSepAll

    # update table's view
    scroll 0
}

::itcl::body TableView::appendDefaultCol {label {def {}}} {
    appendCol $label [getDefaultCol $def]
}

::itcl::body TableView::deleteCol {j} {
    shiftColTracesLeft $j

    # update data
    $table deleteCol $j

    # update labels
    set labels [lreplace $labels $j $j]
    for {set _j $j} {$_j < $vcols} {incr _j} {
	$itk_component(cl$_j) configure -text [lindex $labels $_j]
    }

    # delete last label
    deleteColLabel $vcols

    # delete last column
    for {set i 1} {$i <= $vrows} {incr i} {
	deleteEntry $i $vcols
    }

    # repack the vertical bar
    incr vcols -1
    #grid $itk_component(vscroll) -row 1 -column 2 -rowspan $vrows -sticky ns
    #grid columnconfigure $itk_interior [expr {$vcols + 1}] -weight 0

    initColColors
    packSepAll

    # update table's view
    scroll 0
}

::itcl::body TableView::insertCol {j label cdata} {
    # update data
    $table insertCol $j $cdata

    # update labels list
    set labels [linsert $labels $j $label]

    incr vcols

    # build another column label at the end
    buildColLabel $vcols
    grid $itk_component(cl$vcols) -row 0 -column $vcols -sticky nsew

    # update label text
    for {set jtmp $j} {$jtmp <= $vcols} {incr jtmp} {
	$itk_component(cl$jtmp) configure -text [lindex $labels $jtmp]
    }

    # update entries
    deactivateTraces
    for {set i 1} {$i <= $vrows} {incr i} {
	buildEntry $i $vcols
	if {[grid info $itk_component(rl$i)] != ""} {
	    grid $itk_component(e$i,$vcols) \
		-row $i -column $vcols -sticky nsew
	}
    }
    activateTraces
    shiftColTracesRight $j

    # decide which row to use for configuring the rows of a column
    if {$highlightRow != 1} {
	set trow e1
    } else {
	set trow e2
    }

    # since we really create a column at the end, we
    # need to shift the bindings and configuration options
    set elist {}
    # note - we are not shifting the -textvariable option
    for {set j1 [expr {$vcols - 1}]; set j2 $vcols} {$j < $j2} {incr j1 -1; incr j2 -1} {
	# shift bindings from j1 to j2
	set elist [bind $itk_component(e1,$j1)]

	foreach event $elist {
	    bindCol $j2 $event [bind $itk_component(e1,$j1) $event]
	}

	# shift configuration options from j1 to j2
	foreach o $entryOptionList {
	    configCol $j2 $o [$itk_component($trow,$j1) cget $o]
	}
    }

    # now turn off the bindings for column j
    foreach event $elist {
	bindCol $j $event {}
    }

    # use default configuration for column j
    #    foreach o $entryOptionList {
    #	set config [$itk_component($trow,$j1) configure $o]
    #	configCol $j $o [lindex $config end-1]
    #    }

    #grid columnconfigure $itk_interior $vcols -weight 1

    # repack the vertical bar
    #grid $itk_component(vscroll) -row 1 -column 2 -rowspan $vrows -sticky ns
    #grid columnconfigure $itk_interior [expr {$vcols + 1}] -weight 0

    initColColors
    packSepAll

    # update table's view
    scroll 0
}

::itcl::body TableView::insertDefaultCol {j label {def {}}} {
    insertCol $j $label [getDefaultCol $def]
}

::itcl::body TableView::initCol {j ival} {
    $table initCol $j $ival

    # update table's view
    scroll 0
}

::itcl::body TableView::initColRange {j i1 i2 ival} {
    $table initColRange $j $i1 $i2 $ival

    # update table's view
    scroll 0
}

::itcl::body TableView::appendSep {text} {
    set srow [getDefaultRow]
    set srow [lreplace $srow 0 0 [eval list $SEP_MARK $text]]
    appendRow $srow
}

::itcl::body TableView::deleteSep {i} {
    deleteRow $i
}

::itcl::body TableView::insertSep {i text} {
    set srow [getDefaultRow]
    set srow [lreplace $srow 0 0 [eval list $SEP_MARK $text]]
    insertRow $i $srow
}

::itcl::body TableView::getSepText {i} {
    set srow [$table getRow $i]
    set item1 [lindex $srow 0]
    if {[lindex $item1 0] == $SEP_MARK} {
	return [lrange $item1 1 end]
    }

    return ""
}

::itcl::body TableView::setSepText {i text} {
    set item1 [$table getEntry $i 1] 
    if {[lindex $item1 0] == $SEP_MARK} {
	if {2 <= [llength $item1]} {
	    # replace existing text
	    set item1 [eval lreplace \$item1 1 end $text]
	} else {
	    # append new text
	    eval lappend item1 $text
	}

	$table setEntry $i 1 $item1
    } else {
	error "TableView::setSepText: row $i is not a separator"
    }

    scroll 0
}

::itcl::body TableView::bindSep {args} {
    for {set i 1} {$i <= $vrows} {incr i} {
	eval bind $itk_component(sl$i) $args
    }
}

::itcl::body TableView::configSep {args} {
    for {set i 1} {$i <= $vrows} {incr i} {
	eval $itk_component(sl$i) configure $args
    }
}

::itcl::body TableView::traceCol {j tcmd {ops {}} {cmd {}}} {
    if {![$table isValidCol $j]} {
	error "TableView::traceCol: bad column value - $j, must be in the range 1-$vcols"
    }

    if {$tcmd != "variable" && $tcmd != "vdelete"} {
	error "TableView::traceCol: bad command - $tcmd, should be \"variable\" or \"vdelete\""
    }

    switch -- $tcmd {
	variable {
	    for {set i 1} {$i <= $vrows} {incr i} {
		trace variable evar($i,$j) $ops $cmd
		lappend traces [list evar($i,$j) $ops $cmd]
	    }
	}
	vdelete {
	    for {set i 1} {$i <= $vrows} {incr i} {
		trace vdelete evar($i,$j) $ops $cmd
		set si [lsearch -exact $traces [list evar($i,$j) $ops $cmd]]
		if {$si != -1} {
		    set traces [lreplace $traces $si $si]
		}
	    }
	}
    }
}

::itcl::body TableView::makeRowTraces {i} {
    set rowTraces {}

    foreach t $traces {
	for {set j 1} {$j <= $vcols} {incr j} {
	    if {[lindex $t 0] == "evar(1,$j)"} {
		lappend rowTraces [list evar($i,$j) [lindex $t 1] [lindex $t 2]]
	    }
	}
    }

    return $rowTraces
}

::itcl::body TableView::deleteRowTraces {i} {
    for {set j 1} {$j <= $vcols} {incr j} {
	foreach vt [trace vinfo evar($i,$j)] { 
	    eval trace vdelete evar($i,$j) $vt
	    set si [lsearch -exact $traces [list evar($i,$j) [lindex $vt 0] [lindex $vt 1]]]
	    if {$si != -1} {
		set traces [lreplace $traces $si $si]
	    }
	}
    }
}

::itcl::body TableView::shiftColTracesLeft {j} {
    if {$j == 1} {
	return
    }

    if {![$table isValidCol $j]} {
	error "TableView::shiftColTracesLeft: bad column value - $j, must be in the range 1-$vcols"
    }

    # now shift traces left
    for {set j1 [expr {$j - 1}]; set j2 $j} {$j1 < $vcols} {incr j1; incr j2} {
	# delete previous traces
	foreach vt [trace vinfo evar(1,$j1)] {
	    if {[llength $vt] == 2} {
		eval traceCol $j1 vdelete $vt
	    }
	}

	# set up new traces
	foreach vt [trace vinfo evar(1,$j2)] {
	    if {[llength $vt] == 2} {
		eval traceCol $j1 variable $vt
	    }
	}
    }

    # delete last column's traces
    foreach vt [trace vinfo evar(1,$vcols)] {
	if {[llength $vt] == 2} {
	    eval traceCol $vcols vdelete $vt
	}
    }
}

::itcl::body TableView::shiftColTracesRight {j} {
    if {$j == $vcols} {
	return
    }

    if {![$table isValidCol $j]} {
	error "TableView::shiftColTracesRight: bad column value - $j, must be in the range 1-$vcols"
    }

    # now shift traces right
    for {set j1 [expr {$vcols - 1}]; set j2 $vcols} {$j < $j2} {incr j1 -1; incr j2 -1} {
	# delete previous traces
	foreach vt [trace vinfo evar(1,$j2)] {
	    if {[llength $vt] == 2} {
		eval traceCol $j2 vdelete $vt
	    }
	}

	# set up new traces
	foreach vt [trace vinfo evar(1,$j1)] {
	    if {[llength $vt] == 2} {
		eval traceCol $j2 variable $vt
	    }
	}
    }


    # delete the j column's traces
    foreach vt [trace vinfo evar(1,$j)] {
	if {[llength $vt] == 2} {
	    eval traceCol $j vdelete $vt
	}
    }
}

::itcl::body TableView::bindCol {j args} {
    if {![$table isValidCol $j]} {
	error "TableView::bindCol: bad column value - $j, must be in the range 1-$vcols"
    }

    for {set i 1} {$i <= $vrows} {incr i} {
	eval bind $itk_component(e$i,$j) $args
    }
}

::itcl::body TableView::configCol {j args} {
    if {![$table isValidCol $j]} {
	error "TableView::configCol: bad column value - $j, must be in the range 1-$vcols"
    }

    for {set i 1} {$i <= $vrows} {incr i} {
	eval $itk_component(e$i,$j) configure $args
    }

    initColColors
}

::itcl::body TableView::bindRowLabel {i args} {
    if {![string is digit $i] || $i < 1 || $vrows < $i} {
	error "TableView::bindRowLabel: bad row number - $i, must be in the range 1-$vrows"
    }

    eval bind $itk_component(rl$i) $args
}

::itcl::body TableView::configRowLabel {i args} {
    if {![string is digit $i] || $i < 1 || $vrows < $i} {
	error "TableView::configRowLabel: bad row number - $i, must be in the range 1-$vrows"
    }

    eval $itk_component(rl$i) configure $args
}

::itcl::body TableView::bindColLabel {j args} {
    if {![string is digit $j] || $j < 1 || $vcols < $j} {
	error "TableView::bindColLabel: bad column number - $j, must be in the range 1-$vcols"
    }

    eval bind $itk_component(cl$j) $args
}

::itcl::body TableView::configColLabel {j args} {
    if {![string is digit $j] || $j < 1 || $vcols < $j} {
	error "TableView::configColLabel: bad column number - $j, must be in the range 1-$vcols"
    }

    eval $itk_component(cl$j) configure $args
}

::itcl::body TableView::getColLabel {j} {
    if {![$table isValidCol $j]} {
	error "TableView::getColLabel: bad column - $j, must be in the range 1-$vcols"
    }

    return [lindex $labels $j]
}

::itcl::body TableView::setColLabel {j label} {
    if {![$table isValidCol $j]} {
	error "TableView::setColLabel: bad column - $j, must be in the range 1-$vcols"
    }

    $itk_component(cl$j) configure -text $label

    set labels [lreplace $labels $j $j $label]
}

::itcl::body TableView::scroll {n} {
    if {![string is int $n]} {
	error "TableView::scroll: bad scroll value - $n"
    }

    # just in case table has been modified
    updateDrows
    incr firstrow $n
    set lastrow [expr {$firstrow + $vrows - 1}]
    if {$lastrow < $vrows} {
	set firstrow 1
	set lastrow $vrows
    }
    updateVscroll

    # add empty row(s) to table
    if {$drows < $lastrow} {
	# generate empty row
	set rdata [getDefaultRow]

	# calculate number of rows to add
	set n [expr {$lastrow - $drows}]
	for {set i 0} {$i < $n} {incr i} {
	    $table appendRow $rdata
	}

	updateDrows
	updateVscroll
    }

    deactivateTraces
    updateTable [expr {$firstrow - 1}]
    activateTraces
}

::itcl::body TableView::xview {cmd args} {
    eval $itk_component(canvas) xview $cmd $args
}

::itcl::body TableView::yview {cmd args} {
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
		    scroll [expr $i * $vrows]
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

::itcl::body TableView::getFirstVisibleRow {} {
    return $firstrow
}

::itcl::body TableView::getNumVisibleSep {} {
}

::itcl::body TableView::getLastVisibleRow {} {
    return [expr {$firstrow + $vrows - 1}]
}

::itcl::body TableView::getVisibleRows {} {
    return "[getFirstVisibleRow] [getLastVisibleRow]"
}

::itcl::body TableView::getNumRows {} {
    return [$table cget -rows]
}

::itcl::body TableView::getNumCols {} {
    return [$table cget -cols]
}

## - updateRowLabels
#
# Updates the table's row labels.
#
::itcl::body TableView::updateRowLabels {first} {
    if {![string is int $first]} {
	error "TableView::updateRowLabels: bad value - $first"
    }

    set firstrow $first
    set prevFirstHasSep 0
    set sepIsFirst 0
    set lastrow [getLastVisibleRow]
    if {$lastrow < $vrows} {
	set firstrow 1
	set lastrow [getLastVisibleRow]
    }

    # update row labels
    for {set i 1; set li $firstrow} {$i <= $vrows} {incr i; incr li} {
	$itk_component(rl$i) configure -text $li
    }
}

::itcl::body TableView::getRowIndex {w} {
    # Strip away everything except the row number.
    #
    # First try entry
    if {[regsub {^.+e([0-9]+),[0-9]+$} $w {\1} i]} {
	return [expr $firstrow + $i - 1]
    }

    # Trying separator
    if {[regsub {^.+sl([0-9]+)$} $w {\1} i]} {
	return [expr $firstrow + $i - 1]
    }

    # bad
    return 0
}

::itcl::body TableView::getDataRowIndex {w} {
    if {[set ri [getRowIndex $w]] < 1} {
	error "TableView::getDataRowIndex: $w is not a valid entry window"
    }

    set di [$table countData $ri $SEP_MARK]
    if {![$table isData $firstrow $SEP_MARK] && $di < $firstrow} {
	incr di
    }

    return $di
}

::itcl::body TableView::getColIndex {w} {
    # Strip away everything except the column number.
    #
    # First try entry
    if {[regsub {^.+e[0-9]+,([0-9]+)$} $w {\1} j]} {
	return $j
    }

    # Trying separator
    if {[regsub {^.+sl[0-9]+$} $w {} j]} {
	return 1
    }

    # bad
    return 0
}

::itcl::body TableView::getFirstEmptyRow {j} {
    $table getFirstEmptyRow $j
}

::itcl::body TableView::getLastRow {} {
    return [$table cget -rows]
}

::itcl::body TableView::getLastCol {} {
    return [$table cget -cols]
}


::itcl::body TableView::getDefaultRow {{def {}}} {
    # generate empty row
    set rdata {}
    for {set j 0} {$j < $vcols} {incr j} {
	lappend rdata $def
    }

    return $rdata
}

::itcl::body TableView::getDefaultSepRow {{def {}}} {
    return [lreplace [getDefaultRow $def] 0 0 "$SEP_MARK {}"]
}

::itcl::body TableView::getDefaultCol {{def {}}} {
    # generate empty column
    set cdata {}
    set nrows [$table cget -rows]
    for {set i 0} {$i < $nrows} {incr i} {
	lappend cdata $def
    }

    return $cdata
}

::itcl::body TableView::growCol {j weight} {
    if {![$table isValidCol $j]} {
	error "TableView::growCol: bad column - $j, must be in the range 1-$vcols"
    }

    if {![string is digit $weight]} {
	error "TableView::growCol: bad weight - $weight, must be a digit"
    }
    #grid columnconfigure $itk_interior $j -weight $weight
}

## - highlightRow
#
# A nonzero digit indicates which data row to highlight.
#
::itcl::body TableView::highlightRow {i} {
    if {[$table isValidRow $i]} {
	set highlightRow $i
    } else {
	set highlightRow 0
    }

    if {$i < $firstrow} {
	scroll [expr {$i - $firstrow}]
    } elseif {$lastrow < $i} {
	scroll [expr {$i - $lastrow}]
    } else {
	scroll 0
    }
}

################################ Protected Methods ################################

## - updateTable
#
# Updates the table entries with data from the associated data list
# found in table. The "roffset" parameter is used as the row offset
# into table.
#
::itcl::body TableView::updateTable {{roffset 0}} {
    if {![string is digit $roffset]} {
	error "TableView::updateTable: bad row offset - $roffset"
    }

    if {$vrows < 1} {
	return
    }

    set i1 [expr {$roffset + 1}]
    set i2 [expr {$roffset + $vrows}]
    set rows [$table getRows $i1 $i2]

    # gi is used to indicate the GUI row
    set gi 1

    # ti is used to indicate the table row
    set ti $firstrow

    # di is used to indicate the data row
    set di [$table countData $firstrow $SEP_MARK]

    if {![$table isData $firstrow $SEP_MARK] && $di < $firstrow} {
	incr di
    }
    foreach row $rows {
	set item1 [lindex $row 0]
	if {[lindex $item1 0] == $SEP_MARK} {

	    # Here we need to view a separator, but we have widgets
	    # packed for viewing data.
	    if {[grid info $itk_component(rl$gi)] != ""} {
#		grid $itk_component(sl$gi) -row $gi -column 1 -columnspan [expr {$vcols - 1}] -sticky nsew
		raise $itk_component(sl$gi)
		    

		set j 1
		foreach val $row {
#		    grid forget $itk_component(e$gi,$j)
		    incr j
		}

#		grid forget $itk_component(rl$gi)
		# update row label
		#$itk_component(rl$gi) configure -text ""
		set rlvar($gi) ""
	    }

	    $itk_component(sl$gi) configure -text [lrange $item1 1 end]
	} else {
	    # Here we need to view data, but we have a widget packed
	    # for viewing a separator.
	    if {[grid info $itk_component(sl$gi)] != ""} {
		# pack row label
#		grid $itk_component(rl$gi) -row $gi -column 0 -sticky nsew
		raise $itk_component(rl$gi)

		set j 1
		foreach val $row {
		    # pack row entry
#		    grid $itk_component(e$gi,$j) -row $gi -column $j -sticky nsew
		    raise $itk_component(e$gi,$j)

		    set evar($gi,$j) $val
		    incr j
		}

#		grid forget $itk_component(sl$gi)
	    } else {
		set j 1
		foreach val $row {
		    set evar($gi,$j) $val
		    incr j
		}
	    }

	    # use the selectcolors to highlight the row
	    if {$highlightRow == $ti} {
		for {set j 1} {$j <= $vcols} {incr j} {
		    set sbg [$itk_component(e$gi,$j) cget -selectbackground]
		    set sfg [$itk_component(e$gi,$j) cget -selectforeground]
		    $itk_component(e$gi,$j) configure \
			-bg $sbg \
			-fg $sfg
		}
	    } else {
		for {set j 1} {$j <= $vcols} {incr j} {
		    set colColor [lindex $colColors $j]
		    $itk_component(e$gi,$j) configure \
			-bg [lindex $colColor 0] \
			-fg [lindex $colColor 1]
		}
	    }

	    # update row label
	    #$itk_component(rl$gi) configure -text $di
	    set rlvar($gi) $di
	    incr di
	}

	incr gi
	incr ti
    }
}

## - updateDrows
#
# Updates four variables used for adjusting/setting the vertical scrollbar.
#
::itcl::body TableView::updateDrows {} {
    set drows [$table cget -rows]

    if {$vrows < $drows} {
	set invdrows [expr {1.0 / double($drows - 1)}]
	set vshown [expr {$vrows * $invdrows}]
#	set vshown [expr {$vrows / double($drows)}]
    } else {
	set invdrows 0
	set vshown 1
    }

    set vhidden [expr {1.0 - $vshown}]
}

## - updateVscroll
#
# Update/adjust the vertical scroll bar.
#
::itcl::body TableView::updateVscroll {} {
    # adjust vscroll
    set y1 [expr {($firstrow - 1) * $invdrows}]
    if {$y1 < 0} {
	set y1 0
    }
    set y2 [expr {$y1 + $vshown}]
    $itk_component(vscroll) set $y1 $y2
}

::itcl::body TableView::activateTraces {} {
    foreach trace $traces {
	eval trace variable $trace
    }
}

::itcl::body TableView::deactivateTraces {} {
    foreach trace $traces {
	eval trace vdelete $trace
    }
}

::itcl::body TableView::traceEntry {i j tcmd {ops {}} {cmd {}}} {
    if {![string is digit $i] || $i < 1 || $vrows < $i} {
	error "TableView::traceEntry: bad row value - $i, must be in the range 1-$vrows"
    }

    if {![$table isValidCol $j]} {
	error "TableView::traceEntry: bad column value - $j, must be in the range 1-$vcols"
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

::itcl::body TableView::handleConfigure {} {
    # column label height
    set clh [winfo height $itk_component(cl1)]

    set elh [winfo height $itk_component(e1,1)]

    # total height of the table
    set winh [winfo height [namespace tail $this]]

    set height [expr {$winh - $clh}]

    set nrows [expr {int($height / double($elh)) - 1}]

    if {$nrows < 3} {
	set nrows 3
    }

    if {$nrows == $vrows} {
	return
    }

    if {$nrows < $vrows} {
	# delete extra rows
	for {} {$nrows < $vrows} {incr vrows -1} {
	    deleteRowTraces $vrows
	    destroyRow $vrows
	}
    } else {
	set newTraces {}
	deactivateTraces
	# create extra rows
	for {set i [expr {$vrows + 1}]} {$i <= $nrows} {incr i} {
	    buildRow $i
	    packRow $i
	    eval lappend newTraces [makeRowTraces $i]

	    # configure options and set bindings for row entries
	    # (i.e. we're basically copying row 1's options and
	    #       bindings to row i)
	    for {set j 1} {$j <= $vcols} {incr j} {
		# set bindings for entry e$i,$j using e1,$j
		set elist [bind $itk_component(e1,$j)]
		foreach event $elist {
		    bind $itk_component(e$i,$j) $event [bind $itk_component(e1,$j) $event]
		}

		# configure options for entry e$i,$j using e1,$j
		foreach o $entryOptionList {
		    $itk_component(e$i,$j) configure $o [$itk_component(e1,$j) cget $o]
		}
	    }

	    # set bindings for separator i using sl1
	    set elist [bind $itk_component(sl1)]
	    foreach event $elist {
		bind $itk_component(sl$i) $event [bind $itk_component(sl1) $event]
	    }

	    # configure options for separator i using sl1
	    foreach o $labelOptionList {
		$itk_component(sl$i) configure $o [$itk_component(sl1) cget $o]
	    }
	}

	set vrows $nrows

	eval lappend traces $newTraces
	activateTraces
    }

    # repack the vertical bar
    if {0} {
	if {0 < $vrows} {
	    grid $itk_component(vscroll) -row 1 -column 2 -rowspan $vrows -sticky ns
	} else {
	    grid forget $itk_component(vscroll)
	}
    }

    packSepAll

    # repack table
    #grid $itk_component(table) -row 0 -column 1 -sticky nsew

    scroll 0
}

::itcl::body TableView::initColColors {} {
    # decide which row to use for configuring the rows of a column
    if {$highlightRow != 1} {
	set trow e1
    } else {
	set trow e2
    }

    set colColors {{}}
    for {set j 1} {$j <= $vcols} {incr j} {
	lappend colColors [list [$itk_component($trow,$j) cget -bg] \
			       [$itk_component($trow,$j) cget -fg]]
    }    
}

::itcl::body TableView::hscroll {first last} {
    $itk_component(hscroll) set $first $last
}

# Borrowed from iwidgets::Scrolledframe and
# modified to work herein.
#
::itcl::body TableView::configureCanvas {} {
    set sr [$itk_component(canvas) cget -scrollregion]
    set srw [lindex $sr 2]
    set srh [lindex $sr 3]
    
    $itk_component(table) configure -height $srh -width $srw
}

# Borrowed from iwidgets::Scrolledframe and
# modified to work herein.
#
::itcl::body TableView::configureTable {} {
    $itk_component(canvas) configure \
	    -scrollregion [$itk_component(canvas) bbox tableTag] 
}

################################ Private Methods ################################

::itcl::body TableView::packRow {i} {
    # pack row label
    grid $itk_component(rl$i) \
	-row $i -column 0 -sticky nsew

    for {set j 1} {$j <= $vcols} {incr j} {
	grid $itk_component(e$i,$j) \
	    -row $i -column $j -sticky nw
    }
}

::itcl::body TableView::packAll {} {
    for {set j 1} {$j <= $vcols} {incr j} {
	grid $itk_component(cl$j) -row 0 -column $j -sticky nsew
    }

    for {set i 1} {$i <= $vrows} {incr i} {
	packRow $i
    }

    packSepAll
}

::itcl::body TableView::packSepAll {} {
    for {set i 1} {$i <= $vrows} {incr i} {
	grid $itk_component(sl$i) -row $i -column 1 \
	    -columnspan $vcols -sticky nsew
    }
}

::itcl::body TableView::deleteColLabel {j} {
    itk_component delete cl$j
    destroy $itk_component(table).cl$j
}

::itcl::body TableView::buildColLabel {j} {
    itk_component add cl$j {
	::label $itk_component(table).cl$j -text [lindex $labels $j]
    } {
	keep -borderwidth
	rename -font -colfont colfont Font
    }
}

::itcl::body TableView::deleteEntry {i j} {
    # deactivate the text variable
#    unset evar($i,$j)

    itk_component delete e$i,$j
    destroy $itk_component(table).e$i,$j
}

::itcl::body TableView::buildEntry {i j} {
    itk_component add e$i,$j {
	# activate the text variable
	set evar($i,$j) ""

	::entry $itk_component(table).e$i,$j -textvariable [::itcl::scope evar($i,$j)]
    } {
	keep -borderwidth -cursor -foreground -highlightcolor \
	    -highlightthickness -insertbackground -insertborderwidth \
	    -insertofftime -insertontime -insertwidth -justify \
	    -relief -selectbackground -selectborderwidth \
	    -selectforeground -show

	rename -font -rowfont rowfont Font
	rename -highlightbackground -background background Background
	rename -background -entrybackground entryBackground Background
    }
}

::itcl::body TableView::buildRow {i} {
    # create row label
    itk_component add rl$i {
	# activate the text variable
	set rlvar($i) $i
	::entry $itk_component(rowLabels).rl$i -textvariable [::itcl::scope rlvar($i)] \
		-width 4 -justify right -state disabled -relief flat

	#::label $itk_component(rowLabels).rl$i -text $i
    } {
	rename -borderwidth -rlborderwidth rlborderwidth Rlborderwidth
	#rename -width -rlwidth rlwidth Rlwidth
	rename -font -rowfont rowfont Font
    }

    # create separator label
    itk_component add sl$i {
	::label $itk_component(table).sl$i -anchor w
    } {
	keep -borderwidth
	rename -font -rowfont rowfont Font
    }

    # create entries for row
    for {set j 1} {$j <= $vcols} {incr j} {
	buildEntry $i $j
    }
}

::itcl::body TableView::destroyRow {i} {
    # destroy row label
    itk_component delete rl$i
    destroy $itk_component(rowLabels).rl$i

    # destroy separator label
    itk_component delete sl$i
    destroy $itk_component(table).sl$i

    # create entries for row
    for {set j 1} {$j <= $vcols} {incr j} {
	deleteEntry $i $j
    }
}

# The idea to use a canvas for scrolling and the
# code for implementation was borrowed from iwidgets::Scrollframe.
#
::itcl::body TableView::buildTable {} {
    itk_component add canvas {
	::canvas $itk_interior.canvas \
		-height 1.0 -width 1.0 \
                -scrollregion "0 0 1 1" \
                -xscrollcommand [::itcl::code $this hscroll] \
	        -highlightthickness 0 -takefocus 0
    } {
	ignore -highlightcolor -highlightthickness
	keep -background -cursor
    }

    # build table frame
    itk_component add table {
	::frame $itk_component(canvas).table
    } {}

    # build table row label frame
    itk_component add rowLabels {
	::frame $itk_interior.rowLabels
    } {}

    # create row label 0
    itk_component add rl0 {
	::entry $itk_component(rowLabels).rl0  -textvariable [::itcl::scope rlvar(0)] \
		-width 4 -justify right -state disabled -relief flat

	#::label $itk_component(rowLabels).rl0 -text ""
    } {
	rename -borderwidth -rlborderwidth rlborderwidth Rlborderwidth
	#rename -width -rlwidth rlwidth Rlwidth
	rename -font -rowfont rowfont Font
    }

    # pack row label 0
    grid $itk_component(rl0) \
	-row 0 -column 0 -sticky nw

    # create column labels
    for {set j 1} {$j <= $vcols} {incr j} {
	buildColLabel $j
	grid $itk_component(cl$j) -row 0 -column $j -sticky nsew
    }

    for {set i 1} {$i <= $vrows} {incr i} {
	buildRow $i
	packRow $i
    }

    packSepAll

    pack $itk_component(table) -expand yes -fill both
    $itk_component(canvas) create window 0 0 -tags tableTag \
            -window $itk_component(table) -anchor nw
    grid $itk_component(rowLabels) -row 0 -column 0 -sticky nw
    grid $itk_component(canvas) -row 0 -column 1 -sticky nsew

    bind $itk_component(canvas) <Configure> [::itcl::code $this configureCanvas]
    bind $itk_component(table) <Configure> [::itcl::code $this configureTable]

    # configure rows
    for {set i 1} {$i <= $vrows} {incr i} {
	#grid rowconfigure $itk_interior $i -weight 0
    }

    # configure columns
    for {set j 1} {$j <= $vcols} {incr j} {
	#grid columnconfigure $itk_interior $j -weight 0
    }

    # Set trace on write for entire array.
    # Note - this will be called before any traces
    #        on individual array elements.
    trace variable evar w [::itcl::code $this watchTable]
    lappend traces [list evar w [::itcl::code $this watchTable]]
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
::itcl::body TableView::watchTable {var index op} {
#puts "watchTable: var - $var, index - $index"
#    if {$watchingTables} {
#	return
#    }

#    set watchingTables 1

    set tindex [split $index ,]
    set i [expr {[lindex $tindex 0] + $firstrow - 1}]
    set j [lindex $tindex 1]

    # update row i in table
    $table setEntry $i $j $evar($index)

#    set watchingTables 0
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
