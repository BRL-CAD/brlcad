
if { $::tcl_platform(platform) eq "windows" } {
    event add <<ContextualPress>> <ButtonPress-3>
    event add <<Contextual>> <ButtonRelease-3>
    event add <<Contextual>> <App>
} elseif { $::tcl_platform(os) eq "Darwin" } {
    event add <<ContextualPress>> <ButtonPress-2>
    event add <<Contextual>> <ButtonRelease-2>
} else {
    event add <<ContextualPress>> <ButtonPress-3>
    event add <<Contextual>> <ButtonRelease-3>
}

bind FullTreeCtrl <Double-ButtonPress-1> {
    TreeCtrl::FullTkTreeEditCancel %W
    TreeCtrl::DoubleButton1 %W %x %y
    break
}
bind FullTreeCtrl <Control-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) toggle
    TreeCtrl::FullTkTreeButton1 %W %x %y
    break
}
bind FullTreeCtrl <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) add
    TreeCtrl::FullTkTreeButton1 %W %x %y
    break
}
bind FullTreeCtrl <ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) set
    TreeCtrl::FullTkTreeButton1 %W %x %y
    break
}
bind FullTreeCtrl <Button1-Motion> {
    TreeCtrl::FullTkTreeMotion1 %W %x %y
    break
}
bind FullTreeCtrl <Button1-Leave> {
    TreeCtrl::FullTkTreeLeave1 %W %x %y
    break
}
bind FullTreeCtrl <ButtonRelease-1> {
    TreeCtrl::FullTkTreeRelease1 %W %x %y
}

# MouseWheel
if {[string equal "x11" [tk windowingsystem]]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #        http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind FullTreeCtrl <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -3 units
	}
	break
    }
    bind FullTreeCtrl <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 3 units
	}
	break
    }
} elseif {[string equal [tk windowingsystem] "aqua"]} {
    bind FullTreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D)}] units
	break
    }
} else {
    bind FullTreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D / 120) * 3}] units
	break
    }
}


namespace eval TreeCtrl {
    variable Priv
    set Priv(edit,delay) 500
}

proc ::TreeCtrl::FullTkTreeButton1 {T x y} {
    variable Priv
    focus $T
    set id [$T identify $x $y]
    set marquee 0
    set Priv(buttonMode) ""
    if {[winfo exists $T.entry] && [winfo ismapped $T.entry]} {
	EntryClose $T 1
    }
    if {[winfo exists $T.text] && [winfo ismapped $T.text]} {
	TextClose $T 1
    }
    FullTkTreeEditCancel $T
    # Click outside any item
    if {$id eq ""} {
	set marquee 1

	# Click in header
    } elseif {[lindex $id 0] eq "header"} {
	ButtonPress1 $T $x $y

	# Click in item
    } else {
	foreach {where item arg1 arg2 arg3 arg4} $id {}
	switch $arg1 {
	    button -
	    line {
		ButtonPress1 $T $x $y
	    }
	    column {
		set ok 0
		# Clicked an element
		if {[llength $id] == 6} {
		    set E [lindex $id 5]
		    foreach list $Priv(sensitive,$T) {
		        set C [lindex $list 0]
		        set S [lindex $list 1]
		        set eList [lrange $list 2 end]
		        if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		            if {[$T column compare $arg2 != $C]} continue
		        } else {
		            if { $arg2 != $C } continue
		        }
		        if {[$T item style set $item $C] ne $S} continue
		        if {[lsearch -exact $eList $E] == -1} continue
		        set ok 1
		        break
		    }
		}
		if {$ok} {
		    set Priv(drag,motion) 0
		    set Priv(drag,click,x) $x
		    set Priv(drag,click,y) $y
		    set Priv(drag,x) [$T canvasx $x]
		    set Priv(drag,y) [$T canvasy $y]
		    set Priv(drop) ""
		    set Priv(drag,wasSel) [$T selection includes $item]
		    set Priv(drag,C) $C
		    set Priv(drag,E) $E
		    $T activate $item
		    if {$Priv(selectMode) eq "add"} {
		        BeginExtend $T $item
		    } elseif {$Priv(selectMode) eq "toggle"} {
		        BeginToggle $T $item
		    } elseif {![$T selection includes $item]} {
		        BeginSelect $T $item
		    }

		    # Changing the selection might change the list
		    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		        if {[$T item id $item] eq ""} return
		    } else {
		        if { [$T index $item] eq ""} return
		    }

		    # ramsan
		    if { ![info exists Priv(dragimage,$T)] || \
		        $Priv(dragimage,$T) eq "" } { return }

		    # Click selected item to drag
		    if {[$T selection includes $item]} {
		        set Priv(buttonMode) drag
		    }
		} else {
		    set marquee 1
		}
	    }
	}
    }
    if {$marquee} {
	set Priv(buttonMode) marquee
	if {$Priv(selectMode) ne "set"} {
	    set Priv(selection) [$T selection get]
	} else {
	    $T selection clear
	    set Priv(selection) {}
	}
	MarqueeBegin $T $x $y
    }
    return
}

proc ::TreeCtrl::FullTkTreeMotion1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return

    if { [info exists Priv(after_open_dir,$T)] } {
	after cancel $Priv(after_open_dir,$T)
	unset Priv(after_open_dir,$T)
    }

    switch $Priv(buttonMode) {
	"drag" {
	    set Priv(autoscan,command,$T) {FullTkTreeMotion %T %x %y}
	    AutoScanCheck $T $x $y
	    FullTkTreeMotion $T $x $y

	    set id [$T identify $x $y]
	    if { $id eq "" || [lindex $id 0] ne "item" || [lindex $id 1] eq "" } {
		return
	    }
	    set item [lindex $id 1]
	    if { [$T item numchildren $item] && ![$T item state get $item open] } {
		set Priv(after_open_dir,$T) [after 500 [list \
		            ::TreeCtrl::FullTkTreeMotion1OpenDir $T $item]]
	    }
	}
	"marquee" {
	    set mode [$T cget -selectmode]
	    if {[string equal $mode "single"] || [string equal $mode "browse"]} {
		Motion1 $T $x $y
		return
	    }
	    set Priv(autoscan,command,$T) {FullTkTreeMotion %T %x %y}
	    AutoScanCheck $T $x $y
	    FullTkTreeMotion $T $x $y
	}
	default {
	    Motion1 $T $x $y
	}
    }
}

