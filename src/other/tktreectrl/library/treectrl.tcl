# Copyright (c) 2002-2011 Tim Baker

bind TreeCtrl <Motion> {
    TreeCtrl::CursorCheck %W %x %y
    TreeCtrl::MotionInHeader %W %x %y
    TreeCtrl::MotionInButtons %W %x %y
}
bind TreeCtrl <Leave> {
    TreeCtrl::CursorCancel %W
    TreeCtrl::MotionInHeader %W
    TreeCtrl::MotionInButtons %W
}
bind TreeCtrl <ButtonPress-1> {
    TreeCtrl::ButtonPress1 %W %x %y
}
bind TreeCtrl <Double-ButtonPress-1> {
    TreeCtrl::DoubleButton1 %W %x %y
}
bind TreeCtrl <Button1-Motion> {
    TreeCtrl::Motion1 %W %x %y
}
bind TreeCtrl <ButtonRelease-1> {
    TreeCtrl::Release1 %W %x %y
}
bind TreeCtrl <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginExtend %W [%W item id {nearest %x %y}]
}
# Command-click should provide a discontinuous selection on OSX
switch -- [tk windowingsystem] {
    "aqua" { set modifier Command }
    default { set modifier Control }
}
bind TreeCtrl <$modifier-ButtonPress-1> {
    set TreeCtrl::Priv(buttonMode) normal
    TreeCtrl::BeginToggle %W [%W item id {nearest %x %y}]
}
bind TreeCtrl <Button1-Leave> {
    TreeCtrl::Leave1 %W %x %y
}
bind TreeCtrl <Button1-Enter> {
    TreeCtrl::Enter1 %W %x %y
}

bind TreeCtrl <KeyPress-Up> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W active -1]
}
bind TreeCtrl <Shift-KeyPress-Up> {
    TreeCtrl::Extend %W above
}
bind TreeCtrl <KeyPress-Down> {
    TreeCtrl::SetActiveItem %W [TreeCtrl::UpDown %W active 1]
}
bind TreeCtrl <Shift-KeyPress-Down> {
    TreeCtrl::Extend %W below
}
bind TreeCtrl <KeyPress-Left> {
    if {![TreeCtrl::Has2DLayout %W]} {
	%W item collapse [%W item id active]
    } else {
	TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W active -1]
    }
}
bind TreeCtrl <Shift-KeyPress-Left> {
    TreeCtrl::Extend %W left
}
bind TreeCtrl <Control-KeyPress-Left> {
    %W xview scroll -1 pages
}
bind TreeCtrl <KeyPress-Right> {
    if {![TreeCtrl::Has2DLayout %W]} {
	%W item expand [%W item id active]
    } else {
	TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W active 1]
    }
}
bind TreeCtrl <Shift-KeyPress-Right> {
    TreeCtrl::Extend %W right
}
bind TreeCtrl <Control-KeyPress-Right> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Prior> {
    %W yview scroll -1 pages
    if {[%W item id {nearest 0 0}] ne ""} {
	%W activate {nearest 0 0}
    }
}
bind TreeCtrl <KeyPress-Next> {
    %W yview scroll 1 pages
    if {[%W item id {nearest 0 0}] ne ""} {
	%W activate {nearest 0 0}
    }
}
bind TreeCtrl <Control-KeyPress-Prior> {
    %W xview scroll -1 pages
}
bind TreeCtrl <Control-KeyPress-Next> {
    %W xview scroll 1 pages
}
bind TreeCtrl <KeyPress-Home> {
    %W xview moveto 0
}
bind TreeCtrl <KeyPress-End> {
    %W xview moveto 1
}
bind TreeCtrl <Control-KeyPress-Home> {
    TreeCtrl::SetActiveItem %W [%W item id {first visible state enabled}]
}
bind TreeCtrl <Shift-Control-KeyPress-Home> {
    TreeCtrl::DataExtend %W [%W item id {first visible state enabled}]
}
bind TreeCtrl <Control-KeyPress-End> {
    TreeCtrl::SetActiveItem %W [%W item id {last visible state enabled}]
}
bind TreeCtrl <Shift-Control-KeyPress-End> {
    TreeCtrl::DataExtend %W [%W item id {last visible state enabled}]
}
bind TreeCtrl <<Copy>> {
    if {[string equal [selection own -displayof %W] "%W"]} {
	clipboard clear -displayof %W
	clipboard append -displayof %W [selection get -displayof %W]
    }
}
bind TreeCtrl <KeyPress-space> {
    TreeCtrl::BeginSelect %W [%W item id active]
}
bind TreeCtrl <KeyPress-Select> {
    TreeCtrl::BeginSelect %W [%W item id active]
}
bind TreeCtrl <Control-Shift-KeyPress-space> {
    TreeCtrl::BeginExtend %W [%W item id active]
}
bind TreeCtrl <Shift-KeyPress-Select> {
    TreeCtrl::BeginExtend %W [%W item id active]
}
bind TreeCtrl <KeyPress-Escape> {
    TreeCtrl::Cancel %W
}
bind TreeCtrl <Control-KeyPress-slash> {
    TreeCtrl::SelectAll %W
}
bind TreeCtrl <Control-KeyPress-backslash> {
    if {[string compare [%W cget -selectmode] "browse"]} {
	%W selection clear
    }
}

bind TreeCtrl <KeyPress-plus> {
    %W item expand [%W item id active]
}
bind TreeCtrl <KeyPress-minus> {
    %W item collapse [%W item id active]
}
bind TreeCtrl <KeyPress-Return> {
    %W item toggle [%W item id active]
}


# Additional Tk bindings that aren't part of the Motif look and feel:

bind TreeCtrl <ButtonPress-2> {
    focus %W
    TreeCtrl::ScanMark %W %x %y
}
bind TreeCtrl <Button2-Motion> {
    TreeCtrl::ScanDrag %W %x %y
}

if {$tcl_platform(platform) eq "windows"} {
    bind TreeCtrl <Control-ButtonPress-3> {
	TreeCtrl::ScanMark %W %x %y
    }
    bind TreeCtrl <Control-Button3-Motion> {
	TreeCtrl::ScanDrag %W %x %y
    }
}
if {[string equal [tk windowingsystem] "aqua"]} {
    # Middle mouse on Mac OSX
    bind TreeCtrl <ButtonPress-3> {
	TreeCtrl::ScanMark %W %x %y
    }
    bind TreeCtrl <Button3-Motion> {
	TreeCtrl::ScanDrag %W %x %y
    }
}

# MouseWheel
if {[string equal "x11" [tk windowingsystem]]} {
    # Support for mousewheels on Linux/Unix commonly comes through mapping
    # the wheel to the extended buttons.  If you have a mousewheel, find
    # Linux configuration info at:
    #	http://www.inria.fr/koala/colas/mouse-wheel-scroll/
    bind TreeCtrl <4> {
	if {!$tk_strictMotif} {
	    %W yview scroll -5 units
	}
    }
    bind TreeCtrl <Shift-4> {
	if {!$tk_strictMotif} {
	    %W xview scroll -5 units
	}
    }
    bind TreeCtrl <5> {
	if {!$tk_strictMotif} {
	    %W yview scroll 5 units
	}
    }
    bind TreeCtrl <Shift-5> {
	if {!$tk_strictMotif} {
	    %W xview scroll 5 units
	}
    }
} elseif {[string equal [tk windowingsystem] "aqua"]} {
    bind TreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D)}] units
    }
    bind TreeCtrl <Option-MouseWheel> {
        %W yview scroll [expr {-10 * (%D)}] units
    }
    bind TreeCtrl <Shift-MouseWheel> {
        %W xview scroll [expr {- (%D)}] units
    }
    bind TreeCtrl <Shift-Option-MouseWheel> {
        %W xview scroll [expr {-10 * (%D)}] units
    }
} else {
    bind TreeCtrl <MouseWheel> {
	%W yview scroll [expr {- (%D / 120) * 4}] units
    }
    bind TreeCtrl <Shift-MouseWheel> {
        %W xview scroll [expr {- (%D / 120) * 4}] units
    }
}

