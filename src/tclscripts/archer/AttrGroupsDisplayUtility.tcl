#                A T T R G R O U P S U T I L I T Y . T C L
# BRL-CAD
#
# Copyright (c) 2002-2009 United States Government as represented by
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
#	 This is an Archer class for displaying objects by attribute.
#

::itcl::class AttrGroupsDisplayUtility {
    inherit Utility

    constructor {_archer args} {}
    destructor {}

    public {
	# Override's for the Utility class
	common utilityMajorType $Archer::pluginMajorTypeUtility
	common utilityMinorType $Archer::pluginMinorTypeMged
	common utilityName "AttrGroupsDisplay Utility"
	common utilityVersion "1.0"
	common utilityClass AttrGroupsDisplayUtility

	common ATTRIBUTE_GROUPS_LIST "Attribute_Groups_List"
    }

    protected {
	variable archer ""
	variable archersGed
	variable statusMsg ""
	variable masterAttrGroupsList ""
	variable drawArgs ""
	variable highlightColor "255/255/255"
	variable highlightedAttr ""
	variable alistLabelVar ""
	variable currentAttr ""
	variable currentGroup ""
	variable parentBg ""
	variable attrGroupsName "Groups"
	variable status ""
	variable toplevel

	method buildExportMenu {_parent}

	method eraseDrawArgs {}
	method highlightObjectWithAttr {}
	method initLists {}
	method loadAttrList {}
	method loadAttrGroupsList {}
	method readAttrGroupsList {_file}
	method setDrawArgs {_attr _alist}
	method updateBgColor {_bg}

	method deleteCurrAttrVal {}
	method deleteCurrAttrGroup {}
	method deleteAttrFromCurrGroup {_aval}

	method loadAttrMap {}
	method readAttrMap {_file}

	method exportAttrGroups {}
	method exportAttrMap {}
	method exportToPng {}
	method save {}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body AttrGroupsDisplayUtility::constructor {_archer args} {
    global env

    set parent [winfo parent $itk_interior]
    set parentBg [$parent cget -background]

    set toplevel [winfo toplevel $itk_interior]
    wm geometry $toplevel "600x400"


    # Build toolbar
    itk_component add toolbar {
	::iwidgets::toolbar $itk_interior.toolbar \
	    -helpvariable [::itcl::scope status]
    } {}

    $itk_component(toolbar) add button load \
	-text "Load Groups" \
	-balloonstr "Load attribute groups" \
	-helpstr "Load attribute groups from a file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this loadAttrGroupsList]
    $itk_component(toolbar) add button attrmap \
	-text "Attribute Map" \
	-balloonstr "Load attributes to regions map" \
	-helpstr "Load attributes to regions map from a file" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this loadAttrMap]
    $itk_component(toolbar) add button save \
	-text "Save" \
	-balloonstr "Save attribute groups to _GLOBAL" \
	-helpstr "Save attribute groups to _GLOBAL" \
	-relief flat \
	-overrelief raised \
	-command [::itcl::code $this save]
    $itk_component(toolbar) add menubutton export \
	-text "Export" \
	-relief flat

    set parent [$itk_component(toolbar) component export]
    buildExportMenu $parent
    $parent configure \
	-menu $itk_component(exportMenu) \
	-activebackground [$parent cget -background]

    # Build scrolled lists
    itk_component add pane {
	iwidgets::panedwindow $itk_interior.pane \
	    -orient vertical
    } {}

    $itk_component(pane) add glistWindow
    $itk_component(pane) add alistWindow

    set parent [$itk_component(pane) childsite glistWindow]
    itk_component add glistFrame {
	::iwidgets::labeledframe $parent.glistF \
	    -labelvariable [::itcl::scope attrGroupsName]
    }

    set parent [$itk_component(glistFrame) childsite]
    itk_component add glist {
	::iwidgets::scrolledlistbox $parent.glistLB \
	    -selectioncommand [::itcl::code $this loadAttrList]
    } {}

    set parent [$itk_component(pane) childsite alistWindow]
    itk_component add alistFrame {
	::iwidgets::labeledframe $parent.alistF \
	    -labelvariable [::itcl::scope alistLabelVar]
    }
    set parent [$itk_component(alistFrame) childsite]
    itk_component add alist {
	::iwidgets::scrolledlistbox $parent.alistLB \
	    -selectioncommand [::itcl::code $this highlightObjectWithAttr]
    } {}

    set lb [$itk_component(alist) component listbox]
    bind $lb <Delete> [::itcl::code $this deleteCurrAttrVal]

    set lb [$itk_component(glist) component listbox]
    bind $lb <Delete> [::itcl::code $this deleteCurrAttrGroup]

    itk_component add status {
	::label $itk_interior.status \
	    -anchor w \
	    -textvariable [::itcl::scope status]
    } {}


    # Make windows visible
    pack $itk_component(glist) -expand yes -fill both
    pack $itk_component(alist) -expand yes -fill both
    pack $itk_component(glistFrame) -expand yes -fill both
    pack $itk_component(alistFrame) -expand yes -fill both

    set row 0
    grid $itk_component(toolbar) \
	-row $row \
	-sticky ew
    incr row
    grid $itk_component(pane) \
	-row $row \
	-sticky nsew
    grid rowconfigure $itk_interior $row -weight 1
    incr row
    grid $itk_component(status) \
	-row $row \
	-sticky ew

    grid columnconfigure $itk_interior 0 -weight 1


    set archer $_archer
    set archersGed [Archer::pluginGed $archer]

    # process options
    eval itk_initialize $args

    initLists

    $itk_component(glistFrame) configure -labelfont [list $::ArcherCore::SystemWindowFont 12]
    $itk_component(alistFrame) configure -labelfont [list $::ArcherCore::SystemWindowFont 12]
}

::itcl::body AttrGroupsDisplayUtility::destructor {} {
    # nothing for now
}

::itcl::body AttrGroupsDisplayUtility::buildExportMenu {_parent} {
    itk_component add exportMenu {
	::menu $_parent.exportMenu \
	    -tearoff 0
    } {}

    $itk_component(exportMenu) add command \
	-label "Attribute Groups" \
	-command [::itcl::code $this exportAttrGroups]
    $itk_component(exportMenu) add command \
	-label "Attributes Map" \
	-command [::itcl::code $this exportAttrMap]
    $itk_component(exportMenu) add command \
	-label "Png" \
	-command [::itcl::code $this exportToPng]
}

#
# Erase the objects associated with the previous attribute grouping
#
::itcl::body AttrGroupsDisplayUtility::eraseDrawArgs {} {
    if {$drawArgs != {}} {
	eval $archersGed erase -oA $drawArgs
	set drawArgs {}
    }
}

::itcl::body AttrGroupsDisplayUtility::highlightObjectWithAttr {} {
    if {$highlightedAttr != ""} {
	# Redraw using the default color
	eval $archersGed draw -m2 -A $currentAttr $highlightedAttr

	set highlightedAttr ""
    }

    set highlightedAttr [$itk_component(alist) getcurselection]
    if {$highlightedAttr != ""} {
	eval $archersGed draw -C$highlightColor -m2 -A $currentAttr $highlightedAttr
    }
}

::itcl::body AttrGroupsDisplayUtility::initLists {} {
    $itk_component(alist) clear
    $itk_component(glist) clear

    eraseDrawArgs

    if {$masterAttrGroupsList == ""} {
	# Try to get it from _GLOBAL
	if {[catch {$archersGed attr get _GLOBAL $ATTRIBUTE_GROUPS_LIST} masterAttrGroupsList]} {
	    set masterAttrGroupsList ""
	    return
	}
    }

    set masterAttrGroupsList [lsort -dictionary -index 0 $masterAttrGroupsList]

    foreach item $masterAttrGroupsList {
	$itk_component(glist) insert end [lindex $item 0]
    }
}

::itcl::body AttrGroupsDisplayUtility::loadAttrList {} {
    set cs [$itk_component(glist) getcurselection]
    if {$cs == ""} {
	return
    }
    set csindex [lsearch -index 0 $masterAttrGroupsList $cs]
    set glist [lindex $masterAttrGroupsList $csindex]
    set glen [llength $glist]
    if {$glen != 3} {
	set msg "The $ATTRIBUTE_GROUPS_LIST is not in the proper format.\nEach item should contain three elements: gname attr {val1 val2 ... valN}"
	tk_messageBox -message $msg
	return
    }

    set currentGroup $cs
    set currentAttr [lindex $glist 1]
    set alist [lindex $glist 2]
    $itk_component(alist) clear
    eval $itk_component(alist) insert end $alist

    eraseDrawArgs

    set highlightedAttr ""
    set alistLabelVar "$currentAttr's ($cs)"

    setDrawArgs $currentAttr $alist

    if {$drawArgs != ""} {
	eval $archersGed draw -m2 -oA $drawArgs
    }
}

::itcl::body AttrGroupsDisplayUtility::loadAttrGroupsList {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getOpenFile -parent $itk_interior \
		  -initialdir [$archer getLastSelectedDir] \
		  -title "Load Attribute Groups List" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    readAttrGroupsList $file
    initLists
}

#
# Group Name,Attr Name,Value1,Value2,...,ValueN
#
::itcl::body AttrGroupsDisplayUtility::readAttrGroupsList {_file} {
    set fh [open $_file r]
    set data [read $fh]
    close $fh
    set lines [split $data "\n"]

    foreach line $lines {
	# skip comments
	if {[regexp {^[ \t]*\#} $line]} {
#	    puts "skipping \"$line\""
	    continue
	}

	set line [split $line ",:"]
	set len [llength $line]
	if {$len < 3} {
#	    puts "skipping \"$line\""
	    continue
	}

	set groupName [lindex $line 0]
	set attrName [lindex $line 1]
	set vals [lrange $line 2 end]
	set vals [lsort -dictionary $vals]

	lappend attrGroupsList [list $groupName $attrName $vals]
    }

    set masterAttrGroupsList $attrGroupsList
}

::itcl::body AttrGroupsDisplayUtility::setDrawArgs {_attr _alist} {
    set drawArgs {}
    foreach ditem $_alist {
	lappend drawArgs $_attr $ditem
    }
}

::itcl::body AttrGroupsDisplayUtility::updateBgColor {_bg} {
    $itk_component(pane) configure -background $_bg
    $itk_component(glistFrame) configure -background $_bg
    $itk_component(glist) configure -background $_bg
    $itk_component(alist) configure -background $_bg
    $itk_component(alistFrame) configure -background $_bg
}

::itcl::body AttrGroupsDisplayUtility::deleteCurrAttrVal {} {
    set currSel [$itk_component(alist) curselection]
    if {$currSel == ""} {
	return
    }

    set attrVal [$itk_component(alist) get $currSel]
    deleteAttrFromCurrGroup $attrVal

    if {$drawArgs != ""} {
	eval $archersGed draw -m2 -oA $drawArgs
    }

    $itk_component(alist) delete $currSel
    $itk_component(alist) activate $currSel
    $itk_component(alist) selection set active

    set highlightedAttr ""
    highlightObjectWithAttr
}

::itcl::body AttrGroupsDisplayUtility::deleteCurrAttrGroup {} {
    set currSel [$itk_component(glist) curselection]
    if {$currSel == ""} {
	set currentGroup ""
	set currentAttr ""
	return
    }

    set gname [$itk_component(glist) get $currSel]
    set gindex [lsearch -index 0 $masterAttrGroupsList $gname]
    set masterAttrGroupsList [lreplace $masterAttrGroupsList $gindex $gindex]

    $itk_component(glist) delete $currSel
    $itk_component(glist) activate $currSel
    $itk_component(glist) selection set active

    if {$masterAttrGroupsList != ""} {
	loadAttrList
    } else {
	$itk_component(alist) clear
	eraseDrawArgs
    }
}

::itcl::body AttrGroupsDisplayUtility::deleteAttrFromCurrGroup {_aval} {
    if {$_aval == ""} {
	return
    }

    set gindex [lsearch -index 0 $masterAttrGroupsList $currentGroup]
    set glist [lindex $masterAttrGroupsList $gindex]
    set gname [lindex $glist 0]
    set attr [lindex $glist 1]
    set alist [lindex $glist 2]

    set i [lsearch $alist $_aval]
    if {$i != -1} {
	$archersGed erase -oA $attr $_aval
	
	set alist [lreplace $alist $i $i]
	set glist [list $gname $attr $alist]
	set masterAttrGroupsList [lreplace $masterAttrGroupsList $gindex $gindex $glist]
	setDrawArgs $currentAttr $alist
    }
}

::itcl::body AttrGroupsDisplayUtility::loadAttrMap {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getOpenFile -parent $itk_interior \
		  -initialdir [$archer getLastSelectedDir] \
		  -title "Load Attributes Map" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    readAttrMap $file
}

#
# ATTR1 ATTRVAL1 rid1 rid2 ... ridN
# ATTR2 ATTRVAL2 rid1 rid2 ... ridN
#
::itcl::body AttrGroupsDisplayUtility::readAttrMap {_file} {
    set fh [open $_file r]
    set data [read $fh]
    close $fh
    set lines [split $data "\n"]

    set sflag 0
    foreach line $lines {
	# skip comments
	if {[regexp {^[ \t]*\#} $line]} {
#	    puts "skipping \"$line\""
	    continue
	}

	set line [split $line ",: "]
	set len [llength $line]
	if {$len < 3} {
#	    puts "skipping \"$line\""
	    continue
	}

	set attr_name [lindex $line 0]
	set attr_val [lindex $line 1]

	set rids [lrange $line 2 end]
	foreach rid $rids {
	    set objects [$archersGed search -attr region_id=$rid]

	    foreach object $objects {
		set obj [file tail $object]

		if {[catch {$archersGed attr set $obj $attr_name $attr_val} msg]} {
		    puts $msg
		} else {
		    set sflag 1
		}
	    }
	}
    }

    if {$sflag} {
	$archer setSave
    }
}


::itcl::body AttrGroupsDisplayUtility::exportAttrGroups {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$archer getLastSelectedDir] \
		  -title "Export Attribute Groups List" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    set fh [open $file w]
    foreach glist $masterAttrGroupsList {
	set gname [lindex $glist 0]
	set attr [lindex $glist 1]
	set alist [lindex $glist 2]

	set line ""
	append line "$gname,$attr"
	foreach aval $alist {
	    append line ",$aval"
	}

	puts $fh $line
    }
    close $fh
}

::itcl::body AttrGroupsDisplayUtility::exportAttrMap {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$archer getLastSelectedDir] \
		  -title "Export Attributes Map" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    set fh [open $file w]
    foreach region [$archersGed ls -r] {
	if {[catch {$archersGed attr get $region "region_id"} rid]} {
	    continue
	}

	foreach {attr aval} [$archersGed attr get $region] {
	    # Ignore standard attributes
	    switch -- $attr {
		"aircode" -
		"inherit" -
		"material_id" -
		"los" -
		"oshader" -
		"region" -
		"region_id" -
		"rgb" {
		    # do nothing
		}
		default {
		    puts $fh "$attr,$aval,$rid"
		}
	    }
	}
    }
    close $fh
}

::itcl::body AttrGroupsDisplayUtility::exportToPng {} {
    set typelist {
	{"Text" {".png"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$archer getLastSelectedDir] \
		  -title "Export display to png." \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    wm withdraw $toplevel
    ::update
    $archer refreshDisplay
    $archersGed png $file
    wm deiconify $toplevel
}

::itcl::body AttrGroupsDisplayUtility::save {} {
    $archer attr set _GLOBAL Attribute_Groups_List $masterAttrGroupsList
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
