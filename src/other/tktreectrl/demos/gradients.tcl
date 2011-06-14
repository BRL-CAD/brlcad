# RCS: @(#) $Id$

proc DemoGradients {} {

    set T [DemoList]

    if {[lsearch -exact [font names] DemoGradientFont] == -1} {
	array set fontInfo [font actual [$T cget -font]]
	set fontInfo(-weight) bold
	eval font create DemoGradientFont [array get fontInfo]
    }

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no -showlines no -showroot no \
	-xscrollincrement 20 -yscrollincrement 20 \
	-xscrollsmoothing yes -yscrollsmoothing yes \
	-canvaspadx 20

    #
    # Create columns
    #

    for {set i 0} {$i < 6} {incr i} {
	$T column create -text "Column $i" -width 100 -tags C$i
    }

    #
    # Define new states
    #

    $T state define openW
    $T state define openN

    #
    # Create elements
    #

    $T element create elemTextIntro text -width [expr {600 - 4 * 2}] -font DemoGradientFont
    $T element create elemText text -width [expr {600 - 4 * 2}]

    $T element create elemBox rect -width 100 -height 30 \
	-outline black -outlinewidth 2 -open {wn {openW openN} w openW n openN}

    #
    # Create styles using the elements
    #

    $T style create styleIntro
    $T style elements styleIntro elemTextIntro
    $T style layout styleIntro elemTextIntro -padx 4 -pady {3 0}

    $T style create styleText
    $T style elements styleText elemText
    $T style layout styleText elemText -padx 4 -pady {15 6}

    $T style create styleBox
    $T style elements styleBox elemBox

    #
    # Create items and assign styles
    #

    set stops {{0.0 blue} {0.5 green} {1.0 red}}
    set steps 10

    set I [$T item create -parent root]
    $T item style set $I C0 styleIntro
    $T item span $I C0 6
    $T item text $I C0 "In all the examples below, each item has the same\
	style in every column painted with a gradient.  Every gradient has\
	exactly the same colors. The items appear different only because of\
	the different coordinates specified for each gradient."

    # Example 1: no gradient coords specified
    $T gradient create G1 -stops $stops -steps $steps
    $T gradient configure G1 -left {} -right {} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 1:  A single item with a single horizontal\
	gradient.  The coordinates are unspecified, so the gradient brush has the\
	same bounds as each rectangle being painted\n\
	G1 -left { } -right { } -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G1


    # Example 2: -left {0.0 item} -right {0.5 item}
    $T gradient create G2 -stops $stops -steps $steps
    $T gradient configure G2 -left {0.0 item} -right {0.5 item} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 2:  A single item with a single horizontal\
	gradient.  The right side of the gradient is set to 1/2 the width\
	of the item, which results in the gradient pattern being tiled when\
	painting the other half of the item.\n\
	G2 -left {0.0 item} -right {0.5 item} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G2


    # Example 3: -left {0.0 item} -right {1.0 item}
    $T gradient create G3 -stops $stops -steps $steps
    $T gradient configure G3 -left {0.0 item} -right {1.0 item} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 3:  A single item with a single horizontal \
	gradient.  The gradient extends from the left side to the right side\
	of the item.\n\
	G3 -left {0.0 item} -right {1.0 item} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G3


    # Example 4: 3 items, vertical gradient, no gradient coords specified
    $T gradient create G4 -stops $stops -steps $steps -orient vertical
    $T gradient configure G4 -left {} -right {} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 4:  3 items with a single vertical gradient. \
	The coordinates are unspecified, so the gradient brush has the\
	same bounds as each rectangle being painted\n\
	G4 -left { } -right { } -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G4

    set I [$T item create -parent root]
    $T item state set $I openN
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G4

    set I [$T item create -parent root]
    $T item state set $I openN
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G4


    # Example 5: 3 items, 3 vertical gradients
    $T gradient create G5.1 -stops $stops -steps $steps -orient vertical
    $T gradient configure G5.1 -left {} -right {} -top {} -bottom {3.0 item}

    $T gradient create G5.2 -stops $stops -steps $steps -orient vertical
    $T gradient configure G5.2 -left {} -right {} -top {-1.0 item} -bottom {2.0 item}

    $T gradient create G5.3 -stops $stops -steps $steps -orient vertical
    $T gradient configure G5.3 -left {} -right {} -top {-2.0 item} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 5:  3 items with 3 vertical gradients.  Each\
	gradient uses item-relative coordinates to give the appearance of\
	a single seamless gradient.\n\
	G5.1 -left { } -right { } -top { } -bottom {3.0 item}\n\
	G5.2 -left { } -right { } -top {-1.0 item} -bottom {2.0 item}\n\
	G5.3 -left { } -right { } -top {-2.0 item} -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G5.1

    set I [$T item create -parent root]
    $T item state set $I openN
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G5.2

    set I [$T item create -parent root]
    $T item state set $I openN
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G5.3


    # Example 6: 3 items, 1 vertical gradient
    $T gradient create G6 -stops $stops -steps $steps -orient vertical

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 6:  3 items with a single vertical gradient. \
	The -top and -bottom coordinates specify the first and third items\
	respectively.  The appearance is exactly the same as the previous\
	example, but uses only 1 gradient instead of 3.  The words \"I6.1\"\
	and \"I6.3\" are item -tags.\n\
	G6 -top {0.0 item I6.1} -bottom {1.0 item I6.3}"

    set I1 [$T item create -parent root -tags I6.1]
    $T item state forcolumn $I1 {range 1 end} openW
    $T item style set $I1 all styleBox
    $T item element configure $I1 all elemBox -fill G6

    set I2 [$T item create -parent root -tags I6.2]
    $T item state set $I2 openN
    $T item state forcolumn $I2 {range 1 end} openW
    $T item style set $I2 all styleBox
    $T item element configure $I2 all elemBox -fill G6

    set I3 [$T item create -parent root -tags I6.3]
    $T item state set $I3 openN
    $T item state forcolumn $I3 {range 1 end} openW
    $T item style set $I3 all styleBox
    $T item element configure $I3 all elemBox -fill G6

    $T gradient configure G6 -top {0.0 item I6.1} -bottom {1.0 item I6.3}


    # Example 7: column-relative
    $T gradient create G7 -stops $stops -steps $steps
    $T gradient configure G7 -left {0.0 column} -right {1.0 column} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 7:  A single item with a single horizontal\
	gradient.  The gradient brush starts at the left edge of each column\
	and extends to the right edge of the same column.\n\
	G7 -left {0.0 column} -right {1.0 column} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G7


    # Example 8: column-relative
    $T gradient create G8 -stops $stops -steps $steps
    $T gradient configure G8 -left {0.0 column} -right {1.5 column} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 8:  A single item with a single horizontal\
	gradient.  The gradient brush starts at the left edge of each column\
	and extends to the half-way point of the next visible column.  Notice\
	that the right-most column shows all of the gradient since there\
	is no visible column to the right of it.\n\
	G8 -left {0.0 column} -right {1.5 column} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G8


    # Example 9: column-relative
    $T gradient create G9 -stops $stops -steps $steps
    $T gradient configure G9 -left {-0.5 column} -right {1.0 column} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 9:  A single item with a single horizontal\
	gradient.  The gradient brush starts at the half-way point of the\
	previous visible column and extends to the right edge of each column. \
	Notice that the left-most column shows all of the gradient since there\
	is no visible column to the left of it.\n\
	G9 -left {-0.5 column} -right {1.0 column} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G9


    # Example 10: column-relative
    $T gradient create G10 -stops $stops -steps $steps
    $T gradient configure G10 -left {0.0 column C0} -right {1.0 column C2} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 10:  A single item with a single horizontal\
	gradient.  The gradient brush starts at the left edge of column 0\
	and extends to the right edge of column 2.  The gradient pattern is\
	tiled when painting the other half of the item.  The words \"C0\" and\
	\"C2\" are column -tags.\n\
	G10 -left {0.0 column C0} -right {1.0 column C2} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G10


    # Example 11: content-relative
    $T gradient create G11 -stops $stops -steps $steps
    $T gradient configure G11 -left {0.0 area content} -right {1.0 area content} -top {} -bottom {}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 11:  A single item with a single horizontal\
	gradient.  The gradient brush starts inside the left window border and\
	extends to the inside edge of the right window border (the 'content'\
	area).  The gradient brush changes width as the demo window is resized.\n\
	G11 -left {0.0 area content} -right {1.0 area content} -top { } -bottom { }"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G11


    # Example 12: content-relative
    $T gradient create G12 -stops $stops -steps $steps -orient vertical
    $T gradient configure G12 -left {} -right {} -top {0.0 area content} -bottom {1.0 area content}

    set I [$T item create -parent root]
    $T item style set $I C0 styleText
    $T item span $I C0 6
    $T item text $I C0 "Example 12:  A single item with a single vertical\
	gradient.  The gradient brush starts inside the top window border and\
	extends to the inside edge of the bottom window border (the 'content'\
	area).  The gradient brush changes height as the demo window is resized,\
	and scrolling the list vertically changes which part of the gradient is\
	painted in the item.\n\
	G12 -left { } -right { } -top {0.0 area content} -bottom {1.0 area content}"

    set I [$T item create -parent root]
    $T item state forcolumn $I {range 1 end} openW
    $T item style set $I all styleBox
    $T item element configure $I all elemBox -fill G12

    return
}