namespace eval ::TreeCtrl {
    variable Priv
    array set Priv {
	prev {}
    }

    if {[info procs ::lassign] eq ""} {
	proc lassign {values args} {
	    uplevel 1 [list foreach $args [linsert $values end {}] break]
	    lrange $values [llength $args] end
	}
    }
}

# Retrieve filelist bindings from this dir
source [file join [file dirname [info script]] filelist-bindings.tcl]

# ::TreeCtrl::ColumnCanResizeLeft --
#
# Return 1 if the given column should be resized by the left edge.
#
# Arguments:
# w		The treectrl widget.
# column	The column.

proc ::TreeCtrl::ColumnCanResizeLeft {w column} {
    if {[$w column cget $column -lock] eq "right"} {
	return 1
    }
    return 0
}

# ::TreeCtrl::ColumnCanMoveHere --
#
# Return 1 if the given column can be moved before another.
#
# Arguments:
# w		The treectrl widget.
# column	The column.
# before	The column to place 'column' before.

proc ::TreeCtrl::ColumnCanMoveHere {w column before} {
    if {[$w column compare $column == $before] ||
	    ([$w column order $column] == [$w column order $before] - 1)} {
	return 0
    }
    set lock [$w column cget $column -lock]
    return [expr {[$w column compare $before >= "first lock $lock"] &&
	[$w column compare $before <= "last lock $lock next"]}]
}

# ::TreeCtrl::ColumnDragFindBefore --
#
# This is called when dragging a column header. The result is 1 if the given
# coordinates are near a column header before which the dragged column can
# be moved.
#
# Arguments:
# w		The treectrl widget.
# x		Window x-coord.
# y		Window y-coord.
# dragColumn	The column being dragged.
# indColumn_	Out: what to set -indicatorcolumn to.
# indSide_	Out: what to set -indicatorside to.

proc ::TreeCtrl::ColumnDragFindBefore {w x y dragColumn indColumn_ indSide_} {
    upvar $indColumn_ indColumn
    upvar $indSide_ indSide

    set lock [$w column cget $dragColumn -lock]
    scan [$w bbox header.$lock] "%d %d %d %d" minX y1 maxX y2
    if {$x < $minX} {
	set x $minX
    }
    if {$x >= $maxX} {
	set x [expr {$maxX - 1}]
    }
    $w identify -array id $x $y
    if {$id(where) ne "header"} {
	return 0
    }
    set indColumn $id(column)
    if {[$w column compare $indColumn == $dragColumn]} {
	return 0
    }

    # The given $x is either the left edge or the right edge of the column
    # header that is being dragged depending on which direction the user
    # is dragging the column.
    # When dragging to the left, the indicator column is chosen to be the
    # leftmost column whose mid-way point is greater than the left edge of the
    # dragged header.
    # When dragging to the right, the indicator column is chosen to be the
    # rightmost column whose mid-way point is less than the right edge of the
    # dragged header.
    if {[$w column compare $indColumn != "tail"]} {
	variable Priv
	scan [$w header bbox $Priv(header) $indColumn] "%d %d %d %d" x1 y1 x2 y2
	# Hack - ignore canvaspadx
	if {[$w column cget $indColumn -lock] eq "none" &&
		[$w column compare $indColumn == "first visible lock none"]} {
	    incr x1 [lindex [$w cget -canvaspadx] 0]
	}
	if {[$w column compare $dragColumn < $indColumn]} {
	    if {$x < $x1 + ($x2 - $x1) / 2} {
		set indColumn [$w column id "$indColumn prev visible"]
		set indColumn [GetSpanStartColumn $w $Priv(header) $indColumn]
	    }
	} else {
	    if {$x > $x1 + ($x2 - $x1) / 2} {
		# Find the column at the start of the next visible span
		set starts [GetSpanStarts $w $Priv(header)]
		for {set i [$w column order $indColumn]} {true} {incr i} {
		    if {[$w column compare [lindex $starts $i] > $indColumn]} break
		}
		set indColumn [lindex $starts $i]
	    }
	}
    }

    set before $indColumn
    set prev [$w column id "$dragColumn prev visible"]
    set next [$w column id "$dragColumn next visible"]
    if {[$w column compare $indColumn == "tail"]} {
	set indSide left
	set indColumn [$w column id "last lock none visible"]
	set indSide right
    } elseif {$prev ne "" && [$w column compare $prev == $indColumn]} {
	set indSide left
    } elseif {$next ne "" && [$w column compare $next == $indColumn]} {
	set before [$w column id "$indColumn next visible"]
	set indSide right
    } else {
	scan [$w column bbox $indColumn] "%d %d %d %d" x1 y1 x2 y2
	if {$x < $x1 + ($x2 - $x1) / 2} {
	    set indSide left
	} else {
	    set before [$w column id "$indColumn next visible"]
	    set indSide right
	}
    }
    if {$before eq "" || [$w column compare $before > "last lock $lock next"]} {
	set before [$w column id "last lock $lock next"]
    }
    return [ColumnCanMoveHere $w $dragColumn $before]
}

# ::TreeCtrl::ListElementWindows --
#
# Return a list of Tk windows in window elements in a column header.
#
# Arguments:
# T		The treectrl widget.
# H		Header id
# C		Column id

proc ::TreeCtrl::ListElementWindows {T H C} {
    set S [$T header style set $H $C]
    if {$S eq ""} return
    set result {}
    foreach E [$T header style elements $H $C] {
	if {[$T element type $E] eq "window"} {
	    set window [$T header element cget $H $C $E -window]
	    if {$window ne ""} {
		lappend result $window
	    }
	}
    }
    return $result
}

# ::TreeCtrl::ColumnDragRestackWindows --
#
# Restack windows in window elements so that windows in dragged headers
# are above all other windows in undragged headers.
#
# Arguments:
# T		The treectrl widget.

proc ::TreeCtrl::ColumnDragRestackWindows {T} {
    variable Priv
    set C [$T header dragcget -imagecolumn]
    set lock [$T column cget $C -lock]
    set span [$T header dragcget -imagespan]
    set last [$T column id [list $C span $span]]
    set dragged [$T column id [list range $C $last]]
    foreach H [$T header id all] {
	set prev ""
	set lowest ""
	foreach C $dragged {
	    foreach win [ListElementWindows $T $H $C] {
		if {$prev eq ""} {
		    set lowest $win
		} else {
		    raise $win $prev
		}
		set prev $win
	    }
	}
	if {$lowest eq ""} continue
	foreach C [$T column id "lock $lock !tail"] {
	    if {[lsearch -exact $dragged $C] != -1} continue
	    foreach win [ListElementWindows $T $H $C] {
		lower $win $lowest
	    }
	}
    }
    return
}

