# RCS: @(#) $Id$

namespace eval StyleEditor {
    variable Info
    array unset Info
}

proc StyleEditor::Info {info args} {

    variable Info

    if {[llength $args]} {
	set Info($info) [lindex $args 0]
    }
    return $Info($info)
}

proc StyleEditor::Init {Tdemo} {

    set w .styleEditor
    toplevel $w
    wm title $w "TkTreeCtrl Style Editor"

    Info Tdemo $Tdemo

    panedwindow $w.pwH -orient horizontal -borderwidth 0 -sashwidth 6

    panedwindow $w.pwH.pwV -orient vertical -borderwidth 0 -sashwidth 6

    TreePlusScrollbarsInAFrame $w.pwH.pwV.styleList 1 1
    set T $w.pwH.pwV.styleList.t
    $T configure -showbuttons no -showlines no -showroot no -width 150 -height 200
    $T column create -text "Styles" -expand yes -button no -tags C0
    $T configure -treecolumn C0

    $T notify bind $T <Selection> {
	StyleEditor::SelectStyle
    }

    Info styleList $T

    TreePlusScrollbarsInAFrame $w.pwH.pwV.elementList 1 1
    set T $w.pwH.pwV.elementList.t
    $T configure -showbuttons no -showlines no -showroot no -width 150 -height 200
    $T column create -text "Elements" -expand yes -button no -tags C0
    $T configure -treecolumn C0

    $T notify bind $T <Selection> {
	StyleEditor::SelectElement
    }

    Info elementList $T

    $w.pwH.pwV add $w.pwH.pwV.styleList $w.pwH.pwV.elementList

    set fRight [panedwindow $w.pwH.pwV2 -orient vertical -sashwidth 6]
    if {[Platform macosx]} {
	$fRight configure -width 500
    }

    #
    # Property editor
    #

    TreePlusScrollbarsInAFrame $fRight.propertyList 1 1
    set T $fRight.propertyList.t
    $T configure -showbuttons no -showlines no -showroot no
    if {[Platform macosx]} {
	$T configure -height 300
    }
    $T column create -text "Property" -expand yes -button no -tags {C0 property}
    $T column create -text "Value" -expand yes -button no -tags {C1 value}
    $T configure -treecolumn property

    $T notify bind $T <Selection> {
	StyleEditor::SelectProperty %S %D
    }

    Info propertyList $T

    #
    # Style canvas
    #

    set fCanvas [frame $fRight.fCanvas -borderwidth 1 -relief sunken]
    set canvas [canvas $fCanvas.canvas -background white -height 300 \
	-scrollregion {0 0 0 0} -borderwidth 0 -highlightthickness 0 \
	-xscrollcommand [list sbset $fCanvas.xscroll] \
	-yscrollcommand [list sbset $fCanvas.yscroll]]
    scrollbar $fCanvas.xscroll -orient horizontal \
	-command [list $canvas xview]
    scrollbar $fCanvas.yscroll -orient vertical \
	-command [list $canvas yview]

    # Copy element config info from the selected item in the demo list
    $Tdemo notify bind StyleEditor <Selection> {
	if {[winfo ismapped .styleEditor]} {
	    StyleEditor::StyleToCanvas
	}
    }

    Info canvas $canvas

    Info selectedStyle ""
    Info selectedElement ""

    #
    # Create some scale controls to test expansion and squeezing
    #

    $::scaleCmd $canvas.scaleX \
	-orient horizontal -length 300 -from 5 -to 150 \
	-command StyleEditor::ScaleX
    if {!$::tile} {$canvas.scaleX configure -showvalue no}
    place $canvas.scaleX -rely 1.0 -anchor sw
    $::scaleCmd $canvas.scaleY \
	-orient vertical -length 300 -from 5 -to 150 \
	-command StyleEditor::ScaleY
    if {!$::tile} {$canvas.scaleY configure -showvalue no}
    place $canvas.scaleY -relx 1.0 -anchor ne
    $canvas.scaleX set 150
    $canvas.scaleY set 150
    Info scaleX,widget $canvas.scaleX
    Info scaleY,widget $canvas.scaleY

    # Create a new treectrl to copy the states/style/elements into, so I don't
    # have to worry about column width or item visibility in the demo list
    set T [treectrl $canvas.t]
    $T configure -itemheight 0 -minitemheight 0 -showbuttons no -showlines no \
	-font [$Tdemo cget -font]
    $T column create

    grid rowconfigure $fCanvas 0 -weight 1
    grid columnconfigure $fCanvas 0 -weight 1
    grid $canvas -row 0 -column 0 -sticky news
    grid $fCanvas.xscroll -row 1 -column 0 -sticky we
    grid $fCanvas.yscroll -row 0 -column 1 -sticky ns

    $w.pwH.pwV2 add $fRight.propertyList $fCanvas

    $w.pwH add $w.pwH.pwV $w.pwH.pwV2

    grid rowconfigure $w 0 -weight 1
    grid columnconfigure $w 0 -weight 1
    grid $w.pwH -row 0 -column 0 -sticky news

    wm protocol $w WM_DELETE_WINDOW "ToggleStyleEditorWindow"
    if {[Platform macosx macintosh]} {
	wm geometry $w +6+30
    } else {
	wm geometry $w -0+0
    }

    return
}