proc ::TreeCtrl::FullTkTreeMotion1OpenDir { T item } {
    variable Priv

    unset Priv(after_open_dir,$T)
    $T item expand $item
}

proc ::TreeCtrl::FullTkTreeMotion {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    MarqueeUpdate $T $x $y
	    set select $Priv(selection)
	    set deselect {}

	    # Check items covered by the marquee
	    foreach list [$T marque identify] {
		set item [lindex $list 0]

		# Check covered columns in this item
		foreach sublist [lrange $list 1 end] {
		    set column [lindex $sublist 0]
		    set ok 0

		    # Check covered elements in this column
		    foreach E [lrange $sublist 1 end] {
		        foreach sList $Priv(sensitive,$T) {
		            set sC [lindex $sList 0]
		            set sS [lindex $sList 1]
		            set sEList [lrange $sList 2 end]
		            if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		                if {[$T column compare $column != $sC]} continue
		            } else {
		                if { $column != $sC } continue
		            }
		            if {[$T item style set $item $sC] ne $sS} continue
		            if {[lsearch -exact $sEList $E] == -1} continue
		            set ok 1
		            break
		        }
		    }
		    # Some sensitive elements in this column are covered
		    if {$ok} {

		        # Toggle selected status
		        if {$Priv(selectMode) eq "toggle"} {
		            set i [lsearch -exact $Priv(selection) $item]
		            if {$i == -1} {
		                lappend select $item
		            } else {
		                set i [lsearch -exact $select $item]
		                set select [lreplace $select $i $i]
		            }
		        } else {
		            lappend select $item
		        }
		    }
		}
	    }
	    $T selection modify $select all
	}
	"drag" {
	    if {!$Priv(drag,motion)} {
		# Detect initial mouse movement
		if {(abs($x - $Priv(drag,click,x)) <= 4) &&
		    (abs($y - $Priv(drag,click,y)) <= 4)} return

		set Priv(selection) [$T selection get]
		set Priv(drop) ""
		$T dragimage clear
		# For each selected item, add some elements to the dragimage
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
		TryEvent $T Drag begin {}
	    }
	    # Find the element under the cursor
	    set drop ""
	    set id [$T identify $x $y]
	    set ok 0
	    if {($id ne "") && ([lindex $id 0] eq "item") && ([llength $id] == 6)} {
		set item [lindex $id 1]
		set column [lindex $id 3]
		set E [lindex $id 5]
		foreach list $Priv(sensitive,$T) {
		    set C [lindex $list 0]
		    set S [lindex $list 1]
		    set eList [lrange $list 2 end]
		    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		        if {[$T column compare $column != $C]} continue
		    } else {
		        if { $column != $C } continue
		    }
		    if {[$T item style set $item $C] ne $S} continue
		    if {[lsearch -exact $eList $E] == -1} continue
		    set ok 1
		    break
		}
	    }
	    if {$ok} {
		# If the item is not in the pre-drag selection
		# (i.e. not being dragged) and it is a directory,
		# see if we can drop on it

		if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		    set order [expr {[$T item order $item -visible] < $Priv(DirCnt,$T)}]
		} else {
		    set order [expr {[lindex [$T item index $item] 1] < $Priv(DirCnt,$T)}]
		}
		if {[lsearch -exact $Priv(selection) $item] == -1} {
		    if { $order } {
		        set drop $item
		        # We can drop if dragged item isn't an ancestor
		        foreach item2 $Priv(selection) {
		            if {[$T item isancestor $item2 $item]} {
		                set drop ""
		                break
		            }
		        }
		    }
		}
	    }

	    # Select the directory under the cursor (if any) and deselect
	    # the previous drop-directory (if any)
	    $T selection modify $drop $Priv(drop)

	    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		foreach item $Priv(drop) {
		    for { set col 0 } { $col < [$T column count] } { incr col } {
		        $T item element configure $item $col e_selmarker_up -fill ""
		        $T item element configure $item $col e_selmarker_down -fill ""
		        $T item element configure $item $col e_rect -draw 1
		        $T item element configure $item $col e_text_sel -fill \
		            [list $fulltktree::SystemHighlightText {selected focus}]
		    }
		}
		foreach item $drop {
		    foreach "- y1 - y2" [$T item bbox $item] break
		    for { set col 0 } { $col < [$T column count] } { incr col } {
		        if { $y < [expr {.667*$y1+.333*$y2}] } {
		            $T item element configure $item $col e_selmarker_up -fill \
		                [list black {selected focus} gray {selected !focus}]
		            $T item element configure $item $col e_selmarker_down -fill ""
		            $T item element configure $item $col e_rect -draw 0
		            $T item element configure $item $col e_text_sel -fill \
		                [list "" {selected focus}]
		        } elseif { $y < [expr {.333*$y1+.667*$y2}] } {
		            $T item element configure $item $col e_selmarker_up -fill ""
		            $T item element configure $item $col e_selmarker_down -fill ""
		            $T item element configure $item $col e_rect -draw 1
		            $T item element configure $item $col e_text_sel -fill \
		                [list $fulltktree::SystemHighlightText {selected focus}]
		        } else {
		            $T item element configure $item $col e_selmarker_up -fill ""
		            $T item element configure $item $col e_selmarker_down -fill \
		                [list black {selected focus} gray {selected !focus}]
		            $T item element configure $item $col e_rect -draw 0
		            $T item element configure $item $col e_text_sel -fill \
		                [list "" {selected focus}]
		        }
		    }
		}
	    }
	    set Priv(drop) $drop

	    # Show the dragimage in its new position
	    set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
	    set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
	    $T dragimage offset $x $y
	    $T dragimage configure -visible yes
	}
	default {
	    Motion1 $T $x $y
	}
    }
    return
}