# ::TreeCtrl::CursorAction --
#
# If the given point is at the left or right edge of a resizable column
# header, the result is "action header-resize header H column C".
# If the given point is in a header with -button=TRUE, the result is
# "action header-button header H column C".
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::CursorAction {w x y var_} {
    upvar $var_ var
    variable Priv
    $w identify -array id $x $y

    set var(action) ""
    if {$id(where) eq "header"} {
	set var(header) $id(header)
	set column $id(column)
	set side $id(side)
	if {$side eq ""} {
	    if {[scan [$w bbox header.left] "%d %d %d %d" x1 y1 x2 y2] == 4} {
		if {$x < $x2 + 4 && $x >= $x2} {
		    set column [$w column id "last visible lock left"]
		    set side right
		}
	    }
	    if {[scan [$w bbox header.right] "%d %d %d %d" x1 y1 x2 y2] == 4} {
		if {$x >= $x1 - 4 && $x < $x1} {
		    set column [$w column id "first visible lock right"]
		    set side left
		}
	    }
	}
	if {$side eq "left"} {
	    if {[ColumnCanResizeLeft $w $column]} {
		if {[$w column cget $column -resize]} {
		    array set var [list action "header-resize" column $column]
		    return
		}
	    } else {
		# Resize the previous column
		if {[$w column compare $column == tail]} {
		    set prev [$w column id "last visible lock none"]
		    if {$prev eq ""} {
			set prev [$w column id "last visible lock left"]
		    }
		} else {
		    set prev [$w column id "$column prev visible"]
		}
		if {$prev ne "" && [$w column cget $prev -resize]} {
		    array set var [list action "header-resize" column $prev]
		    return
		}
	    }
	} elseif {$side eq "right"} {
	    # Get the last visible column in the span
	    set span [$w header span $id(header) $column]
	    set last [$w column id "$column span $span"]
	    set columns [$w column id [list range $column $last visible]]
	    set column2 [lindex $columns end]
	    if {[ColumnCanResizeLeft $w $column2]} {
		# Resize the next column
		set next [$w column id "$column2 next visible !tail"]
		if {$next ne "" && [$w column cget $next -resize]} {
		    array set var [list action "header-resize" column $next]
		    return
		}
	    } else {
		if {[$w column cget $column2 -resize]} {
		    array set var [list action "header-resize" column $column2]
		    return
		}
	    }
	}
	if {[$w column compare $column == "tail"]} {
	    # Can't -resize or -button the tail column
	} elseif {[$w header cget $id(header) $column -button]} {
	    array set var [list action "header-button" column $column]
	    return
	}
    }
    return
}

# ::TreeCtrl::CursorCheck --
#
# Sees if the given pointer coordinates are near the edge of a resizable
# column in the header. If so and the treectrl's cursor is not already
# set to sb_h_double_arrow, then the current cursor is saved and changed
# to sb_h_double_arrow, and an [after] callback to CursorCheckAux is
# scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::CursorCheck {w x y} {
    variable Priv
    CursorAction $w $x $y action
    # If we are in the middle of resizing a column, don't cancel the cursor
    if {[info exists Priv(buttonMode)] && $Priv(buttonMode) eq "resize"} {
	array set action {action "header-resize" header XXX column XXX}
    }
    if {$action(action) ne "header-resize"} {
	CursorCancel $w
	return
    }
    set cursor sb_h_double_arrow
    if {$cursor ne [$w cget -cursor]} {
	if {![info exists Priv(cursor,$w)]} {
	    set Priv(cursor,$w) [$w cget -cursor]
	}
	$w configure -cursor $cursor
    }
    if {[info exists Priv(cursor,afterId,$w)]} {
	after cancel $Priv(cursor,afterId,$w)
    }
    set Priv(cursor,afterId,$w) [after 150 [list TreeCtrl::CursorCheckAux $w]]
    return
}

# ::TreeCtrl::CursorCheckAux --
#
# Get's the location of the pointer and calls CursorCheck if the treectrl's
# cursor was previously set to sb_h_double_arrow.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::CursorCheckAux {w} {
    variable Priv
    if {![winfo exists $w]} return
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    if {[info exists Priv(cursor,$w)]} {
	set x [expr {$x - [winfo rootx $w]}]
	set y [expr {$y - [winfo rooty $w]}]
	CursorCheck $w $x $y
    }
    return
}

# ::TreeCtrl::CursorCancel --
#
# Restores the treectrl's cursor if it was changed to sb_h_double_arrow.
# Cancels any pending [after] callback to CursorCheckAux.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::CursorCancel {w} {
    variable Priv
    if {[info exists Priv(cursor,$w)]} {
	$w configure -cursor $Priv(cursor,$w)
	unset Priv(cursor,$w)
    }
    if {[info exists Priv(cursor,afterId,$w)]} {
	after cancel $Priv(cursor,afterId,$w)
	unset Priv(cursor,afterId,$w)
    }
    return
}

# ::TreeCtrl::GetSpanStarts --
#
# This procedure returns a list of column ids, one per tree column.
# Each column id indicates the column at the start of a span.
#
# Arguments:
# T		The treectrl widget.
# H		Header id

