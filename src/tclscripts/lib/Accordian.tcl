#                      A C C O R D I A N . T C L
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
# Description - This is an Itcl/Itk implementation of an Accordian widget. The
#   behavior is similar to tabs in that it displays/hides data depending on
#   which button/tab is pressed. However this widget is better suited to applications
#   where there is limited horizontal real estate (i.e. vertical situations).
#   This widget has two modes: single selection and multiple selection, which is
#   different from tabs which operate only in a single selection mode.

::itcl::class cadwidgets::Accordian {
    inherit ::itk::Widget

    itk_option define -singleselect singleselect Singleselect 1

    constructor {args} {}
    destructor {}

    public {
	method delete {_first {_last ""}}
	method insert {_index args}
	method itemChildsite {_index}
	method togglePanel {_index}
    }

    protected {
	common ACC_PREFIX "acc_win_"

	variable mItemList {}
	variable mCurrIndex -1

	method getIndex {_index}
	method resetSingleSelect {}
	method toggleDisplay {_item}
    }

    private {
    }
}

::itcl::body cadwidgets::Accordian::constructor {args} {
    grid columnconfigure $itk_interior 0 -weight 1

    eval itk_initialize $args
}

::itcl::body cadwidgets::Accordian::destructor {} {
}


############################### Configuration Options ###############################

::itcl::configbody cadwidgets::Accordian::singleselect {
    if {$itk_option(-singleselect) != "true" &&
	$itk_option(-singleselect) != "false" &&
	$itk_option(-singleselect) != 1 &&
	$itk_option(-singleselect) != 0} {
	error "bad value - $itk_option(-singleselect)"
    }

    if {$itk_option(-singleselect)} {
	resetSingleSelect
    }
}



################################# Public Methods ################################

::itcl::body cadwidgets::Accordian::delete {_first {_last ""}} {
    set i [getIndex $_first]

    if {$i == -1} {
	error "Bad index - $_first"
    }

    if {$_last != ""} {
	set j [getIndex $_last]

	if {$j == -1} {
	    error "Bad index - $_last"
	}
    } else {
	set j $i
    }

    if {$i > $j} {
	set tmp $i
	set i $j
	set j $tmp
    }

    set max [expr {[llength $mItemList] - 1}]
    if {$i > $max} {
	error "Bad index - $_first"
    }
    if {$j > $max} {
	error "Bad index - $_last"
    }

    set dlist [lrange $mItemList $i $j]
    set mItemList [lreplace $mItemList $i $j]

    # Delete the specified panel(s)
    set curr_i $i
    foreach item $dlist {
	set name "$ACC_PREFIX[regsub -all { } $item "_"]"
	grid forget $itk_component($name\F)
	rename $itk_component($name\F) ""

	incr curr_i
    }

    # Update mCurrIndex
    set di [expr {$j - $i + 1}]
    if {$i <= $mCurrIndex && $mCurrIndex <= $j} {
	set mCurrIndex -1
    } elseif {$j < $mCurrIndex} {
	incr mCurrIndex -$di
    }

    # Reconfigure a few grid settings for everything after the items
    # that were deleted (i.e. things get shifted)
    set curr_i $i
    foreach item [lrange $mItemList $i end] {
	set name "$ACC_PREFIX[regsub -all { } $item "_"]"
	grid configure $itk_component($name\F) -row $curr_i -sticky nsew

	if {[grid slaves $itk_component($name\F) -row 1] != ""} {
	    # This one is open, so reset the row weight to 1
	    grid rowconfigure $itk_interior $curr_i -weight 1
	} else {
	    grid rowconfigure $itk_interior $curr_i -weight 0
	}

	incr curr_i
    }

    # Set the row weight to 0 for the last di rows which are no longer occupied
    for {set m 0} {$m < $di} {incr m} {
	grid rowconfigure $itk_interior $curr_i -weight 0
	incr curr_i
    }
}


