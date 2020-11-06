#             O V E R L A P F I L E T O O L . T C L
# BRL-CAD
#
# Copyright (c) 2018-2020 United States Government as represented by
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
#
# This is the Overlaps File tool.
#

package require Tk
package require Itcl
package require Itk
package require Iwidgets

# go ahead and blow away the class if we are reloading
catch {delete class OverlapFileTool} error

package provide OverlapFileTool 1.0

::itcl::class OverlapFileTool {
    inherit ::itk::Widget

    constructor { args } {}
    public {
	variable pairsList
	variable overlapsList
	variable firstFlag
	variable runCheckCallback

	method runTools {} {}

	method getNodeChildren { { node "" }} {}
	method selectNode { node } {}
	method manualObjs { option } {}

	method clearSelection { } { }
    }
    private {
	variable _objs
	variable _entryObjs
	variable _statusText
	variable _progressValue

	method sortPairs {} {}
	method rmDupPairs {} {}

	method addToList { new_list } {}
	method UpdateObjsList { } {}

	method removeParentMark { node } {}
	method removeChildrenMark { node } {}

	method markNode { node } {}
	method unmarkNode { { node "" } } {}
    }
}

::itcl::body OverlapFileTool::constructor { args } {
    set firstFlag false
    set _statusText "Ready"
    set _progressValue 0
    set _entryObjs ""

    itk_component add ovFrame {
	ttk::labelframe $itk_interior.ovFrame -padding 8 -text " Manually Enter The Object(s): "
    } {}
    itk_component add objectsEntry {
	tk::entry $itk_component(ovFrame).objectsEntry -textvariable [scope _entryObjs]
    } {}
    itk_component add buttonAdd {
	ttk::button $itk_component(ovFrame).buttonAdd \
	-text "Add" -padding 5 -command [code $this manualObjs add ]
    } {}
    itk_component add buttonRemove {
	ttk::button $itk_component(ovFrame).buttonRemove \
	-text "Remove" -padding 5 -command [code $this manualObjs remove ]
    } {}

    itk_component add objFrame {
	ttk::frame $itk_interior.objFrame -padding 10
    } {}
    itk_component add objectsTree {
	Hierarchy $itk_component(objFrame).objectsTree \
	    -labeltext "Double click to select/deselect objects" \
	    -querycommand [ code $this getNodeChildren %n ] \
	    -dblclickcommand [ code $this selectNode %n ] \
	    -markforeground black \
	    -markbackground yellow \
	    -visibleitems 40x20 \
	    -alwaysquery 1
    } {}
    itk_component add objectsList {
	scrolledlistbox $itk_component(objFrame).objectsList \
	    -labelpos n \
	    -dblclickcommand [code $this unmarkNode ] \
	    -visibleitems 40x18 \
	    -selectbackground yellow \
	    -selectforeground black \
	    -labeltext "Selected Items:" \
    } {}

    itk_component add ovButtonFrame {
    	ttk::frame $itk_interior.ovButtonFrame -padding 4
    } {}
    itk_component add buttonGo {
	ttk::button $itk_component(ovButtonFrame).buttonGo \
	-text "Check For Overlaps" -padding 5 -command [code $this runTools ]
    } {}
    itk_component add buttonClear {
	ttk::button $itk_component(ovButtonFrame).buttonClear \
	-text "Clear Selection" -padding 5 -command [code $this clearSelection ]
    } {}

    itk_component add progressFrame {
	ttk::frame $itk_interior.progressFrame -padding 4
    } {}
    itk_component add statusLabel {
	ttk::label $itk_component(progressFrame).statusLabel \
	-textvariable [scope _statusText] -justify center -wraplength 500
    } {}
    itk_component add progressBar {
	ttk::progressbar $itk_component(progressFrame).progressBar -variable [scope _progressValue]
    } {}

    eval itk_initialize $args

    pack $itk_component(ovFrame) -side top -fill x -pady 8 -padx 8
    pack $itk_component(objectsEntry) -side left -expand true -fill x
    pack $itk_component(buttonAdd) -side left -padx 8
    pack $itk_component(buttonRemove) -side right

    pack $itk_component(objFrame) -side top
    pack $itk_component(objectsTree) -side left -padx {0 8}
    pack $itk_component(objectsList) -side right -padx {8 0}

    pack $itk_component(ovButtonFrame) -side top
    pack $itk_component(buttonGo) -side left -padx 8
    pack $itk_component(buttonClear) -side right -padx 8

    pack $itk_component(progressFrame) -side top -expand true -fill both
    pack $itk_component(statusLabel)
    pack $itk_component(progressBar) -side left -expand true -fill x -anchor nw -pady 8 -padx 8

    set visibleObjs [who]
    foreach vobj $visibleObjs {
	if { [string index $vobj 0] ne "/" } {
	    $this markNode "/$vobj"
	} else {
	    $this markNode $vobj
	}
    }
}


