# Copyright (c) 2002-2011 Tim Baker

bind TreeCtrlFileList <Double-ButtonPress-1> {
    TreeCtrl::FileListEditCancel %W
    TreeCtrl::DoubleButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Control-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) toggle
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Shift-ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) add
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonPress-1> {
    set TreeCtrl::Priv(selectMode) set
    TreeCtrl::FileListButton1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Motion> {
    TreeCtrl::FileListMotion1 %W %x %y
    break
}
bind TreeCtrlFileList <Button1-Leave> {
    TreeCtrl::FileListLeave1 %W %x %y
    break
}
bind TreeCtrlFileList <ButtonRelease-1> {
    TreeCtrl::FileListRelease1 %W %x %y
    break
}

# Escape cancels any drag-and-drop operation in progress
bind TreeCtrlFileList <KeyPress-Escape> {
    TreeCtrl::FileListEscapeKey %W
}

## Bindings for the Entry widget used for editing

# Accept edit when we lose the focus
bind TreeCtrlEntry <FocusOut> {
    if {[winfo ismapped %W]} {
	TreeCtrl::EditClose [winfo parent %W] entry 1 0
    }
}

# Accept edit on <Return>
bind TreeCtrlEntry <KeyPress-Return> {
    TreeCtrl::EditClose [winfo parent %W] entry 1 1
    break
}

# Cancel edit on <Escape>, use break as we are doing a "closing" action
# and don't want that propagated upwards
bind TreeCtrlEntry <KeyPress-Escape> {
    TreeCtrl::EditClose [winfo parent %W] entry 0 1
    break
}

## Bindings for the Text widget used for editing

# Accept edit when we lose the focus
bind TreeCtrlText <FocusOut> {
    if {[winfo ismapped %W]} {
	TreeCtrl::EditClose [winfo parent %W] text 1 0
    }
}

# Accept edit on <Return>
bind TreeCtrlText <KeyPress-Return> {
    TreeCtrl::EditClose [winfo parent %W] text 1 1
    break
}

# Cancel edit on <Escape>, use break as we are doing a "closing" action
# and don't want that propagated upwards
bind TreeCtrlText <KeyPress-Escape> {
    TreeCtrl::EditClose [winfo parent %W] text 0 1
    break
}

namespace eval TreeCtrl {
    variable Priv

    # Number of milliseconds after clicking a selected item before the Edit
    # widget appears.
    set Priv(edit,delay) 500

    # Try to deal with people importing ttk::entry into the global namespace;
    # we want tk::entry
    if {[llength [info commands ::tk::entry]]} {
	set Priv(entryCmd) ::tk::entry
    } else {
	set Priv(entryCmd) ::entry
    }
}

# ::TreeCtrl::IsSensitive
#
# Returns 1 if the given window coordinates are over an element that should
# respond to mouse clicks.  The list of elements that respond to mouse clicks
# is set by calling ::TreeCtrl::SetSensitive.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::IsSensitive {T x y} {
    variable Priv
    $T identify -array id $x $y
    if {$id(where) ne "item" || $id(element) eq ""} {
	return 0
    }
    if {![$T item enabled $id(item)]} {
	return 0
    }
    foreach list $Priv(sensitive,$T) {
	set eList [lassign $list C S]
	if {[$T column compare $id(column) != $C]} continue
	if {[$T item style set $id(item) $C] ne $S} continue
	if {[lsearch -exact $eList $id(element)] == -1} continue
	return 1
    }
    return 0
}