proc StyleEditor::SetListOfStyles {} {

    set T [Info styleList]
    set Tdemo [Info Tdemo]

    # Create elements and styles the first time this is called
    if {[llength [$T style names]] == 0} {
	$T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
	$T element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	    -showfocus yes

	$T style create s1
	$T style elements s1 {e2 e1}
	$T style layout s1 e2 -union [list e1] -ipadx 2 -ipady 2 -iexpand e

	$T column configure C0 -itemstyle s1
    }

    # Clear the list
    $T item delete all

    # One item for each style in the demo list
    foreach style [lsort -dictionary [$Tdemo style names]] {
	set I [$T item create]
	$T item text $I C0 $style
	$T item lastchild root $I

	Info item2style,$I $style
    }

    return
}

proc StyleEditor::SelectStyle {} {

    set T [Info styleList]
    set Tdemo [Info Tdemo]

    set selection [$T selection get]
    if {![llength $selection]} {
	[Info elementList] item delete all
	Info selectedStyle ""
	StyleToCanvas
	return
    }

    set I [lindex $selection 0]
    set style [Info item2style,$I]
    Info selectedStyle $style
    SetListOfElements $style

    Info -orient [$Tdemo style cget $style -orient]
    [Info scaleX,widget] set 150
    [Info scaleY,widget] set 150

    StyleToCanvas 1

    return
}

proc StyleEditor::SetListOfElements {style} {

    set T [Info elementList]
    set Tdemo [Info Tdemo]

    # Create elements and styles the first time this is called
    if {[llength [$T style names]] == 0} {
	$T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
	$T element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	    -showfocus yes

	$T style create s1
	$T style elements s1 {e2 e1}
	$T style layout s1 e2 -union [list e1] -ipadx 2 -ipady 2 -iexpand e

	$T column configure C0 -itemstyle s1
    }

    # Clear the list
    $T item delete all

    # One item for each element in the style
    foreach E [$Tdemo style elements $style] {
	set I [$T item create]
	$T item text $I C0 "$E ([$Tdemo element type $E])"
	$T item lastchild root $I

	Info item2element,$I $E
    }

    return
}

proc StyleEditor::SelectElement {} {

    set T [Info elementList]
    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]

    set selection [$T selection get]
    if {![llength $selection]} {
	Info selectedElement ""
	SetPropertyList
	CanvasSelectElement
	return
    }

    set I [lindex $selection 0]
    set element [Info item2element,$I]
    Info selectedElement $element

    SetPropertyList
    CanvasSelectElement

    return
}

