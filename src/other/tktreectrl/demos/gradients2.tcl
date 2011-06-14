# RCS: @(#) $Id$

proc DemoGradients2 {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure \
	-showbuttons no -showlines no -showroot no \
	-xscrollincrement 20 -yscrollincrement 20 \
	-xscrollsmoothing yes -yscrollsmoothing yes \

    #
    # Create columns
    #

    for {set i 0} {$i < 6} {incr i} {
	$T column create -text "Column $i" -width 100 -tags C$i
    }

    set steps 25
    set stops {{0.0 "light green"} {1.0 white}}

    $T gradient create G_C0 -stops $stops -steps $steps -orient vertical
    $T column configure C0 -itembackground G_C0

    $T gradient create G_C1 -stops $stops -steps $steps -orient vertical
    $T gradient configure G_C1 -top {0.0 area content} -bottom {1.0 area content}
    $T column configure C1 -itembackground G_C1

    $T gradient create G_C2 -stops $stops -steps $steps -orient vertical
    $T gradient configure G_C2 -top {0.0 canvas} -bottom {1.0 canvas}
    $T column configure C2 -itembackground G_C2

    $T gradient create G_C3 -stops $stops -steps $steps -orient horizontal
    $T gradient configure G_C3 -left {} -right {}
    $T column configure C3 -itembackground G_C3

    $T gradient create G_C4 -stops $stops -steps $steps -orient horizontal
    $T gradient configure G_C4 -left {0.0 area content} -right {1.0 area content}
    $T column configure C4 -itembackground G_C4

    $T gradient create G_C5 -stops $stops -steps $steps -orient horizontal
    $T gradient configure G_C5 -left {0.0 canvas} -right {1.0 canvas}
    $T column configure C5 -itembackground G_C5

    #
    # Define new states
    #

    $T state define openW
    $T state define openN

    #
    # Create elements
    #

    $T element create elemTextIntro text -width [expr {600 - 4 * 2}]
    $T element create elemText text -width [expr {600 - 4 * 2}]

    $T element create elemBox rect -width 100 -height 50 \
	-outline gray -outlinewidth 1 -open {wn {openW openN} w openW n openN}

    #
    # Create styles using the elements
    #

    $T style create styleIntro
    $T style elements styleIntro elemTextIntro
    $T style layout styleIntro elemTextIntro -padx 4 -pady {3 0}

    $T style create styleBox
    $T style elements styleBox elemBox

    #
    # Create items and assign styles
    #

    set I [$T item create -parent root]
    $T item style set $I C0 styleIntro
    $T item span $I C0 6
    $T item text $I C0 "This demonstrates column -itembackground colors with gradients.\n
Column 0 has a vertical gradient with unspecified bounds, so the gradient\
    is as tall as each item.\n    \
    G_C0 -top { } -bottom { }\n
Column 1 has a vertical gradient as tall as the content area.  The colors\
    remain 'locked in place' as the list is scrolled up and down.\n    \
    G_C1 -top {0.0 area content} -bottom {1.0 area content}\n
Column 2 has a vertical gradient as tall as the canvas.  The first stop color\
    begins at the top of the first item and the last stop color ends at the\
    bottom of the last item.\n    \
    G_C2 -top {0.0 canvas} -bottom {1.0 canvas}\n
Column 3 has a horizontal gradient with unspecified bounds, so the gradient\
    is as wide as the column.\n    \
    G_C3 -left {} -right {}\n
Column 4 has a horizontal gradient as wide as the content area.  The colors\
    remain 'locked in place' as the list is scrolled left and right, and\
    resizing the window changes the part of the gradient in that column.\n    \
    G_C4 -left {0.0 area content} -right {1.0 area content}\n
Column 5 has a horizontal gradient as wide as the canvas.  The first stop color\
    begins at the left edge of the first column and the last stop color ends at the\
    right edge of the last column.  If the window is wider than the items then\
    the gradient ends at the window border.\n    \
    G_C5 -left {0.0 canvas} -right {1.0 canvas}"


    for {set i 0} {$i < 25} {incr i} {
	set I [$T item create -parent root]
	$T item state forcolumn $I {range 1 end} openW
	$T item style set $I all styleBox
    }

    return
}