proc ::TreeCtrl::FullTkTreeLeave1 {T x y} {
    variable Priv
    # This gets called when I click the mouse on Unix, and buttonMode is unset
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	default {
	    Leave1 $T $x $y
	}
    }
    return
}

proc ::TreeCtrl::FullTkTreeRelease1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    AutoScanCancel $T
	    MarqueeEnd $T $x $y
	}
	"drag" {
	    AutoScanCancel $T

	    # Some dragging occurred
	    if {$Priv(drag,motion)} {
		$T dragimage configure -visible no
		if {$Priv(drop) ne ""} {
		    $T selection modify {} $Priv(drop)
		    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
		        foreach item $Priv(drop) {
		            for { set col 0 } { $col < [$T column count] } { incr col } {
		                $T item element configure $item $col e_selmarker_up -fill ""
		                $T item element configure $item $col e_selmarker_down -fill ""
		                $T item element configure $item $col e_rect -draw 1
		                $T item element configure $item $col e_text_sel -fill \
		                    [list $fulltktree::SystemHighlightText {selected focus}]
		            }
		        }
		    }
		    TryEvent $T Drag receive \
		        [list W $T I $Priv(drop) l $Priv(selection) x $x y $y]
		}
		TryEvent $T Drag end {}
	    } elseif {$Priv(selectMode) eq "toggle"} {
		# don't rename

		# Clicked/released a selected item, but didn't drag
	    } elseif {$Priv(drag,wasSel)} {
#                 if { [package vcompare [package present treectrl] 2.1] >= 0 } {
#                     set I [$T item id active]
#                 } else {
#                     set I [$T index active]
#                 }
#                 set C $Priv(drag,C)
#                 set E $Priv(drag,E)
#                 set S [$T item style set $I $C]
#                 set ok 0
#                 foreach list $Priv(edit,$T) {
#                     set eC [lindex $list 0]
#                     set eS [lindex $list 1]
#                     set eEList [lrange $list 2 end]
#                     if { [package vcompare [package present treectrl] 2.1] >= 0 } {
#                         if {[$T column compare $C != $eC]} continue
#                     } else {
#                         if { $C != $eC } continue
#                     }
#                     if {$S ne $eS} continue
#                     if {[lsearch -exact $eEList $E] == -1} continue
#                     set ok 1
#                     break
#                 }
#                 if {$ok} {
#                     FullTkTreeEditCancel $T
#                     set Priv(editId,$T) \
#                         [after $Priv(edit,delay) [list ::TreeCtrl::FullTkTreeEdit $T $I $C $E]]
#                 }
	    }
	}
	default {
	    Release1 $T $x $y
	}
    }
    set Priv(buttonMode) ""
    return
}

proc ::TreeCtrl::FullTkTreeEdit {T I C E} {
    variable Priv
    array unset Priv editId,$T

    set lines [$T item element cget $I $C $E -lines]
    if {$lines eq ""} {
	set lines [$T element cget $E -lines]
    }

    # Scroll item into view
    $T see $I ; update

    # Multi-line edit
    if {$lines ne "1"} {
	scan [$T item bbox $I $C] "%d %d %d %d" x1 y1 x2 y2
	set S [$T item style set $I $C]
	set padx [$T style layout $S $E -padx]
	if {[llength $padx] == 2} {
	    set padw [lindex $padx 0]
	    set pade [lindex $padx 1]
	} else {
	    set pade [set padw $padx]
	}
	foreach E2 [$T style elements $S] {
	    if {[lsearch -exact [$T style layout $S $E2 -union] $E] == -1} continue
	    foreach option {-padx -ipadx} {
		set pad [$T style layout $S $E2 $option]
		if {[llength $pad] == 2} {
		    incr padw [lindex $pad 0]
		    incr pade [lindex $pad 1]
		} else {
		    incr padw $pad
		    incr pade $pad
		}
	    }
	}
	TextExpanderOpen $T $I $C $E [expr {$x2 - $x1 - $padw - $pade}]

    # Single-line edit
    } else {
	EntryExpanderOpen $T $I $C $E
    }

    TryEvent $T Edit begin [list I $I C $C E $E]

    return
}

proc ::TreeCtrl::FullTkTreeEditCancel {T} {
    variable Priv
    if {[info exists Priv(editId,$T)]} {
	after cancel $Priv(editId,$T)
	array unset Priv editId,$T
    }
    return
}

proc ::TreeCtrl::SetDragImage { T args } {
    variable Priv
    
    switch [llength $args] {
	0 {
	    return $Priv(dragimage,$T)
	} 1 {
	    set listOfLists [lindex $args 0]
	}
	default {
	    error "error in TreeCtrl::SetDragImage"
	}
    }
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	    if {[$T column id $column] eq ""} {
		error "column \"$column\" doesn't exist"
	    }
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	}
    }
    set Priv(dragimage,$T) $listOfLists
    return
}

proc ::TreeCtrl::SetEditable { T args } {
    variable Priv
    
    switch [llength $args] {
	0 {
	    return $Priv(edit,$T)
	} 1 {
	    set listOfLists [lindex $args 0]
	}
	default {
	    error "error in TreeCtrl::SetEditable"
	}
    }
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	    if {[$T column id $column] eq ""} {
		error "column \"$column\" doesn't exist"
	    }
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
#             if {[$T element type $element] ne "text"} {
#                 error "element \"$element\" is not of type \"text\""
#             }
	}
    }
    set Priv(edit,$T) $listOfLists
    return
}

proc ::TreeCtrl::SetSensitive { T args } {
    variable Priv
    
    switch [llength $args] {
	0 {
	    return $Priv(sensitive,$T)
	} 1 {
	    set listOfLists [lindex $args 0]
	}
	default {
	    error "error in TreeCtrl::SetSensitive"
	}
    }
    foreach list $listOfLists {
	set column [lindex $list 0]
	set style [lindex $list 1]
	set elements [lrange $list 2 end]
	if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	    if {[$T column id $column] eq ""} {
		error "column \"$column\" doesn't exist"
	    }
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	}
    }
    set Priv(sensitive,$T) $listOfLists
    return
}