proc StyleEditor::SetPropertyList {} {

    set T [Info propertyList]
    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]

    # Create elements and styles the first time this is called
    if {[llength [$T style names]] == 0} {
	$T state define header

	$T element create e1 text \
	    -fill [list white header $::SystemHighlightText selected] \
	    -font [list DemoFontBold header]

	set headerBG #ACA899
	if {[winfo depth $T] >= 16} {
	    $T gradient create G_header -steps 16 \
		-stops {{0.0 #ACA899} {0.5 #ACA899} {1.0 #d5d2ca}}
	    set headerBG G_header
	}
	$T element create e2 rect \
	    -fill [list $headerBG header $::SystemHighlight selected] \
	    -outline black -outlinewidth 1 -open nw -showfocus no

	$T element create eWindow window

	set S [$T style create s1]
	$T style elements $S {e2 e1}
	$T style layout $S e2 -detach yes -indent no -iexpand xy
	$T style layout $S e1 -expand ns -padx {4 0}

	set S [$T style create sWindow]
	$T style elements $S {e2 eWindow}
	$T style layout $S e2 -detach yes -indent no -iexpand xy
	$T style layout $S eWindow -expand ns -padx {0 1} -pady {0 1}

	set S [$T style create sHeader]
	$T style elements $S {e2 e1}
	$T style layout $S e2 -detach yes -iexpand xy
	$T style layout $S e1 -expand ns -padx {4 0}

	Info editor,pad [MakePadEditor $T]
	Info editor,expand [MakeExpandEditor $T]
	Info editor,iexpand [MakeIExpandEditor $T]
	Info editor,squeeze [MakeSqueezeEditor $T]
	Info editor,boolean [MakeBooleanEditor $T]
	Info editor,pixels [MakePixelsEditor $T]
	Info editor,string [MakeStringEditor $T]

	update idletasks
	set height 0
	foreach editor {pad expand iexpand squeeze boolean pixels string} {
	    set heightWin [winfo reqheight [Info editor,$editor]]
	    incr heightWin
	    if {$heightWin > $height} {
		set height $heightWin
	    }
	}
	$T configure -font [[Info editor,pad].v1 cget -font] \
	    -minitemheight $height

	$T column configure C0 -itemstyle s1
    }

    $T item delete all

    if {$element eq ""} return

    foreach {header option} {
	"Draw and Visible" ""
	"" -draw
	"" -visible
	"Detach" ""
	"" -detach
	"" -indent
	"Union" ""
	"" -union
	"Expand and Squeeze" ""
	"" -expand
	"" -iexpand
	"" -squeeze
	"Sticky" ""
	"" -sticky
	"Padding" ""
	"" -ipadx
	"" -ipady
	"" -padx
	"" -pady
	"Height" ""
	"" -minheight
	"" -height
	"" -maxheight
	"Width" ""
	"" -minwidth
	"" -width
	"" -maxwidth
    } {
 	set I [$T item create]
	if {$header ne ""} {
	    $T item style set $I C0 sHeader
	    $T item span $I C0 2
	    $T item element configure $I C0 e1 -text $header ; # -fill White
	    $T item state set $I header
	    $T item enabled $I false
	    $T item tag add $I header
	} else {
	    $T item style set $I value s1
	    $T item text $I property $option value [$Tdemo style layout $style $element $option]
	}
	$T item lastchild root $I
   }

    $T column configure C0 -width [expr {[$T column neededwidth C0] * 1.0}]

    return
}

proc StyleEditor::SelectProperty {select deselect} {

    set T [Info propertyList]
    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]

    if {[llength $deselect] && ($element ne "")} { 
	set I [lindex $deselect 0]
	if {[$T item tag expr $I !header]} {
	    set option [$T item text $I property]
	    $T item style set $I value s1
	    $T item text $I value [$Tdemo style layout $style $element $option]
	}
    }

    set selection [$T selection get]
    if {![llength $selection]} {
	Info selectedOption ""
	return
    }

    set I [lindex $selection 0]
    if {[$T item tag expr $I header]} {
	Info selectedOption ""
	return
    }
    set option [$T item text $I property]
    Info selectedOption $option

    $T item style set $I value sWindow
    switch -- $option {
	-draw -
	-visible -
	-union {
	    $T item element configure $I value eWindow -window [Info editor,string]
	    Info -string [$Tdemo style layout $style $element $option]
	}
	-padx -
	-pady -
	-ipadx -
	-ipady {
	    $T item element configure $I value eWindow -window [Info editor,pad]
	    set pad [$Tdemo style layout $style $element $option]
	    if {[llength $pad] == 2} {
		Info -pad,1 [lindex $pad 0]
		Info -pad,2 [lindex $pad 1]
		Info -pad,equal 0
	    } else {
		Info -pad,1 $pad
		Info -pad,2 $pad
		Info -pad,equal 1
	    }
	    Info -pad,edit ""
	}
	-expand -
	-sticky {
	    $T item element configure $I value eWindow -window [Info editor,expand]
	    set value [$Tdemo style layout $style $element $option]
	    foreach flag {n s w e} {
		Info -expand,$flag [expr {[string first $flag $value] != -1}]
	    }
	}
	-iexpand {
	    $T item element configure $I value eWindow -window [Info editor,iexpand]
	    set value [$Tdemo style layout $style $element $option]
	    foreach flag {x y n s w e} {
		Info -iexpand,$flag [expr {[string first $flag $value] != -1}]
	    }
	}
	-detach -
	-indent {
	    $T item element configure $I value eWindow -window [Info editor,boolean]
	    Info -boolean [$Tdemo style layout $style $element $option]
	}
	-squeeze {
	    $T item element configure $I value eWindow -window [Info editor,squeeze]
	    set value [$Tdemo style layout $style $element $option]
	    foreach flag {x y} {
		Info -squeeze,$flag [expr {[string first $flag $value] != -1}]
	    }
	}
	-minheight -
	-height -
	-maxheight -
	-minwidth -
	-width -
	-maxwidth {
	    $T item element configure $I value eWindow -window [Info editor,pixels]
	    Info -pixels [$Tdemo style layout $style $element $option]
	    if {[Info -pixels] eq ""} {
		Info -pixels,empty 1
		[Info editor,pixels].v1 conf -state disabled
	    } else {
		Info -pixels,empty 0
		[Info editor,pixels].v1 conf -state normal
	    }
	}
    }

    return
}