::itcl::body cadwidgets::Accordian::insert {_index args} {
    set i [getIndex $_index]
    if {$i == -1} {
	error "Bad index - $_index"
    }

    set nameList {}
    set max [expr {[llength $mItemList] - 1}]
    if {$i <= $max} {
	# Temporarily remove items after the insert point. They are
	# added again after the new items are inserted.
	foreach item [lrange $mItemList $i end] {
	    set name "$ACC_PREFIX[regsub -all { } $item "_"]"
	    grid remove $itk_component($name\F)
	    lappend nameList $name
	}
    }

    # If necessary, shift the grid settings for the current panel
    set di [llength $args]
    if {$i <= $mCurrIndex} {
	grid rowconfigure $itk_interior $mCurrIndex -weight 0
	incr mCurrIndex $di
	grid rowconfigure $itk_interior $mCurrIndex -weight 1
    }

    set curr_i $i
    foreach item $args {
	set name "$ACC_PREFIX[regsub -all { } $item "_"]"

	if {[lsearch $mItemList $item] != -1} {
 	    puts "Accordian::insert: $item has already been added."
	    continue
	}

	itk_component add $name\F {
	    ::ttk::frame $itk_interior.$name\F
	} {}

	itk_component add $name\B {
	    ::ttk::button $itk_component($name\F).button \
		-text $item \
		-command [::itcl::code $this toggleDisplay $item]
	} {}
	itk_component add $name\CS {
	    ::ttk::frame $itk_component($name\F).childsite
	} {}

	grid $itk_component($name\B) -row 0 -sticky nsew
	grid columnconfigure $itk_component($name\F) 0 -weight 1
	grid rowconfigure $itk_component($name\F) 1 -weight 1

	grid $itk_component($name\F) -row $curr_i -sticky nsew

	set mItemList [linsert $mItemList $curr_i $item]
	incr curr_i
    }

    foreach name $nameList {
	grid $itk_component($name\F) -row $curr_i -sticky nsew
	incr curr_i
    }
}


::itcl::body cadwidgets::Accordian::itemChildsite {_index} {
    set i [getIndex $_index]
    if {$i == -1} {
	error "Bad index - $_index"
    }

    set item [lindex $mItemList $i]
    set name "$ACC_PREFIX[regsub -all { } $item "_"]\CS"
    return $itk_component($name)
}


::itcl::body cadwidgets::Accordian::togglePanel {_index} {
    set i [getIndex $_index]

    if {$i == -1} {
	error "Bad index - $_index"
    }

    set len [llength $mItemList]
    if {$i >= $len} {
	error "Bad index - $_index"
    }

    set item [lindex $mItemList $i]
    toggleDisplay $item
}



################################# Protected Methods ################################

::itcl::body cadwidgets::Accordian::getIndex {_index} {
    set len [llength $mItemList]

    if {$_index == "end"} {
	return [expr {$len - 1}]
    }

    if {[regexp {^end(\+|-)([0-9]+)$} $_index all op n]} {
	if {$op == "-"} {
	    set i [expr {$len - $n - 1}]
	    if {$i < 0} {
		return 0
	    }

	    return $i
	}

	return $len
    }

    if {[string is digit $_index]} {
	return $_index
    }

    # _index must be a search string
    set i [lsearch $mItemList $_index]
    return $i
}


::itcl::body cadwidgets::Accordian::resetSingleSelect {} {
    # Deselect all
    set i 0
    foreach item $mItemList {
	set name "$ACC_PREFIX[regsub -all { } $item "_"]"

	if {$mCurrIndex == -1 && [grid slaves $itk_component($name\F) -row 1] != ""} {
	    set mCurrIndex $i
	}

	grid forget $itk_component($name\CS)
	grid rowconfigure $itk_interior $i -weight 0
	incr i
    }

    # Select the current panel
    if {$mCurrIndex != -1} {
	set item [lindex $mItemList $mCurrIndex]
	set name "$ACC_PREFIX[regsub -all { } $item "_"]"
	grid $itk_component($name\CS) -row 1 -stick nsew
	grid rowconfigure $itk_interior $mCurrIndex -weight 1
    }
}


::itcl::body cadwidgets::Accordian::toggleDisplay {_item} {
    set i [lsearch $mItemList $_item]
    if {$i == -1} {
	# This shouldn't happen
	return
    }

    if {$itk_option(-singleselect)} {
	if {$i != $mCurrIndex} {
	    if {$mCurrIndex != -1} {
		set item [lindex $mItemList $mCurrIndex]
		set name "$ACC_PREFIX[regsub -all { } $item "_"]"
		grid forget $itk_component($name\CS)

		grid rowconfigure $itk_interior $mCurrIndex -weight 0
	    }

	    set name "$ACC_PREFIX[regsub -all { } $_item "_"]"
	    grid $itk_component($name\CS) -row 1 -sticky nsew

	    # Update the current index
	    set mCurrIndex $i

	    grid rowconfigure $itk_interior $mCurrIndex -weight 1
	}

	return
    }

    set name "$ACC_PREFIX[regsub -all { } $_item "_"]"

    if {[grid slaves $itk_component($name\F) -row 1] == ""} {
	grid $itk_component($name\CS) -row 1 -sticky nsew
	grid rowconfigure $itk_interior $i -weight 1

	set mCurrIndex $i
    } else {
	grid forget $itk_component($name\CS)
	grid rowconfigure $itk_interior $i -weight 0

	set mCurrIndex -1
    }
}