proc ::TreeCtrl::EntryOpen {T item column element} {

    variable Priv

    set Priv(open_widget) entry
    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	set font [$T item element perstate $item $column $element -font]
    } else {
	set font [$T item element actual $item $column $element -font]
    }
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    if {[winfo exists $T.entry]} {
	$T.entry delete 0 end
    } else {
	entry $T.entry -borderwidth 1 -highlightthickness 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.entry <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::EntryClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.entry <KeyPress-Return> {
	    TreeCtrl::EntryClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.entry <KeyPress-Escape> {
	    TreeCtrl::EntryClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	    break
	}
	bind $T.entry <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::EntryClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
		break
	    }
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.entry <Scroll> {
	TreeCtrl::EntryClose %T 0
	focus $TreeCtrl::Priv(entry,%T,focus)
    }

    $T.entry configure -font $font
    $T.entry insert end $text
    $T.entry selection range 0 end

    set ebw [$T.entry cget -borderwidth]
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $T.entry -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $T.entry -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $T.entry -width $width

    SetGrabAndFocus $T.entry $T.entry
    return $T.entry
}

proc ::TreeCtrl::SetGrabAndFocus { focuswin grabwin } {

    set oldGrab [grab current $grabwin]
    if { $oldGrab ne "" && [winfo exists $oldGrab] } {
	set grabStatus [grab status $oldGrab]
	grab release $oldGrab
    } else {
	set grabStatus ""
    }
    tkwait  visibility $grabwin
    focus $focuswin
    grab $grabwin
    bind $grabwin <Unmap> +[list ::TreeCtrl::ReleaseGrabAndFocus $oldGrab $grabStatus]
}

proc ::TreeCtrl::ReleaseGrabAndFocus { oldGrab grabStatus } {
    if { $oldGrab ne "" }  {
	if { $grabStatus ne "global" } {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab $oldGrab }
	} else {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab -global $oldGrab }
	}
    }
}

# Like EntryOpen, but Entry widget expands/shrinks during typing
proc ::TreeCtrl::EntryExpanderOpen {T item column element { text - } } {

    variable Priv

    set Priv(open_widget) entry
    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2
    if { $y1 == $y2 } {
	scan [$T item bbox $item $column] "%d %d %d %d" - y1 - y2
	incr y1 2
    }

    # Get the font used by the Element
    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	set font [$T item element perstate $item $column $element -font]
    } else {
	set font [$T item element actual $item $column $element -font]
    }
    if {$font eq ""} {
	set font [$T cget -font]
    }

    set Priv(entry,$T,font) $font

    # Get the text used by the Element. Could check master Element too.
    if { $text eq "-" } {
	set text [$T item element cget $item $column $element -text]
    }
    # Create the Entry widget if needed
    if {[winfo exists $T.entry]} {
	$T.entry delete 0 end
    } else {
	entry $T.entry -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.entry <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::EntryClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.entry <KeyPress-Return> {
	    TreeCtrl::EntryClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.entry <KeyPress-Escape> {
	    TreeCtrl::EntryClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
	    break
	}

	# Resize as user types
	bind $T.entry <KeyPress> {
	    after idle [list TreeCtrl::EntryExpanderKeypress [winfo parent %W]]
	}
	bind $T.entry <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::EntryClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(entry,[winfo parent %W],focus)
		break
	    }
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.entry <Scroll> {
	TreeCtrl::EntryClose %T 0
	focus $TreeCtrl::Priv(entry,%T,focus)
    }

    $T.entry configure -font $font -background [$T cget -background]
    $T.entry insert end $text
    $T.entry selection range 0 end

    set ebw [$T.entry cget -borderwidth]
    set ex [expr {$x1 - $ebw - 1}]

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    set justify [$T item element cget $item $column $element -justify]
    if { $justify eq "right" } {
	set width [expr {$right - $ex}]
    } elseif { $justify eq "" } {
	set justify left
    }
    $T.entry configure -justify $justify
    place $T.entry -x $ex -y [expr {$y1 - $ebw - 1}] \
	-bordermode outside -width $width

    SetGrabAndFocus $T.entry $T.entry
    return $T.entry
}

proc ::TreeCtrl::EntryClose {T accept} {

    variable Priv

    place forget $T.entry
    grab release $T.entry
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(entry,$T,item) C $Priv(entry,$T,column) \
		E $Priv(entry,$T,element) t [$T.entry get]]
    }

    $T notify bind $T.entry <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(entry,$T,item) C $Priv(entry,$T,column) \
	    E $Priv(entry,$T,element)]

    set Priv(open_widget) ""
    return
}

proc ::TreeCtrl::EntryExpanderKeypress {T} {

    variable Priv

    set font $Priv(entry,$T,font)
    set text [$T.entry get]
    set ebw [$T.entry cget -borderwidth]
    set ex [winfo x $T.entry]

    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]

    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $T.entry -width $width

    return
}