proc StyleEditor::MakePadEditor {parent} {

    set f [frame $parent.editPad -borderwidth 0 -background $::SystemHighlight]
    spinbox $f.v1 -from 0 -to 100 -width 3 \
	-command {StyleEditor::Sync_pad 1} \
	-textvariable ::StyleEditor::Info(-pad,1)
    spinbox $f.v2 -from 0 -to 100 -width 3 \
	-command {StyleEditor::Sync_pad 2} \
	-textvariable ::StyleEditor::Info(-pad,2)
    $::checkbuttonCmd $f.cb -text "Equal" \
	-command {StyleEditor::Sync_pad_equal} \
	-variable ::StyleEditor::Info(-pad,equal)
    pack $f.v1 -side left -padx {0 10} -pady 0
    pack $f.v2 -side left -padx {0 10} -pady 0
    pack $f.cb -side left -padx 0 -pady 0

    bind $f.v1 <KeyPress-Return> {
	StyleEditor::Sync_pad 1
    }
    bind $f.v2 <KeyPress-Return> {
	StyleEditor::Sync_pad 2
    }

    return $f
}

proc StyleEditor::MakeExpandEditor {parent} {

    set f [frame $parent.editExpand -borderwidth 0 -background $::SystemHighlight]
    foreach flag {w n e s} {
	$::checkbuttonCmd $f.$flag -text $flag -width 1 \
	    -variable ::StyleEditor::Info(-expand,$flag) \
	    -command {StyleEditor::Sync_expand}
	pack $f.$flag -side left -padx 10
    }

    return $f
}

