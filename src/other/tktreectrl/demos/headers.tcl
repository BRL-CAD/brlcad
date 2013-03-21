# Copyright (c) 2011 Tim Baker

namespace eval DemoHeaders {}

proc DemoHeaders::Init {T} {

    $T configure \
	-showroot no -xscrollsmoothing yes -yscrollsmoothing yes \
	-selectmode multiple -xscrollincrement 20 -canvaspadx 0

    #
    # Create one locked column on each side plus 8 non-locked columns
    #

    set itembg {linen {} #e0e8f0 {}}

    $T column create -text "Left" -tags Cleft -width 80 -justify center \
	-gridrightcolor gray90 -itembackground $itembg \
	-lock left -arrow none -arrowside left \
	-visible no

    for {set i 1} {$i <= 8} {incr i} {
	$T column create -text "C$i" -tags C$i -width 80 -justify center \
	    -gridrightcolor gray90 -itembackground $itembg
    }

    $T column create -text "Right" -tags Cright -width 80 -justify center \
	-gridrightcolor gray90 -itembackground $itembg  \
	-lock right -visible no

    #
    # Create an image element to use as the sort arrow for some header
    # styles.
    #

    InitSortImages blue 5
    $T element create header.sort image -statedomain header \
	-image {::DemoHeaders::arrow-down down ::DemoHeaders::arrow-up up}

    #
    # Create a style for our custom headers,
    # a raised border with centered text.
    #

    $T element create header.border border -statedomain header \
	-background $::SystemButtonFace \
	-relief {sunken pressed raised {}} -thickness 2 -filled yes
    $T element create header.text text -statedomain header \
	-lines 1 -fill black

    set S [$T style create header1 -orient horizontal -statedomain header]
    $T style elements $S {header.border header.text header.sort}
    $T style layout $S header.border -detach yes -indent no -iexpand xy
    $T style layout $S header.text -center xy -padx 6 -pady 2 -squeeze x
    $T style layout $S header.sort -expand nws -padx {0 6} \
	-visible {no {!down !up}}

    #
    # Create a style for our custom headers,
    # a light-blue rounded rectangle with centered text.
    #

    set radius 9
    if {[Platform unix]} {
	set radius 7
    }
    $T element create header.rrect rect -statedomain header \
	-rx $radius -fill {
	    #cee8f0 active
	    #87c6da pressed
	    #87c6da up
	    #87c6da down
	    {light blue} {}
	}

    set S [$T style create header2 -orient horizontal -statedomain header]
    $T style elements $S {header.rrect header.text header.sort}
    $T style layout $S header.rrect -detach yes -iexpand xy -padx {1 0} -pady 1
    $T style layout $S header.text -center xy -padx 6 -pady 4 -squeeze x
    $T style layout $S header.sort -expand nws -padx {0 6} \
	-visible {no {!down !up}}

    #
    # Create a style for our custom headers,
    # Window 7 Explorer type headers.
    #

    $T gradient create G.header3.fill.active -orient vertical \
	-stops {{0.0 #f3f8fd} {1.0 #eff3f9}} -steps 8
    $T gradient create G.header3.fill.pressed -orient vertical \
	-stops {{0.0 #c1ccda} {0.2 white} {1.0 white}} -steps 8
    $T gradient create G.header3.outline.normal -orient vertical \
	-stops {{0.0 #e3e8ee} {1.0 white}} -steps 8

    $T element create header.outline3 rect -statedomain header \
	-outline {#e3e8ee active #c0cbd9 pressed G.header3.outline.normal normal} \
	-outlinewidth 1 -open {w normal}
    $T element create header.rect3 rect -statedomain header \
	-fill G.header3.fill.active
    $T element create header.rect3.pressed rect -statedomain header \
	-fill G.header3.fill.pressed

    set S [$T style create header3 -orient horizontal -statedomain header]
    $T style elements $S {header.rect3.pressed header.outline3 header.rect3 header.text header.sort}
    $T style layout $S header.outline3 -detach yes -iexpand xy
    $T style layout $S header.rect3 -detach yes -iexpand xy \
	-padx 2 -pady 2 -visible {no !active}
    $T style layout $S header.rect3.pressed -detach yes -iexpand xy \
	-visible {no !pressed}
    $T style layout $S header.text -center xy -padx 6 -pady {6 3} -squeeze x
    $T style layout $S header.sort -detach yes -expand we -pady 2

    #
    # Create a style for our custom headers,
    # a header element with a checkbox image and centered text.
    #

    InitPics *checked

    $T header state define CHECK
    $T element create header.header header -statedomain header
    $T element create header.check image -statedomain header \
	-image {checked CHECK unchecked {}}
    set S [$T style create header4 -statedomain header]
    $T style elements $S {header.header header.check header.text}
    $T style layout $S header.header -union {header.check header.text} -iexpand news
    $T style layout $S header.check -expand nes -padx {6 0}
    $T style layout $S header.text -center xy -padx 6 -squeeze x

    #
    # Create a style for our custom headers,
    # Mac OS X type headers.
    #

    $T gradient create Gnormal -orient vertical -stops {{0.0 white} {0.5 gray87} {1.0 white}} -steps 6
    $T gradient create Gactive -orient vertical -stops {{0.0 white} {0.5 gray90} {1.0 white}} -steps 6
    $T gradient create Gpressed -orient vertical -stops {{0.0 white} {0.5 gray82} {1.0 white}} -steps 6
    $T gradient create Gsorted -orient vertical -stops {{0.0 white} {0.5 {sky blue}} {1.0 white}} -steps 6
    $T gradient create Gactive_sorted -orient vertical -stops {{0.0 white} {0.5 {light blue}} {1.0 white}} -steps 6
    $T gradient create Gpressed_sorted -orient vertical -stops {{0.0 white} {0.5 {sky blue}} {1.0 white}} -steps 6
    $T element create header.rect5 rect -statedomain header \
    -fill {
	Gactive_sorted {active up}
	Gpressed_sorted {pressed up}
	Gactive_sorted {active down}
	Gpressed_sorted {pressed down}
	Gsorted up
	Gsorted down
	Gactive active
	Gpressed pressed
	Gnormal {}
    } -outline {
	{sky blue} up
	{sky blue} down
	gray {}
    } -outlinewidth 1 -open {
	nw !pressed
    }

    set S [$T style create header5 -orient horizontal -statedomain header]
    $T style elements $S {header.rect5 header.text header.sort}
    $T style layout $S header.rect5 -detach yes -iexpand xy
    $T style layout $S header.text -center xy -padx 6 -pady 2 -squeeze x
    $T style layout $S header.sort -expand nws -padx {0 6} \
	-visible {no {!down !up}}

    #
    # Create a style for our custom headers,
    # a gradient-filled rectangle with centered text.
    #

    $T gradient create G_orange1 -orient vertical -steps 4 \
	-stops {{0 #fde8d1} {0.3 #fde8d1} {0.3 #ffce69} {0.6 #ffce69} {1 #fff3c3}}
    $T gradient create G_orange2 -orient vertical -steps 4 \
	-stops {{0 #fffef6} {0.3 #fffef6} {0.3 #ffef9a} {0.6 #ffef9a} {1 #fffce8}}

    $T element create orange.outline rect -statedomain header \
	-outline #ffb700 -outlinewidth 1 \
	-rx 1 -open {
	    nw !pressed
	}
    $T element create orange.box rect -statedomain header \
	-fill {
	    G_orange1 active
	    G_orange1 up
	    G_orange1 down
	    G_orange2 {}
	}

    set S [$T style create header6 -orient horizontal -statedomain header]
    $T style elements $S {orange.outline orange.box header.text header.sort}
    $T style layout $S orange.outline -union orange.box -ipadx 2 -ipady 2
    $T style layout $S orange.box -detach yes -iexpand xy
    $T style layout $S header.text -center xy -padx 6 -pady 4 -squeeze x
    $T style layout $S header.sort -expand nws -padx {0 6} \
	-visible {no {!down !up}}

    #
    # Configure 3 rows of column headers
    #

    set S header2

    $T header configure first -tags header1
    set H header1
    $T header configure $H all -arrowgravity right -justify center
    $T header style set $H all $S
    $T header span $H all 4
    foreach {C text} [list Cleft Left C1 A C5 H Cright Right] {
	$T header configure $H $C -text $text
	$T header text $H $C $text
    }

    set H [$T header create -tags header2]
    $T header configure $H all -arrowgravity right -justify center
    $T header style set $H all $S
    $T header span $H all 2
    foreach {C text} [list Cleft Left C1 B C3 C C5 I C7 J Cright Right] {
	$T header configure $H $C -text $text
	$T header text $H $C $text
    }

    set H [$T header create -tags header3]
    $T header configure $H all -arrowgravity right -justify center
    $T header style set $H all $S
    foreach {C text} [list Cleft Left C1 D C2 E C3 F C4 G C5 K C6 L C7 M C8 N Cright Right] {
	$T header configure $H $C -text $text
	$T header text $H $C $text
    }

    #
    # Create a 4th row of column headers to test embedded windows.
    #

    $T element create header.window window -statedomain header -clip yes
    $T element create header.divider rect -statedomain header -fill gray -height 2

    set S [$T style create headerWin -orient horizontal -statedomain header]
    $T style elements $S {header.divider header.window}
    $T style layout $S header.divider -detach yes -expand n -iexpand x
    $T style layout $S header.window -iexpand x -squeeze x -padx 1 -pady {0 2}

    set H [$T header create -tags header4]
    $T header dragconfigure $H -enable no
    $T header style set $H all $S
    foreach C [$T column list] {
        set f [frame $T.frame${H}_$C -borderwidth 0]
	set w [entry $f.entry -highlightthickness 1]
	$w insert end $C
	$T header element configure $H $C header.window -window $f
    }

    #
    #
    #

    $T item state define current

    $T element create theme.rect rect \
	-fill {{light blue} current white {}} \
	-outline gray50 -outlinewidth 2 -open s
    $T element create theme.text text \
	-lines 0
    $T element create theme.button window -clip yes
    set S [$T style create theme -orient vertical]
    $T style elements $S {theme.rect theme.text theme.button}
    $T style layout $S theme.rect -detach yes -iexpand xy
    $T style layout $S theme.text -padx 6 -pady 3 -squeeze x
    $T style layout $S theme.button -expand we -pady {3 6}

    NewButtonItem "" \
	"Use no style, just the built-in header background, sort arrow and text."
    NewButtonItem header1 \
	"Use the 'header1' style, consisting of a border element for the background and an image for the sort arrow." \
	black black
    NewButtonItem header2 \
	"Use the 'header2' style, consisting of a rounded rectangle element for the background and an image for the sort arrow." \
	black blue
    NewButtonItem header3 \
	"Use the 'header3' style, consisting of a gradient-filled rectangle element for the background and an image for the sort arrow." \
	#6d6d6d win7
    NewButtonItem header4 \
	"Use the 'header4' style, consisting of a header element to display the background and sort arrow, and an image element serving as a checkbutton."
    NewButtonItem header5 \
	"Use the 'header5' style, consisting of a gradient-filled rectangle element for the background and an image for the sort arrow." \
	black #0080FF
    NewButtonItem header6 \
	"Use the 'header6' style, consisting of a gradient-filled rectangle element for the background and an image for the sort arrow." \
	red orange

    $T item state set styleheader2 current

    #
    # Create 100 regular non-locked items
    #

    $T element create item.sel rect \
	-fill {gray {selected !focus} blue selected}
    $T element create item.text text \
	-text "Item" -fill {white selected}

    set S [$T style create item]
    $T style elements $S {item.sel item.text}
    $T style layout $S item.sel -detach yes -iexpand xy
    $T style layout $S item.text -expand news -padx 2 -pady 2

    $T column configure !tail -itemstyle $S
    $T item create -count 100 -parent root

    # Remember which column header is displaying the sort arrow, and
    # initialize the sort order in each column.
    variable Sort
    set Sort(header) ""
    set Sort(column) ""
    foreach C [$T column list] {
	set Sort(direction,$C) down
    }

    # The <Header-state> event is generated in response to Motion and
    # Button events in headers.
    $T notify install <Header-state>
    $T notify bind $T <Header-state> {
	DemoHeaders::HeaderState %H %C %s
    }

    # The <Header-invoke> event is generated when the left mouse button is
    # pressed and released over a column header.
    $T notify bind $T <Header-invoke> {
	DemoHeaders::HeaderInvoke %H %C
    }

    $T notify bind $T <ColumnDrag-begin> {
	DemoHeaders::ColumnDragBegin %H %C
    }

    # Disable the demo.tcl binding on <ColumnDrag-receive> and install our
    # own to deal with multiple rows of column headers.
    $T notify configure DontDelete <ColumnDrag-receive> -active no
    $T notify bind $T <ColumnDrag-receive> {
	DemoHeaders::ColumnDragReceive %H %C %b
    }

    bindtags $T [list $T DemoHeaders TreeCtrl [winfo toplevel $T] all]
    bind DemoHeaders <ButtonPress-1> {
	DemoHeaders::ButtonPress1 %x %y
    }

    return
}

# This procedure creates a new item with descriptive text and a pushbutton
# to change the style used in the column headers.
proc DemoHeaders::NewButtonItem {S text args} {
    set T [DemoList]
    set I [$T item create -parent root -tags [list style$S config]]
    $T item style set $I C1 theme
    $T item span $I all [$T column count {lock none}]
    $T item text $I C1 $text
    frame $T.frame$I -borderwidth 0
    $::buttonCmd $T.frame$I.button -text "Configure headers" \
	-command [eval list [list DemoHeaders::ChangeHeaderStyle $S] $args]
    $T item element configure $I C1 theme.button -window $T.frame$I
    return
}

proc DemoHeaders::InitSortImages {color height} {
    set img ::DemoHeaders::arrow-down
    image create photo $img
    if {$color eq "win7"} {
	$img put {{#3c5e72 #629ab9 #70a9c6 #72abca #83bad9 #95c6e0 #9ac7e0}} -to 0 0
	$img put {{        #528bab #73b2d8 #99d0ee #b3dbf1 #c4e3f4        }} -to 1 1
	$img put {{                #67acd3 #a6d8f3 #c4e3f4                }} -to 2 2
	$img put {{                        #9acbe6                        }} -to 3 3
    } else {
	for {set i 0} {$i < $height} {incr i} {
	    set width [expr {2 * $height - ($i * 2 + 1)}]
	    $img put [list [string repeat "$color " $width]] -to $i $i
	}
    }

    set img ::DemoHeaders::arrow-up
    image create photo $img
    if {$color eq "win7"} {
	$img put {{                        #406274                        }} -to 3 0
	$img put {{                #3c5e72 #569bc0 #5e88a1                }} -to 2 1
	$img put {{        #3c596c #6196b6 #86c8eb #91cbec #9ab6c5        }} -to 1 2
	$img put {{#435f6f #87b1c5 #bae2f4 #b5ddf2 #c4e3f4 #cae6f5 #c3e4f5}} -to 0 3
    } else {
	for {set i 0} {$i < $height} {incr i} {
	    set width [expr {($i * 2 + 1)}]
	    $img put [list [string repeat "$color " $width]] -to [expr {$height - $i - 1}] $i
	}
    }

    return
}

proc DemoHeaders::ChangeHeaderStyle {style {textColor ""} {sortColor ""} {imgHeight 5}} {
    variable HeaderStyle
    variable Sort
    set T [DemoList]
    # To override the system theme color on Gtk+, set the header element color
    # and not the widget's -headerforeground color.
    $T element configure header.text -fill $textColor
    if {$sortColor ne ""} {
	InitSortImages $sortColor $imgHeight
    }
    set HeaderStyle $style
    set S $HeaderStyle
    foreach H [$T header id !header4] {
	$T header style set $H all $S
	if {$S ne ""} {
	    $T header configure all all -textpadx 6
	    foreach C [$T column list] {
		$T header text $H $C [$T header cget $H $C -text]
	    }
	}
    }
    if {$Sort(header) ne ""} {
	ShowSortArrow $Sort(header) $Sort(column)
    }
    $T item state set {state current} !current
    $T item state set style$style current
    return
}

# This procedure is called to handle the <Header-state> event generated by
# the treectrl.tcl library script.
proc DemoHeaders::HeaderState {H C state} {
    return
}

# This procedure is called to handle the <Header-invoke> event generated by
# the treectrl.tcl library script.
# If the given column header is already displaying a sort arrow, the sort
# arrow direction is toggled.  Otherwise the sort arrow is removed from all
# other column headers and displayed in the given column header.
proc DemoHeaders::HeaderInvoke {H C} {
    variable Sort
    set T [DemoList]
#    if {![$T item tag expr $I header3]} return
    if {$Sort(header) eq ""} {
	ShowSortArrow $H $C
    } else {
	if {[$T header compare $H == $Sort(header)] &&
		[$T column compare $C == $Sort(column)]} {
	    ToggleSortArrow $H $C
	} else {
	    HideSortArrow $Sort(header) $Sort(column)
	    ShowSortArrow $H $C
	}
    }
    set Sort(header) $H
    set Sort(column) $C
    return
}

# Sets the -arrow option of a column header to 'up' or 'down'.
proc DemoHeaders::ShowSortArrow {H C} {
    variable Sort
    set T [DemoList]
    $T header configure $H $C -arrow $Sort(direction,$C)
    return
}

# Sets the -arrow option of a column header to 'none'.
proc DemoHeaders::HideSortArrow {H C} {
    set T [DemoList]
    $T header configure $H $C -arrow none
    return
}

# Toggles a sort arrow between up and down
proc DemoHeaders::ToggleSortArrow {H C} {
    variable Sort
    if {$Sort(direction,$C) eq "up"} {
	set Sort(direction,$C) down
    } else {
	set Sort(direction,$C) up
    }
    ShowSortArrow $H $C
    return
}

# This procedure is called to handle the <ColumnDrag-begin> event generated
# by the treectrl.tcl library script.
# When dragging in the top row, all header-rows provide feedback.
# When dragging in the second row, the 2nd, 3rd and 4th rows provide feedback.
# When dragging in the third row, only the 3rd and 4rd row provides feedback.
proc DemoHeaders::ColumnDragBegin {H C} {
    set T [DemoList]
    $T header dragconfigure all -draw yes
    if {[$T header compare $H > header1]} {
	$T header dragconfigure header1 -draw no
    }
    if {[$T header compare $H > header2]} {
	$T header dragconfigure header2 -draw no
    }
    return
}

# This procedure is called to handle the <ColumnDrag-receive> event generated
# by the treectrl.tcl library script.
proc DemoHeaders::ColumnDragReceive {H C b} {
    set T [DemoList]

    # Get the range of columns in the span of the dragged header.
    set span [$T header span $H $C]
    set last [$T column id "$C span $span"]
    set columns [$T column id "range $C $last"]

    set span1 [$T header span header1]
    set span2 [$T header span header2]
    set text1 [$T header text header1]
    set text2 [$T header text header2]

    set columnLeft [$T column id "first lock none"]

    foreach C $columns {
	$T column move $C $b
    }

    # Restore the appearance of the top row if dragging happened below
    if {[$T header compare $H > header1]} {
	foreach span $span1 text $text1 C [$T column list] {
	    $T header span header1 $C $span
	    $T header text header1 $C $text
	    $T header configure header1 $C -text $text
	}
    }

    # Restore the appearance of the second row if dragging happened below
    if {[$T header compare $H > header2]} {
	foreach span $span2 text $text2 C [$T column list] {
	    $T header span header2 $C $span
	    $T header text header2 $C $text
	    $T header configure header2 $C -text $text
	}
    }

    # For each of the items displaying a button widget to change the header
    # style, transfer the style from the old left-most column to the new
    # left-most column.
    if {[$T column compare $columnLeft != "first lock none"]} {
	foreach I [$T item id "tag config"] {
	    TransferItemStyle $T $I $columnLeft "first lock none"
	}
    }

    return
}

# Copy the style and element configuration from one column of an item to
# another.
proc DemoHeaders::TransferItemStyle {T I Cfrom Cto} {
    set S [$T item style set $I $Cfrom]
    $T item style set $I $Cto $S
    foreach E [$T item style elements $I $Cfrom] {
	foreach info [$T item element configure $I $Cfrom $E] {
	    lassign $info option x y z value
	    $T item element configure $I $Cto $E $option $value
	}
    }
    $T item style set $I $Cfrom ""
    return
}

# This procedure is called to handle the <ButtonPress1> event.
# If the click was in the checkbutton image element, toggle the CHECK state.
proc DemoHeaders::ButtonPress1 {x y} {
    set T [DemoList]
    $T identify -array id $x $y
    if {$id(where) eq "header" && $id(element) eq "header.check"} {
	$T header state forcolumn $id(header) $id(column) ~CHECK
	return -code break
    }
    return
}