proc ::TreeCtrl::TextOpen {T item column element {width 0} {height 0}} {
    variable Priv

    set Priv(open_widget) text
    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    # Get the font used by the Element
    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	set font [$T item element perstate $item $column $element -font]
    } else {
	set font [$T item element actual $item $column $element -font]
    }
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Text widget if needed
    if {[winfo exists $T.text]} {
	$T.text delete 1.0 end
    } else {
	text $T.text -borderwidth 1 -highlightthickness 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.text <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::TextClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
	bind $T.text <KeyPress-Return> {
	    TreeCtrl::TextClose [winfo parent %W] 1
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.text <KeyPress-Escape> {
	    TreeCtrl::TextClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	    break
	}
	bind $T.text <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::TextClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
		break
	    }
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.text <Scroll> {
	TreeCtrl::TextClose %T 0
	focus $TreeCtrl::Priv(text,%T,focus)
    }
    set justify [$T element cget $element -justify]
    elseif { $justify eq "" } {
	set justify left
    }
    $T.text tag configure TAG -justify $justify
    $T.text configure -font $font
    $T.text insert end $text
    $T.text tag add sel 1.0 end
    $T.text tag add TAG 1.0 end

    set tbw [$T.text cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    place $T.text -x $tx -y [expr {$y1 - $tbw - 1}] \
	-width [expr {$x2 - $x1 + ($tbw + 1) * 2}] \
	-height [expr {$y2 - $y1 + ($tbw + 1) * 2}] \
	-bordermode outside

    SetGrabAndFocus $T.text $T.text
    return $T.text
}

# Like TextOpen, but Text widget expands/shrinks during typing
proc ::TreeCtrl::TextExpanderOpen {T item column element width { text - } } {

    variable Priv

    set Priv(open_widget) text
    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

#     set Priv(text,$T,center) [expr {$x1 + ($x2 - $x1) / 2}]
    set Priv(text,$T,x1) $x1

    # Get the font used by the Element
    if { [package vcompare [package present treectrl] 2.1] >= 0 } {
	set font [$T item element perstate $item $column $element -font]
    } else {
	set font [$T item element actual $item $column $element -font]
    }
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    if { $text eq "-" } {
	set text [$T item element cget $item $column $element -text]
    }
    set justify [$T element cget $element -justify]
    if {$justify eq ""} {
	set justify left
    }

    # Create the Text widget if needed
    if {[winfo exists $T.text]} {
	$T.text delete 1.0 end
    } else {
	text $T.text -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.text <FocusOut> {
	    if {[winfo ismapped %W]} {
		TreeCtrl::TextClose [winfo parent %W] 1
	    }
	}

	# Accept edit on <Return>
#         bind $T.text <KeyPress-Return> {
#             TreeCtrl::TextClose [winfo parent %W] 1
#             focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
#             break
#         }

	# Cancel edit on <Escape>
	bind $T.text <KeyPress-Escape> {
	    TreeCtrl::TextClose [winfo parent %W] 0
	    focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
	    break
	}

	# Resize as user types
	bind $T.text <KeyPress> {
	    after idle TreeCtrl::TextExpanderKeypress [winfo parent %W]
	}
	bind $T.text <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::TextClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(text,[winfo parent %W],focus)
		break
	    }
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.text <Scroll> {
	TreeCtrl::TextClose %T 0
	focus $TreeCtrl::Priv(text,%T,focus)
    }

    $T.text tag configure TAG -justify $justify
    $T.text configure -font $font -background [$T cget -background]
    $T.text insert end $text
    $T.text tag add sel 1.0 end
    $T.text tag add TAG 1.0 end
    
    set tbw [$T.text cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$tx + $width > $right} {
	set width [expr {$right - $tx}]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$tx + $width > $right} {
	set width [expr {$right - $tx}]
    }

    set Priv(text,$T,font) $font
    set Priv(text,$T,justify) $justify
    set Priv(text,$T,width) $width

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    incr tbw
    place $T.text -x $tx -y [expr {$y1 - $tbw-1}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}] \
	-bordermode outside

    SetGrabAndFocus $T.text $T.text
    return $T.text
}

proc ::TreeCtrl::TextClose {T accept} {

    variable Priv

    place forget $T.text
    grab release $T.text
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(text,$T,item) C $Priv(text,$T,column) \
		E $Priv(text,$T,element) t [$T.text get 1.0 end-1c]]
    }

    $T notify bind $T.text <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(text,$T,item) C $Priv(text,$T,column) \
		E $Priv(text,$T,element)]
    set Priv(open_widget) ""
    return
}

proc ::TreeCtrl::TextExpanderKeypress {T} {

    variable Priv

    set font $Priv(text,$T,font)
    set justify $Priv(text,$T,justify)
    set width $Priv(text,$T,width)
    set x1 $Priv(text,$T,x1)

    set text [$T.text get 1.0 end-1c]

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    incr tbw
    place configure $T.text \
	-x [expr {$x1 - $tbw - 1}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}]

    $T.text tag add TAG 1.0 end

    return
}

proc ::TreeCtrl::ComboOpen {T item column element editable values { defvalue -NONE- } \
    { dict "" }  } {

    variable Priv

    set Priv(open_widget) combo
    set Priv(combo,$T,item) $item
    set Priv(combo,$T,column) $column
    set Priv(combo,$T,element) $element
    set Priv(combo,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]
    if { $font eq "" } { set font TkDefaultFont }

    # Get the text used by the Element. Could check master Element too.
    
    if { $defvalue ne "-NONE-" } {
	set text $defvalue
    } else {
	set text [$T item element cget $item $column $element -text]
    }

    # Create the Combo widget if needed
    if {[winfo exists $T.combo]} {
	$T.combo state !readonly
	$T.combo delete 0 end
    } else {
	if { [info commands cu::combobox] ne "" } {
	    cu::combobox $T.combo
	} else {
	    ttk::combobox $T.combo
	}
	#$T.combo.e configure -borderwidth 1 -highlightthickness 0 -relief solid

	# Accept edit when we lose the focus
	bind $T.combo <FocusOut> {
	    if {[winfo ismapped %W]} {
		if { [info exists TreeCtrl::Priv(combo,%W,posting)] } {
		    unset TreeCtrl::Priv(combo,%W,posting)
		    focus %W
		    return
		}
		set fw [focus]
		set found 0
		while { $fw ne "." && $fw ne "" } {
		    if { $fw eq "%W" } {
		        set found 1
		        break
		    }
		    set fw [winfo parent $fw]
		}
		if { !$found } {
		    TreeCtrl::ComboClose [winfo parent %W] 1
		} else {
		    set TreeCtrl::Priv(combo,%W,posting) 1
		    after 200 [list unset -nocomplain TreeCtrl::Priv(combo,%W,posting)] 
		}
	    }
	}

	# Accept edit on <Return>
	bind $T.combo <KeyPress-Return> {
	    set w [winfo parent %W]
	    TreeCtrl::ComboClose $w 1
	    focus $TreeCtrl::Priv(combo,$w,focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.combo <KeyPress-Escape> {
	    set w [winfo parent %W]
	    TreeCtrl::ComboClose $w 0
	    focus $TreeCtrl::Priv(combo,$w,focus)
	    break
	}
	bind $T.combo <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::ComboClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(combo,[winfo parent %W],focus)
		break
	    }
	    ttk::combobox::Press "" %W %x %y
	    break
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.combo <Scroll> {
	TreeCtrl::ComboClose %T 0
	focus $TreeCtrl::Priv(combo,%T,focus)
    }

    catch { $T.combo configure -font $font }
    $T.combo insert end $text
    $T.combo selection range 0 end

    switch $editable 1 { set state !readonly } 0 { set state readonly }
    $T.combo configure -values $values -state $state
    if { $dict ne "" } {
	$T.combo configure -dict $dict
    }

    #set ebw [$T.combo cget -borderwidth]
    set ebw 1
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $T.combo -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $T.combo -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    # Make the Combo as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set len [string length [$T item element cget $item $column $element -text]]
    
    foreach i $values {
	if { [string length $i] > $len } {
	    set len [string length $i]
	}
    }
    if { $len < 4 } { set len 4 }
    if { $len > 10 } {
	set len [expr {int($len*.6)}]
    }
    if { [string length $text] < $len } {
	append text [string repeat W [expr {$len-[string length $text]}]]
    }
    set width [font measure $font ${text}W]

    set width2 [font measure $font [$T item element cget $item $column $element -text]]
    if { $width2 > $width } { set width $width2 }

    set width [expr {$width +16+ ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1 }]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1}]
    }
    place configure $T.combo -width $width

    SetGrabAndFocus $T.combo $T.combo
    return $T.combo
}

