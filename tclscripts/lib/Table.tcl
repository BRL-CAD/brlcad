##                 T A B L E . T C L
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
#       This class comprises a collection of methods for querying/modifying tables.
#       For the purposes of this class, tables are lists of lists with each sublist
#       being a row in the table.
#

itcl::class Table {
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
	method setRow {i row}
	method appendRow {row}
	method deleteRow {i}
	method insertRow {i row}

	# methods that operate on columns
	method getCol {j}
	method getCols {j1 j2}
	method setCol {j col}
	method appendCol {col}
	method deleteCol {j}
	method insertCol {j col}
    }
}

itcl::configbody Table::table {
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

itcl::configbody Table::rows {
    error "Table: this option is read-only"
}

itcl::configbody Table::cols {
    error "Table: this option is read-only"
}

itcl::body Table::constructor {args} {
    # process options
    eval configure $args
}

itcl::body Table::getEntry {i j} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "Table::getEntry: i must be in the range (1,$rows) inclusive"
    }

    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "Table::getEntry: j must be in the range (1,$cols) inclusive"
    }

    incr i -1
    incr j -1
    lindex [lindex $table $i] $j
}

itcl::body Table::setEntry {i j val} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "Table::setEntry: i must be in the range (1,$rows) inclusive"
    }

    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "Table::setEntry: j must be in the range (1,$cols) inclusive"
    }

    incr i -1
    incr j -1
    set table [lreplace $table $i $i [lreplace [lindex $table $i] $j $j $val]]
    return
}

itcl::body Table::getRow {i} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "Table::getRow: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    lindex $table $i
}

itcl::body Table::getRows {i1 i2} {
    if {![string is digit $i1] || $i1 < 1 || $rows < $i1} {
	error "Table::getRows: i1 must be in the range (1,$rows) inclusive"
    }

    if {![string is digit $i2] || $i2 < 1 || $rows < $i2} {
	error "Table::getRows: i2 must be in the range (1,$rows) inclusive"
    }

    incr i1 -1
    incr i2 -1
    lrange $table $i1 $i2
}

itcl::body Table::setRow {i row} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "Table::setRow: i must be in the range (1,$rows) inclusive"
    }

    incr i -1
    if {[llength $row] != $cols} {
	error "Table::setRow: number of values in row must be $cols"
    }

    set table [lreplace $table $i $i $row]
    return
}

itcl::body Table::appendRow {row} {
    if {[llength $row] != $cols} {
	error "Table::appendRow: number of values in row must be $cols"
    }

    lappend table $row
    incr rows
    return
}

itcl::body Table::deleteRow {i} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
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

itcl::body Table::insertRow {i row} {
    if {![string is digit $i] || $i < 1 || $rows < $i} {
	error "Table::insertRow: i must be in the range (1, $rows) inclusive"
    }

    if {[llength $row] != $cols} {
	error "Table::insertRow: number of values in row must be $cols"
    }
	
    incr i -1
    if {0 < $i} {
	set _i [expr {$i - 1}]
	set newtable [lrange $table 0 $_i]
    } else {
	set newtable ""
    }

    lappend newtable $row
    eval lappend newtable [lrange $table $i end]
    set table $newtable
    incr rows
    return
}

itcl::body Table::getCol {j} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "Table::getCol: j must be in the range (1,$cols) inclusive"
    }

    incr j -1
    foreach row $table {
	lappend vals [lindex $row $j]
    }

    return $vals
}

itcl::body Table::getCols {j1 j2} {
    if {![string is digit $j1] || $j1 < 1 || $cols < $j1} {
	error "Table::getCols: j must be in the range (1,$cols) inclusive"
    }

    if {![string is digit $j2] || $j2 < 1 || $cols < $j2} {
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

itcl::body Table::setCol {j col} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "Table::setCol: j must be in the range (1,$cols) inclusive"
    }

    if {[llength $col] != $rows} {
	error "Table::setCol: number of values in column must be $rows"
    }

    incr j -1
    set newtable ""
    foreach row $table val $col {
	lappend newtable [lreplace $row $j $j $val]
    }
    set table $newtable
    return
}

itcl::body Table::appendCol {col} {
    if {[llength $col] != $rows} {
	error "Table::appendCol: number of values in column must be $rows"
    }

    set newtable ""
    foreach row $table val $col {
	lappend newtable [lappend row $val]
    }
    set table $newtable
    incr cols
    return
}

itcl::body Table::deleteCol {j} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
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

itcl::body Table::insertCol {j col} {
    if {![string is digit $j] || $j < 1 || $cols < $j} {
	error "Table::insertCol: j must be in the range (1,$cols) inclusive"
    }

    if {[llength $col] != $rows} {
	error "Table::insertCol: number of values in column must be $rows"
    }

    incr j -1
    incr cols
    set newtable ""
    if {0 < $j} {
	set _j [expr {$j - 1}]
	foreach row $table val $col {
	    lappend newtable "[lrange $row 0 $_j] $val [lrange $row $j end]"
	}
    } else {
	foreach row $table val $col {
	    lappend newtable "$val $row"
	}
    }
    set table $newtable
    return
}