# ::TreeCtrl::IsSensitiveMarquee
#
# Returns 1 if the given window coordinates are over an element that
# should respond to the marquee.  The list of elements that respond to the
# marquee is set by calling ::TreeCtrl::SetSensitiveMarquee, or if that list
# is empty then the same list passed to ::TreeCtrl::SetSensitive.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::IsSensitiveMarquee {T x y} {
    variable Priv
    $T identify -array id $x $y
    if {$id(where) ne "item" || $id(element) eq ""} {
	return 0
    }
    if {![$T item enabled $id(item)]} {
	return 0
    }
    if {![info exists Priv(sensitive,marquee,$T)]} {
	set sensitive $Priv(sensitive,$T)
    } elseif {[llength $Priv(sensitive,marquee,$T)] == 0} {
	set sensitive $Priv(sensitive,$T)
    } else {
	set sensitive $Priv(sensitive,marquee,$T)
    }
    foreach list $sensitive {
	set eList [lassign $list C S]
	if {[$T column compare $id(column) != $C]} continue
	if {[$T item style set $id(item) $C] ne $S} continue
	if {[lsearch -exact $eList $id(element)] == -1} continue
	return 1
    }
    return 0
}

# ::TreeCtrl::FileListButton1
#
# Handle <ButtonPress-1>.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::FileListButton1 {T x y} {
    variable Priv
    focus $T
    $T identify -array id $x $y
    set marquee 0
    set Priv(buttonMode) ""
    foreach e {text entry} {
	if {[winfo exists $T.$e] && [winfo ismapped $T.$e]} {
	    EditClose $T $e 1 0
	}
    }
    FileListEditCancel $T
    # Click outside any item
    if {$id(where) eq ""} {
	set marquee 1

    # Click in header
    } elseif {$id(where) eq "header"} {
	ButtonPress1 $T $x $y

    # Click in item
    } elseif {$id(where) eq "item"} {
	if {$id(button) || $id(line) ne ""} {
	    ButtonPress1 $T $x $y
	} elseif {$id(column) ne ""} {
	    set item $id(item)
	    set drag 0
	    if {[IsSensitive $T $x $y]} {
		set Priv(drag,wasSel) [$T selection includes $item]
		$T activate $item
		if {$Priv(selectMode) eq "add"} {
		    BeginExtend $T $item
		} elseif {$Priv(selectMode) eq "toggle"} {
		    BeginToggle $T $item
		} elseif {![$T selection includes $item]} {
		    BeginSelect $T $item
		}

		# Changing the selection might change the list
		if {[$T item id $item] eq ""} return

		# Click selected item(s) to drag or rename
		if {[$T selection includes $item]} {
		    set drag 1
		}
	    } elseif {[FileListEmulateWin7 $T] && [IsSensitiveMarquee $T $x $y]} {
		# Click selected item(s) to drag or rename
		if {[$T selection includes $item]} {
		    set Priv(drag,wasSel) 1
		    $T activate $item
		    set drag 1
		# Click marquee-sensitive parts of an unselected item
		# in single-select mode changes nothing until a drag
		# occurs or the mouse button is released.
		} elseif {[$T cget -selectmode] eq "single"} {
		    set Priv(drag,wasSel) 0
		    set drag 1
		} else {
		    set marquee 1
		}
	    } else {
		set marquee 1
	    }
	    if {$drag} {
		set Priv(drag,motion) 0
		set Priv(drag,click,x) $x
		set Priv(drag,click,y) $y
		set Priv(drag,x) [$T canvasx $x]
		set Priv(drag,y) [$T canvasy $y]
		set Priv(drop) ""
		set Priv(drag,item) $item
		set Priv(drag,C) $id(column)
		set Priv(drag,E) $id(element)
		set Priv(buttonMode) drag
	    }
	}
    }
    if {$marquee && [$T cget -selectmode] eq "single"} {
	set marquee 0
	$T selection clear
    }
    if {$marquee} {
	set Priv(buttonMode) marquee
	if {![info exists Priv(sensitive,marquee,$T)]} {
	    set Priv(sensitive,marquee,$T) {}
	}
	if {$Priv(selectMode) ne "set"} {
	    set Priv(selection) [$T selection get]
	} else {
	    if {![FileListEmulateWin7 $T]} {
		$T selection clear
	    }
	    set Priv(selection) {}
	}
	MarqueeBegin $T $x $y

	set Priv(marquee,motion) 0
	if {[FileListEmulateWin7 $T]} {
	    if {[IsSensitiveMarquee $T $x $y]} {
		set item $id(item)
		$T activate $item
		if {$Priv(selectMode) ne "add"} {
		    $T selection anchor $item
		}
	    }
	}
    }
    return
}

