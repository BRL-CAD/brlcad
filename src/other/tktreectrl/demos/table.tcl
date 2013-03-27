# Copyright (c) 2011 Tim Baker

namespace eval DemoTable {}

proc DemoTable::Init {T} {
    variable Priv

    $T configure -showroot no -usetheme no -xscrollsmoothing yes

    $T column create -tags rowtitle -lock left -button no
    for {set i 1} {$i <= 10} {incr i} {
	$T column create -text $i -minwidth 20
    }
    $T column configure all -background gray90 -borderwidth 1
    $T column configure all -justify center -itemjustify left

    #
    # Create custom item states to change the appearance of items during
    # drag and drop.  The 'drag' state is also used when resizing spans.
    #

    $T item state define drag
    $T item state define drop

    # Another state to highlight the cell under the mouse pointer.
    $T item state define mouseover

    #
    # Create a style for the row titles
    #

    $T element create rowtitle.border border \
	-background gray90 -thickness 1 -relief raised -filled yes
    $T element create rowtitle.text text
    set S [$T style create rowtitle]
    $T style elements $S {rowtitle.border rowtitle.text}
    $T style layout $S rowtitle.border \
	-union rowtitle.text -ipadx 3 -ipady 2 -iexpand news
    $T style layout $S rowtitle.text -center x

    #
    # Create a style for each cell
    #

    $T element create cell.text text
    $T element create cell.border border -background gray -thickness 2 -relief groove
    $T element create cell.bg rect \
	-fill {{light green} drag {light blue} drop gray90 mouseover}

    set S [$T style create cell]
    $T style elements $S {cell.bg cell.border cell.text}
    $T style layout $S cell.bg -detach yes -iexpand xy \
	-visible {yes drag yes drop yes mouseover no {}}
    $T style layout $S cell.border -union cell.text -iexpand wens -ipadx {2 3} -ipady 2
    $T style layout $S cell.text -squeeze x

    #
    # Set default styles and create items
    #

    $T column configure 0 -itemstyle rowtitle
    $T column configure {range 1 10} -itemstyle cell
    foreach I [$T item create -count 5000 -parent root] {
	$T item text $I rowtitle [$T item order $I]
    }
    $T item text {root children} {range 1 10} "edit me"
    $T item text 10 5 "*** DRAG ME ***"
    $T item span 10 5 2
    $T item text 15 2 "RESIZE THE SPAN -->"
    $T item span 15 2 3

    $T notify bind $T <Edit-accept> {
	%T item text %I %C %t
	set DemoTable::EditAccepted 1
    }
    $T notify bind $T <Edit-end> {
	if {!$DemoTable::EditAccepted} {
	    %T item element configure %I %C %E -text $DemoTable::OrigText
	}
	%T item element configure %I %C %E -textvariable ""
    }

    # Set the minimum item height to be as tall as the style and the
    # entry widget used to edit text need.  Text elements may wrap lines
    # causing an item to become even taller.
    set height [font metrics [$T cget -font] -linespace]
    incr height [expr {[$T style layout cell cell.border -ipady] * 2}]
    incr height 2 ; # entry widget YPAD
    $T configure -minitemheight $height

    set Priv(buttonMode) ""
    set Priv(cursor,want) ""
    set Priv(cursor,set) 0
    set Priv(highlight) ""

    bind DemoTable <ButtonPress-1> {
	DemoTable::ButtonPress1 %W %x %y
    }
    bind DemoTable <Button1-Motion> {
	DemoTable::Button1Motion %W %x %y
	DemoTable::MaintainHighlight %W
    }
    bind DemoTable <ButtonRelease-1> {
	DemoTable::ButtonRelease1 %W %x %y
	DemoTable::Motion %W %x %y
	DemoTable::MaintainCursor %W
	DemoTable::MaintainHighlight %W
    }

    # Control-drag to copy text
    bind DemoTable <Control-ButtonRelease-1> {
	DemoTable::ButtonRelease1 %W %x %y control
	DemoTable::Motion %W %x %y
	DemoTable::MaintainCursor %W
	DemoTable::MaintainHighlight %W
    }
    if {[tk windowingsystem] eq "aqua" } {
	bind DemoTable <Command-ButtonRelease-1> {
	    DemoTable::ButtonRelease1 %W %x %y command
	    DemoTable::Motion %W %x %y
	    DemoTable::MaintainCursor %W
	    DemoTable::MaintainHighlight %W
	}
    }

    bind DemoTable <Motion> {
	DemoTable::Motion %W %x %y
	DemoTable::MaintainCursor %W
	DemoTable::MaintainHighlight %W
    }
    bindtags $T [list $T DemoTable TreeCtrl [winfo toplevel $T] all]

    return
}

