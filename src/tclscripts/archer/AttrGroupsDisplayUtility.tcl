#                A T T R G R O U P S U T I L I T Y . T C L
# BRL-CAD
#
# Copyright (c) 2002-2011 United States Government as represented by
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

    itk_option define -attrcolor attrColor AttrColor "255/255/0"

    constructor {_archer _handleToplevel args} {}
    destructor {}

    public {
	# Override's for the Utility class
	common utilityMajorType $Archer::pluginMajorTypeUtility
	common utilityMinorType $Archer::pluginMinorTypeMged
	common utilityName "AttrGroupsDisplay Utility"
	common utilityVersion "1.0"
	common utilityClass AttrGroupsDisplayUtility

	common ATTRIBUTE_GROUPS_LIST "Attribute_Groups_List"
	common ATTRIBUTE_EXCLUDE_LIST "Attribute_Exclude_List"

	method exportAttrGroups {}
	method exportAttrMap {}
	method exportToObj {}
	method exportToPng {}
	method getCurrentGroup {}
	method hideStatusbar {}
	method hideToolbar {}
	method highlightSelectedAttr {}
	method importAttrGroups {}
	method importAttrMap {}
	method initLists {}
	method selectHighlightedAttr {}
    }

    protected {
	variable mAlistLabelVar ""
	variable mArcher ""
	variable mArchersGed
	variable mAttrGroupsName "Groups"
	variable mCurrentAttr ""
	variable mCurrentGroup ""
	variable mDrawArgs ""
	variable mHandleToplevel 0
	variable mHighlightedAttr ""
	variable mManageRefresh 1
	variable mMasterAttrGroups ""
	variable mParentBg ""
	variable mStatus ""
	variable mToplevel ""

	variable mListForegroundDefault black
	variable mListForegroundHighlight1 blue
	variable mListForegroundHighlight2 red

	method buildExportMenu {_parent}
	method buildImportMenu {_parent}

	method deleteAttrFromCurrGroup {_aval}
	method deleteCurrAttrGroup {}
	method deleteCurrAttrVal {}

	method eraseDrawArgs {}
	method getAttrArgs {_atype _alist}
	method getAttrList {}
	method getSelectedAttr {}
	method highlightCurrentAttr {}
	method loadAttrList {}
	method readAttrGroups {_file}
	method readAttrMap {_file}
	method setDrawArgs {_atype _alist}
	method selectCurrentGroup {}
	method updateBgColor {_bg}
    }

    private {
    }
}