proc ::TreeCtrl::GetSpanStarts {T H} {
    set columns [list]
    set spans [$T header span $H]
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

# ::TreeCtrl::GetSpanStartColumn --
#
# This procedure returns the column at the start of a span which covers the
# given column.
#
# Arguments:
# T		The treectrl widget.
# H		Header id
# C		Column id

proc ::TreeCtrl::GetSpanStartColumn {T H C} {
    set columns [GetSpanStarts $T $H]
    return [lindex $columns [$T column order $C]]
}

# ::TreeCtrl::SetHeaderState --
#
# This procedure sets the state of a header-column and remembers that
# header-column.  If a different header-column is passed later the previous
# header-column's state is set to 'normal'.
#
# Arguments:
# T		The treectrl widget.
# H		Header id
# C		Column id
# state		active|normal|pressed

proc ::TreeCtrl::SetHeaderState {T H C state} {
    variable Priv
    if {[info exists Priv(inheader,$T)]} {
	lassign $Priv(inheader,$T) Hprev Cprev
    } else {
	if {$H eq "" || $C eq ""} return
	set Hprev [set Cprev ""]
    }
    if {$H ne $Hprev || $C ne $Cprev} {
	if {$Hprev ne "" && [$T header id $Hprev] ne ""} {
	    if {$Cprev ne "" && [$T column id $Cprev] ne ""} {
		$T header configure $Hprev $Cprev -state normal
		TryEvent $T Header state [list H $Hprev C $Cprev s normal]
	    }
	}
    }
    if {$H eq "" || $C eq ""} {
	unset Priv(inheader,$T)
    } else {
	$T header configure $H $C -state $state
	TryEvent $T Header state [list H $H C $C s $state]
	set Priv(inheader,$T) [list $H $C]
    }
    return
}

# ::TreeCtrl::ClearHeaderState --
#
# If a header-column's state was previously set via SetHeaderState then
# that column's state is set to normal and the header-column is forgotten.
#
# Arguments:
# T		The treectrl widget.
# H		Header id
# C		Column id
# state		active|normal|pressed

proc ::TreeCtrl::ClearHeaderState {T} {
    SetHeaderState $T "" "" ""
    return
}

# ::TreeCtrl::MotionInHeader --
#
# This procedure updates the active/normal states of column headers as the
# mouse pointer moves in and out of them. Typically this results in visual
# feedback by changing the appearance of the headers.
#
# Arguments:
# w		The treectrl widget.
# args		x y coords if the pointer is in the window, or an empty list.

proc ::TreeCtrl::MotionInHeader {w args} {
    variable Priv
    if {[llength $args]} {
	set x [lindex $args 0]
	set y [lindex $args 1]
	CursorAction $w $x $y action
    } else {
	array set action {action ""}
    }
    if {[info exists Priv(inheader,$w)]} {
	lassign $Priv(inheader,$w) headerPrev columnPrev
    } else {
	set headerPrev [set columnPrev ""]
    }
    set header ""
    set column ""
    if {$action(action) eq "header-button"} {
	set header $action(header)
	set column $action(column)
    } elseif {$action(action) eq "header-resize"} {
	set header $action(header)
	set column [GetSpanStartColumn $w $header $action(column)]
    }
    if {$header ne $headerPrev || $column ne $columnPrev} {
	if {$column ne ""} {
	    SetHeaderState $w $header $column active
	} else {
	    ClearHeaderState $w
	}
    }
    return
}

# ::TreeCtrl::MotionInButtons --
#
# This procedure updates the active/normal states of item buttons.
# Typically this results in visual feedback by changing the appearance
# of the buttons.
#
# Arguments:
# T		The treectrl widget.
# args		x y coords if the pointer is in the window, or an empty list.

proc ::TreeCtrl::MotionInButtons {T args} {
    variable Priv
    set button ""
    if {[llength $args]} {
	set x [lindex $args 0]
	set y [lindex $args 1]
	$T identify -array id $x $y
	if {$id(where) eq "item" && $id(button)} {
	    set button $id(item)
	}
    }
    if {[info exists Priv(inbutton,$T)]} {
	set prevButton $Priv(inbutton,$T)
    } else {
	set prevButton ""
    }
    if {$button ne $prevButton} {
	if {$prevButton ne ""} {
	    if {[$T item id $prevButton] ne ""} {
		$T item buttonstate $prevButton normal
	    }
	}
	if {$button ne ""} {
	    $T item buttonstate $button active
	    set Priv(inbutton,$T) $button
	} else {
	    unset Priv(inbutton,$T)
	}
    }
    if {[$T notify bind TreeCtrlButtonNotifyScroll] eq ""} {
	$T notify bind TreeCtrlButtonNotifyScroll <Scroll> {
	    TreeCtrl::ButtonNotifyScroll %T
	}
    }
    return
}

# ::TreeCtrl::ButtonNotifyScroll --
#
# Called when a <Scroll> event occurs and a button is in the active state.
# Finds the mouse pointer coords and calls MotionInButtons to update the
# state of affected buttons.
#
# Arguments:
# T		The treectrl widget.

proc ::TreeCtrl::ButtonNotifyScroll {T} {
    set x [expr {[winfo pointerx $T] - [winfo rootx $T]}]
    set y [expr {[winfo pointery $T] - [winfo rooty $T]}]
    MotionInButtons $T $x $y
    return
}

# ::TreeCtrl::ButtonPress1 --
#
# Handle <ButtonPress-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::ButtonPress1 {w x y} {
    variable Priv
    focus $w

    $w identify -array id $x $y
    if {$id(where) eq ""} {
	return
    }

    if {$id(where) eq "item"} {
	set item $id(item)
	if {$id(button)} {
	    if {[$w cget -buttontracking]} {
		$w item buttonstate $item pressed
		set Priv(buttonMode) buttonTracking
		set Priv(buttontrack,item) $item
	    } else {
		$w item toggle $item -animate
	    }
	    return
	} elseif {$id(line) ne ""} {
	    $w item toggle $id(line)
	    return
	}
    }
    set Priv(buttonMode) ""
    if {$id(where) eq "header"} {
	CursorAction $w $x $y action
	if {$action(action) eq "header-resize"} {
	    set column $action(column)
	    set Priv(buttonMode) resize
	    set Priv(header) $action(header)
	    set Priv(column) $column
	    set Priv(x) $x
	    set Priv(y) $y
	    set Priv(width) [$w column width $column]
	    return
	}
	set column $id(column)
	if {$action(action) eq "header-button"} {
	    set Priv(buttonMode) header
	    SetHeaderState $w $action(header) $column pressed
	} else {
	    if {[$w column compare $column == "tail"]} return
	    if {![$w header dragcget -enable]} return
	    if {![$w header dragcget $action(header) -enable]} return
	    set Priv(buttonMode) dragColumnWait
	}
	set Priv(header) $action(header)
	set Priv(column) $column
	set Priv(columnDrag,x) $x
	set Priv(columnDrag,y) $y
	return
    }
    set item $id(item)
    if {![$w item enabled $item]} {
	return
    }

    # If the initial mouse-click is in a locked column, restrict scrolling
    # to the vertical.
    set count [scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2]
    if {$count != -1 && $x >= $x1 && $x < $x2} {
	set Priv(autoscan,direction,$w) xy
    } else {
	set Priv(autoscan,direction,$w) y
    }

    set Priv(buttonMode) normal
    BeginSelect $w $item
    return
}

# ::TreeCtrl::DoubleButtonPress1 --
#
# Handle <Double-ButtonPress-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::DoubleButton1 {w x y} {

    $w identify -array id $x $y
    if {$id(where) eq ""} {
	return
    }
    if {$id(where) eq "item"} {
	if {$id(button)} {
	    if {[$w cget -buttontracking]} {
		# There is no <ButtonRelease> so just toggle it
		$w item toggle $id(item) -animate
	    } else {
		$w item toggle $id(item) -animate
	    }
	    return
	} elseif {$id(line) ne ""} {
	    $w item toggle $id(line)
	    return
	}
    }
    if {$id(where) eq "header"} {
	CursorAction $w $x $y action
	# Double-click between columns to set default column width
	if {$action(action) eq "header-resize"} {
	    set column $action(column)
	    $w column configure $column -width ""
	    CursorCheck $w $x $y
	    MotionInHeader $w $x $y
	} else {
	    ButtonPress1 $w $x $y
	}
    }
    return
}