###########
# begin public methods
###########

# runTools
#
# main driver that calls the commands and
# creates the overlaps file
#
body OverlapFileTool::runTools { } {
    # get _objs from list
    set _objs ""
    foreach obj [$itk_component(objectsList) get 0 end] {
	set objn [string trim $obj "/"]
	append _objs $objn
    }
    # check if user passed the objects list
    if { [llength $_objs] == 0 } {
	tk_messageBox -icon info -type ok -title "No Objects Specified" -message "Please input objects names in the objects field"
	return
    }

    # disable the go button and entry before proceeding to prevent any re-runs
    $itk_component(buttonGo) configure -state disabled
    $itk_component(objectsEntry) configure -state disabled
    $this configure -cursor watch

    # delete any previous overlaps files in the db directory
    set db_path [eval opendb]
    set dir [file dirname $db_path]
    set name [file tail $db_path]
    set ol_dir [file join $dir "${name}.ck"]
    set filename [file join $dir "${name}.ck" "ck.${name}.overlaps"]
    file delete -force -- $ol_dir

    # run overlaps check for all the specified objects
    if { [catch {exec [file join [bu_dir bin] gchecker] $db_path $_objs}] } {
	set gcmd "[file join [bu_dir bin] gchecker] $db_path $_objs"
	puts "gchecker run failed: $gcmd"
    }

    # check for the count of overlaps detected
    set fp [open $filename r]
    set ldata [read $fp]
    set ov_count [llength [split $ldata "\n"]]
    incr ov_count -1

    if { $ov_count == 0 } {
	tk_messageBox -type ok -title "No Overlaps Found" -message "No Overlaps Found"
	$itk_component(buttonGo) configure -state normal
	$itk_component(objectsEntry) configure -state normal
	$this configure -cursor ""
	set _progressValue 0
	set _statusText "Ready"
	return
    }

    puts "\nCount of overlaps: $ov_count\n"

    puts "\nOverlaps file saved: $filename"

    # run checker tool
    eval $runCheckCallback
}

# getNodeChildren is the -querycommand
#
# returns the geometry at any node in the object tree.
body OverlapFileTool::getNodeChildren { { node "" } } {
    # get a list of children for the current node.  the result in childList
    # should be a list of adorned children nodes.
    set childList ""
    if {$node == ""} {
	# process top geometry
	set topsCommand "tops -n"

	if [ catch $topsCommand roots ] {
	    puts $roots
	    return
	}

	# the children are all of the top geometry
	set childList ""
	foreach topItem $roots {
	    # XXX handle the case where tops returns decorated paths
	    if { [ llength [ split $topItem / ] ] >= 2 } {
		set topItem [ lindex [ split $topItem / ] 0 ]
	    }
	    lappend childList "/$topItem"
	}

    } else {
	# process some combination or region or primitive

	set lsNodeName [ls [ lindex [ split $node / ] end ]]
	# if region don't expand
	if { [ string compare "R" [ string index $lsNodeName end-1 ] ] == 0 } {
	    if { [ string compare "/" [ string index $lsNodeName end-2 ] ] == 0 } {
		return
	    }
	}
	set parentName [ lindex [ split $node / ] end ]

	set childrenPairs ""
	if [ catch { lt $parentName } childrenPairs ] {
	    set childrenPairs ""
	}

	foreach child $childrenPairs {
	    lappend childList "$node/[ lindex $child 1 ]"
	}
    }

    # generate the final child list including determining whether the object is a
    # primitive or a combinatorial (branch or leaf node).
    set children ""
    foreach child $childList {

	set childName [ lindex [ split $child / ] end ]

	# we do not call getObjectType for performance reasons (big directories
	# can choke.  Sides, we do not need to know the type, just whether the
	# node is a branch or not.
	if { [ catch { ls $childName } lsName ] } {
	    puts "$lsName"
	    continue
	}
	set nodeType leaf
	if { [ string compare "/" [ string index $lsName end-1 ] ] == 0 } {
	    set nodeType branch
	}

	lappend children [ list "$node/$childName" "$childName" $nodeType ]
    }
    # done iterating over children

    return $children
}

# selectNode - dblclickcommand
#
# to select/deselect the nodes in the object tree on double click
#
body OverlapFileTool::selectNode { node } {
    set nodeName [ lindex $node 1 ]
    set markedobjs [ $itk_component(objectsTree) mark get ]

    if {[lsearch $markedobjs $nodeName] >= 0} {
	$this unmarkNode $nodeName
    } else {
	$this markNode $nodeName
    }
    return
}

