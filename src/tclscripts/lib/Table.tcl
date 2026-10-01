#                       T A B L E . T C L
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
# Description -
#       This class comprises a collection of methods for querying/modifying tables.
#       For the purposes of this class, a table is a list of lists with each sublist
#       being a row in the table.
#

package provide cadwidgets::Table 1.0

::itcl::class ::cadwidgets::Table {
    constructor {args} {}
    destructor {}

    public {
	common SEP_MARK "XXX_SEP"

	# initialize table with 1 row and 1 col
	variable table {{{}}}
	variable rows 1
	variable cols 1

	# methods that operate on items
	method getEntry {i j}
	method setEntry {i j val}

	# methods that operate on rows
	method getRow {i}
	method getRows {i1 i2}
	method setRow {i rdata}
	method appendRow {rdata}
	method deleteRow {i}
	method insertRow {i rdata}
	method moveRow {srcIndex destIndex}

	# methods that operate on columns
	method getCol {j}
	method getCols {j1 j2}
	method setCol {j cdata}
	method appendCol {cdata}
	method deleteCol {j}
	method insertCol {j cdata}
	method initCol {j ival}
	method initColRange {j i1 i2 ival}

	method isValidRow {i}
	method isValidCol {j}
	method getFirstEmptyRow {j}

	method isData {i mark}
	method countData {i mark}

	method printColRange {chan j1 j2}
	method sortByCol {j type order}
	method searchCol {j i str}
	method deleteEmptyRows {}
    }

    protected {
    }
}

::itcl::configbody ::cadwidgets::Table::table {
    if {![llength $table]} {
	error "Table: table is zero length"
    }

    set i 1
    set j [llength [lindex $table 0]]
    foreach row [lrange $table 1 end] {
	incr i
	if {[llength $row] != $j} {
	    error "Table: table has differing row lengths, expecting $j, got [llength $row]\n\toffending row - $row"
	}
    }

    set rows $i
    set cols $j
}

::itcl::configbody ::cadwidgets::Table::rows {
    error "Table: this option is read-only"
}

::itcl::configbody ::cadwidgets::Table::cols {
    error "Table: this option is read-only"
}

::itcl::body ::cadwidgets::Table::constructor {args} {
    # process options
    eval configure $args
}

::itcl::body ::cadwidgets::Table::getEntry {i j} {
    if {![isValidRow $i]} {
	error "Table::getEntry: i must be in the range (1,$rows) inclusive"
    }

    if {![isValidCol $j]} {
	error "Table::getEntry: j must be in the range (1,$cols) inclusive"
    }

    incr i -1
    incr j -1
    lindex [lindex $table $i] $j
}

::itcl::body ::cadwidgets::Table::setEntry {i j val} {
    if {![isValidRow $i]} {
	error "Table::setEntry: i must be in the range (1,$rows) inclusive"
    }

    if {![isValidCol $j]} {
	error "Table::setEntry: j must be in the range (1,$cols) inclusive"
    }

    incr i -1
    incr j -1
    set table [lreplace $table $i $i [lreplace [lindex $table $i] $j $j $val]]
    return
}