# ::TreeCtrl::FileListMotion1
#
# Override default <Button1-Motion> to handle "drag" and "marquee".
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::FileListMotion1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"drag" -
	"marquee" {
	    set Priv(autoscan,command,$T) {FileListMotion %T %x %y}
	    AutoScanCheck $T $x $y
	    FileListMotion $T $x $y
	}
	default {
	    Motion1 $T $x $y
	}
    }
    return
}

# ::TreeCtrl::FileListMotion
#
# Handle <Button1-Motion>.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::FileListMotion {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    MarqueeUpdate $T $x $y
	    set select $Priv(selection)
	    set deselect {}
	    set items {}

	    set Priv(marquee,motion) 1

	    set sensitive $Priv(sensitive,marquee,$T)
	    if {[llength $sensitive] == 0} {
		set sensitive $Priv(sensitive,$T)
	    }

	    # Check items covered by the marquee
	    foreach list [$T marque identify] {
		set item [lindex $list 0]
		if {![$T item enabled $item]} continue

		# Check covered columns in this item
		foreach sublist [lrange $list 1 end] {
		    set column [lindex $sublist 0]
		    set ok 0

		    # Check covered elements in this column
		    foreach E [lrange $sublist 1 end] {
			foreach sList $sensitive {
			    set sEList [lassign $sList sC sS]
			    if {[$T column compare $column != $sC]} continue
			    if {[$T item style set $item $sC] ne $sS} continue
			    if {[lsearch -exact $sEList $E] == -1} continue
			    set ok 1
			    break
			}
		    }
		    # Some sensitive elements in this column are covered
		    if {$ok} {
			lappend items $item
		    }
		}
	    }
	    foreach item $items {
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
	    $T selection modify $select all
	}
	"drag" {
	    if {!$Priv(drag,motion)} {
		# Detect initial mouse movement
		if {(abs($x - $Priv(drag,click,x)) <= 4) &&
		    (abs($y - $Priv(drag,click,y)) <= 4)} return

		# In Win7 single-selectmode, when the insensitive parts of an
		# unselected item are clicked, the active item and selection
		# aren't changed until the drag begins.
		if {[FileListEmulateWin7 $T]
			&& [$T cget -selectmode] eq "single"
			&& !$Priv(drag,wasSel)} {
		    $T activate $Priv(drag,item)
		    $T selection modify $Priv(drag,item) all
		}

		set Priv(selection) [$T selection get]
		set Priv(drop) ""
		$T dragimage clear
		# For each selected item, add some elements to the dragimage
		foreach I $Priv(selection) {
		    foreach list $Priv(dragimage,$T) {
			set EList [lassign $list C S]
			if {[$T item style set $I $C] eq $S} {
			    eval $T dragimage add $I $C $EList
			}
		    }
		}
		set Priv(drag,motion) 1
		TryEvent $T Drag begin {}
	    }

	    # Find the element under the cursor
	    set drop ""
	    $T identify -array id $x $y
	    if {[IsSensitive $T $x $y]} {
		set sensitive 1
	    } elseif {[FileListEmulateWin7 $T] && [IsSensitiveMarquee $T $x $y]} {
		set sensitive 1
	    } else {
		set sensitive 0
	    }
	    if {$sensitive} {
		set item $id(item)
		# If the item is not in the pre-drag selection
		# (i.e. not being dragged) and it is a directory,
		# see if we can drop on it
		if {[lsearch -exact $Priv(selection) $item] == -1} {
		    if {[$T item order $item -visible] < $Priv(DirCnt,$T)} {
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
	    set Priv(drop) $drop

	    # Show the dragimage in its new position
if {0 && [$T dragimage cget -style] ne ""} {
    set x [$T canvasx $x]
    set y [$T canvasy $y]
} else {
	    set x [expr {[$T canvasx $x] - $Priv(drag,x)}]
	    set y [expr {[$T canvasy $y] - $Priv(drag,y)}]
}
	    $T dragimage offset $x $y
	    $T dragimage configure -visible yes
	}
	default {
	    Motion1 $T $x $y
	}
    }
    return
}

# ::TreeCtrl::FileListLeave1
#
# Handle <Button1-Leave>.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::FileListLeave1 {T x y} {
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

# ::TreeCtrl::FileListRelease1
#
# Handle <Button1-Release>.
#
# Arguments:
# T		The treectrl widget.
# x		Window coord of pointer.
# y		Window coord of pointer.

proc ::TreeCtrl::FileListRelease1 {T x y} {
    variable Priv
    if {![info exists Priv(buttonMode)]} return
    switch $Priv(buttonMode) {
	"marquee" {
	    AutoScanCancel $T
	    MarqueeEnd $T $x $y

	    if {[FileListEmulateWin7 $T]} {
		# If the mouse was clicked in whitespace or insensitive part
		# of an item and the mouse did not move then the selection
		# is not modified until after the mouse button is released.
		if {!$Priv(marquee,motion)} {
		    if {[IsSensitiveMarquee $T $x $y]} {
			set item [$T item id active]
			if {$Priv(selectMode) eq "add"} {
			    BeginExtend $T $item
			} elseif {$Priv(selectMode) eq "toggle"} {
			    BeginToggle $T $item
			} else {
			    BeginSelect $T $item
			}
		    } elseif {$Priv(selectMode) eq "set"} {
			# Clicked whitespace
			$T selection clear
		    }
		}
	    }
	}
	"drag" {
	    AutoScanCancel $T

	    # Some dragging occurred
	    if {$Priv(drag,motion)} {
		$T dragimage configure -visible no
		if {$Priv(drop) ne ""} {
		    $T selection modify {} $Priv(drop)
		    TryEvent $T Drag receive \
			[list I $Priv(drop) l $Priv(selection)]
		}
		TryEvent $T Drag end {}
	    } else {
		set rename 0
		if {[FileListEmulateWin7 $T]} {
		    # If the mouse was clicked in the insensitive parts of
		    # a selected item and multiple items are selected and the
		    # mouse did not move then the selection is not modified
		    # until after the mouse button is released.
		    set item $Priv(drag,item)
		    if {[$T selection count] == 1 && $Priv(selectMode) eq "set"} {
			# In single-selectmode, when clicking the insensitive
			# parts of an unselected item, the active item and
			# selection aren't changed until the button is released.
			if {[$T cget -selectmode] eq "single" && !$Priv(drag,wasSel)} {
			    $T activate $item
			    $T selection modify $item all
			} else {
			    # If clicked already-selected item, rename it
			    set rename $Priv(drag,wasSel)
			}
		    } elseif {[IsSensitive $T $x $y] && !$Priv(drag,wasSel)} {
			# Selection was modified on ButtonPress, do nothing
		    } elseif {$Priv(selectMode) eq "add"} {
			# Shift-click does nothing to already-selected item
			#BeginExtend $T $item
		    } elseif {$Priv(selectMode) eq "toggle"} {
			BeginToggle $T $item
		    } else {
			# Make this the only selected item
			BeginSelect $T $item
		    }
		} elseif {$Priv(selectMode) eq "toggle"} {
		    # don't rename
		} elseif {$Priv(drag,wasSel)} {
		    # Clicked/released a selected item, but didn't drag
		    set rename 1
		}
		if {$rename} {
		    set I [$T item id active]
		    set C $Priv(drag,C)
		    set E $Priv(drag,E)
		    set S [$T item style set $I $C]
		    set ok 0
		    foreach list $Priv(edit,$T) {
			set eEList [lassign $list eC eS]
			if {[$T column compare $C != $eC]} continue
			if {$S ne $eS} continue
			if {[lsearch -exact $eEList $E] == -1} continue
			set ok 1
			break
		    }
		    if {$ok} {
			FileListEditCancel $T
			set Priv(editId,$T) \
			    [after $Priv(edit,delay) [list ::TreeCtrl::FileListEdit $T $I $C $E]]
		    }
		}
	    }
	}
	default {
	    Release1 $T $x $y
	}
    }
    set Priv(buttonMode) ""
    return
}

# ::TreeCtrl::EscapeKey
#
# Handle the <Escape> key.
#
# T		The treectrl widget.

proc ::TreeCtrl::FileListEscapeKey {T} {
    variable Priv
    if {[info exists Priv(buttonMode)] && $Priv(buttonMode) eq "drag"} {
	set Priv(buttonMode) ""
	AutoScanCancel $T
	if {$Priv(drag,motion)} {
	    $T selection modify $Priv(selection) all
	    $T dragimage configure -visible no
	    TryEvent $T Drag end {}
	}
	return -code break
    }
    return
}

# ::TreeCtrl::FileListEdit
#
# Displays an Entry or Text widget to allow the user to edit the specified
# text element.
#
# Arguments:
# T		The treectrl widget.
# I		Item.
# C		Column.
# E		Element.

proc ::TreeCtrl::FileListEdit {T I C E} {
    variable Priv
    array unset Priv editId,$T

    if {![winfo exists $T]} return

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
	# FIXME: max of padx or union padding
	GetPadding $padx padw pade
	AddUnionPadding $T $S $E padw pade
	TextExpanderOpen $T $I $C $E [expr {$x2 - $x1 - $padw - $pade}]

    # Single-line edit
    } else {
	EntryExpanderOpen $T $I $C $E
    }

    TryEvent $T Edit begin [list I $I C $C E $E]

    return
}