# ::TreeCtrl::Motion1 --
#
# Handle <Button1-Motion> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Motion1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    $w identify -array id $x $y
	    if {$id(where) ne "header" ||
		    $id(header) ne $Priv(header) ||
		    $id(column) ne $Priv(column)} {
		if {[$w header cget $Priv(header) $Priv(column) -state] eq "pressed"} {
		    ClearHeaderState $w
		}
	    } else {
		if {[$w header cget $Priv(header) $Priv(column) -state] ne "pressed"} {
		    SetHeaderState $w $Priv(header) $Priv(column) pressed
		}
		if {[$w header dragcget -enable] &&
		    [$w header dragcget $Priv(header) -enable] &&
		    (abs($Priv(columnDrag,x) - $x) > 4)} {
		    set Priv(columnDrag,x) $x
		    $w header dragconfigure \
			-imagecolumn $Priv(column) \
			-imageoffset [expr {$x - $Priv(columnDrag,x)}] \
			-imagespan [$w header span $Priv(header) $Priv(column)]
		    ColumnDragRestackWindows $w
		    set Priv(buttonMode) dragColumn
		    TryEvent $w ColumnDrag begin [list H $Priv(header) C $Priv(column)]
		    # Allow binding scripts to cancel the drag
		    if {[$w header dragcget -imagecolumn] eq ""} {
			set Priv(buttonMode) header
		    }
		}
	    }
	}
	buttonTracking {
	    $w identify -array id $x $y
	    set itemTrack $Priv(buttontrack,item)
	    set exists [expr {[$w item id $itemTrack] ne ""}]
	    set mouseover 0
	    if {$id(where) eq "item" && $id(button)} {
		if {$exists && [$w item compare $itemTrack == $id(item)]} {
		    set mouseover 1
		}
	    }
	    if {$mouseover} {
		$w item buttonstate $itemTrack pressed
	    } elseif {$exists} {
		$w item buttonstate $itemTrack normal
	    }
	}
	dragColumnWait {
	    if {(abs($Priv(columnDrag,x) - $x) > 4)} {
		set Priv(columnDrag,x) $x
		$w header dragconfigure \
		    -imagecolumn $Priv(column) \
		    -imageoffset [expr {$x - $Priv(columnDrag,x)}] \
		    -imagespan [$w header span $Priv(header) $Priv(column)]
		ColumnDragRestackWindows $w
		set Priv(buttonMode) dragColumn
		TryEvent $w ColumnDrag begin [list H $Priv(header) C $Priv(column)]
		# Allow binding scripts to cancel the drag
		if {[$w header dragcget -imagecolumn] eq ""} {
		    unset Priv(buttonMode)
		}
	    }
	}
	dragColumn {
	    scan [$w bbox header] "%d %d %d %d" x1 y1 x2 y2
	    if {$y < $y1 - 30 || $y >= $y2 + 30} {
		set inside 0
	    } else {
		set inside 1
	    }
	    if {$inside && ([$w header dragcget -imagecolumn] eq "")} {
		$w header dragconfigure -imagecolumn $Priv(column)
	    } elseif {!$inside && ([$w header dragcget -imagecolumn] ne "")} {
		$w header dragconfigure -imagecolumn "" -indicatorcolumn ""
	    }
	    if {$inside} {
		set offset [expr {$x - $Priv(columnDrag,x)}]
		$w header dragconfigure -imageoffset $offset

		# When dragging to the left, use the left edge of the dragged
		# header to choose the -indicatorcolumn.  When dragging to the
		# right, use the right edge.
		scan [$w header bbox $Priv(header) $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		if {$offset > 0} {
		    set xEdge [expr {$offset + $x2}]
		} else {
		    set xEdge [expr {$offset + $x1}]
		}

		if {[ColumnDragFindBefore $w $xEdge $Priv(columnDrag,y) $Priv(column) indColumn indSide]} {
		    set prevIndColumn [$w header dragcget -indicatorcolumn]
		    $w header dragconfigure \
			-indicatorcolumn $indColumn \
			-indicatorside $indSide \
			-indicatorspan [$w header span $Priv(header) $indColumn]
		    if {$indColumn != $prevIndColumn} {
			TryEvent $w ColumnDrag indicator [list H $Priv(header) C $indColumn]
		    }
		} else {
		    $w header dragconfigure -indicatorcolumn ""
		}
	    }
	    if {[$w column cget $Priv(column) -lock] eq "none"} {
		ColumnDragScrollCheck $w $x $y
	    }
	}
	normal {
	    set Priv(x) $x
	    set Priv(y) $y
	    SelectionMotion $w [$w item id [list nearest $x $y]]
	    set Priv(autoscan,command,$w) {SelectionMotion %T [%T item id "nearest %x %y"]}
	    AutoScanCheck $w $x $y
	}
	resize {
	    if {[ColumnCanResizeLeft $w $Priv(column)]} {
		set width [expr {$Priv(width) + $Priv(x) - $x}]
	    } else {
		set width [expr {$Priv(width) + $x - $Priv(x)}]
	    }
	    set minWidth [$w column cget $Priv(column) -minwidth]
	    set maxWidth [$w column cget $Priv(column) -maxwidth]
	    if {$minWidth eq ""} {
		set minWidth 0
	    }
	    if {$width < $minWidth} {
		set width $minWidth
	    }
	    if {($maxWidth ne "") && ($width > $maxWidth)} {
		set width $maxWidth
	    }
	    if {$width == 0} {
		incr width
	    }
	    switch -- [$w cget -columnresizemode] {
		proxy {
		    scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		    if {[ColumnCanResizeLeft $w $Priv(column)]} {
			# Use "ne" because -columnproxy could be ""
			if {$x2 - $width ne [$w cget -columnproxy]} {
			    $w configure -columnproxy [expr {$x2 - $width}]
			}
		    } else {
			if {($x1 + $width - 1) ne [$w cget -columnproxy]} {
			    $w configure -columnproxy [expr {$x1 + $width - 1}]
			}
		    }
		}
		realtime {
		    if {[$w column cget $Priv(column) -width] != $width} {
			$w column configure $Priv(column) -width $width
		    }
		}
	    }
	}
    }
    return
}

# ::TreeCtrl::Leave1 --
#
# Handle <Button1-Leave> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Leave1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w header cget $Priv(header) $Priv(column) -state] eq "pressed"} {
		ClearHeaderState $w
	    }
	}
    }
    return
}

# ::TreeCtrl::Enter1 --
#
# Handle <Button1-Enter> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Enter1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	default {}
    }
    return
}

# ::TreeCtrl::Release1 --
#
# Handle <ButtonRelease-1> event.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::Release1 {w x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	header {
	    if {[$w header cget $Priv(header) $Priv(column) -state] eq "pressed"} {
		SetHeaderState $w $Priv(header) $Priv(column) active
		TryEvent $w Header invoke [list H $Priv(header) C $Priv(column)]
	    }
	    CursorCheck $w $x $y
	    MotionInHeader $w $x $y
	}
	buttonTracking {
	    $w identify -array id $x $y
	    set itemTrack $Priv(buttontrack,item)
	    set exists [expr {[$w item id $itemTrack] ne ""}]
	    if {$id(where) eq "item" && $id(button)} {
		if {$exists && [$w item compare $itemTrack == $id(item)]} {
		    $w item buttonstate $id(item) active
		    $w item toggle $itemTrack -animate
		}
	    }
	}
	dragColumn {
	    AutoScanCancel $w
	    ClearHeaderState $w
	    if {[$w header dragcget -imagecolumn] ne ""} {
		set visible 1
	    } else {
		set visible 0
	    }
	    set column [$w header dragcget -indicatorcolumn]
	    $w header dragconfigure -imagecolumn "" -indicatorcolumn ""
	    if {$visible && ($column ne "")} {
		# If dragging to the right, drop after the last column in the
		# span of the indicator column.
		if {[$w column order $Priv(column)] < [$w column order $column]} {
		    set span [$w header dragcget -indicatorspan]
		    set column [$w column id "$column span $span next"]
		}
		set lock [$w column cget $Priv(column) -lock]
		if {$column eq "" || [$w column compare $column > "last lock $lock next"]} {
		    set column [$w column id "last lock $lock next"]
		}
		TryEvent $w ColumnDrag receive [list H $Priv(header) C $Priv(column) b $column]
	    }
	    CursorCheck $w $x $y
	    MotionInHeader $w $x $y
	    TryEvent $w ColumnDrag end [list H $Priv(header) C $Priv(column)]
	}
	normal {
	    AutoScanCancel $w
	    set nearest [$w item id [list nearest $x $y]]
	    if {$nearest ne ""} {
		$w activate $nearest
	    }
set Priv(prev) ""
	}
	resize {
	    if {[$w cget -columnproxy] ne ""} {
		scan [$w column bbox $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
		if {[ColumnCanResizeLeft $w $Priv(column)]} {
		    set width [expr {$x2 - [$w cget -columnproxy]}]
		} else {
		    set width [expr {[$w cget -columnproxy] - $x1 + 1}]
		}
		$w configure -columnproxy {}
		$w column configure $Priv(column) -width $width
	    }
	    # Clear buttonMode early so CursorCheck doesn't exit
	    unset Priv(buttonMode)
	    CursorCheck $w $x $y
	    MotionInHeader $w $x $y
	    return
	}
    }
    unset Priv(buttonMode)
    return
}