::itcl::body ::cadwidgets::Table::getRow {i} {
    if {![isValidRow $i]} {
	error "Table::getRow: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    lindex $table $i
}

::itcl::body ::cadwidgets::Table::getRows {i1 i2} {
    if {![isValidRow $i1]} {
	error "Table::getRows: i1 must be in the range (1,$rows) inclusive"
    }

    if {![isValidRow $i2]} {
	error "Table::getRows: i2 must be in the range (1,$rows) inclusive"
    }

    incr i1 -1
    incr i2 -1
    lrange $table $i1 $i2
}

::itcl::body ::cadwidgets::Table::setRow {i rdata} {
    if {![isValidRow $i]} {
	error "Table::setRow: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    if {[llength $rdata] != $cols} {
	error "Table::setRow: number of values in row must be $cols"
    }

    set table [lreplace $table $i $i $rdata]
    return
}

::itcl::body ::cadwidgets::Table::appendRow {rdata} {
    if {[llength $rdata] != $cols} {
	error "Table::appendRow: number of values in row must be $cols"
    }

    lappend table $rdata
    incr rows
    return
}

::itcl::body ::cadwidgets::Table::deleteRow {i} {
    if {![isValidRow $i]} {
	error "Table::deleteRow: i must be in the range (1, $rows) inclusive"
    }

    if {$rows < 2} {
	error "Table::deleteRow: can't delete last row"
    }

    incr i -1
    incr rows -1
    set table [lreplace $table $i $i]
    return
}

::itcl::body ::cadwidgets::Table::insertRow {i rdata} {
    if {![isValidRow $i]} {
	error "Table::insertRow: i must be in the range (1, $rows) inclusive"
    }

    if {[llength $rdata] != $cols} {
	error "Table::insertRow: number of values in row must be $cols"
    }

    incr i -1
    if {0 < $i} {
	set _i [expr {$i - 1}]
	set newtable [lrange $table 0 $_i]
    } else {
	set newtable ""
    }

    lappend newtable $rdata
    eval lappend newtable [lrange $table $i end]
    set table $newtable
    incr rows
    return
}

::itcl::body ::cadwidgets::Table::moveRow {srcIndex destIndex} {
    if {![isValidRow $srcIndex] || ![isValidRow $destIndex]} {
	error "Table::insertRow: srcIndex and destIndex must be in the range (1, $rows) inclusive"
    }

    if {$srcIndex == $destIndex} {
	# Nothing to do
	return
    }

    incr srcIndex -1
    incr destIndex -1

    set rdata [lindex $table $srcIndex]
    set newtable {}

    if {0 < $destIndex} {
	if {0 < $srcIndex} {
	    if {$srcIndex < $destIndex} {
		set i1 $srcIndex
		set i2 $destIndex

		eval lappend newtable [lrange $table 0 [expr {$i1 - 1}]]
		eval lappend newtable [lrange $table [expr {$i1 + 1}] \
					   [expr {$i2 - 1}]]
		lappend newtable $rdata
		eval lappend newtable [lrange $table $i2 end]
	    } else {
		set i1 $destIndex
		set i2 $srcIndex

		eval lappend newtable [lrange $table 0 [expr {$i1 - 1}]]
		lappend newtable $rdata
		eval lappend newtable [lrange $table $i1 \
					   [expr {$i2 - 1}]]
		eval lappend newtable [lrange $table [expr {$i2 + 1}] end]
	    }

	} else {
	    eval lappend newtable [lrange $table 1 [expr {$destIndex - 1}]]
	    lappend newtable $rdata
	    eval lappend newtable [lrange $table $destIndex end]
	}
    } else {
	lappend newtable $rdata
	eval lappend newtable [lrange $table 0 [expr {$srcIndex - 1}]]
	eval lappend newtable [lrange $table [expr {$srcIndex + 1}] end]
    }

    set table $newtable
}

::itcl::body ::cadwidgets::Table::getCol {j} {
    if {![isValidCol $j]} {
	error "Table::getCol: j must be in the range (1,$cols) inclusive"
    }

    incr j -1
    foreach row $table {
	lappend vals [lindex $row $j]
    }

    return $vals
}

::itcl::body ::cadwidgets::Table::getCols {j1 j2} {
    if {![isValidCol $j1]} {
	error "Table::getCols: j must be in the range (1,$cols) inclusive"
    }

    if {![isValidCol $j2]} {
	error "Table::getCols: j must be in the range (1,$cols) inclusive"
    }

    # quietly return
    if {$j2 < $j1} {
	return
    }

    incr j1 -1
    incr j2 -1

    # create local variables to hold each column
    foreach row $table {
	for {set j $j1} {$j <= $j2} {incr j} {
	    lappend val$j [lindex $row $j]
	}
    }

    # bundle up the cols
    for {set j $j1} {$j <= $j2} {incr j} {
	lappend vals [subst $[subst val$j]]
    }

    return $vals
}

::itcl::body ::cadwidgets::Table::setCol {j cdata} {
    if {![isValidCol $j]} {
	error "Table::setCol: j must be in the range (1,$cols) inclusive"
    }

    set len [llength $cdata]
    if {$len < $rows} {
	# pad cdata
	for {set i $len} {$i < $rows} {incr i} {
	    lappend cdata {}
	}
    } elseif {$rows < $len} {
	# truncate cdata
	set cdata [lrange $cdata 0 [expr {$rows - 1}]]
    }

    incr j -1
    set newtable ""
    foreach row $table val $cdata {
	lappend newtable [lreplace $row $j $j $val]
    }
    set table $newtable
    return
}

::itcl::body ::cadwidgets::Table::appendCol {cdata} {
    if {[llength $cdata] != $rows} {
	error "Table::appendCol: number of values in column must be $rows"
    }

    set newtable ""
    foreach row $table val $cdata {
	lappend newtable [lappend row $val]
    }
    set table $newtable
    incr cols
    return
}

::itcl::body ::cadwidgets::Table::deleteCol {j} {
    if {![isValidCol $j]} {
	error "Table::deleteCol: j must be in the range (1,$cols) inclusive"
    }

    if {$cols < 2} {
	error "Table::deleteCol: can't delete last col"
    }

    incr j -1
    incr cols -1
    set newtable ""
    foreach row $table {
	lappend newtable [lreplace $row $j $j]
    }
    set table $newtable
    return
}

::itcl::body ::cadwidgets::Table::insertCol {j cdata} {
    if {![isValidCol $j]} {
	error "Table::insertCol: j must be in the range (1,$cols) inclusive"
    }

    if {[llength $cdata] != $rows} {
	error "Table::insertCol: number of values in column must be $rows"
    }

    incr j -1
    incr cols
    set newtable ""
    if {0 < $j} {
	set _j [expr {$j - 1}]
	foreach row $table val $cdata {
	    lappend newtable "[lrange $row 0 $_j] $val [lrange $row $j end]"
	}
    } else {
	foreach row $table val $cdata {
	    lappend newtable "$val $row"
	}
    }
    set table $newtable
    return
}

::itcl::body ::cadwidgets::Table::initCol {j ival} {
    if {![isValidCol $j]} {
	error "Table::initCol: j must be in the range (1,$cols) inclusive"
    }

    incr j -1
    set newtable ""
    foreach row $table {
	lappend newtable [lreplace $row $j $j $ival]
    }
    set table $newtable
    return
}

::itcl::body ::cadwidgets::Table::initColRange {j i1 i2 ival} {
    if {![isValidCol $j]} {
	error "Table::initColRange: j must be in the range (1,$cols) inclusive"
    }

    if {![isValidRow $i1] ||
	![isValidRow $i2] ||
	$i2 < $i1} {
	error "Table::initColRange: bad values for i1($i1) or i2($i2)"
    }

    incr j -1
    incr i1 -1
    incr i2 -1
    set i 0
    set newtable ""
    foreach row $table {
	if {$i1 <= $i &&
	    $i <= $i2} {
	    lappend newtable [lreplace $row $j $j $ival]
	} else {
	    lappend newtable $row
	}
	incr i
    }
    set table $newtable
    return
}

::itcl::body ::cadwidgets::Table::isValidRow {i} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	return 0
    }

    return 1
}