proc StyleEditor::MakeIExpandEditor {parent} {

    set f [frame $parent.editIExpand -borderwidth 0 -background $::SystemHighlight]
    foreach flag {x y w n e s} {
	$::checkbuttonCmd $f.$flag -text $flag -width 1 \
	    -variable ::StyleEditor::Info(-iexpand,$flag) \
	    -command {StyleEditor::Sync_iexpand}
	pack $f.$flag -side left -padx 10
    }

    return $f
}

proc StyleEditor::MakePixelsEditor {parent} {

    set f [frame $parent.editPixels -borderwidth 0]
    spinbox $f.v1 -from 0 -to 10000 -width 10 \
	-command {StyleEditor::Sync_pixels} \
	-textvariable ::StyleEditor::Info(-pixels) \
	-validate key -validatecommand {string is integer %P}
    $::checkbuttonCmd $f.cb -text "Unspecified" \
	-command {StyleEditor::Sync_pixels} \
	-variable ::StyleEditor::Info(-pixels,empty)
    pack $f.v1 -side left -padx 0 -pady 0
    pack $f.cb -side left -padx 0 -pady 0

    bind $f.v1 <KeyPress-Return> {
	StyleEditor::Sync_pixels
    }

    return $f
}

proc StyleEditor::MakeSqueezeEditor {parent} {

    set f [frame $parent.editSqueeze -borderwidth 0 -background $::SystemHighlight]
    foreach flag {x y} {
	$::checkbuttonCmd $f.$flag -text $flag -width 1 \
	    -variable ::StyleEditor::Info(-squeeze,$flag) \
	    -command {StyleEditor::Sync_squeeze}
	pack $f.$flag -side left -padx 10
    }

    return $f
}

proc StyleEditor::MakeStringEditor {parent} {
    set f [frame $parent.editString -borderwidth 0]
    $::entryCmd $f.entry -textvariable ::StyleEditor::Info(-string)
    bind $f.entry <Return> {
	::StyleEditor::Sync_string
    }
    pack $f.entry -expand yes -fill both
    return $f
}

proc StyleEditor::MakeBooleanEditor {parent} {

    set f [frame $parent.editBoolean -borderwidth 0 -background $::SystemHighlight]
    foreach value {yes no} {
	$::radiobuttonCmd $f.$value -text $value \
	    -variable ::StyleEditor::Info(-boolean) \
	    -value $value \
	    -command {StyleEditor::Sync_boolean}
	pack $f.$value -side left -padx 10
    }

    return $f
}

proc StyleEditor::Sync_orient {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    $Tdemo style configure $style -orient [Info -orient]
    return
}

proc StyleEditor::Sync_pad {index} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    if {[Info -pad,equal]} {
	if {$index == 1} {
	    Info -pad,2 [Info -pad,1]
	} else {
	    Info -pad,1 [Info -pad,2]
	}
    }
    $Tdemo style layout $style $element $option [list [Info -pad,1] [Info -pad,2]]
    Info -pad,edit $index
    StyleToCanvas
    return
}

proc StyleEditor::Sync_pad_equal {} {
    if {![Info -pad,equal]} return
    if {[Info -pad,edit] eq ""} {
	Info -pad,edit 1
    }
    if {[Info -pad,edit] == 1} {
	Info -pad,2 [Info -pad,1]
    } else {
	Info -pad,1 [Info -pad,2]
    }
    Sync_pad [Info -pad,edit]
    return
}

proc StyleEditor::Sync_expand {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    set value ""
    foreach flag {n s w e} {
	if {[Info -expand,$flag]} {
	    append value $flag
	}
    }
    $Tdemo style layout $style $element $option $value
    StyleToCanvas
    return
}