# ::TreeCtrl::BeginSelect --
#
# This procedure is typically invoked on button-1 presses.  It begins
# the process of making a selection in the treectrl.  Its exact behavior
# depends on the selection mode currently in effect for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginSelect {w item} {
    variable Priv
    if {$item eq ""} return
    if {[string equal [$w cget -selectmode] "multiple"]} {
	if {[$w selection includes $item]} {
	    $w selection clear $item
	} else {
	    $w selection add $item
	}
    } else {
	$w selection anchor $item
	$w selection modify $item all
	set Priv(selection) {}
	set Priv(prev) $item
    }
    return
}

# ::TreeCtrl::SelectionMotion --
#
# This procedure is called to process mouse motion events while
# button 1 is down.  It may move or extend the selection, depending
# on the treectrl's selection mode.
#
# Arguments:
# w		The treectrl widget.
# item-		The item under the pointer.

proc ::TreeCtrl::SelectionMotion {w item} {
    variable Priv

    if {$item eq ""} return
    set item [$w item id $item]
    if {$item eq $Priv(prev)} return
    if {![$w item enabled $item]} return

    switch [$w cget -selectmode] {
	browse {
	    $w selection modify $item all
	    set Priv(prev) $item
	}
	extended {
	    set i $Priv(prev)
	    set select {}
	    set deselect {}
	    if {$i eq ""} {
		set i $item
		lappend select $item
		set hack [$w item compare $item == anchor]
	    } else {
		set hack 0
	    }
	    if {[$w selection includes anchor] || $hack} {
		set deselect [concat $deselect [$w item range $i $item]]
		set select [concat $select [$w item range anchor $item]]
	    } else {
		set deselect [concat $deselect [$w item range $i $item]]
		set deselect [concat $deselect [$w item range anchor $item]]
	    }
	    if {![info exists Priv(selection)]} {
		set Priv(selection) [$w selection get]
	    }
	    while {[$w item compare $i < $item] && [$w item compare $i < anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    lappend select $i
		}
		set i [$w item id "$i next visible"]
	    }
	    while {[$w item compare $i > $item] && [$w item compare $i > anchor]} {
		if {[lsearch $Priv(selection) $i] >= 0} {
		    lappend select $i
		}
		set i [$w item id "$i prev visible"]
	    }
	    set Priv(prev) $item
	    $w selection modify $select $deselect
	}
    }
    return
}

# ::TreeCtrl::BeginExtend --
#
# This procedure is typically invoked on shift-button-1 presses.  It
# begins the process of extending a selection in the treectrl.  Its
# exact behavior depends on the selection mode currently in effect
# for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item-		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginExtend {w item} {
    if {[string equal [$w cget -selectmode] "extended"]} {
	if {[$w selection includes anchor]} {
	    SelectionMotion $w $item
	} else {
	    # No selection yet; simulate the begin-select operation.
	    BeginSelect $w $item
	}
    }
    return
}

# ::TreeCtrl::BeginToggle --
#
# This procedure is typically invoked on control-button-1 presses.  It
# begins the process of toggling a selection in the treectrl.  Its
# exact behavior depends on the selection mode currently in effect
# for the treectrl.
#
# Arguments:
# w		The treectrl widget.
# item		The item for the selection operation (typically the
#		one under the pointer).

proc ::TreeCtrl::BeginToggle {w item} {
    variable Priv
    if {$item eq ""} return
    if {[string equal [$w cget -selectmode] "extended"]} {
	set Priv(selection) [$w selection get]
	set Priv(prev) $item
	$w selection anchor $item
	if {[$w selection includes $item]} {
	    $w selection clear $item
	} else {
	    $w selection add $item
	}
    }
    return
}

# ::TreeCtrl::AutoScanCheck --
#
# Sees if the given pointer coords are outside the content area of the
# treectrl (ie, not including borders or column headers) or within
# -scrollmargin distance of the edges of the content area. If so and
# auto-scanning is not already in progress, then the window is scrolled
# and an [after] callback to AutoScanCheckAux is scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window x coord.
# y		Window y coord.

proc ::TreeCtrl::AutoScanCheck {w x y} {
    variable Priv
    # Could have clicked in locked column
    if {[scan [$w contentbox] "%d %d %d %d" x1 y1 x2 y2] == -1} {
	if {[scan [$w bbox left] "%d %d %d %d" x1 y1 x2 y2] == -1} {
	    scan [$w bbox right] "%d %d %d %d" x1 y1 x2 y2
	}
    }
    set margin [winfo pixels $w [$w cget -scrollmargin]]
    if {![info exists Priv(autoscan,direction,$w)]} {
	set Priv(autoscan,direction,$w) xy
    }
    set scrollX [string match *x* $Priv(autoscan,direction,$w)]
    set scrollY [string match *y* $Priv(autoscan,direction,$w)]
    if {($scrollX && (($x < $x1 + $margin) || ($x >= $x2 - $margin))) ||
	($scrollY && (($y < $y1 + $margin) || ($y >= $y2 - $margin)))} {
	if {[info exists Priv(autoscan,afterId,$w)]} return
	if {$scrollY && $y >= $y2 - $margin} {
	    $w yview scroll 1 units
	    set delay [$w cget -yscrolldelay]
	} elseif {$scrollY && $y < $y1 + $margin} {
	    $w yview scroll -1 units
	    set delay [$w cget -yscrolldelay]
	} elseif {$scrollX && $x >= $x2 - $margin} {
	    $w xview scroll 1 units
	    set delay [$w cget -xscrolldelay]
	} elseif {$scrollX && $x < $x1 + $margin} {
	    $w xview scroll -1 units
	    set delay [$w cget -xscrolldelay]
	}
	set count [scan $delay "%d %d" d1 d2]
	if {[info exists Priv(autoscan,scanning,$w)]} {
	    if {$count == 2} {
		set delay $d2
	    }
	} else {
	    if {$count == 2} {
		set delay $d1
	    }
	    set Priv(autoscan,scanning,$w) 1
	}
	if {$Priv(autoscan,command,$w) ne ""} {
	    set command [string map [list %T $w %x $x %y $y] $Priv(autoscan,command,$w)]
	    eval $command
	}
	set Priv(autoscan,afterId,$w) [after $delay [list TreeCtrl::AutoScanCheckAux $w]]
	return
    }
    AutoScanCancel $w
    return
}

