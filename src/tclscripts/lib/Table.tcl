#                       T A B L E . T C L
# BRL-CAD
#
# Copyright (c) 1998-2004 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Author -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
#
# Description -
#       This class comprises a collection of methods for querying/modifying tables.
#       For the purposes of this class, a table is a list of lists with each sublist
#       being a row in the table.
#

::itcl::class Table {
    constructor {args} {}
    destructor {}

    public {
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
    }
}

::itcl::configbody Table::table {
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

::itcl::configbody Table::rows {
    error "Table: this option is read-only"
}

::itcl::configbody Table::cols {
    error "Table: this option is read-only"
}

::itcl::body Table::constructor {args} {
    # process options
    eval configure $args
}

::itcl::body Table::getEntry {i j} {
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

::itcl::body Table::setEntry {i j val} {
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

::itcl::body Table::getRow {i} {
    if {![isValidRow $i]} {
	error "Table::getRow: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    lindex $table $i
}

::itcl::body Table::getRows {i1 i2} {
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

::itcl::body Table::setRow {i rdata} {
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

::itcl::body Table::appendRow {rdata} {
    if {[llength $rdata] != $cols} {
	error "Table::appendRow: number of values in row must be $cols"
    }

    lappend table $rdata
    incr rows
    return
}

::itcl::body Table::deleteRow {i} {
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

::itcl::body Table::insertRow {i rdata} {
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

::itcl::body Table::getCol {j} {
    if {![isValidCol $j]} {
	error "Table::getCol: j must be in the range (1,$cols) inclusive"
    }

    incr j -1
    foreach row $table {
	lappend vals [lindex $row $j]
    }

    return $vals
}

::itcl::body Table::getCols {j1 j2} {
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

::itcl::body Table::setCol {j cdata} {
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

::itcl::body Table::appendCol {cdata} {
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

::itcl::body Table::deleteCol {j} {
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

::itcl::body Table::insertCol {j cdata} {
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

::itcl::body Table::initCol {j ival} {
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

::itcl::body Table::initColRange {j i1 i2 ival} {
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

::itcl::body Table::isValidRow {i} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	return 0
    }

    return 1
}

::itcl::body Table::isValidCol {j} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	return 0
    }

    return 1
}

::itcl::body Table::getFirstEmptyRow {j} {
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

::itcl::body Table::isData {i mark} {
    if {![isValidRow $i]} {
	error "Table::isData: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    if {[lindex [lindex [lindex $table $i] 0] 0] != $mark} {
	return 1
    }

    return 0
}

::itcl::body Table::countData {i mark} {
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

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
