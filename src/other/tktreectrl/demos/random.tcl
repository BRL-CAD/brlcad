# RCS: @(#) $Id$

set RandomN 500
set RandomDepth 5

#
# Demo: random N items
#
proc DemoRandom {} {

    set T [DemoList]

    InitPics folder-* small-*

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -itemheight $height -selectmode extended \
	-showroot yes -showrootbutton yes -showbuttons yes \
	-showlines [ShouldShowLines $T] \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    #
    # Create columns
    #

    $T column create -expand yes -weight 4 -text Item -itembackground {#e0e8f0 {}} \
	-tags colItem
    $T column create -text Parent -justify center -itembackground {gray90 {}} \
	-uniform a -expand yes -tags colParent
    $T column create -text Depth -justify center -itembackground {linen {}} \
	-uniform a -expand yes -tags colDepth

    $T configure -treecolumn colItem

    #
    # Create elements
    #

    $T element create elemImgFolder image -image {folder-open {open} folder-closed {}}
    $T element create elemImgFile image -image small-file
    $T element create elemTxtName text -wrap none \
	-fill [list $::SystemHighlightText {selected focus}]
    $T element create elemTxtCount text -fill blue
    $T element create elemTxtAny text
    $T element create elemRectSel rect -showfocus yes \
	-fill [list $::SystemHighlight {selected focus} gray {selected !focus}]

    #
    # Create styles using the elements
    #

    set S [$T style create styFolder]
    $T style elements $S {elemRectSel elemImgFolder elemTxtName elemTxtCount}
    $T style layout $S elemImgFolder -padx {0 4} -expand ns
    $T style layout $S elemTxtName -minwidth 12 -padx {0 4} -expand ns -squeeze x
    $T style layout $S elemTxtCount -padx {0 6} -expand ns
    $T style layout $S elemRectSel -union [list elemTxtName] -iexpand ns -ipadx 2

    set S [$T style create styFile]
    $T style elements $S {elemRectSel elemImgFile elemTxtName}
    $T style layout $S elemImgFile -padx {0 4} -expand ns
    $T style layout $S elemTxtName -minwidth 12 -padx {0 4} -expand ns -squeeze x
    $T style layout $S elemRectSel -union [list elemTxtName] -iexpand ns -ipadx 2

    set S [$T style create styAny]
    $T style elements $S {elemTxtAny}
    $T style layout $S elemTxtAny -padx 6 -expand ns

    TreeCtrl::SetSensitive $T {
	{colItem styFolder elemRectSel elemImgFolder elemTxtName}
	{colItem styFile elemRectSel elemImgFile elemTxtName}
    }
    TreeCtrl::SetDragImage $T {
	{colItem styFolder elemImgFolder elemTxtName}
	{colItem styFile elemImgFile elemTxtName}
    }

    #
    # Create items and assign styles
    #

    TimerStart
    $T item configure root -button auto
    set items [$T item create -count [expr {$::RandomN - 1}] -button auto]
    set added root
    foreach itemi $items {
	set j [expr {int(rand() * [llength $added])}]
	set itemj [lindex $added $j]
	if {[$T depth $itemj] < $::RandomDepth - 1} {
	    lappend added $itemi
	}
	if {rand() * 2 > 1} {
	    $T item collapse $itemi
	}
	if {rand() * 2 > 1} {
	    $T item lastchild $itemj $itemi
	} else {
	    $T item firstchild $itemj $itemi
	}
    }
    puts "created $::RandomN-1 items in [TimerStop] seconds"

    TimerStart
    lappend items [$T item id root]
    foreach item $items {
	set numChildren [$T item numchildren $item]
	if {$numChildren} {
	    $T item style set $item colItem styFolder colParent styAny colDepth styAny
	    $T item element configure $item \
		colItem elemTxtName -text "Item $item" + elemTxtCount -text "($numChildren)" , \
		colParent elemTxtAny -text "[$T item parent $item]" , \
		colDepth elemTxtAny -text "[$T depth $item]"
	} else {
	    $T item style set $item colItem styFile colParent styAny colDepth styAny
	    $T item element configure $item \
		colItem elemTxtName -text "Item $item" , \
		colParent elemTxtAny -text "[$T item parent $item]" , \
		colDepth elemTxtAny -text "[$T depth $item]"
	}
    }
    puts "configured $::RandomN items in [TimerStop] seconds"

    bind DemoRandom <Double-ButtonPress-1> {
	TreeCtrl::DoubleButton1 %W %x %y
	break
    }
    bind DemoRandom <Control-ButtonPress-1> {
	set TreeCtrl::Priv(selectMode) toggle
	RandomButton1 %W %x %y
	break
    }
    bind DemoRandom <Shift-ButtonPress-1> {
	set TreeCtrl::Priv(selectMode) add
	RandomButton1 %W %x %y
	break
    }
    bind DemoRandom <ButtonPress-1> {
	set TreeCtrl::Priv(selectMode) set
	RandomButton1 %W %x %y
	break
    }
    bind DemoRandom <Button1-Motion> {
	RandomMotion1 %W %x %y
	break
    }
    bind DemoRandom <ButtonRelease-1> {
	RandomRelease1 %W %x %y
	break
    }

    bindtags $T [list $T DemoRandom TreeCtrl [winfo toplevel $T] all]

    return
}

proc RandomButton1 {T x y} {
    variable TreeCtrl::Priv
    focus $T
    set id [$T identify $x $y]
    set Priv(buttonMode) ""

    # Click outside any item
    if {$id eq ""} {
	$T selection clear

    # Click in header
    } elseif {[lindex $id 0] eq "header"} {
	TreeCtrl::ButtonPress1 $T $x $y

    # Click in item
    } else {
	lassign $id where item arg1 arg2 arg3 arg4
	switch $arg1 {
	    button {
		TreeCtrl::ButtonPress1 $T $x $y
	    }
	    line {
		TreeCtrl::ButtonPress1 $T $x $y
	    }
	    column {
		if {![TreeCtrl::IsSensitive $T $x $y]} {
		    $T selection clear
		    return
		}

		set Priv(drag,motion) 0
		set Priv(drag,click,x) $x
		set Priv(drag,click,y) $y
		    set Priv(drag,x) [$T canvasx $x]
		set Priv(drag,y) [$T canvasy $y]
		set Priv(drop) ""

		if {$Priv(selectMode) eq "add"} {
		    TreeCtrl::BeginExtend $T $item
		} elseif {$Priv(selectMode) eq "toggle"} {
		    TreeCtrl::BeginToggle $T $item
		} elseif {![$T selection includes $item]} {
		    TreeCtrl::BeginSelect $T $item
		}
		$T activate $item

		if {[$T selection includes $item]} {
		    set Priv(buttonMode) drag
		}
	    }
	}
    }
    return
}

proc RandomMotion1 {T x y} {
    variable TreeCtrl::Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"drag" {
	    set Priv(autoscan,command,$T) {RandomMotion %T %x %y}
	    TreeCtrl::AutoScanCheck $T $x $y
	    RandomMotion $T $x $y
	}
	default {
	    TreeCtrl::Motion1 $T $x $y
	}
    }
    return
}