proc ::TreeCtrl::GetPadding {pad _padw _pade} {
    upvar $_padw padw
    upvar $_pade pade
    if {[llength $pad] == 2} {
	lassign $pad padw pade
    } else {
	set pade [set padw $pad]
    }
    return
}

# Recursively adds -padx and -ipadx values of other elements in a style that
# contain the given element in its -union.
proc ::TreeCtrl::AddUnionPadding {T S E _padw _pade} {
    upvar $_padw padw
    upvar $_pade pade
    foreach E2 [$T style elements $S] {
	set union [$T style layout $S $E2 -union]
	if {[lsearch -exact $union $E] == -1} continue
	foreach option {-padx -ipadx} {
	    set pad [$T style layout $S $E2 $option]
	    GetPadding $pad p1 p2
	    # FIXME: max of padx or union padding
	    incr padw $p1
	    incr pade $p2
	}
	AddUnionPadding $T $S $E2 padw pade
    }
    return
}

# ::TreeCtrl::FileListEditCancel
#
# Aborts any scheduled display of the text-edit widget.
#
# Arguments:
# T		The treectrl widget.

proc ::TreeCtrl::FileListEditCancel {T} {
    variable Priv
    if {[info exists Priv(editId,$T)]} {
	after cancel $Priv(editId,$T)
	array unset Priv editId,$T
    }
    return
}