proc ::TreeCtrl::ComboClose {T accept} {

    variable Priv

    place forget $T.combo
    grab release $T.combo
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(combo,$T,item) C $Priv(combo,$T,column) \
		E $Priv(combo,$T,element) t [$T.combo get]]
    }

    $T notify bind $T.combo <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(combo,$T,item) C $Priv(combo,$T,column) \
		E $Priv(combo,$T,element)]
    set Priv(open_widget) ""
    return
}

proc ::TreeCtrl::EntryComboOpen {T item column element editable values defval1 defval2 } {

    variable Priv

    set Priv(open_widget) entrycombo
    set Priv(entrycombo,$T,item) $item
    set Priv(entrycombo,$T,column) $column
    set Priv(entrycombo,$T,element) $element
    set Priv(entrycombo,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    #for debugging
    destroy $T.fcombo

    # Create the Combo widget if needed
    if {[winfo exists $T.fcombo]} {
	$T.fcombo.e delete 0 end
	$T.fcombo.c state !readonly
	$T.fcombo.c delete 0 end
    } else {
	ttk::panedwindow $T.fcombo -orient horizontal
	ttk::entry $T.fcombo.e -width 2
	ttk::combobox $T.fcombo.c -width 1
	$T.fcombo add $T.fcombo.e -weight 2
	$T.fcombo add $T.fcombo.c -weight 1

	# Accept edit when we lose the focus
	bind $T.fcombo.e <FocusOut> [list ::TreeCtrl::EntryComboFocusOut $T.fcombo]
	bind $T.fcombo.c <FocusOut> [list ::TreeCtrl::EntryComboFocusOut $T.fcombo]

	# Accept edit on <Return>
	bind $T.fcombo.e <KeyPress-Return> {
	    set w [winfo parent [winfo parent %W]]
	    TreeCtrl::EntryComboClose $w 1
	    focus $TreeCtrl::Priv(entrycombo,$w,focus)
	    break
	}
	bind $T.fcombo.c <KeyPress-Return> {
	    set w [winfo parent [winfo parent %W]]
	    TreeCtrl::EntryComboClose $w 1
	    focus $TreeCtrl::Priv(entrycombo,$w,focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.fcombo.e <KeyPress-Escape> {
	    set w [winfo parent [winfo parent %W]]
	    TreeCtrl::EntryComboClose $w 0
	    focus $TreeCtrl::Priv(entrycombo,$w,focus)
	    break
	}
	bind $T.fcombo.c <KeyPress-Escape> {
	    set w [winfo parent [winfo parent %W]]
	    TreeCtrl::EntryComboClose $w 0
	    focus $TreeCtrl::Priv(entrycombo,$w,focus)
	    break
	}
	bind $T.fcombo <1> {
	    set w [winfo parent %W]
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::EntryComboClose $w 1
		focus $TreeCtrl::Priv(entrycombo,$w,focus)
		break
	    }
	}
	bind $T.fcombo.e <1> {
	    set w0 [winfo parent %W]
	    set w [winfo parent $w0]
	    if { %x < 0 || %x > [winfo width $w0] || 
		%y < 0 || %y > [winfo height $w0] } {
		TreeCtrl::EntryComboClose $w 1
		focus $TreeCtrl::Priv(entrycombo,$w,focus)
		break
	    }
	}
	bind $T.fcombo.c <1> {
	    set w0 [winfo parent %W]
	    set w [winfo parent $w0]
	    if { %x < 0 || %x > [winfo width $w0] || 
		%y < 0 || %y > [winfo height $w0] } {
		TreeCtrl::EntryComboClose $w 1
		focus $TreeCtrl::Priv(entrycombo,$w,focus)
		break
	    }
	}
    }
    $T.fcombo.c configure -values $values 

    # Pesky MouseWheel
    $T notify bind $T.fcombo.c <Scroll> {
	TreeCtrl::EntryComboClose %T 0
	focus $TreeCtrl::Priv(entrycombo,%T,focus)
    }
    $T notify bind $T.fcombo.e <Scroll> {
	TreeCtrl::EntryComboClose %T 0
	focus $TreeCtrl::Priv(entrycombo,%T,focus)
    }

    $T.fcombo.e insert end $defval1
    $T.fcombo.c insert end $defval2
    $T.fcombo.e selection range 0 end

    if { !$editable } { $T.fcombo.c state readonly }

    set ebw 2
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $T.fcombo -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $T.fcombo -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    # Make the Combo as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width 100
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
#     scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
#     if {$ex + $width > $right} {
#         set width [expr {$right - $ex}]
#     }
    set width [expr {$right - $ex}]
    place configure $T.fcombo -width $width

    SetGrabAndFocus $T.fcombo.e $T.fcombo
    return
}

proc ::TreeCtrl::EntryComboFocusOut { w } {
    if { ![winfo ismapped $w] } { return }
    if { [info exists TreeCtrl::Priv(entrycombo,$w,posting)] } {
	unset TreeCtrl::Priv(entrycombo,$w,posting)
	focus $w
	return
    }
    set fw [focus]
    set found 0
    while { $fw ne "." && $fw ne "" } {
	if { $fw eq $w } {
	    set found 1
	    break
	}
	set fw [winfo parent $fw]
    }
    if { !$found } {
	TreeCtrl::EntryComboClose [winfo parent $w] 1
    } else {
	set TreeCtrl::Priv(entrycombo,$w,posting) 1
    }
}

proc ::TreeCtrl::EntryComboClose {T accept} {

    variable Priv

    place forget $T.fcombo
    grab release $T.fcombo
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(entrycombo,$T,item) C $Priv(entrycombo,$T,column) \
		E $Priv(entrycombo,$T,element) t [list [$T.fcombo.e get] \
		    [$T.fcombo.c get]]]
    }

    $T notify bind $T.fcombo.c <Scroll> {}
    $T notify bind $T.fcombo.e <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(entrycombo,$T,item) C $Priv(entrycombo,$T,column) \
		E $Priv(entrycombo,$T,element)]
    set Priv(open_widget) ""
    return
}