proc RandomMotion {T x y} {
    variable TreeCtrl::Priv
    switch $Priv(buttonMode) {
	"drag" {
	    if {!$Priv(drag,motion)} {
		# Detect initial mouse movement
		if {(abs($x - $Priv(drag,click,x)) <= 4) &&
		    (abs($y - $Priv(drag,click,y)) <= 4)} return

		set Priv(selection) [$T selection get]
		set Priv(drop) ""
		$T dragimage clear
		# For each selected item, add 2nd and 3rd elements of
		# column "item" to the dragimage
		foreach I $Priv(selection) {
		    foreach list $Priv(dragimage,$T) {
			set C [lindex $list 0]
			set S [lindex $list 1]
			if {[$T item style set $I $C] eq $S} {
			    eval $T dragimage add $I $C [lrange $list 2 end]
			}
		    }
		}
		set Priv(drag,motion) 1
	    }

	    # Find the item under the cursor
	    set cursor X_cursor
	    set drop ""
	    set id [$T identify $x $y]
	    if {[TreeCtrl::IsSensitive $T $x $y]} {
		set item [lindex $id 1]
		# If the item is not in the pre-drag selection
		# (i.e. not being dragged) see if we can drop on it
		if {[lsearch -exact $Priv(selection) $item] == -1} {
		    set drop $item
		    # We can drop if dragged item isn't an ancestor
		    foreach item2 $Priv(selection) {
			if {[$T item isancestor $item2 $item]} {
			    set drop ""
			    break
			}
		    }
		    if {$drop ne ""} {
			scan [$T item bbox $drop] "%d %d %d %d" x1 y1 x2 y2
			if {$y < $y1 + 3} {
			    set cursor top_side
			    set Priv(drop,pos) prevsibling
			} elseif {$y >= $y2 - 3} {
			    set cursor bottom_side
			    set Priv(drop,pos) nextsibling
			} else {
			    set cursor ""
			    set Priv(drop,pos) lastchild
			}
		    }
		}
	    }

	    if {[$T cget -cursor] ne $cursor} {
		$T configure -cursor $cursor
	    }

	    # Select the item under the cursor (if any) and deselect
	    # the previous drop-item (if any)
	    $T selection modify $drop $Priv(drop)
	    set Priv(drop) $drop

	    # Show the dragimage in its new position
	    set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
	    set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
	    $T dragimage offset $x $y
	    $T dragimage configure -visible yes
	}
	default {
	    TreeCtrl::Motion1 $T $x $y
	}
    }
    return
}