# ::TreeCtrl::AutoScanCheckAux --
#
# Gets the location of the pointer and calls AutoScanCheck.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::AutoScanCheckAux {w} {
    variable Priv
    if {![winfo exists $w]} return
    # Not quite sure how this can happen
    if {![info exists Priv(autoscan,afterId,$w)]} return
    unset Priv(autoscan,afterId,$w)
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    set x [expr {$x - [winfo rootx $w]}]
    set y [expr {$y - [winfo rooty $w]}]
    AutoScanCheck $w $x $y
    return
}

# ::TreeCtrl::AutoScanCancel --
#
# Cancels any pending [after] callback to AutoScanCheckAux.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::AutoScanCancel {w} {
    variable Priv
    if {[info exists Priv(autoscan,afterId,$w)]} {
	after cancel $Priv(autoscan,afterId,$w)
	unset Priv(autoscan,afterId,$w)
    }
    unset -nocomplain Priv(autoscan,scanning,$w)
    return
}

# ::TreeCtrl::ColumnDragScrollCheck --
#
# Sees if the given pointer coords are outside the left or right edges of
# the content area of the treectrl (ie, not including borders). If so and
# auto-scanning is not already in progress, then the window is scrolled
# horizontally and the column drag-image is repositioned, and an [after]
# callback to ColumnDragScrollCheckAux is scheduled.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::ColumnDragScrollCheck {w x y} {
    variable Priv

    # When dragging to the left, use the left edge of the dragged
    # header to choose the -indicatorcolumn.  When dragging to the
    # right, use the right edge.
    scan [$w header bbox $Priv(header) $Priv(column)] "%d %d %d %d" x1 y1 x2 y2
    set offset [$w header dragcget -imageoffset]
    if {$offset > 0} {
	set xEdge [expr {$offset + $x2}]
    } else {
	set xEdge [expr {$offset + $x1}]
    }

    scan [$w bbox header.none] "%d %d %d %d" x1 y1 x2 y2

    if {($x < $x1) || ($x >= $x2)} {
	if {![info exists Priv(autoscan,afterId,$w)]} {
	    set bbox1 [$w column bbox $Priv(column)]
	    if {$xEdge >= $x2} {
		$w xview scroll 1 units
	    } else {
		$w xview scroll -1 units
	    }
	    set bbox2 [$w column bbox $Priv(column)]
	    if {[lindex $bbox1 0] != [lindex $bbox2 0]} {
		incr Priv(columnDrag,x) [expr {[lindex $bbox2 0] - [lindex $bbox1 0]}]
		$w header dragconfigure -imageoffset [expr {$x - $Priv(columnDrag,x)}]

		if {[ColumnDragFindBefore $w $xEdge $Priv(columnDrag,y) $Priv(column) indColumn indSide]} {
		    $w header dragconfigure -indicatorcolumn $indColumn \
			-indicatorside $indSide
		} else {
		    $w header dragconfigure -indicatorcolumn ""
		}
	    }
	    set Priv(autoscan,afterId,$w) [after 50 [list TreeCtrl::ColumnDragScrollCheckAux $w]]
	}
	return
    }
    AutoScanCancel $w
    return
}

# ::TreeCtrl::ColumnDragScrollCheckAux --
#
# Gets the location of the pointer and calls ColumnDragScrollCheck.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::ColumnDragScrollCheckAux {w} {
    variable Priv
    if {![winfo exists $w]} return
    # Not quite sure how this can happen
    if {![info exists Priv(autoscan,afterId,$w)]} return
    unset Priv(autoscan,afterId,$w)
    set x [winfo pointerx $w]
    set y [winfo pointery $w]
    set x [expr {$x - [winfo rootx $w]}]
    set y [expr {$y - [winfo rooty $w]}]
    ColumnDragScrollCheck $w $x $y
    return
}

# ::TreeCtrl::Has2DLayout --
#
# Determine if items are displayed in a 2-dimensional arrangement.
# This is used by the <Left> and <Right> bindings.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::Has2DLayout {T} {
    if {[$T cget -orient] ne "vertical" || [$T cget -wrap] ne ""} {
	return 1
    }
    set item [$T item id "last visible"]
    if {$item ne ""} {
	lassign [$T item rnc $item] row column
	if {$column > 0} {
	    return 1
	}
    }
    return 0
}

# ::TreeCtrl::UpDown --
#
# Returns the id of an item above or below the given item that the active
# item could be set to. If the given item isn't visible, the first visible
# enabled item is returned. An attempt is made to choose an item in the
# same column over repeat calls; this gives a better result if some rows
# have less items than others. Only enabled items are considered.
#
# Arguments:
# w		The treectrl widget.
# item		Item to move from, typically the active item.
# n		+1 to move down, -1 to move up.

proc ::TreeCtrl::UpDown {w item n} {
    variable Priv
    set rnc [$w item rnc $item]
    if {$rnc eq ""} {
	return [$w item id {first visible state enabled}]
    }
    scan $rnc "%d %d" row col
    set Priv(keyNav,row,$w) [expr {$row + $n}]
    if {![info exists Priv(keyNav,rnc,$w)] || $rnc ne $Priv(keyNav,rnc,$w)} {
	set Priv(keyNav,col,$w) $col
    }
    set item2 [$w item id "rnc $Priv(keyNav,row,$w) $Priv(keyNav,col,$w)"]
    if {[$w item compare $item == $item2]} {
	set Priv(keyNav,row,$w) $row
	if {![$w item enabled $item2]} {
	    return ""
	}
    } else {
	set Priv(keyNav,rnc,$w) [$w item rnc $item2]
	if {![$w item enabled $item2]} {
	    return [UpDown $w $item2 $n]
	}
    }
    return $item2
}

# ::TreeCtrl::LeftRight --
#
# Returns the id of an item left or right of the given item that the active
# item could be set to. If the given item isn't visible, the first visible
# enabled item is returned. An attempt is made to choose an item in the
# same row over repeat calls; this gives a better result if some columns
# have less items than others. Only enabled items are considered.
#
# Arguments:
# w		The treectrl widget.
# item		Item to move from, typically the active item.
# n		+1 to move right, -1 to move left.

proc ::TreeCtrl::LeftRight {w item n} {
    variable Priv
    set rnc [$w item rnc $item]
    if {$rnc eq ""} {
	return [$w item id {first visible state enabled}]
    }
    scan $rnc "%d %d" row col
    set Priv(keyNav,col,$w) [expr {$col + $n}]
    if {![info exists Priv(keyNav,rnc,$w)] || $rnc ne $Priv(keyNav,rnc,$w)} {
	set Priv(keyNav,row,$w) $row
    }
    set item2 [$w item id "rnc $Priv(keyNav,row,$w) $Priv(keyNav,col,$w)"]
    if {[$w item compare $item == $item2]} {
	set Priv(keyNav,col,$w) $col
	if {![$w item enabled $item2]} {
	    return ""
	}
    } else {
	set Priv(keyNav,rnc,$w) [$w item rnc $item2]
	if {![$w item enabled $item2]} {
	    return [LeftRight $w $item2 $n]
	}
    }
    return $item2
}

