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
	variable masterAttrGroupsList {}
	variable drawArgs {}
	variable highlightColor "255/255/255"
	variable highlightedAttr ""
	variable alistLabelVar ""
	variable currentAttr ""
	variable parentBg ""
	variable attrGroupsName "Groups"

	method highlightObjectWithAttr {}
	method initLists {}
	method loadAttrList {}
	method updateBgColor {_bg}
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

    pack $itk_component(glist) -expand yes -fill both
    pack $itk_component(alist) -expand yes -fill both
    pack $itk_component(glistFrame) -expand yes -fill both
    pack $itk_component(alistFrame) -expand yes -fill both
    pack $itk_component(pane) -expand yes -fill both

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

::itcl::body AttrGroupsDisplayUtility::highlightObjectWithAttr {} {
    if {$highlightedAttr != ""} {
	# Redraw using the default color
	eval $archersGed draw -m2 -A $currentAttr $highlightedAttr

	set highlightedAttr ""
    }

    set highlightedAttr [$itk_component(alist) getcurselection]
    eval $archersGed draw -C$highlightColor -m2 -A $currentAttr $highlightedAttr
}

::itcl::body AttrGroupsDisplayUtility::initLists {} {
    if {[catch {$archersGed attr get _GLOBAL $ATTRIBUTE_GROUPS_LIST} masterAttrGroupsList]} {
	return
    }

    set masterAttrGroupsList [lsort -dictionary -index 0 $masterAttrGroupsList]

    foreach item $masterAttrGroupsList {
	$itk_component(glist) insert end [lindex $item 0]
    }
}

::itcl::body AttrGroupsDisplayUtility::loadAttrList {} {
    set cs [$itk_component(glist) getcurselection]
    set csindex [lsearch -index 0 $masterAttrGroupsList $cs]
    set glist [lindex $masterAttrGroupsList $csindex]

    set glen [llength $glist]
    if {$glen != 3} {
	set msg "The $ATTRIBUTE_GROUPS_LIST is not in the proper format.\nEach item should contain three elements: gname attr {val1 val2 ... valN}"
	tk_messageBox -message $msg
	return
    }

    set currentAttr  [lindex $glist 1]
    set alist [lindex $glist 2]
    $itk_component(alist) clear
    eval $itk_component(alist) insert end $alist

    # Erase the objects associated with the previous attribute grouping
    if {$drawArgs != {}} {
	eval $archersGed erase -oA $drawArgs
	set drawArgs {}
    }

    set highlightedAttr ""
    set alistLabelVar "$currentAttr's ($cs)"

    foreach ditem $alist {
	lappend drawArgs $currentAttr $ditem
    }

    eval $archersGed draw -m2 -oA $drawArgs
}

::itcl::body AttrGroupsDisplayUtility::updateBgColor {_bg} {
    $itk_component(pane) configure -background $_bg
    $itk_component(glistFrame) configure -background $_bg
    $itk_component(glist) configure -background $_bg
    $itk_component(alist) configure -background $_bg
    $itk_component(alistFrame) configure -background $_bg
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