proc RandomRelease1 {T x y} {
    variable TreeCtrl::Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"drag" {
	    TreeCtrl::AutoScanCancel $T
	    $T dragimage configure -visible no
	    $T selection modify {} $Priv(drop)
	    $T configure -cursor ""
	    if {$Priv(drop) ne ""} {
		RandomDrop $T $Priv(drop) $Priv(selection) $Priv(drop,pos)
	    }
	    unset Priv(buttonMode)
	}
	default {
	    TreeCtrl::Release1 $T $x $y
	}
    }
    return
}

proc RandomDrop {T target source pos} {
    set parentList {}
    switch -- $pos {
	lastchild { set parent $target }
	prevsibling { set parent [$T item parent $target] }
	nextsibling { set parent [$T item parent $target] }
    }
    foreach item $source {

	# Ignore any item whose ancestor is also selected
	set ignore 0
	foreach ancestor [$T item ancestors $item] {
	    if {[lsearch -exact $source $ancestor] != -1} {
		set ignore 1
		break
	    }
	}
	if {$ignore} continue

	# Update the old parent of this moved item later
	if {[lsearch -exact $parentList $item] == -1} {
	    lappend parentList [$T item parent $item]
	}

	# Add to target
	$T item $pos $target $item

	# Update text: parent
	$T item element configure $item colParent elemTxtAny -text $parent

	# Update text: depth
	$T item element configure $item colDepth elemTxtAny -text [$T depth $item]

	# Recursively update text: depth
	foreach item [$T item descendants $item] {
	    $T item element configure $item colDepth elemTxtAny -text [$T depth $item]
	}
    }

    # Update items that lost some children
    foreach item $parentList {
	set numChildren [$T item numchildren $item]
	if {$numChildren == 0} {
	    $T item style map $item colItem styFile {elemTxtName elemTxtName}
	} else {
	    $T item element configure $item colItem elemTxtCount -text "($numChildren)"
	}
    }

    # Update the target that gained some children
    if {[$T item style set $parent colItem] ne "styFolder"} {
	$T item style map $parent colItem styFolder {elemTxtName elemTxtName}
    }
    set numChildren [$T item numchildren $parent]
    $T item element configure $parent colItem elemTxtCount -text "($numChildren)"
    return
}

#
# Demo: random N items, button images
#
proc DemoRandom2 {} {

    set T [DemoList]

    DemoRandom

    InitPics mac-*

    $T configure -buttonimage {mac-collapse open mac-expand {}} \
	-showlines no

    return
}