proc DemoTable::ButtonPress1 {T x y} {
    variable Priv
    if {[winfo exists $T.entry] && [winfo ismapped $T.entry]} {
	TreeCtrl::EditClose $T entry 1 0
    }
    set Priv(buttonMode) ""
    $T identify -array id $x $y
    if {$id(where) ne "item"} return
    if {$id(column) eq "tail"} return
    if {[$T column tag expr $id(column) rowtitle]} return

    switch -- [WhichSide $T $id(item) $id(column) $x $y] {
	left {
	    if {[$T column compare $id(column) > "first visible lock none"]} {
		set Priv(buttonMode) resize
		set Priv(item) $id(item)
		set Priv(column) [StartOfPrevSpan $T $id(item) $id(column)]
		set Priv(y) $y
		$T item state forcolumn $Priv(item) $Priv(column) drag
		return
	    }
	}
	right {
	    if {[$T column compare $id(column) < "last visible lock none"]} {
		set Priv(buttonMode) resize
		set Priv(item) $id(item)
		set Priv(column) $id(column)
		set Priv(y) $y
		$T item state forcolumn $Priv(item) $Priv(column) drag
		return
	    }
	}
    }

    set Priv(buttonMode) dragWait
    set Priv(item) $id(item)
    set Priv(column) $id(column)
    set Priv(x) $x
    set Priv(y) $y

    return
}

proc DemoTable::Button1Motion {T x y} {
    variable Priv
    set Priv(highlight,want) {}
    switch $Priv(buttonMode) {
	dragWait {
	    set Priv(highlight,want) [list $Priv(item) $Priv(column) mouseover]
	    if {(abs($Priv(x) - $x) > 4) || (abs($Priv(y) - $y) > 4)} {
		set Priv(buttonMode) drag
		$T item state forcolumn $Priv(item) $Priv(column) drag
		set Priv(cx) [$T canvasx $x]
		set Priv(cy) [$T canvasy $y]
		$T dragimage clear
		$T dragimage add $Priv(item) $Priv(column) cell.border
		$T dragimage configure -visible yes
	    }
	}
	drag {
	    $T identify -array id $x $y
	    if {$id(where) eq "item" && [$T column cget $id(column) -lock] eq "none"} {
		set Priv(highlight,want) [list $id(item) $id(column) drop]
	    }
	    set dx [expr {[$T canvasx $x] - $Priv(cx)}]
	    set dy [expr {[$T canvasy $y] - $Priv(cy)}]
	    $T dragimage offset $dx $dy
	}
	resize {
	    $T identify -array id $x $Priv(y)
	    if {$id(where) eq "item"} {
		set C [ColumnUnderPoint $T $x $y]
		if {[WhichHalf $T $C $x $y] eq "right"} {
		    if {[$T column compare $id(column) > $Priv(column)]} {
			IncrSpan $T $Priv(item) $Priv(column) $C
		    }
		    if {[$T column compare $C >= $Priv(column)] &&
			    ([$T item span $Priv(item) $Priv(column)] > 1)} {
			DecrSpan $T $Priv(item) $Priv(column) $C
		    }
		}
		if {[WhichHalf $T $C $x $y] eq "left"} {
		    if {[$T column compare $C == $Priv(column)]} {
			DecrSpan $T $Priv(item) $Priv(column) $C
		    }
		}
	    }
	}
    }
    return
}