## - constructor
#
#
#
::itcl::body AttrGroupsDisplayUtility::constructor {_archer _handleToplevel args} {
    global env

    set mArcher $_archer
    set mHandleToplevel $_handleToplevel

    set parent [winfo parent $itk_interior]
    set mParentBg [$parent cget -background]

    # Build toolbar
    itk_component add toolbar {
	::iwidgets::toolbar $itk_interior.toolbar \
	    -helpvariable [::itcl::scope mStatus]
    } {}

    $itk_component(toolbar) add menubutton import \
	-text "Import" \
	-relief flat
    $itk_component(toolbar) add menubutton export \
	-text "Export" \
	-relief flat

    set parent [$itk_component(toolbar) component import]
    buildImportMenu $parent
    $parent configure \
	-menu $itk_component(importMenu) \
	-activebackground [$parent cget -background]

    set parent [$itk_component(toolbar) component export]
    buildExportMenu $parent
    $parent configure \
	-menu $itk_component(exportMenu) \
	-activebackground [$parent cget -background]

    itk_component add vpane {
	iwidgets::panedwindow $itk_interior.vpane \
	    -orient horizontal \
	    -thickness 5 \
	    -sashborderwidth 1 \
	    -sashcursor sb_v_double_arrow \
	    -showhandle 0 \
	    -background $ArcherCore::LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(vpane) add topWindow

    set parent [$itk_component(vpane) childsite topWindow]
    itk_component add hpane {
	iwidgets::panedwindow $parent.hpane \
	    -orient vertical \
	    -thickness 5 \
	    -sashborderwidth 1 \
	    -sashcursor sb_h_double_arrow \
	    -showhandle 0 \
	    -background $ArcherCore::LABEL_BACKGROUND_COLOR
    } {}

    $itk_component(hpane) add glistWindow
    $itk_component(hpane) add alistWindow

    set parent [$itk_component(hpane) childsite glistWindow]
    itk_component add glistFrame {
	::iwidgets::labeledframe $parent.glistF \
	    -labelvariable [::itcl::scope mAttrGroupsName]
    }

    set parent [$itk_component(glistFrame) childsite]
    itk_component add glist {
	::iwidgets::scrolledlistbox $parent.glistLB \
	    -selectioncommand [::itcl::code $this loadAttrList]
    } {}

    set parent [$itk_component(hpane) childsite alistWindow]
    itk_component add alistFrame {
	::iwidgets::labeledframe $parent.alistF \
	    -labelvariable [::itcl::scope mAlistLabelVar]
    }
    set parent [$itk_component(alistFrame) childsite]
    itk_component add alist {
	::iwidgets::scrolledlistbox $parent.alistLB \
	    -selectioncommand [::itcl::code $this highlightSelectedAttr]
    } {}

    set lb [$itk_component(alist) component listbox]
    bind $lb <Delete> [::itcl::code $this deleteCurrAttrVal]

    set lb [$itk_component(glist) component listbox]
    bind $lb <Delete> [::itcl::code $this deleteCurrAttrGroup]

    itk_component add status {
	::label $itk_interior.status \
	    -anchor w \
	    -textvariable [::itcl::scope mStatus]
    } {}

    # Make windows visible
    pack $itk_component(hpane) -expand yes -fill both
    pack $itk_component(glist) -expand yes -fill both
    pack $itk_component(alist) -expand yes -fill both
    pack $itk_component(glistFrame) -expand yes -fill both
    pack $itk_component(alistFrame) -expand yes -fill both

    set row 0
    grid $itk_component(toolbar) \
	-row $row \
	-sticky ew
    incr row
    grid $itk_component(vpane) \
	-row $row \
	-sticky nsew
    grid rowconfigure $itk_interior $row -weight 1
    incr row
    grid $itk_component(status) \
	-row $row \
	-sticky ew

    grid columnconfigure $itk_interior 0 -weight 1

    # process options
    eval itk_initialize $args

    initLists

    $itk_component(glistFrame) configure -labelfont [list $::ArcherCore::SystemWindowFont 12]
    $itk_component(alistFrame) configure -labelfont [list $::ArcherCore::SystemWindowFont 12]

    if {$mHandleToplevel} {
	set mToplevel [winfo toplevel $itk_interior]
	wm geometry $mToplevel "400x300"
    }
}

::itcl::body AttrGroupsDisplayUtility::destructor {} {
    # nothing for now
}

::itcl::configbody AttrGroupsDisplayUtility::attrcolor {
    if {$itk_option(-attrcolor) != ""} {
	highlightCurrentAttr
    }
}

################################### Public Section ###################################
::itcl::body AttrGroupsDisplayUtility::exportAttrGroups {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Export Attribute Groups List" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    set fh [open $file w]
    foreach glist $mMasterAttrGroups {
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
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Export Attributes Map" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    set lines {}

    # First output _GLOBAL attributes
    foreach {attr aval} [$mArchersGed attr get _GLOBAL] {
	# Ignore standard attributes
	switch -- $attr {
	    "title" -
	    "units" -
	    "regionid_colortable" {
		    # do nothing
	    }
	    default {
		lappend lines "$attr,$aval,_GLOBAL"
	    }
	}
    }

    # Now handle region attributes
    set fh [open $file w]
    foreach region [$mArchersGed ls -r] {
	if {[catch {$mArchersGed attr get $region "region_id"} rid]} {
	    continue
	}

	foreach {attr aval} [$mArchersGed attr get $region] {
	    # Ignore standard attributes
	    switch -- $attr {
		"aircode" -
		"inherit" -
		"material_id" -
		"los" -
		"shader" -
		"region" -
		"region_id" -
		"rgb" {
		    # do nothing
		}
		default {
		    lappend lines "$attr,$aval,$rid"
		}
	    }
	}
    }

    foreach line [lsort -unique -dictionary $lines] {
	puts $fh $line
    }
    close $fh
}

::itcl::body AttrGroupsDisplayUtility::exportToObj {} {
    set typelist {
	{"Text" {".obj"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Export display to OBJ (Wavefront)." \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    $mArchersGed dbot_dump -o $file -t obj
}

::itcl::body AttrGroupsDisplayUtility::exportToPng {} {
    set typelist {
	{"PNG" {".png"}}
	{"All Files" {*}}
    }

    set file [tk_getSaveFile -parent $itk_interior \
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Export display to png." \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    if {$mHandleToplevel} {
	wm withdraw $mToplevel
    }

    #XXX Hack to get around the dialog window (i.e. so it doesn't show up in the image)
    # Remove this hack when png is updated to draw to and pull from an in-memory buffer
    ::update idletasks
    after 1000
    $mArcher refreshDisplay

    $mArchersGed png $file

    if {$mHandleToplevel} {
	wm deiconify $mToplevel
    }
}

::itcl::body AttrGroupsDisplayUtility::getCurrentGroup {} {
    return $mCurrentGroup
}

::itcl::body AttrGroupsDisplayUtility::hideStatusbar {} {
    grid forget $itk_component(status)
}

::itcl::body AttrGroupsDisplayUtility::hideToolbar {} {
    grid forget $itk_component(toolbar)
}

::itcl::body AttrGroupsDisplayUtility::highlightSelectedAttr {} {
    if {$mManageRefresh} {
	$mArchersGed refresh_off
    }

    set alist [getAttrList]

    if {$mHighlightedAttr != ""} {
	set i [lsearch $alist $mHighlightedAttr]
        if {$i != -1} {
            $itk_component(alist) itemconfigure $i -background white
        }

	set rlist [getAttrArgs $mCurrentAttr $mHighlightedAttr]
	set slist [lindex $rlist 0]
	set slen [llength $slist]

	# Redraw using the default color
	if {$slen} {
	    eval $mArchersGed draw -m1 $slist
	}

	set mHighlightedAttr ""
    }

    set mHighlightedAttr [getSelectedAttr]
    if {$mHighlightedAttr != ""} {
	set i [lsearch $alist $mHighlightedAttr]
        if {$i != -1} {
            $itk_component(alist) itemconfigure $i -background \#c3c3c3
        }
    }
    set rlist [highlightCurrentAttr]
    if {$rlist != {}} {
	if {$mManageRefresh} {
	    $mArchersGed refresh_on
	    $mArchersGed refresh_all
	}

	return $rlist
    }

    if {$mManageRefresh} {
	$mArchersGed refresh_on
	$mArchersGed refresh_all
    }

    return {}
}

::itcl::body AttrGroupsDisplayUtility::importAttrGroups {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getOpenFile -parent $itk_interior \
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Import Attribute Groups List" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    readAttrGroups $file
}

::itcl::body AttrGroupsDisplayUtility::importAttrMap {} {
    set typelist {
	{"Text" {".txt"}}
	{"All Files" {*}}
    }

    set file [tk_getOpenFile -parent $itk_interior \
		  -initialdir [$mArcher getLastSelectedDir] \
		  -title "Import Attributes Map" \
		  -filetypes $typelist]

    if {$file == ""} {
	return
    }

    readAttrMap $file
}

::itcl::body AttrGroupsDisplayUtility::initLists {} {
    set mArchersGed [Archer::pluginGed $mArcher]

    $itk_component(alist) clear
    $itk_component(glist) clear

    set mCurrentGroup ""
    set mCurrentAttr ""
    set mCurrentHighlightedAttr ""

    eraseDrawArgs

    if {$mMasterAttrGroups == ""} {
	# Try to get it from _GLOBAL
	if {[catch {$mArchersGed attr get _GLOBAL $ATTRIBUTE_GROUPS_LIST} mMasterAttrGroups]} {
	    set mMasterAttrGroups ""
	    return
	}
    }

    set mMasterAttrGroups [lsort -dictionary -index 0 $mMasterAttrGroups]

    foreach item $mMasterAttrGroups {
	$itk_component(glist) insert end [lindex $item 0]
    }
}

::itcl::body AttrGroupsDisplayUtility::selectHighlightedAttr {} {
    if {$mHighlightedAttr == ""} {
	return
    }

    if {[catch {$itk_component(alist) index $mHighlightedAttr} cindex]} {
	set mHighlightedAttr ""
	return
    }

    $itk_component(alist) activate $cindex
    $itk_component(alist) selection set active
}


################################### End Public Section ###################################


################################### Protected Section ###################################
################################### End Protected Section ###################################
::itcl::body AttrGroupsDisplayUtility::buildExportMenu {_parent} {
    itk_component add exportMenu {
	::menu $_parent.exportMenu \
	    -tearoff 0
    } {}

    $itk_component(exportMenu) add command \
	-label "Attribute Groups" \
	-command [::itcl::code $this exportAttrGroups]
    $itk_component(exportMenu) add command \
	-label "Attribute Mappings" \
	-command [::itcl::code $this exportAttrMap]
    $itk_component(exportMenu) add command \
	-label "Png" \
	-command [::itcl::code $this exportToPng]
}

::itcl::body AttrGroupsDisplayUtility::buildImportMenu {_parent} {
    itk_component add importMenu {
	::menu $_parent.importMenu \
	    -tearoff 0
    } {}

    $itk_component(importMenu) add command \
	-label "Attribute Groups" \
	-command [::itcl::code $this importAttrGroups]
    $itk_component(importMenu) add command \
	-label "Attribute Mappings" \
	-command [::itcl::code $this importAttrMap]
}

::itcl::body AttrGroupsDisplayUtility::deleteAttrFromCurrGroup {_aval} {
    if {$_aval == ""} {
	return
    }

    set gindex [lsearch -index 0 $mMasterAttrGroups $mCurrentGroup]
    set glist [lindex $mMasterAttrGroups $gindex]
    set gname [lindex $glist 0]
    set attr [lindex $glist 1]
    set alist [lindex $glist 2]

    set i [lsearch $alist $_aval]
    if {$i != -1} {
	$mArchersGed erase -oA $attr $_aval
	
	set alist [lreplace $alist $i $i]
	set glist [list $gname $attr $alist]
	set mMasterAttrGroups [lreplace $mMasterAttrGroups $gindex $gindex $glist]
	setDrawArgs $mCurrentAttr $alist
    }
}

::itcl::body AttrGroupsDisplayUtility::deleteCurrAttrGroup {} {
    set currSel [$itk_component(glist) curselection]
    if {$currSel == ""} {
	set mCurrentGroup ""
	set mCurrentAttr ""
	return
    }

    set gname [$itk_component(glist) get $currSel]
    set gindex [lsearch -index 0 $mMasterAttrGroups $gname]
    set mMasterAttrGroups [lreplace $mMasterAttrGroups $gindex $gindex]

    $itk_component(glist) delete $currSel
    $itk_component(glist) activate $currSel
    $itk_component(glist) selection set active

    if {$mMasterAttrGroups != ""} {
	loadAttrList
    } else {
	$itk_component(alist) clear
	eraseDrawArgs
    }
}

::itcl::body AttrGroupsDisplayUtility::deleteCurrAttrVal {} {
#    set currSel [$itk_component(alist) curselection]
#    if {$currSel == ""} {
#	return
#    }

#    set attrVal [$itk_component(alist) get $currSel]
#    deleteAttrFromCurrGroup $attrVal

    if {$mHighlightedAttr == ""} {
	return
    }
    set cindex [$itk_component(alist) index $mHighlightedAttr]
    deleteAttrFromCurrGroup $mHighlightedAttr

    if {$mDrawArgs != ""} {
	eval $mArchersGed draw -m1 $mDrawArgs
    }

    $itk_component(alist) delete $cindex
    $itk_component(alist) activate $cindex
    $itk_component(alist) selection set active

    set mHighlightedAttr ""
    highlightSelectedAttr
}

#
# Erase the objects associated with the previous attribute grouping
#
::itcl::body AttrGroupsDisplayUtility::eraseDrawArgs {} {
    if {$mDrawArgs != {}} {
	eval $mArchersGed erase $mDrawArgs
	set mDrawArgs {}
    }
}

::itcl::body AttrGroupsDisplayUtility::getAttrArgs {_atype _alist} {
    if {$_alist == ""} {
	return
    }

    set aspec {}
    foreach item $_alist {
	lappend aspec $_atype $item
    }

    set rlist [eval $mArchersGed ls -oA $aspec]
    set rlist [regsub -all {(/)|(/R)|(\n)} $rlist " "]
    set rlist [lsort -dictionary -unique $rlist]

    set slist {}
    set hlist {}
    foreach r $rlist {
	if {[catch {$mArchersGed attr get $r $ATTRIBUTE_EXCLUDE_LIST} hfil]} {
	    lappend slist $r
	    continue
	}

	set i [lsearch $hfil $mCurrentGroup]
	if {$i != -1} {
	    lappend hlist $r
	} else {
	    lappend slist $r
	}
    }

    return [list $slist $hlist]
}

::itcl::body AttrGroupsDisplayUtility::getAttrList {} {
    return [$itk_component(alist) get 0 end]
}

::itcl::body AttrGroupsDisplayUtility::getSelectedAttr {} {
    return [$itk_component(alist) getcurselection]
}

::itcl::body AttrGroupsDisplayUtility::highlightCurrentAttr {} {
    if {$mHighlightedAttr != ""} {
	set rlist [getAttrArgs $mCurrentAttr $mHighlightedAttr]
	set slist [lindex $rlist 0]
	set slen [llength $slist]

	if {$slen} {
	    eval $mArchersGed draw -C$itk_option(-attrcolor) -m1 $slist
	}

	if {$mManageRefresh} {
	    $mArchersGed refresh_on
	    $mArchersGed refresh_all
	}

	return $rlist
    }

    return {}
}

::itcl::body AttrGroupsDisplayUtility::loadAttrList {} {
    set cs [$itk_component(glist) getcurselection]
    if {$cs == ""} {
	return
    }

    set csindex [lsearch -index 0 $mMasterAttrGroups $cs]
    if {$csindex == -1} {
        return
    }

    set glist [lindex $mMasterAttrGroups $csindex]
    set glen [llength $glist]
    if {$glen != 3} {
	set msg "The $ATTRIBUTE_GROUPS_LIST is not in the proper format.\nEach item should contain three elements: gname attr {val1 val2 ... valN}"
	tk_messageBox -message $msg
	return
    }

    if {$mManageRefresh} {
	$mArchersGed refresh_off
    }

    if {$mCurrentGroup != ""} {
        set i [lsearch -index 0 $mMasterAttrGroups $mCurrentGroup]
        if {$i != -1} {
            $itk_component(glist) itemconfigure $i -background white
        }
    }
    $itk_component(glist) itemconfigure $csindex -background \#c3c3c3

    set mCurrentGroup $cs
    set mCurrentAttr [lindex $glist 1]
    set alist [lindex $glist 2]
    $itk_component(alist) clear
    eval $itk_component(alist) insert end $alist

    eraseDrawArgs

    set mHighlightedAttr ""
    set mAlistLabelVar "$mCurrentAttr's ($cs)"

    setDrawArgs $mCurrentAttr $alist

    if {$mDrawArgs != ""} {
	eval $mArchersGed draw -m1 $mDrawArgs
    }

    if {$mManageRefresh} {
	$mArchersGed refresh_on
	$mArchersGed refresh_all
    }
}

#
# Group Name,Attr Name,Value1,Value2,...,ValueN
#
::itcl::body AttrGroupsDisplayUtility::readAttrGroups {_file} {
    set fh [open $_file r]
    set data [read $fh]
    close $fh
    set lines [split $data "\n"]

    foreach line $lines {
	# skip blank lines
	if {[regexp {^[ \t]*$} $line]} {
	    continue
	}

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

	lappend attrGroups [list $groupName $attrName $vals]
    }

    set mMasterAttrGroups $attrGroups
    $mArcher attr set _GLOBAL $ATTRIBUTE_GROUPS_LIST $mMasterAttrGroups
    initLists
}

#
# ATTR1,ATTRVAL1,rid1,rid2, ... ridN
# ATTR2,ATTRVAL2,rid1,rid2, ... ridN
#
::itcl::body AttrGroupsDisplayUtility::readAttrMap {_file} {
    set fh [open $_file r]
    set data [read $fh]
    close $fh
    set lines [lsort -unique -dictionary [split $data "\n"]]

    # Build associative array indexed by rid, values are lists of regions
    # whose region ids equal rid
    set rmap(-1) ""
    foreach region [$mArchersGed ls -r] {
	if {[catch {$mArchersGed attr get $region "region_id"} rid]} {
	    continue
	}

	lappend rmap($rid) $region
    }    

    set sflag 0
    foreach line $lines {
	# skip blank lines
	if {[regexp {^[ \t]*$} $line]} {
	    continue
	}

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

	set attr_name [lindex $line 0]
	set attr_val [lindex $line 1]

	set rids [lrange $line 2 end]
	foreach rid $rids {
	    # Using search here is a huge bottle neck (i.e. extremely slow)
	    #set objects [$mArchersGed search -attr region_id=$rid]

	    # This is also extremely slow
	    #set objects [$mArchersGed ls -A region_id $rid]
	    #set objects [regsub -all {(/R)|(\n)} $objects " "]

	    # Again, extremely slow
	    #set objects [$mArchersGed whichid -s $rid]

	    # Ahh, much better. All of the previous cases search the entire database.
	    # This version uses an associative array.
	    if {[string is digit $rid]} {
		if {[catch {set rmap($rid)} objects]} {
		    continue
		}
	    } else {
		# Must be a regular object name
		set objects $rid
	    }
#	    if {$rid == "_GLOBAL"} {
#		set objects "_GLOBAL"
#	    } else {
#		if {[catch {set rmap($rid)} objects]} {
#		    continue
#		}
#	    }

	    foreach object $objects {
		set obj [file tail $object]

		if {[catch {$mArchersGed attr set $obj $attr_name $attr_val} msg]} {
		    $mArcher putString $msg
		} else {
		    set sflag 1
		}
	    }
	}
    }

    if {[catch {$mArchersGed attr get _GLOBAL $ATTRIBUTE_GROUPS_LIST} mMasterAttrGroups]} {
	set mMasterAttrGroups ""
    }
    initLists

    if {$sflag} {
	$mArcher setSave
    }
}

::itcl::body AttrGroupsDisplayUtility::setDrawArgs {_atype _alist} {
    set rlist [getAttrArgs $_atype $_alist]
    set mDrawArgs [lindex $rlist 0]
}

::itcl::body AttrGroupsDisplayUtility::selectCurrentGroup {} {
    if {$mCurrentGroup == ""} {
	return
    }

    if {[catch {$itk_component(glist) index $mCurrentGroup} cindex]} {
	set mCurrentGroup ""
	set mCurrentAttr ""
	return
    }

    $itk_component(glist) activate $cindex
    $itk_component(glist) selection set active
}

::itcl::body AttrGroupsDisplayUtility::updateBgColor {_bg} {
    $itk_component(hpane) configure -background $_bg
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
