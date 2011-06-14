# RCS: @(#) $Id$

#
# Demo: iMovie
#
proc DemoIMovie {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode browse -orient horizontal -wrap window \
	-showheader no -background #dcdcdc -yscrollsmoothing yes

    $T configure -canvaspadx 8 -canvaspady 8 \
	-itemgapx 8 -itemgapy 8

    #
    # Create columns
    #

    $T column create -tags C0

    InitPics imovie-*

    switch -- $::thisPlatform {
	macintosh -
	macosx {
	    set font1 {Geneva 10}
	    set font2 {Geneva 11}
	}
	unix {
	    set font1 {Helvetica 12}
	    set font2 {Helvetica 14}
	}
	default {
	    set font1 {Helvetica 8}
	    set font2 {Helvetica 10}
	}
    }

    #
    # Create elements
    #

    $T element create elemTime text -font [list $font1]
    $T element create elemName text -font [list $font2] -lines 1 -width 80
    $T element create elemRect rect -fill {#ffdc5a {selected} white {}} \
	-outline #827878 -outlinewidth 1
    $T element create elemImg image
    $T element create elemShadow rect -outline gray -outlinewidth 1 -open wn

    #
    # Create styles using the elements
    #

    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemShadow elemRect elemTime elemImg elemName}
    $T style layout $S elemShadow -detach yes -padx {1 0} -pady {1 0} -iexpand xy
    $T style layout $S elemTime -padx {2 0}
    $T style layout $S elemImg -pady {0 1}
    $T style layout $S elemName -expand we -ipady {0 2} -padx {0 3} -squeeze x
    $T style layout $S elemRect -union {elemTime elemImg elemName} \
	-ipadx 6 -padx {0 1} -pady {0 1}

    # Set default item style
    $T column configure C0 -itemstyle $S

    #
    # Create items and assign styles
    #

    for {set i 0} {$i < 5} {incr i} {
	foreach {time name image} {
	    15:20 "Clip 1" imovie-01
	    19:18 "Clip 2" imovie-02
	    07:20 "Clip 3" imovie-03
	    07:20 "Clip 4" imovie-04
	    07:20 "Clip 5" imovie-05
	    07:20 "Clip 6" imovie-06
	    07:20 "Clip 7" imovie-07
	} {
	    set I [$T item create]
#			$T item style set $I C0 $S
	    $T item element configure $I C0 \
		elemTime -text $time + \
		elemImg -image $image + \
		elemName -text $name
	    $T item lastchild root $I
	}
    }

    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }

    bind DemoIMovie <ButtonPress-1> {
	iMovieButton1 %W %x %y
    }

    bindtags $T [list $T DemoIMovie TreeCtrl [winfo toplevel $T] all]

    return
}

proc iMovieButton1 {T x y} {
    focus $T
    set id [$T identify $x $y]

    # Click outside any item
    if {$id eq ""} {

    # Click in header
    } elseif {[lindex $id 0] eq "header"} {
	::TreeCtrl::ButtonPress1 $T $x $y

    # Click in item
    } elseif {[lindex $id 0] eq "item"} {
	::TreeCtrl::ButtonPress1 $T $x $y
	update
	lassign $id where item arg1 arg2 arg3 arg4
	switch $arg1 {
	    column {
		set I [lindex $id 1]
		if {[llength $id] == 6} {
		    set E [lindex $id end]
		    if {$E eq "elemName"} {
			set exists [winfo exists $T.entry]
			::TreeCtrl::EntryOpen $T $I C0 $E
			if {!$exists} {
			    $T.entry configure -borderwidth 0 -justify center \
				-background #ffdc5a
			    scan [$T item bbox $I C0 $E] "%d %d %d %d" x1 y1 x2 y2
			    place $T.entry -y [expr {$y1 - 1}]
			}
			$T.entry selection clear
			scan [$T item bbox $I C0 elemImg] "%d %d %d %d" x1 y1 x2 y2
			set left $x1
			set right $x2
			place $T.entry -x $left -width [expr {$right - $left}]
			$T.entry icursor [$T.entry index @[expr {$x - ($x1 + 1)}]]
			# Disable mouse tracking
			unset ::TreeCtrl::Priv(buttonMode)
		    }
		}
	    }
	}
    }
    return -code break
}

#
# Demo: iMovie (Wrap)
#
proc DemoIMovieWrap {} {

    DemoIMovie

    set T [DemoList]
    $T configure -wrap "" -xscrollsmoothing yes
    $T item configure "root child 4" -wrap yes
    $T item configure "root child 5" -wrap yes
    $T item configure "root child 8" -wrap yes
    $T item configure "root child 10" -wrap yes
    $T item configure "root child 15" -wrap yes
#    $T item configure "root child 15" -wrap yes
    $T item configure "root child 25" -wrap yes
    return
}