proc ::TreeCtrl::ComboTreeOpen {T item column element editable values_tree { defvalue -NONE- } } {

    variable Priv

    set Priv(open_widget) combotree
    set Priv(combotree,$T,item) $item
    set Priv(combotree,$T,column) $column
    set Priv(combotree,$T,element) $element
    set Priv(combotree,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element actual $item $column $element -font]
    if { $font eq "" } { set font TkDefaultFont }

    # Get the text used by the Element. Could check master Element too.
    
    if { $defvalue ne "-NONE-" } {
	set text $defvalue
    } else {
	set text [$T item element cget $item $column $element -text]
    }

    # Create the Combotree widget if needed
    if {[winfo exists $T.combotree]} {
	$T.combotree state !readonly
	$T.combotree delete 0 end
	$T.combotree clear
    } else {
	cu::combobox_tree $T.combotree        

	# Accept edit when we lose the focus
	bind $T.combotree <FocusOut> {
	    if {[winfo ismapped %W]} {
		if { [info exists TreeCtrl::Priv(combotree,%W,posting)] } {
		    unset TreeCtrl::Priv(combotree,%W,posting)
		    focus %W
		    return
		}
		set fw [focus]
		set found 0
		while { $fw ne "." && $fw ne "" } {
		    if { $fw eq "%W" } {
		        set found 1
		        break
		    }
		    set fw [winfo parent $fw]
		}
		if { !$found } {
		    TreeCtrl::ComboTreeClose [winfo parent %W] 1
		} else {
		    set TreeCtrl::Priv(combotree,%W,posting) 1
		    after 200 [list unset -nocomplain TreeCtrl::Priv(combotree,%W,posting)] 
		}
	    }
	}

	# Accept edit on <Return>
	bind $T.combotree <KeyPress-Return> {
	    set w [winfo parent %W]
	    TreeCtrl::ComboTreeClose $w 1
	    focus $TreeCtrl::Priv(combotree,$w,focus)
	    break
	}

	# Cancel edit on <Escape>
	bind $T.combotree <KeyPress-Escape> {
	    set w [winfo parent %W]
	    TreeCtrl::ComboTreeClose $w 0
	    focus $TreeCtrl::Priv(combotree,$w,focus)
	    break
	}
	bind $T.combotree <1> {
	    if { %x < 0 || %x > [winfo width %W] || 
		%y < 0 || %y > [winfo height %W] } {
		TreeCtrl::ComboTreeClose [winfo parent %W] 1
		focus $TreeCtrl::Priv(combotree,[winfo parent %W],focus)
		break
	    }
	}
    }

    # Pesky MouseWheel
    $T notify bind $T.combotree <Scroll> {
	TreeCtrl::ComboTreeClose %T 0
	focus $TreeCtrl::Priv(combotree,%T,focus)
    }

    catch { $T.combotree configure -font $font }
    $T.combotree insert end $text
    $T.combotree selection range 0 end

    switch $editable 1 { set state !readonly } 0 { set state readonly }
    $T.combotree configure -state $state
    
    $T.combotree tree_insert_batch $values_tree
#     set last_item(-1) root
#     foreach i $values_tree {
#         foreach "level name fname image selectable" $i break
#         set parent $last_item([expr {$level-1}])
#         set it [$T.combotree tree_insert -image $image \
#                 -active $selectable end \
#                 $name $fname $parent]
#         set last_item($level) $it
#     }

    #set ebw [$T.combotree cget -borderwidth]
    set ebw 1
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $T.combotree -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $T.combotree -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    # Make the Combo as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set len [string length [$T item element cget $item $column $element -text]]
    if { $len < 4 } { set len 4 }
    if { $len > 10 } {
	set len [expr {int($len*.6)}]
    }
    if { [string length $text] < $len } {
	append text [string repeat W [expr {$len-[string length $text]}]]
    }
    set width [font measure $font ${text}W]

    set width2 [font measure $font [$T item element cget $item $column $element -text]]
    if { $width2 > $width } { set width $width2 }

    set width [expr {$width +16+ ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1 }]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1}]
    }
    place configure $T.combotree -width $width

    SetGrabAndFocus $T.combotree $T.combotree
    return $T.combotree
}

proc ::TreeCtrl::ComboTreeClose {T accept} {

    variable Priv

    place forget $T.combotree
    grab release $T.combotree
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(combotree,$T,item) C $Priv(combotree,$T,column) \
		E $Priv(combotree,$T,element) t [$T.combotree get]]
    }

    $T notify bind $T.combotree <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(combotree,$T,item) C $Priv(combotree,$T,column) \
		E $Priv(combotree,$T,element)]
    set Priv(open_widget) ""
    return
}