proc DemoTable::ButtonRelease1 {T x y args} {
    variable Priv
    array set modifiers { shift 0 control 0 command 0 }
    foreach modifier $args {
	set modifiers($modifier) 1
    }
    switch $Priv(buttonMode) {
	dragWait {
	    # FIXME: EntryExpanderOpen doesn't work with master elements
	    $T see $Priv(item) $Priv(column)
	    set text [$T item text $Priv(item) $Priv(column)]
	    $T item text $Priv(item) $Priv(column) $text
	    set exists [winfo exists $T.entry]
	    TreeCtrl::EntryExpanderOpen $T $Priv(item) $Priv(column) cell.text
	    if {!$exists} {
		$T.entry configure -borderwidth 0
		scan [$T item bbox $Priv(item) $Priv(column) cell.text] "%d %d %d %d" x1 y1 x2 y2
		place $T.entry -y [expr {$y1 - 1}]
	    }
	    # Remove the <Scroll> binding on the text entry since typing
	    # may resize columns, causing the entry to become hidden.
	    $T notify unbind $T.entry <Scroll>
	    # Set the -textvariable on the text element and the entry widget
	    # to be the same, so typing in the entry automatically updates
	    # the text element.
	    set ::DemoTable::TextVar $text
	    set ::DemoTable::OrigText $text
	    set ::DemoTable::EditAccepted 0
	    $T.entry configure -textvariable ::DemoTable::TextVar
	    $T item element configure $Priv(item) $Priv(column) cell.text \
		-textvariable ::DemoTable::TextVar -text ""
	    # Override EntryExpanderKeypress to make the entry widget as wide
	    # as we want.
	    bind $T.entry <KeyPress> {
		after idle [list DemoTable::EntryExpanderKeypress [winfo parent %W]]
	    }
	    # Now that the text is set, reposition the entry widget.
	    scan [$T item bbox $Priv(item) $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
	    set left [expr {$x1 + 2 - 1}]
	    set right [expr {$x2 - 2}]
	    place $T.entry -x $left -width [expr {$right - $left}]
	    update idletasks
	    # Emulate a button press in the entry widget.
	    set entryX [expr {$x - $left}]
	    set pos [$T.entry index @$entryX]
	    set bbox [$T.entry bbox $pos]
	    if {($entryX - [lindex $bbox 0]) >= ([lindex $bbox 2]/2)} {
		incr pos
	    }
	    $T.entry icursor [$T.entry index $pos]
	}
	drag {
	    $T dragimage configure -visible no
	    $T item state forcolumn $Priv(item) $Priv(column) !drag
	    $T identify -array id $x $y
	    if {$id(where) ne "item"} return
	    if {[$T column cget $id(column) -lock] ne "none"} return
	    if {[$T item compare $id(item) == $Priv(item)] &&
		[$T column compare $id(column) == $Priv(column)]} return
	    set textSource [$T item text $Priv(item) $Priv(column)]
	    set textDest [$T item text $id(item) $id(column)]
	    $T item text $id(item) $id(column) $textSource
	    if {!$modifiers(control) && !$modifiers(command)} {
		$T item text $Priv(item) $Priv(column) $textDest
	    }
	}
	resize {
	    $T item state forcolumn $Priv(item) $Priv(column) !drag
	}
    }
    set Priv(buttonMode) ""
    return
}

proc DemoTable::Motion {T x y} {
    variable Priv
    $T identify -array id $x $y
    set Priv(cursor,want) ""
    set Priv(highlight,want) {}
    if {$id(where) ne "item"} return
    if {$id(column) eq "tail"} return
    if {[$T column tag expr $id(column) rowtitle]} return
    set Priv(highlight,want) [list $id(item) $id(column) mouseover]
    switch -- [WhichSide $T $id(item) $id(column) $x $y] {
	left {
	    if {[$T column compare $id(column) > "first visible lock none"]} {
		set Priv(cursor,want) sb_h_double_arrow
		set prev [StartOfPrevSpan $T $id(item) $id(column)]
		set Priv(highlight,want) [list $id(item) $prev mouseover]
	    }
	}
	right {
	    if {[$T column compare $id(column) < "last visible lock none"]} {
		set Priv(cursor,want) sb_h_double_arrow
		set Priv(highlight,want) [list $id(item) $id(column) mouseover]
	    }
	}
    }
    return
}

proc DemoTable::MaintainCursor {T} {
    variable Priv
    if {!$Priv(cursor,set) && $Priv(cursor,want) ne ""} {
	$T configure -cursor $Priv(cursor,want)
	set Priv(cursor,set) 1
	return
    }
    if {$Priv(cursor,set) && [$T cget -cursor] ne $Priv(cursor,want)} {
	$T configure -cursor $Priv(cursor,want)
    }
    if {$Priv(cursor,set) && $Priv(cursor,want) eq ""} {
	set Priv(cursor,set) 0
    }
    return
}

proc DemoTable::MaintainHighlight {T} {
    variable Priv
    if {$Priv(highlight) ne $Priv(highlight,want)} {
	if {$Priv(highlight) ne ""} {
	    lassign $Priv(highlight) item column state
	    $T item state forcolumn $item $column {!drop !mouseover}
	}
	if {$Priv(highlight,want) ne ""} {
	    lassign $Priv(highlight,want) item column state
	    $T item state forcolumn $item $column [list !drop !mouseover $state]
	}
	set Priv(highlight) $Priv(highlight,want)
    }
    return
}

proc DemoTable::StartOfNextSpan {T I C} {
    set span [$T item span $I $C]
    set last [$T column id "$C span $span"]
    return [$T column id "$last next visible"]
}

proc DemoTable::StartOfPrevSpan {T I C} {
    set prev [$T column id "$C prev visible"]
    if {$prev ne ""} {
	set starts [GetSpanStarts $T $I]
	return [lindex $starts [$T column order $prev]]
    }
    return ""
}

proc DemoTable::IncrSpan {T I C newLast} {
    set span [expr {[$T column order $newLast] - [$T column order $C] + 1}]
    $T item span $I $C $span
    return
}

proc DemoTable::DecrSpan {T I C newLast} {
    set span [expr {[$T column order $newLast] - [$T column order $C] + 1}]
    $T item span $I $C $span
    return
}

proc DemoTable::ColumnUnderPoint {T x y} {
    #return [$T column id "nearest $x $y"]
    set totalWidth [lindex [$T cget -canvaspadx] 0]
    foreach C [$T column id "lock none"] {
	incr totalWidth [$T column width $C]
	if {[$T canvasx $x] < $totalWidth} {
	    return $C
	}
    }
    return ""
}

proc DemoTable::WhichSide {T I C x y} {
    scan [$T item bbox $I $C] "%d %d %d %d" x1 y1 x2 y2
    if {$x < $x1 + 5} { return left }
    if {$x >= $x2 - 5} { return right }
    return
}

proc DemoTable::WhichHalf {T C x y} {
    scan [$T column bbox $C] "%d %d %d %d" x1 y1 x2 y2
    if {$x < $x1 + ($x2 - $x1) / 2} { return left }
    return right
}

proc DemoTable::GetSpanStarts {T I} {
    set columns [list]
    set spans [$T item span $I]
    if {[lindex [lsort -integer $spans] end] eq 1} {
	return [$T column list]
    }
    for {set index 0} {$index < [$T column count]} {}  {
	set Cspan [$T column id "order $index"]
	set span [lindex $spans $index]
	if {![$T column cget $Cspan -visible]} {
	    set span 1
	}
	while {$span > 0 && $index < [$T column count]} {
	    if {[$T column cget "order $index" -lock] ne [$T column cget $Cspan -lock]} break
	    lappend columns $Cspan
	    incr span -1
	    incr index
	}
    }
    return $columns
}

# This is bad, relying on all sorts of private stuff in the library code.
proc DemoTable::EntryExpanderKeypress {T} {

    variable ::TreeCtrl::Priv

    if {![winfo exists $T]} return

    set font $Priv(entry,$T,font)
    set text [$T.entry get]
    set ebw [$T.entry cget -borderwidth]
    set ex [winfo x $T.entry]

    scan [$T item bbox $Priv(entry,$T,item) $Priv(entry,$T,column)] "%d %d %d %d" x1 y1 x2 y2
    set left [expr {$x1 + 2 - 1}]
    set right [expr {$x2 - 2}]
    set width [expr {$right - $left}]

    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }

    place configure $T.entry -width $width

    return
}