proc StyleEditor::Sync_iexpand {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    set value ""
    foreach flag {x y n s w e} {
	if {[Info -iexpand,$flag]} {
	    append value $flag
	}
    }
    $Tdemo style layout $style $element $option $value
    StyleToCanvas
    return
}

proc StyleEditor::Sync_squeeze {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    set value ""
    foreach flag {x y} {
	if {[Info -squeeze,$flag]} {
	    append value $flag
	}
    }
    $Tdemo style layout $style $element -squeeze $value
    StyleToCanvas
    return
}

proc StyleEditor::Sync_string {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    if {[catch {
	$Tdemo style layout $style $element $option [Info -string]
	StyleToCanvas
    } msg]} {
	tk_messageBox -parent .styleEditor -icon error -title "Style Editor" \
	    -message $msg
    }
    return
}

proc StyleEditor::Sync_boolean {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    $Tdemo style layout $style $element $option [Info -boolean]
    StyleToCanvas
    return
}

proc StyleEditor::Sync_pixels {} {

    set Tdemo [Info Tdemo]
    set style [Info selectedStyle]
    set element [Info selectedElement]
    set option [Info selectedOption]
    set value [Info -pixels]
    if {[Info -pixels,empty]} {
	set value {}
	[Info editor,pixels].v1 conf -state disabled
    } else {
	[Info editor,pixels].v1 conf -state normal
    }
    $Tdemo style layout $style $element $option $value
    StyleToCanvas
    return
}