# ::TreeCtrl::SetDragImage
#
# Specifies the list of elements that should be added to the dragimage.
#
# Arguments:
# T		The treectrl widget.
# listOfLists	{{column style element ...} {column style element ...}}

proc ::TreeCtrl::SetDragImage {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set elements [lassign $list column style]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
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

# ::TreeCtrl::SetEditable
#
# Specifies the list of text elements that can be edited.
#
# Arguments:
# T		The treectrl widget.
# listOfLists	{{column style element ...} {column style element ...}}

proc ::TreeCtrl::SetEditable {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set elements [lassign $list column style]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
	}
	if {[lsearch -exact [$T style names] $style] == -1} {
	    error "style \"$style\" doesn't exist"
	}
	foreach element $elements {
	    if {[lsearch -exact [$T element names] $element] == -1} {
		error "element \"$element\" doesn't exist"
	    }
	    if {[$T element type $element] ne "text"} {
		error "element \"$element\" is not of type \"text\""
	    }
	}
    }
    set Priv(edit,$T) $listOfLists
    return
}

# ::TreeCtrl::SetSensitive
#
# Specifies the list of elements that respond to mouse clicks.
#
# Arguments:
# T		The treectrl widget.
# listOfLists	{{column style element ...} {column style element ...}}

proc ::TreeCtrl::SetSensitive {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set elements [lassign $list column style]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
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

# ::TreeCtrl::SetSensitiveMarquee
#
# Specifies the list of elements that are sensitive to the marquee.
# If the list is empty then the same list passed to SetSensitive
# is used.
#
# Arguments:
# T		The treectrl widget.
# sensitive	Boolean value.

proc ::TreeCtrl::SetSensitiveMarquee {T listOfLists} {
    variable Priv
    foreach list $listOfLists {
	set elements [lassign $list column style]
	if {[$T column id $column] eq ""} {
	    error "column \"$column\" doesn't exist"
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
    set Priv(sensitive,marquee,$T) $listOfLists
    return
}

# ::TreeCtrl::SetSelectedItemsSensitive
#
# Specifies whether or not entire items are sensitive to mouse clicks
# when they are already selected.
#
# Arguments:
# T		The treectrl widget.
# sensitive	Boolean value.

proc ::TreeCtrl::SetSelectedItemsSensitive {T sensitive} {
    variable Priv
    if {![string is boolean -strict $sensitive]} {
	error "expected boolean but got \"$sensitive\""
    }
    set Priv(sensitiveSelected,$T) $sensitive
    return
}

# ::TreeCtrl::FileListEmulateWin7
#
# Test the flag telling the bindings to use Windows 7 behavior.
#
# Arguments:
# T		The treectrl widget.
# win7		Boolean value.

proc ::TreeCtrl::FileListEmulateWin7 {T args} {
    variable Priv
    if {[llength $args]} {
	set win7 [lindex $args 0]
	if {![string is boolean -strict $win7]} {
	    error "expected boolean but got \"$win7\""
	}
	set Priv(win7,$T) $win7
	return
    }
    if {[info exists Priv(win7,$T)]} {
	return $Priv(win7,$T)
    }
    return 0
}

# ::TreeCtrl::EntryOpen
#
# Display a ::tk::entry so the user can edit the specified text element.
#
# Arguments:
# T		The treectrl widget.
# item		Item.
# column	Column.
# element	Element.

proc ::TreeCtrl::EntryOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    set e $T.entry
    if {[winfo exists $e]} {
	$e delete 0 end
    } else {
	$Priv(entryCmd) $e -borderwidth 1 -relief solid -highlightthickness 0
	bindtags $e [linsert [bindtags $e] 1 TreeCtrlEntry]
    }

    # Pesky MouseWheel
    $T notify bind $e <Scroll> { TreeCtrl::EditClose %T entry 0 1 }

    $e configure -font $font
    $e insert end $text
    $e selection range 0 end

    set ebw [$e cget -borderwidth]
    set ex [expr {$x - $ebw - 1}]
    place $e -x $ex -y [expr {$y - $ebw - 1}] -bordermode outside

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
    place configure $e -width $width

    focus $e

    return
}

# ::TreeCtrl::EntryExpanderOpen
#
# Display a ::tk::entry so the user can edit the specified text element.
# Like EntryOpen, but Entry widget expands/shrinks during typing.
#
# Arguments:
# T		The treectrl widget.
# item		Item.
# column	Column.
# element	Element.

proc ::TreeCtrl::EntryExpanderOpen {T item column element} {

    variable Priv

    set Priv(entry,$T,item) $item
    set Priv(entry,$T,column) $column
    set Priv(entry,$T,element) $element
    set Priv(entry,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d" x y

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    set Priv(entry,$T,font) $font

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    # Create the Entry widget if needed
    set e $T.entry
    if {[winfo exists $e]} {
	$e delete 0 end
    } else {
	$Priv(entryCmd) $e -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid
	bindtags $e [linsert [bindtags $e] 1 TreeCtrlEntry]

	# Resize as user types
	bind $e <KeyPress> {
	    after idle [list TreeCtrl::EntryExpanderKeypress [winfo parent %W]]
	}
    }

    # Pesky MouseWheel
    $T notify bind $e <Scroll> { TreeCtrl::EditClose %T entry 0 1 }

    $e configure -font $font -background [$T cget -background]
    $e insert end $text
    $e selection range 0 end

    set ebw [$e cget -borderwidth]
    set ex [expr {$x - $ebw - 1}]
    place $e -x $ex -y [expr {$y - $ebw - 1}] \
	-bordermode outside

    # Make the Entry as wide as the text plus "W" but keep it within the
    # TreeCtrl borders
    set width [font measure $font ${text}W]
    set width [expr {$width + ($ebw + 1) * 2}]
    scan [$T contentbox] "%d %d %d %d" left top right bottom
    if {$ex + $width > $right} {
	set width [expr {$right - $ex}]
    }
    place configure $e -width $width

    focus $e

    return
}

# ::TreeCtrl::EditClose
#
# Hides the text-edit widget and restores the focus if needed.
# Generates <Edit-accept> and <Edit-end> events as needed.
#
# Arguments:
# T		The treectrl widget.
# type		"entry" or "text".
# accept	0/1: should an <Edit-accept> event be generated.
# refocus	0/1: should the focus be restored to what it was before editing.

proc ::TreeCtrl::EditClose {T type accept {refocus 0}} {
    variable Priv

    set w $T.$type
    # We need the double-idle to get winfo ismapped to report properly
    # so this don't get the FocusOut following Escape immediately
    update idletasks
    place forget $w
    focus $T
    update idletasks

    if {$accept} {
	if {$type eq "entry"} {
	    set t [$w get]
	} else {
	    set t [$w get 1.0 end-1c]
	}
	TryEvent $T Edit accept \
	    [list I $Priv($type,$T,item) C $Priv($type,$T,column) \
		 E $Priv($type,$T,element) t $t]
    }

    $T notify unbind $w <Scroll>

    TryEvent $T Edit end \
	[list I $Priv($type,$T,item) C $Priv($type,$T,column) \
	     E $Priv($type,$T,element)]

    if {$refocus} {
	focus $Priv($type,$T,focus)
    }

    return
}

# ::TreeCtrl::EntryExpanderKeypress
#
# Maintains the width of the text-edit widget during typing.
#
# Arguments:
# T		The treectrl widget.

proc ::TreeCtrl::EntryExpanderKeypress {T} {

    variable Priv

    if {![winfo exists $T]} return

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

# ::TreeCtrl::TextOpen
#
# Display a ::tk::text so the user can edit the specified text element.
#
# Arguments:
# T		The treectrl widget.
# item		Item.
# column	Column.
# element	Element.
# width		unused.
# height	unused.

proc ::TreeCtrl::TextOpen {T item column element {width 0} {height 0}} {
    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    set justify [$T element cget $element -justify]
    if {$justify eq ""} {
	set justify left
    }

    set wrap [$T element cget $element -wrap]
    if {$wrap eq ""} {
	set wrap word
    }

    # Create the Text widget if needed
    set w $T.text
    if {[winfo exists $w]} {
	$w delete 1.0 end
    } else {
	text $w -borderwidth 1 -highlightthickness 0 -relief solid
	bindtags $w [linsert [bindtags $w] 1 TreeCtrlText]
    }

    # Pesky MouseWheel
    $T notify bind $w <Scroll> { TreeCtrl::EditClose %T text 0 1 }

    $w tag configure TAG -justify $justify
    $w configure -font $font -background [$T cget -background] -wrap $wrap
    $w insert end $text
    $w tag add sel 1.0 end
    $w tag add TAG 1.0 end

    set tbw [$w cget -borderwidth]
    set tx [expr {$x1 - $tbw - 1}]
    place $w -x $tx -y [expr {$y1 - $tbw - 1}] \
	-width [expr {$x2 - $x1 + ($tbw + 1) * 2}] \
	-height [expr {$y2 - $y1 + ($tbw + 1) * 2}] \
	-bordermode outside

    focus $w

    return
}

# ::TreeCtrl::TextExpanderOpen
#
# Display a ::tk::text so the user can edit the specified text element.
# Like TextOpen, but Text widget expands/shrinks during typing.
#
# Arguments:
# T		The treectrl widget.
# item		Item.
# column	Column.
# element	Element.
# width		Width of the text element.

proc ::TreeCtrl::TextExpanderOpen {T item column element width} {

    variable Priv

    set Priv(text,$T,item) $item
    set Priv(text,$T,column) $column
    set Priv(text,$T,element) $element
    set Priv(text,$T,focus) [focus]

    # Get window coords of the Element
    scan [$T item bbox $item $column $element] "%d %d %d %d" x1 y1 x2 y2

    set Priv(text,$T,center) [expr {$x1 + ($x2 - $x1) / 2}]

    # Get the font used by the Element
    set font [$T item element perstate $item $column $element -font]
    if {$font eq ""} {
	set font [$T cget -font]
    }

    # Get the text used by the Element. Could check master Element too.
    set text [$T item element cget $item $column $element -text]

    set justify [$T element cget $element -justify]
    if {$justify eq ""} {
	set justify left
    }

    set wrap [$T element cget $element -wrap]
    if {$wrap eq ""} {
	set wrap word
    }

    # Create the Text widget if needed
    set w $T.text
    if {[winfo exists $w]} {
	$w delete 1.0 end
    } else {
	text $w -borderwidth 1 -highlightthickness 0 \
	    -selectborderwidth 0 -relief solid
	bindtags $w [linsert [bindtags $w] 1 TreeCtrlText]

	# Resize as user types
	bind $w <KeyPress> {
	    after idle TreeCtrl::TextExpanderKeypress [winfo parent %W]
	}
    }

    # Pesky MouseWheel
    $T notify bind $w <Scroll> { TreeCtrl::EditClose %T text 0 1 }

    $w tag configure TAG -justify $justify
    $w configure -font $font -background [$T cget -background] -wrap $wrap
    $w insert end $text
    $w tag add sel 1.0 end
    $w tag add TAG 1.0 end

    set Priv(text,$T,font) $font
    set Priv(text,$T,justify) $justify
    set Priv(text,$T,width) $width

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$w cget -borderwidth]
    incr tbw
    place $w -x [expr {$x1 - $tbw}] -y [expr {$y1 - $tbw}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}] \
	-bordermode outside

    focus $w

    return
}

# ::TreeCtrl::TextExpanderKeypress
#
# Maintains the size of the text-edit widget during typing.
#
# Arguments:
# T		The treectrl widget.

proc ::TreeCtrl::TextExpanderKeypress {T} {

    variable Priv

    if {![winfo exists $T]} return

    set font $Priv(text,$T,font)
    set justify $Priv(text,$T,justify)
    set width $Priv(text,$T,width)
    set center $Priv(text,$T,center)

    set text [$T.text get 1.0 end-1c]

    scan [textlayout $font $text -justify $justify -width $width] \
	"%d %d" width height

    set tbw [$T.text cget -borderwidth]
    incr tbw
    place configure $T.text \
	-x [expr {$center - ($width + $tbw * 2) / 2}] \
	-width [expr {$width + $tbw * 2}] \
	-height [expr {$height + $tbw * 2}]

    $T.text tag add TAG 1.0 end

    return
}

