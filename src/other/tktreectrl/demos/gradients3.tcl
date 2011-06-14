# RCS: @(#) $Id$

proc DemoGradients3 {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no -showlines no -showroot no \
	-xscrollincrement 20 -yscrollincrement 20 \
	-xscrollsmoothing yes -yscrollsmoothing yes \
	-canvaspady 5 -itemgapy 8

    #
    # Create columns
    #

    $T column create -text "Column 0" -width 110 -tags C0
    $T column create -text "Column 1" -width 110 -tags C1
    $T column create -text "Column 2" -width 110 -tags C2

    #
    # Create elements
    #

    $T element create elemBox rect -width 100 -height 30 -fill gray95
    $T element create elemText text -width [expr {300 - 4 * 2}]

    #
    # Create styles using the elements
    #

    $T style create styleText
    $T style elements styleText elemText
    $T style layout styleText elemText -padx 4 -pady 6

    $T style create styleBox
    $T style elements styleBox elemBox
    $T style layout styleBox elemBox -padx {5 5}

    $T column configure {all !tail} -itemstyle styleBox

    #
    # Create items and assign styles
    #

    $T gradient create G1 -stops {{0.0 blue} {1.0 green}} -steps 15
    $T gradient create G2 -stops {{0.0 green} {1.0 red}} -steps 15
    $T gradient create G3 -stops {{0.0 red} {1.0 blue}} -steps 15

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 3
    $T item text $I C0 "Outlined rectangles\nThis also demonstrates the -open\
    option of rect elements."

    foreach open {"" w nw n ne e se s sw we ns} {
	set I [$T item create -parent root]
	$T item element configure $I C0 elemBox -outline G1 -outlinewidth 1 -open $open
	$T item element configure $I C1 elemBox -outline G2 -outlinewidth 2 -open $open
	$T item element configure $I C2 elemBox -outline G3 -outlinewidth 3 -open $open
    }

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 3
    $T item text $I C0 "Outlined rounded rectangles\nNote that rounded rectangles\
    will not be filled or outlined with gradients unless the platform supports gradients\
    natively."

    foreach open {"" w nw n ne e se s sw we ns} {
	set I [$T item create -parent root]
	$T item element configure $I C0 elemBox -outline G1 -outlinewidth 1 -open $open -rx 5
	$T item element configure $I C1 elemBox -outline G2 -outlinewidth 2 -open $open -rx 5
	$T item element configure $I C2 elemBox -outline G3 -outlinewidth 3 -open $open -rx 5
    }

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 3
    $T item text $I C0 "Filled rectangles"

    set I [$T item create -parent root]
    $T item element configure $I C0 elemBox -fill G1 -open $open
    $T item element configure $I C1 elemBox -fill G2 -open $open
    $T item element configure $I C2 elemBox -fill G3 -open $open

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 3
    $T item text $I C0 "Filled rounded rectangles"

    foreach open {"" w nw n ne e se s sw we ns} {
	set I [$T item create -parent root]
	$T item element configure $I C0 elemBox -fill G1 -open $open -rx 7
	$T item element configure $I C1 elemBox -fill G2 -open $open -rx 7
	$T item element configure $I C2 elemBox -fill G3 -open $open -rx 7
    }

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 3
    $T item text $I C0 "Eyecandy"

    #####
    
    set height 30
    set steps [expr {($height - 5)/2}]
    $T gradient create G_mouseover       -steps $steps -stops {{0.0 white} {1.0 #ebf3fd}} -orient vertical
    $T gradient create G_selected_active -steps $steps -stops {{0.0 #dcebfc} {1.0 #c1dbfc}} -orient vertical
    $T gradient create G_selected        -steps $steps -stops {{0.0 #ebf4fe} {1.0 #cfe4fe}} -orient vertical
    $T gradient create G_focusout        -steps $steps -stops {{0.0 #f8f8f8} {1.0 #e5e5e5}} -orient vertical

    $T state define mouseover

    $T element create elemRectGradient rect \
	-fill [list	G_selected_active {mouseover} \
			G_focusout {!focus} \
			G_selected {}]

    $T element create elemRectOutline rect -rx 1 \
	-outline [list #7da2ce {mouseover} \
		       #d9d9d9 {!focus} \
		       #7da2ce {}] -outlinewidth 1

    set S [$T style create styleExplorer]
    $T style elements $S {elemRectGradient elemRectOutline}
    $T style layout $S elemRectGradient -padx 0 -pady {2 3} -iexpand xy
    $T style layout $S elemRectOutline -union elemRectGradient -ipadx 2 -ipady 2 -padx 5

    set I [$T item create -parent root -height $height]
    $T item style set $I C0 styleExplorer
    $T item style set $I C1 styleExplorer
    $T item style set $I C2 styleExplorer

    #####

    $T gradient create G_green -orient vertical -steps 4 \
	-stops {{0 #00680a} {0.05 #00680a} {0.1 #197622} {0.45 #197622} {0.5 #00680a} {0.6 #00680a} {1 #00c82c}}
    set I [$T item create -parent root]
    $T item span $I C0 3
    $T item element configure $I C0 elemBox -fill G_green -width [expr {110 * 3 - 10}]

    #####

    $T gradient create G_orange1 -orient vertical -steps 4 \
	-stops {{0 #fde8d1} {0.3 #fde8d1} {0.3 #ffce69} {0.6 #ffce69} {1 #fff3c3}}
    $T gradient create G_orange2 -orient vertical -steps 4 \
	-stops {{0 #fffef6} {0.3 #fffef6} {0.3 #ffef9a} {0.6 #ffef9a} {1 #fffce8}}

    set height 40

    $T element create elemOrangeOutline rect -outline #ffb700 -outlinewidth 1 -rx 1
    $T element create elemOrangeBox rect -fill {G_orange1 mouseover G_orange2 {}} -width [expr {110 * 3 - 2 * 2 - 10}] \
	-height [expr {$height - 2 * 2}]

    set S [$T style create styleOrange]
    $T style elements $S {elemOrangeOutline elemOrangeBox}
    $T style layout $S elemOrangeOutline -union elemOrangeBox -ipadx 2 -ipady 2 -padx 5

    set I [$T item create -parent root]
    $T item span $I C0 3
    $T item style set $I C0 $S

    #####

    $T gradient create G_progressFG -orient vertical -steps 2 \
	-stops {{0 #cdffcd} {0.2 #cdffcd} {0.25 #a2efaf} {0.45 #a2efaf} {0.5 #00d428} {1.0 #1ce233}}
    $T gradient create G_progressBG -orient vertical -steps 2 \
	-stops {{0 white} {0.45 #dbdbdb} {0.5 #cacaca} {1.0 #cacaca}}

    set width [expr {110 * 3 - 2 * 1 - 10}]
    set percent 0.33
    $T element create elemProgressOutline rect -rx 1 -outline gray -outlinewidth 1
    $T element create elemProgressBG rect -fill G_progressBG -height 12 \
	-width $width -outline #eaeaea -outlinewidth 1
    $T element create elemProgressFG rect -fill G_progressFG -height 12 \
	-width [expr {$width * $percent}]

    set S [$T style create styleProgress]
    $T style elements $S {elemProgressOutline elemProgressBG elemProgressFG}
    $T style layout $S elemProgressOutline -union elemProgressBG -padx 5 -ipadx 1 -ipady 1
    $T style layout $S elemProgressFG -detach yes -padx 6 -pady 1

    set I [$T item create -parent root -tags progress]
    $T item span $I C0 3
    $T item style set $I C0 $S C1 "" C2 ""

    set ::Gradients3(percent) 0.0
    set ::Gradients3(afterId) ""

    # Pause/resume animating when the progress bar's visibility changes
    $T notify bind $T <ItemVisibility> {
	Gradients3ItemVisibility %T %v %h
    }

    # Stop animating when the item is deleted
    $T notify bind $T <ItemDelete> {
	after cancel $Gradients3(afterId)
	dbwin "progressbar deleted"
    }

    #####

    set ::Gradients3(prev) ""
    bind DemoGradients3 <Motion> {
	Gradients3Motion %W %x %y
    }
    bind DemoGradients3 <Leave> {
	Gradients3Motion %W -1 -1
    }

    bindtags $T [concat DemoGradients3 [bindtags $T]]

    return
}

proc Gradients3Motion {T x y} {
    global Gradients3
    if {[lsearch -exact [$T state names] mouseover] == -1} return
    set id [$T identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item" && [llength $id] > 4} {
	set item [lindex $id 1]
	set column [lindex $id 3]
	set curr [list $item $column]
	if {$curr ne $Gradients3(prev)} {
	    if {$Gradients3(prev) ne ""} {
		eval $T item state forcolumn $Gradients3(prev) !mouseover
	    }
	    $T item state forcolumn $item $column mouseover
	    set Gradients3(prev) $curr
	}
	return
    }
    if {$Gradients3(prev) ne ""} {
	eval $T item state forcolumn $Gradients3(prev) !mouseover
	set Gradients3(prev) ""
    }
    return
}

proc Gradients3Progress {T} {
    set percent $::Gradients3(percent)
    if {$percent > 1.0} {
	set percent 1.0
	set ::Gradients3(percent) 0.0
    } else {
	set ::Gradients3(percent) [expr {$percent + 0.025}]
    }
    set width [expr {110 * 3 - 2 * 1 - 10}]
    $T element configure elemProgressFG -width [expr {$width * $percent}]
    set ::Gradients3(afterId) [after 100 [list Gradients3Progress $T]]
}

proc Gradients3ItemVisibility {T visible hidden} {
    foreach I $visible {
	if {[$T item tag expr $I progress]} {
	    set ::Gradients3(afterId) [after 100 [list Gradients3Progress $T]]
	    dbwin "progress resumed"
	    return
	}
    }
    foreach I $hidden {
	if {[$T item tag expr $I progress]} {
	    after cancel $::Gradients3(afterId)
	    set ::Gradients3(afterId) ""
	    dbwin "progress paused"
	    return
	}
    }
    return
}

proc rgb {r g b} {
    format #%.2x%.2x%.2x $r $g $b
}