# ::TreeCtrl::SetActiveItem --
#
# Sets the active item, scrolls it into view, and makes it the only selected
# item. If -selectmode is extended, makes the active item the anchor of any
# future extended selection.
#
# Arguments:
# w		The treectrl widget.
# item		The new active item, or "".

proc ::TreeCtrl::SetActiveItem {w item} {
    if {$item eq ""} return
    $w activate $item
    $w see active
    $w selection modify active all
    switch [$w cget -selectmode] {
	extended {
	    $w selection anchor active
	    set Priv(prev) [$w item id active]
	    set Priv(selection) {}
	}
    }
    return
}

# ::TreeCtrl::Extend --
#
# Does nothing unless we're in extended selection mode;  in this
# case it moves the location cursor (active item) up, down, left or
# right, and extends the selection to that point.
#
# Arguments:
# w		The treectrl widget.
# dir		up, down, left or right

proc ::TreeCtrl::Extend {w dir} {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    if {![info exists Priv(selection)]} {
	$w selection add active
	set Priv(selection) [$w selection get]
    }
    switch -- $dir {
	above { set item [UpDown $w active -1] }
	below { set item [UpDown $w active 1] }
	left { set item [LeftRight $w active -1] }
	right { set item [LeftRight $w active 1] }
    }
    if {$item eq ""} return
    $w activate $item
    $w see active
    SelectionMotion $w [$w item id active]
    return
}

# ::TreeCtrl::DataExtend
#
# This procedure is called for key-presses such as Shift-KEndData.
# If the selection mode isn't multiple or extended then it does nothing.
# Otherwise it moves the active item and, if we're in
# extended mode, extends the selection to that point.
#
# Arguments:
# w		The treectrl widget.
# item		Item to become new active item.

proc ::TreeCtrl::DataExtend {w item} {
    if {$item eq ""} return
    set mode [$w cget -selectmode]
    if {[string equal $mode "extended"]} {
	$w activate $item
	$w see $item
        if {[$w selection includes anchor]} {
	    SelectionMotion $w $item
	}
    } elseif {[string equal $mode "multiple"]} {
	$w activate $item
	$w see $item
    }
    return
}

# ::TreeCtrl::Cancel
#
# This procedure is invoked to cancel an extended selection in
# progress.  If there is an extended selection in progress, it
# restores all of the items between the active one and the anchor
# to their previous selection state.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::Cancel w {
    variable Priv
    if {[string compare [$w cget -selectmode] "extended"]} {
	return
    }
    set first [$w item id anchor]
    set last $Priv(prev)
    if { [string equal $last ""] } {
	# Not actually doing any selection right now
	return
    }
    if {[$w item compare $first > $last]} {
	set tmp $first
	set first $last
	set last $tmp
    }
    set select {}
    set deselect {}
    foreach item [$w item id "range $first $last visible"] {
	if {[lsearch $Priv(selection) $item] == -1} {
	    lappend deselect $item
	} else {
	    lappend select $item
	}
    }
    $w selection modify $select $deselect
    return
}

# ::TreeCtrl::SelectAll
#
# This procedure is invoked to handle the "select all" operation.
# For single and browse mode, it just selects the active item.
# Otherwise it selects everything in the widget.
#
# Arguments:
# w		The treectrl widget.

proc ::TreeCtrl::SelectAll w {
    set mode [$w cget -selectmode]
    if {[string equal $mode "single"] || [string equal $mode "browse"]} {
	$w selection modify active all
    } else {
	$w selection add all
    }
    return
}

# ::TreeCtrl::MarqueeBegin --
#
# Shows the selection rectangle at the given coords.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::MarqueeBegin {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee coords $x $y $x $y
    $w marquee configure -visible yes
    return
}

# ::TreeCtrl::MarqueeUpdate --
#
# Resizes the selection rectangle.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::MarqueeUpdate {w x y} {
    set x [$w canvasx $x]
    set y [$w canvasy $y]
    $w marquee corner $x $y
    return
}

# ::TreeCtrl::MarqueeEnd --
#
# Hides the selection rectangle.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::MarqueeEnd {w x y} {
    $w marquee configure -visible no
    return
}

# ::TreeCtrl::ScanMark --
#
# Marks the start of a possible scan drag operation.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::ScanMark {w x y} {
    variable Priv
    $w scan mark $x $y
    set Priv(x) $x
    set Priv(y) $y
    set Priv(mouseMoved) 0
    return
}

# ::TreeCtrl::ScanDrag --
#
# Performs a scan drag if the mouse moved.
#
# Arguments:
# w		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::ScanDrag {w x y} {
    variable Priv
    if {![info exists Priv(x)]} { set Priv(x) $x }
    if {![info exists Priv(y)]} { set Priv(y) $y }
    if {($x != $Priv(x)) || ($y != $Priv(y))} {
	set Priv(mouseMoved) 1
    }
    if {[info exists Priv(mouseMoved)] && $Priv(mouseMoved)} {
	$w scan dragto $x $y
    }
    return
}

# ::TreeCtrl::TryEvent --
#
# This procedure is used to cause a treectrl to generate a dynamic event.
# If the treectrl doesn't have the event defined (because you didn't call
# the [notify install] command) nothing happens. TreeCtrl::PercentsCmd is
# used to perform %-substitution on any scripts bound to the event.
#
# Arguments:
# T		The treectrl widget.
# event		Name of event.
# detail	Name of detail or "".
# charMap	%-char substitution list (even number of elements).

proc ::TreeCtrl::TryEvent {T event detail charMap} {
    if {[lsearch -exact [$T notify eventnames] $event] == -1} return
    if {$detail ne ""} {
	if {[lsearch -exact [$T notify detailnames $event] $detail] == -1} return
	$T notify generate <$event-$detail> $charMap "::TreeCtrl::PercentsCmd $T"
    } else {
	$T notify generate <$event> $charMap "::TreeCtrl::PercentsCmd $T"
    }
    return
}

# ::TreeCtrl::PercentsCmd --
#
# This command is passed to [notify generate] to perform %-substitution on
# scripts bound to dynamic events. It supports the same set of substitution
# characters as the built-in static events (plus any event-specific chars).
#
# Arguments:
# T		The treectrl widget.
# char		%-char to be replaced in bound scripts.
# object	Same arg passed to [notify bind].
# event		Name of event.
# detail	Name of detail or "".
# charMap	%-char substitution list (even number of elements).

proc ::TreeCtrl::PercentsCmd {T char object event detail charMap} {
    if {$detail ne ""} {
	set pattern <$event-$detail>
    } else {
	set pattern <$event>
    }
    switch -- $char {
	d { return $detail }
	e { return $event }
	P { return $pattern }
	W { return $object }
	T { return $T }
	? {
	    array set map $charMap
	    array set map [list T $T W $object P $pattern e $event d $detail]
	    return [array get map]
	}
	default {
	    array set map [list $char $char]
	    array set map $charMap
	    return $map($char)
	}
    }
    return
}

namespace eval TreeCtrl {
catch {
    foreach theme [ttk::style theme names] {
	ttk::style theme settings $theme {
	    ttk::style configure TreeCtrlHeading -relief raised -font TkHeadingFont
	    ttk::style map TreeCtrlHeading -relief {
		pressed sunken
	    }
	}
    }
}
}