::itcl::body ::cadwidgets::Table::isValidCol {j} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	return 0
    }

    return 1
}

::itcl::body ::cadwidgets::Table::getFirstEmptyRow {j} {
    if {![isValidCol $j]} {
	error "Table::getFirstEmptyRow: j must be in the range (1,$cols) inclusive"
    }

    incr j -1
    set i 1
    foreach row $table {
	set val [lindex $row $j]
	if {$val != ""} {
	    incr i
	    continue
	}

	return $i
    }

    # out of range value indicates no empty values in this column
    return 0
}

::itcl::body ::cadwidgets::Table::isData {i mark} {
    if {![isValidRow $i]} {
	error "Table::isData: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    if {[lindex [lindex [lindex $table $i] 0] 0] != $mark} {
	return 1
    }

    return 0
}

::itcl::body ::cadwidgets::Table::countData {i mark} {
    if {![isValidRow $i]} {
	error "Table::countData: i must be in the range (1,$rows) inclusive"
    }

    set ri 0
    set dcount 0
    foreach row $table {
	if {$i <= $ri} {
	    break
	}

	if {[lindex [lindex $row 0] 0] != $mark} {
	    incr dcount
	}

	incr ri
    }

    return $dcount
}

::itcl::body ::cadwidgets::Table::printColRange {chan j1 j2} {
    if {![isValidCol $j1]} {
	error "Table::printColRange: j1 must be in the range (1,$cols) inclusive"
    } else {
	incr j1 -1
    }

    if {$j2 != "end"} {
	if {![isValidCol $j2]} {
	    error "Table::printColRange: j2 must be in the range (1,$cols) inclusive"
	} else {
	    incr j2 -1
	}
    }

    foreach row $table {
	set item1 [lindex $row 0]
	if {[lindex $item1 0] != $SEP_MARK} {
	    foreach item [lrange $row $j1 $j2] {
		puts -nonewline $chan "$item\t"
	    }
	} else {
	    puts -nonewline $chan [lindex $item1 1]
	}
	puts $chan ""
    }
}