proc ::TreeCtrl::AnyWidgetOpen {T item column element widget } {

    variable Priv

    set Priv(open_widget) anywidget
    set Priv(anywidget,widget) $widget
    set Priv(anywidget,T) $T
    set Priv(anywidget,$T,item) $item
    set Priv(anywidget,$T,column) $column
    set Priv(anywidget,$T,element) $element
    set Priv(anywidget,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y
    
    set err [catch { $T item element actual $item $column $element -font } font]
    if { $err || $font eq "" } { set font TkDefaultFont }
    
    # Accept edit when we lose the focus
    
    # Accept edit when we lose the focus
    bind $widget <FocusOut> {
	if {[winfo ismapped %W]} {
	    if { [info exists TreeCtrl::Priv(anywidget,%W,posting)] } {
		unset TreeCtrl::Priv(anywidget,%W,posting)
		focus %W
		return
	    }
	    set T $TreeCtrl::Priv(anywidget,T)
	    set fw [focus]
	    if { $fw eq "" } {
		return
	    }
	    set found 0
	    while { $fw ne "." && $fw ne "" } {
		if { $fw eq $T } {
		    set found 1
		    break
		}
		set fw [winfo parent $fw]
	    }
	    if { !$found } {
		TreeCtrl::AnyWidgetClose $T 1
	    } else {
		set TreeCtrl::Priv(anywidget,%W,posting) 1
		after 200 [list unset -nocomplain TreeCtrl::Priv(anywidget,%W,posting)] 
	    }
	}
    }
    
    # Accept edit on <Return>
    bind $widget <KeyPress-Return> {
	set T $TreeCtrl::Priv(anywidget,T)
	TreeCtrl::AnyWidgetClose $T 1
	focus $TreeCtrl::Priv(anywidget,$T,focus)
	break
    }
    
    # Cancel edit on <Escape>
    bind $widget <KeyPress-Escape> {
	set T $TreeCtrl::Priv(anywidget,T)
	TreeCtrl::AnyWidgetClose $T 0
	focus $TreeCtrl::Priv(anywidget,$T,focus)
	break
    }
    bind $widget <1> {
	set T $TreeCtrl::Priv(anywidget,T)
	if { %x < 0 || %x > [winfo width $T] || 
	    %y < 0 || %y > [winfo height %W] } {
	    TreeCtrl::AnyWidgetClose $T 1
	    focus $TreeCtrl::Priv(anywidget,$T,focus)
	    break
	} elseif { %x > [winfo width %W] } {
	    set item $TreeCtrl::Priv(anywidget,$T,item)
	    set column $TreeCtrl::Priv(anywidget,$T,column)
	    set element $TreeCtrl::Priv(anywidget,$T,element)
	    scan [$T item bbox $item $column] "%%d %%d %%d %%d" left top right bottom
	    scan [$T item bbox $item $column $element] "%%d %%d" x y
	    set ex [expr {$x - 2}]
	    set width [expr {$right - $ex - 1}]
	    place configure %W -width $width
	}
    }
    
    # Pesky MouseWheel
    $T notify bind $widget <Scroll> {
	TreeCtrl::AnyWidgetClose %T 0
	focus $TreeCtrl::Priv(anywidget,%T,focus)
    }

    #set ebw [$T.combo cget -borderwidth]
    set ebw 1
    if 1 {
	set ex [expr {$x - $ebw - 1}]
	place $widget -x $ex -y [expr {$y - $ebw - 1}] \
	    -bordermode outside
    } else {
	set hw [$T cget -highlightthickness]
	set bw [$T cget -borderwidth]
	set ex [expr {$x - $bw - $hw - $ebw - 1}]
	place $widget -x $ex -y [expr {$y - $bw - $hw - $ebw - 1}]
    }

    set width [winfo width $widget]
    if { $width == 1 } {
	set width [winfo reqwidth $widget]
    }
    if { [$T item style set active $column] in "text s_text" } {
	set width2 [font measure $font [$T item element cget $item $column $element -text]]
	if { $width2 > $width } { set width $width2 }
    }
    set width [expr {$width +16+ ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1 }]
    }
    scan [$T item bbox $item $column] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex - 1}]
    }
    place configure $widget -width $width -height [expr {$bottom-$top+1}]
    
    SetGrabAndFocus $widget $widget
    return $widget
}

proc ::TreeCtrl::AnyWidgetClose {T accept} {

    variable Priv

    set widget $Priv(anywidget,widget)
    set column $Priv(anywidget,$T,column)

    if { [$T item style set active $column]  in "text s_text" } {
	set text [$widget get_text]
    } else {
	set text ""   
    }
    destroy $widget
    #place forget $widget
    #grab release $widget
    focus $T
    update

    if {$accept} {
	TryEvent $T Edit accept \
	    [list W $T I $Priv(anywidget,$T,item) C $Priv(anywidget,$T,column) \
		E $Priv(anywidget,$T,element) t $text]
    }

    #$T notify bind $widget <Scroll> {}

    TryEvent $T Edit end \
	[list W $T I $Priv(anywidget,$T,item) C $Priv(anywidget,$T,column) \
	    E $Priv(anywidget,$T,element)]
    set Priv(open_widget) ""
    return
}


if { [package vcompare [package present treectrl] 2.1] < 0 } {
    proc ::TreeCtrl::TryEvent {T event detail charMap} {
	lappend charMap T $T
	if {[lsearch -exact [$T notify eventnames] $event] == -1} return
	if {$detail ne ""} {
	    if {[lsearch -exact [$T notify detailnames $event] $detail] == -1} return
	    $T notify generate <$event-$detail> $charMap
	} else {
	    $T notify generate <$event> $charMap
	}
	return
    }
}

proc ::TreeCtrl::WidgetClose {T accept} {
    variable Priv
    
    if { ![info exists Priv(open_widget)] } { return }
    
    switch $Priv(open_widget) {
	entry { set cmd EntryClose }
	text { set cmd TextClose }
	combo { set cmd ComboClose }
	entrycombo { set cmd EntryComboClose }
	combotree { set cmd ComboTreeClose }
	anywidget { set cmd AnyWidgetClose }
	"" { return }
    }
    lappend cmd $T $accept
    eval $cmd
}














