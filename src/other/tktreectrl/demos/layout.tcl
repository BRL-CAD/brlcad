# RCS: @(#) $Id$

#
# Demo: Layout
#
proc DemoLayout {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showrootbutton yes -showbuttons yes \
	-showlines [ShouldShowLines $T] -itemheight 0 -selectmode browse

    #
    # Create columns
    #

    $T column create -text Layout -itemjustify left -justify center -tags C0
    $T configure -treecolumn C0

    #
    # Create elements
    #

    $T element create e1 rect -width 30 -height 30 -fill gray20
    $T element create e2 rect -width 30 -height 30 -fill gray40 \
	-outline blue -outlinewidth 3
    $T element create e3 rect -fill gray60
    $T element create e4 rect -fill [list $::SystemHighlight {selected focus} gray80 {}] \
	-showfocus yes
    $T element create e5 rect -fill {"sky blue"} -width 20 -height 20
    $T element create e6 rect -fill {"sea green"} -width 30 -height 16
    $T element create e7 rect -fill {"sky blue"} -width 30 -height 16
    $T element create e8 rect -fill gray70 -height 1

    #
    # Create styles using the elements
    #

    set S [$T style create s1]
    $T style elements $S {e4 e3 e1 e2 e5 e6 e7}
    $T style layout $S e1 -padx {28 4} -pady 4
    $T style layout $S e2 -expand es -padx {0 38}
    $T style layout $S e3 -union [list e1 e2] -ipadx 4 -ipady 4 -pady 2
    $T style layout $S e4 -detach yes -iexpand xy
    $T style layout $S e5 -detach yes -padx {2 0} -pady 2 -iexpand y
    $T style layout $S e6 -detach yes -expand ws -padx {0 2} -pady {2 0}
    $T style layout $S e7 -detach yes -expand wn -padx {0 2} -pady {0 2}

    #
    # Create items and assign styles
    #

    set I [$T item create -button yes]
    $T item style set $I C0 $S
    $T item lastchild root $I
    set parent $I

    set I [$T item create]
    $T item style set $I C0 $S
    $T item lastchild $parent $I

    ###

    set S [$T style create s2]
    $T style elements $S {e4 e3 e1}
    $T style layout $S e1 -padx 8 -pady 8 -iexpand x
    $T style layout $S e3 -union e1 -ipadx {20 4} -ipady {4 12}
    $T style layout $S e4 -detach yes -iexpand xy

    set I [$T item create -button yes]
    $T item style set $I C0 $S
    $T item lastchild root $I

    set I2 [$T item create]
    $T item style set $I2 C0 $S
    $T item lastchild $I $I2

    ###

    set S [$T style create s3]
    $T style elements $S {e4 e3 e1 e5 e6}
    $T style layout $S e4 -union {e1 e6} -ipadx 8 -ipady {8 0}
    $T style layout $S e3 -union {e1 e5} -ipadx 4 -ipady 4
    $T style layout $S e5 -height 40

    set I [$T item create -button yes]
    $T item style set $I C0 $S
    $T item lastchild root $I

    set I2 [$T item create]
    $T item style set $I2 C0 $S
    $T item lastchild $I $I2

    ###

    $T element create eb border -background $::SystemButtonFace \
	-relief {sunken {selected} raised {}} -thickness 2 -filled yes
    $T element create et text -lmargin2 20

    set text "Here is a text element surrounded by a border element.\nResize the column to watch me wrap."

    set S [$T style create s4]
    $T style elements $S {eb et}
    $T style layout $S eb -union et -ipadx 2 -ipady 2
    $T style layout $S et -squeeze x

    set I [$T item create -button yes]
    $T item style set $I C0 $S
    $T item text $I C0 $text
    $T item lastchild root $I
    set parent $I

    set I [$T item create]
    $T item style set $I C0 $S
    $T item text $I C0 $text
    $T item lastchild $parent $I

    ###

    set S [$T style create s5]
    $T style elements $S {e1 e2 e3 e4 e5 e6 e7 e8}
    $T style layout $S e1 -union e2 -ipadx 4 -ipady 4
    $T style layout $S e2 -union e3 -ipadx 4 -ipady 4 -visible {no selected}
    $T style layout $S e3 -union e4 -ipadx 4 -ipady 4
    $T style layout $S e4 -width 30 -height 30
    $T style layout $S e5 -union e6 -ipadx 4 -ipady 4
    $T style layout $S e6 -union e7 -ipadx 4 -ipady 4 -visible {no selected}
    $T style layout $S e7 -union e8 -ipadx 4 -ipady 4
    $T style layout $S e8 -width 30 -height 30 -padx {24 0}

    set I [$T item create -button yes]
    $T item style set $I C0 $S
    $T item lastchild root $I

    set I2 [$T item create]
    $T item style set $I2 C0 $S
    $T item lastchild $I $I2

    ###

    set styleNum 6
    foreach {orient expandList} {horizontal {s ns n} vertical {e we w}} {
	foreach expand $expandList {

	    set S [$T style create s$styleNum -orient $orient]
	    $T style elements $S {e4 e8 e3 e2 e5 e6}
	    $T style layout $S e4 -detach yes -iexpand xy
	    $T style layout $S e8 -detach yes -expand n -iexpand x
	    $T style layout $S e3 -union {e2 e5 e6} -ipadx 5 -ipady 5
	    $T style layout $S e2 -expand $expand
	    $T style layout $S e5 -expand $expand -visible {no !selected}
	    $T style layout $S e6 -expand $expand
	    incr styleNum

	    set I [$T item create]
	    $T item style set $I C0 $S
	    $T item lastchild root $I
	}
    }

    return
}