## - sortByCol
#
# Sort the table using column j
#
::itcl::body ::cadwidgets::Table::sortByCol {j type order} {
    if {![isValidCol $j]} {
	error "Table::sortByCol: j must be in the range (1,$cols) inclusive"
    }

    if {$type != "-ascii" &&
	$type != "-dictionary" &&
	$type != "-integer" &&
	$type != "-real"} {
	error "Table::sortByCol: type must be either -ascii, -dictionary, -integer or -real"
    }

    if {$order != "-increasing" &&
	$order != "-decreasing"} {
	error "Table::sortByCol: order must be either -increasing or -decreasing"
    }

    incr j -1
    set newTable {}

    # First, we wack the separator and empty rows
    foreach row $table {
	set item1 [lindex $row 0]
	if {[lindex $item1 0] != $SEP_MARK} {
	    set empty 1

	    foreach item $row {
		if {$item != {}} {
		    set empty 0
		    break
		}
	    }

	    if {$empty} {
		incr rows -1
	    } else {
		lappend newTable $row
	    }
	} else {
	    incr rows -1
	}
    }

    # sort the new table
    set table [lsort -index $j $type $order $newTable]

    return
}

## - searchCol
#
# Search for the string in the specified column
#
::itcl::body ::cadwidgets::Table::searchCol {j i str} {
    set cdata [getCol $j]
    incr i -1
    return [expr {[lsearch -start $i -regexp $cdata $str]} + 1]
}

::itcl::body ::cadwidgets::Table::deleteEmptyRows {} {
    set newTable {}

    foreach row $table {
	set empty 1

	foreach item $row {
	    if {$item != {}} {
		set empty 0
		break
	    }
	}

	if {$empty} {
	    incr rows -1
	} else {
	    lappend newTable $row
	}
    }

    set table $newTable
}

# Retain the original class, including its inheritance behavior, for scripts
# that have not yet adopted the qualified widget name.
::itcl::class ::Table {
    inherit ::cadwidgets::Table

    constructor {args} {
	::cadwidgets::Table::constructor {*}$args
    } {}
}

# SEP_MARK was directly readable from the legacy class namespace.  Keep that
# spelling linked to the canonical common variable during the transition.
# pkg_mkIndex stubs class declarations, so only create the link during a live
# Itcl load.
if {[namespace exists ::cadwidgets::Table]} {
    namespace eval ::Table {
	namespace upvar ::cadwidgets::Table SEP_MARK SEP_MARK
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