# manualObjs
#
# function to run the search command on the manually entered objects
# and add them to the objects list and mark them in the tree.
#
body OverlapFileTool::manualObjs { option } {
    set entrylist {}
    set badentry {}
    set searchcmd "search  / -type c -path"
    foreach object $_entryObjs {
	if { [string index $object 0] ne "/" } {
	    lappend entrylist "/$object"
	} else {
	    lappend entrylist $object
	}
    }
    foreach object $entrylist {
	set objlist [eval $searchcmd $object]
	if { $objlist == "" } {
	    lappend badentry $object
	}
	if { [llength $objlist] > 1 } {
	    set answer [tk_messageBox -title "Warning" \
		 -message "Do you want to $option these [llength $objlist] objects:" \
		 -detail "[lrange [split $objlist \n] 0 end-1]" -type okcancel -icon question]
	    if { $answer == "cancel" } { continue }
	}
	foreach searchobj $objlist {
	    if { $option eq "add" } { 
		$this markNode $searchobj
	    } else { 
		$this unmarkNode $searchobj
	    }
	}
    }
    if { [llength $badentry] > 0 } {
	tk_messageBox -icon error -type ok -title "Bad Object Names" -message "Unrecognized object names:\n$badentry"
    }
}

# clearSelection
#
# function to clear the selection made
#
body OverlapFileTool::clearSelection { } {
    $itk_component(objectsTree) mark clear
    $this UpdateObjsList
}

###########
# end public methods
###########


###########
# begin private methods
###########

# UpdateObjsList
#
# Updates the object list for any marking/unmarking of the nodes
# in the object tree.
#
body OverlapFileTool::UpdateObjsList { } {
    set markedobjs [ $itk_component(objectsTree) mark get ]
    $itk_component(objectsList) clear
    foreach obj [ lsort $markedobjs ] {
	$itk_component(objectsList) insert end $obj
    }
}

# removeParentMark
#
# Removes mark for any parent nodes recursively if a child node is marked
#
body OverlapFileTool::removeParentMark { node } {
    set full ""
    regexp {(.*)/} $node full parent
    if { $full ne "" } {
	$itk_component(objectsTree) mark remove $parent
	$this removeParentMark $parent
    } else {
	return
    }
}

# removeChildrenMark
#
# Removes mark for any child nodes recursively if a parent node is marked
#
body OverlapFileTool::removeChildrenMark { node } {
    set full ""
    regexp {(.*)/(.*)} $node full parent child
    if { $full ne "" } {
	foreach cnodes [$this getNodeChildren $child] {
	    set subchild [ lindex $cnodes 0 ]
	    $itk_component(objectsTree) mark remove $parent/$subchild
	    $this removeChildrenMark "$parent/$subchild"
	}
    } else {
	return
    }
}

# markNode
#
# marks the node in the object tree
#
body OverlapFileTool::markNode { node } {
    # if we are selecting a parent node then disable the mark of children nodes.
    $this removeChildrenMark $node
    # we also have to disable the mark of parent nodes as well
    $this removeParentMark $node

    $itk_component(objectsTree) mark add $node
    $this UpdateObjsList
}

# unmarkNode
#
# unmarks the nodes in the object tree
#
body OverlapFileTool::unmarkNode { { node "" } } {
    # if called from the scrolledlistbox get the current selection
    if {$node == ""} {
	set node [$itk_component(objectsList) getcurselection]
    }
    $itk_component(objectsTree) mark remove $node
    $this UpdateObjsList
}

# addToList
#
# averages the size values for common pairs and
# inserts to overlaps list
#
body OverlapFileTool::addToList { new_list } {
    set avg 0
    foreach pair $new_list {
	set size [lindex $pair 2]
	set avg [expr $avg + [expr $size/16]]
    }
    set common_pair [lindex $new_list 0]
    set common_pair [lreplace $common_pair 2 2 [format "%.4f" $avg]]
    lappend overlapsList $common_pair
}

# rmDupPairs
#
# removes any duplicate pairs by calling addToList
# by aggregating a list of pairs with same names
#
body OverlapFileTool::rmDupPairs { } {
    set _statusText "Removing duplicates from overlaps list"
    set new_list {}

    #take first pair
    set prev_pair [lindex $pairsList 0]
    lappend new_list $prev_pair

    foreach pair [lrange $pairsList 1 end] {
	set prev_reg "[lindex $prev_pair 0] [lindex $prev_pair 1]"
	set reg "[lindex $pair 0] [lindex $pair 1]"

	# check if pairs have same left and right region names
	if {[string compare $reg $prev_reg] == 0} {
	    lappend new_list $pair
	} else {
	    $this addToList $new_list
	    set new_list {}
	    lappend new_list $pair
	    set prev_pair $pair
	}
    }
    set _progressValue 100
    set _statusText "Done"

    #take last pairlist
    $this addToList $new_list
    puts "Unique Overlaps: [llength $overlapsList]\n"
}

# sortPairs
#
# sorts the pairs before removing duplicates
#
body OverlapFileTool::sortPairs { } {
    set _progressValue 95
    set _statusText "Sorting overlaps list"
    set pairsList [lsort $pairsList]
}


###########
# end private methods
###########

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
