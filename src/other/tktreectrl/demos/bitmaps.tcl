# RCS: @(#) $Id$

#
# Demo: Bitmaps
#
proc DemoBitmaps {} {

    set T [DemoList]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode browse -orient horizontal -wrap "5 items" \
	-showheader no -backgroundimage sky

    $T configure -canvaspadx 6 -canvaspady 6 -itemgapx 4 -itemgapy 4

    #
    # Create columns
    #

    $T column create -itembackground {gray90 {}} -tags C0

    #
    # Create elements
    #

    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}]
    $T element create elemSelTxt rect -fill [list $::SystemHighlight {selected focus}] \
	-showfocus yes
    $T element create elemSelBmp rect -outline [list $::SystemHighlight {selected focus}] \
	-outlinewidth 4
    $T element create elemBmp bitmap \
	-foreground [list $::SystemHighlight {selected focus}] \
	-background linen \
	-bitmap {question {selected}}

    #
    # Create styles using the elements
    #

    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemSelBmp elemBmp elemSelTxt elemTxt}
    $T style layout $S elemSelBmp -union elemBmp \
	-ipadx 6 -ipady 6
    $T style layout $S elemBmp -pady {0 6} -expand we
    $T style layout $S elemSelTxt -union elemTxt -ipadx 2
    $T style layout $S elemTxt -expand we

    # Set default item style
    $T column configure C0 -itemstyle $S

    #
    # Create items and assign styles
    #

    set bitmapNames [list error gray75 gray50 gray25 gray12 hourglass info \
	questhead question warning]

    foreach name $bitmapNames {
	set I [$T item create]
#		$T item style set $I 0 $S
	$T item text $I C0 $name
	$T item element configure $I C0 elemBmp -bitmap $name
	$T item lastchild root $I
    }

    foreach name $bitmapNames {
	set I [$T item create]
	$T item style set $I C0 $S
	$T item text $I C0 $name
if 1 {
	$T item element configure $I C0 elemBmp -bitmap $name \
	    -foreground [list brown {}] \
	    -background {"" {}}
} else {
	$T item element configure $I C0 elemBmp -bitmap $name \
	    -foreground [list $::SystemHighlight {selected focus} brown {}] \
	    -background {"" {}}
}
	$T item lastchild root $I
    }

    return
}