proc StyleEditor::StyleToCanvas {{scroll 0}} {

    set Tdemo [Info Tdemo]
    set canvas [Info canvas]
    set style [Info selectedStyle]

    $canvas delete all

    if {$style eq ""} {
	$canvas configure -scrollregion {0 0 0 0}
	return
    }

    set T $canvas.t

    $T configure -itemheight 0
    $T column configure 0 -width {}
    eval $T state undefine [$T state names]
    eval $T style delete [$T style names]
    eval $T element delete [$T element names]
    eval $T gradient delete [$T gradient names]

    # Copy states
    foreach state [$Tdemo state names] {
	$T state define $state
    }

    # Copy gradients (name only)
    foreach gradient [$Tdemo gradient names] {
	$T gradient create $gradient
    }

    # Copy elements
    foreach E [$Tdemo style elements $style] {
	$T element create $E [$Tdemo element type $E]
	foreach list [$Tdemo element configure $E] {
	    lassign $list name x y default current
	    $T element configure $E $name $current
	}
    }

    # Copy style
    $T style create $style -orient [$Tdemo style cget $style -orient]
    $T style elements $style [$Tdemo style elements $style]
    foreach E [$T style elements $style] {
	eval $T style layout $style $E [$Tdemo style layout $style $E]
	#$T style layout $style $E -visible {}
    }

    # Find a selected item using the style to copy element config info from
    set match ""
    foreach I [$Tdemo selection get] {
	foreach S [$Tdemo item style set $I] C [$Tdemo column list] {
	    if {$S eq $style} {
		set match $I
		break
	    }
	}
	if {$match ne ""} break
    }
    # No selected item uses the current style, look for an unselected item
    if {$match eq ""} {
	foreach I [$Tdemo item range first last] {
	    foreach S [$Tdemo item style set $I] C [$Tdemo column list] {
		if {$S eq $style} {
		    set match $I
		    break
		}
	    }
	    if {$match ne ""} break
	}
    }
    if {$match ne ""} {
	set I $match

	if {[$Tdemo selection includes $I]} {
	    $T selection add root
	} else {
	    $T selection clear
	}
	foreach state [$Tdemo item state get $I] {
	    if {[lsearch -exact [$Tdemo state names] $state] != -1} {
		$T item state set root $state
	    }
	}
	foreach state [$Tdemo item state forcolumn $I $C] {
	    $T item state set root $state
	}

	foreach E [$Tdemo item style elements $I $C] {
	    foreach list [$Tdemo item element configure $I $C $E] {
		lassign $list name x y default current
		set masterDefault [$Tdemo element cget $E $name]
		set sameAsMaster [string equal $masterDefault $current]
		if {!$sameAsMaster && ![string length $current]} {
		    set sameAsMaster 1
		    set current $masterDefault
		}
		if {$sameAsMaster} {
		} elseif {$name eq "-window"} {
		    $T style layout $style $E -width [winfo width $current] \
			-height [winfo height $current]
		} else {
		    $T element configure $E $name $current
		}
	    }
	}
    }
if 0 {
    # Do this after creating styles so -defaultstyle works
    foreach list [$Tdemo configure] {
	if {[llength $list] == 2} continue
	lassign $list name x y default current
	$T configure $name $current
    }
}
    set I root
    $T item style set $I 0 $style

    set scale 2
    
    set dx 10
    set dy 10

    scan [$T item bbox $I 0] "%d %d %d %d" x1 y1 x2 y2
    $canvas create rectangle \
	[expr {$dx + $x1 * $scale}] [expr {$dy + $y1 * $scale}] \
	[expr {$dx + $x2 * $scale}] [expr {$dy + $y2 * $scale}] \
	-outline gray90

    foreach E [$T style elements $style] {
	if {[scan [$T item bbox $I 0 $E] "%d %d %d %d" x1 y1 x2 y2] == 4} {
	    $canvas create rectangle \
		[expr {$dx + $x1 * $scale}] [expr {$dy + $y1 * $scale}] \
		[expr {$dx + $x2 * $scale}] [expr {$dy + $y2 * $scale}] \
		-tags [list $E element]
	}
    }

    scan [$T item bbox $I 0] "%d %d %d %d" x1 y1 x2 y2
    incr dy [expr {($y2 - $y1) * $scale + 20}]

    # Now give the style 1.5 times its needed space to test expansion
    scan [$T item bbox $I 0] "%d %d %d %d" x1 y1 x2 y2
    $T column configure 0 -width [expr {($x2 - $x1) * [Info scaleX]}]
    $T configure -itemheight [expr {($y2 - $y1) * [Info scaleY]}]

    scan [$T item bbox $I 0] "%d %d %d %d" x1 y1 x2 y2

    $canvas create rectangle \
	[expr {$dx + $x1 * $scale}] [expr {$dy + $y1 * $scale}] \
	[expr {$dx + $x2 * $scale}] [expr {$dy + $y2 * $scale}] \
	-outline gray90

    foreach E [$T style elements $style] {
	if {[scan [$T item bbox $I 0 $E] "%d %d %d %d" x1 y1 x2 y2] == 4} {
	    $canvas create rectangle \
		[expr {$dx + $x1 * $scale}] [expr {$dy + $y1 * $scale}] \
		[expr {$dx + $x2 * $scale}] [expr {$dy + $y2 * $scale}] \
		-tags [list $E element]
	}
    }

    scan [$canvas bbox all] "%d %d %d %d" x1 y1 x2 y2
    incr x2 10
    incr y2 10
    $canvas configure -scrollregion [list 0 0 $x2 $y2]
    if {$scroll} {
	$canvas xview moveto 0.0
	$canvas yview moveto 0.0
    }

    CanvasSelectElement

    return
}

proc StyleEditor::CanvasSelectElement {} {

    set canvas [Info canvas]
    set style [Info selectedStyle]
    set element [Info selectedElement]

    $canvas itemconfigure element -fill "" -outline black
    if {$element ne ""} {
	$canvas itemconfigure $element -fill gray75 -outline green
    }

    return
}

proc StyleEditor::ScaleX {value} {
    Info scaleX [expr {$value / 100.0}]
    StyleToCanvas
}

proc StyleEditor::ScaleY {value} {
    Info scaleY [expr {$value / 100.0}]
    StyleToCanvas
}

