# RCS: @(#) $Id$

set Dir [file dirname [file dirname [info script]]]

set shellicon 0
# Might work on other windows versions, but only tested on XP and Win7
if {$tcl_platform(os) eq "Windows NT" &&
    ($tcl_platform(osVersion) == 5.1 ||
    $tcl_platform(osVersion) == 6.1)} {
    catch {
	lappend auto_path $treectrl_library
	package require shellicon $VERSION
	set shellicon 1
    }
}

set macBitmap 0
if {[info commands ::tk::mac::iconBitmap] ne {}} {
    set macBitmap 1
}

proc DemoExplorerAux {scriptDir scriptFile {scriptFollowup ""}} {
    global Explorer
    global Dir

    set T [DemoList]

    TimerStart
    set globDirs [glob -nocomplain -types d -dir $Dir *]
    set secondsGlobDirs [TimerStop]

    TimerStart
    set list [lsort -dictionary $globDirs]
    set secondsSortDirs [TimerStop]

    if {[file dirname $Dir] ne $Dir} {
	lappend globDirs ".."
	set list [concat ".." $list]
    }

    TimerStart
    foreach file $list $scriptDir
    set secondsAddDirs [TimerStop]

    $T item tag add "root children" directory

    TimerStart
    set globFiles [glob -nocomplain -types f -dir $Dir *]
    set secondsGlobFiles [TimerStop]

    TimerStart
    set list [lsort -dictionary $globFiles]
    set secondsSortFiles [TimerStop]

    TimerStart
    foreach file $list $scriptFile
    set secondsAddFiles [TimerStop]

    set gd $secondsGlobDirs
    set sd $secondsSortDirs
    set ad $secondsAddDirs
    set gf $secondsGlobFiles
    set sf $secondsSortFiles
    set af $secondsAddFiles
    puts "dirs([llength $globDirs]) glob/sort/add $gd/$sd/$ad\nfiles([llength $globFiles]) glob/sort/add $gf/$sf/$af"

    set ::TreeCtrl::Priv(DirCnt,$T) [llength $globDirs]

    eval $scriptFollowup

    # Double-clicking a directory displays its contents.
    set Explorer(scriptDir) $scriptDir
    set Explorer(scriptFile) $scriptFile
    set Explorer(scriptFollowup) $scriptFollowup

    return
}

proc DemoExplorerBindings {{win7 0}} {

    set T [DemoList]

    # Double-clicking a directory displays its contents.
    bind DemoExplorer <Double-ButtonPress-1> {
	ExplorerDoubleButton1 %W %x %y
    }

    TreeCtrl::FileListEmulateWin7 $T $win7

    if {$win7} {
	set ::Explorer(prev) ""
	$T notify bind $T <ItemDelete> {
	    if {[lsearch -exact %i $Explorer(prev)] != -1} {
		set Explorer(prev) ""
	    }
	}
	bind DemoExplorerWin7 <Motion> {
	    ExplorerMotion %W %x %y
	}
	bind DemoExplorerWin7 <Leave> {
	    ExplorerMotion %W -1 -1
	}
    }
}

#
# Demo: explorer files
#
proc DemoExplorerDetails {} {

    global Explorer

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode extended -xscrollincrement 20 -xscrollsmoothing yes \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    InitPics small-*

    #
    # Create columns
    #

    $T column create -text Name -tags {C0 name} -width 200 \
	-arrow up -itembackground #F7F7F7
    $T column create -text Size -tags size -justify right -width 60 \
	-arrowside left -arrowgravity right
    $T column create -text Type -tags type -width 120
    $T column create -text Modified -tags modified -width 120

    # Demonstration of per-state column options and configure "all"
    $T column configure all -background {gray90 active gray70 normal gray50 pressed}

    #
    # Create elements
    #
    if {$::shellicon} {
	$T element create elemImg shellicon -size small
    } elseif {$::macBitmap} {
        $T element create elemImg bitmap
    } else {
	$T element create elemImg image -image {small-folderSel {selected} small-folder {}}
    }
    $T element create txtName text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create txtType text -lines 1
    $T element create txtSize text -datatype integer -format "%dKB" -lines 1
    $T element create txtDate text -datatype time -format "%d/%m/%y %I:%M %p" -lines 1
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    # column 0: image + text
    set S [$T style create styName -orient horizontal]
    $T style elements $S {elemRectSel elemImg txtName}
    $T style layout $S elemImg -padx {2 0} -expand ns
    $T style layout $S txtName -squeeze x -expand ns
    $T style layout $S elemRectSel -union [list txtName] -ipadx 2 -iexpand ns

    # column 1: text
    set S [$T style create stySize]
    $T style elements $S txtSize
    $T style layout $S txtSize -padx 6 -squeeze x -expand ns

    # column 2: text
    set S [$T style create styType]
    $T style elements $S txtType
    $T style layout $S txtType -padx 6 -squeeze x -expand ns

    # column 3: text
    set S [$T style create styDate]
    $T style elements $S txtDate
    $T style layout $S txtDate -padx 6 -squeeze x -expand ns

    # List of lists: {column style element ...} specifying text elements
    # the user can edit.
    TreeCtrl::SetEditable $T {
	{name styName txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on.
    TreeCtrl::SetSensitive $T {
	{name styName elemImg txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can select with the selection rectangle. Empty means
    # use the same list passed to TreeCtrl::SetSensitive.
    TreeCtrl::SetSensitiveMarquee $T {}

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items
    TreeCtrl::SetDragImage $T {
	{name styName elemImg txtName}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout styName txtName -draw {no edit}
    $T style layout styName elemRectSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item name styName type styType modified styDate
	$T item element configure $item \
	    name txtName -text [file tail $file] , \
	    type txtType -text "Folder" , \
	    modified txtDate -data [file mtime $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item \
		name elemImg -path $file
	}
	if {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item \
		name elemImg -bitmap [list $file]
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item name styName size stySize type styType modified styDate
	switch [file extension $file] {
	    .dll { set img small-dll }
	    .exe { set img small-exe }
	    .txt { set img small-txt }
	    default { set img small-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item \
		name elemImg -path $file + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	} elseif {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item \
		name elemImg -bitmap [list $file] + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	} else {
	    $T item element configure $item \
		name elemImg -image [list ${img}Sel {selected} $img {}] + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile
    DemoExplorerBindings

    set ::Explorer(sortColumn) name
    set ::Explorer(sortColor) #F7F7F7
    $T notify bind $T <Header-invoke> { ExplorerHeaderInvoke %T %C }

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

proc ExplorerHeaderInvoke {T C} {
    global Explorer
    if {[$T column compare $C == $Explorer(sortColumn)]} {
	if {[$T column cget $Explorer(sortColumn) -arrow] eq "down"} {
	    set order -increasing
	    set arrow up
	} else {
	    set order -decreasing
	    set arrow down
	}
    } else {
	if {[$T column cget $Explorer(sortColumn) -arrow] eq "down"} {
	    set order -decreasing
	    set arrow down
	} else {
	    set order -increasing
	    set arrow up
	}
	$T column configure $Explorer(sortColumn) -arrow none -itembackground {}
	set Explorer(sortColumn) $C
    }
    $T column configure $C -arrow $arrow -itembackground $Explorer(sortColor)
    set dirCount $::TreeCtrl::Priv(DirCnt,$T)
    set fileCount [expr {[$T item count] - 1 - $dirCount}]
    set lastDir [expr {$dirCount - 1}]
    switch -glob [$T column cget $C -tags] {
	*name* {
	    if {$dirCount} {
		$T item sort root $order -last "root child $lastDir" -column $C -dictionary
	    }
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -dictionary
	    }
	}
	size {
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
	    }
	}
	type {
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -dictionary -column name -dictionary
	    }
	}
	modified {
	    if {$dirCount} {
		$T item sort root $order -last "root child $lastDir" -column $C -integer -column name -dictionary
	    }
	    if {$fileCount} {
		$T item sort root $order -first "root child $dirCount" -column $C -integer -column name -dictionary
	    }
	}
    }
    return
}

proc DemoExplorerLargeIcons {} {

    set T [DemoList]

    # Item height is 32 for icon, 4 padding, 3 lines of text
    set itemHeight [expr {32 + 4 + [font metrics [$T cget -font] -linespace] * 3}]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode extended -wrap window -orient horizontal \
	-itemheight $itemHeight -itemwidth 75 -showheader no \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    InitPics big-*

    #
    # Create columns
    #

    $T column create -tags C0

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size large
    } elseif {$::macBitmap} {
        $T element create elemImg bitmap
    } else {
	$T element create elemImg image -image {big-folderSel {selected} big-folder {}}
    }
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
	-justify center -lines 1 -width 71 -wrap word
    $T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected}] -showfocus yes

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemSel elemImg elemTxt}
    $T style layout $S elemImg -expand we
    $T style layout $S elemTxt -pady {4 0} -squeeze x -expand we
    $T style layout $S elemSel -union [list elemTxt] -ipadx 2

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
	{C0 STYLE elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on.
    TreeCtrl::SetSensitive $T {
	{C0 STYLE elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can select with the selection rectangle. Empty means
    # use the same list passed to TreeCtrl::SetSensitive.
    TreeCtrl::SetSensitiveMarquee $T {}

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items.
    TreeCtrl::SetDragImage $T {
	{C0 STYLE elemImg elemTxt}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout STYLE elemTxt -draw {no edit}
    $T style layout STYLE elemSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	$T item text $item C0 [file tail $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item C0 \
		elemImg -path $file
	}
	if {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 32 32 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file]
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	switch [file extension $file] {
	    .dll { set img big-dll }
	    .exe { set img big-exe }
	    .txt { set img big-txt }
	    default { set img big-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item C0 \
		elemImg -path $file + \
		elemTxt -text [file tail $file]
	} elseif {$::macBitmap} {
	    ::tk::mac::iconBitmap $file 32 32 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file] + \
		elemTxt -text [file tail $file]
	} else {
	    $T item element configure $item C0 \
		elemImg -image [list ${img}Sel {selected} $img {}] + \
		elemTxt -text [file tail $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile
    DemoExplorerBindings

    $T activate [$T item id "root firstchild"]

    $T notify bind $T <ActiveItem> {
	if {[%T item compare %p != root]} {
	    %T item element configure %p C0 elemTxt -lines {}
	}
	if {[%T item compare %c != root]} {
	    %T item element configure %c C0 elemTxt -lines 3
	}
    }
    $T item element configure active C0 elemTxt -lines 3

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

# Tree is horizontal, wrapping occurs at right edge of window, each item
# is as wide as the smallest needed multiple of 110 pixels
proc DemoExplorerSmallIcons {} {
    set T [DemoList]
    DemoExplorerList
    $T configure -orient horizontal \
	-itemwidthmultiple 110 -itemwidthequal no
    return
}

# Tree is vertical, wrapping occurs at bottom of window, each range has the
# same width (as wide as the longest item), xscrollincrement is by range
proc DemoExplorerList {} {

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode extended -wrap window -showheader no \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50" \
	-itemwidthequal yes

    InitPics small-*

    #
    # Create columns
    #

    $T column create -tags C0

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size small
    } elseif {$::macBitmap} {
        $T element create elemImg bitmap
    } else {
	$T element create elemImg image -image {small-folderSel {selected} small-folder {}}
    }
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}] \
	-lines 1
    $T element create elemSel rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create STYLE]
    $T style elements $S {elemSel elemImg elemTxt}
    $T style layout $S elemImg -expand ns
    $T style layout $S elemTxt -squeeze x -expand ns -padx {2 0}
    $T style layout $S elemSel -union [list elemTxt] -iexpand ns -ipadx 2

    # List of lists: {column style element ...} specifying text elements
    # the user can edit
    TreeCtrl::SetEditable $T {
	{C0 STYLE elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on.
    TreeCtrl::SetSensitive $T {
	{C0 STYLE elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can select with the selection rectangle. Empty means
    # use the same list passed to TreeCtrl::SetSensitive.
    TreeCtrl::SetSensitiveMarquee $T {}

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items.
    TreeCtrl::SetDragImage $T {
	{C0 STYLE elemImg elemTxt}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout STYLE elemTxt -draw {no edit}
    $T style layout STYLE elemSel -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	$T item text $item C0 [file tail $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item C0 \
		elemImg -path $file
	}
	if {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file]
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	switch [file extension $file] {
	    .dll { set img small-dll }
	    .exe { set img small-exe }
	    .txt { set img small-txt }
	    default { set img small-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item C0 \
		elemImg -path $file + \
		elemTxt -text [file tail $file]
	} elseif {$::macBitmap} {
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file] + \
		elemTxt -text [file tail $file]
	} else {
	    $T item element configure $item C0 \
		elemImg -image [list ${img}Sel {selected} $img {}] + \
		elemTxt -text [file tail $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile
    DemoExplorerBindings

    $T activate [$T item firstchild root]

    bindtags $T [list $T DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

proc ExplorerDoubleButton1 {T x y} {
    global Explorer
    global Dir
    set id [$T identify $x $y]
    set sensitive [TreeCtrl::IsSensitive $T $x $y]
    if {[TreeCtrl::FileListEmulateWin7 $T] && [TreeCtrl::IsSensitiveMarquee $T $x $y]} {
	set sensitive 1
    }
    if {$sensitive} {
	set item [lindex $id 1]
	set column [lindex $id 3]
	if {[$T item tag expr $item directory]} {
	    set name [$T item text $item {tag C0}]
	    if {$name eq ".."} {
		set Dir [file dirname $Dir]
	    } else {
		set Dir [file join $Dir $name]
	    }
	    $T item delete all
	    DemoExplorerAux $Explorer(scriptDir) $Explorer(scriptFile) $Explorer(scriptFollowup)
	    if {![TreeCtrl::FileListEmulateWin7 $T]} {
		$T activate "root firstchild"
	    }
	    $T xview moveto 0.0
	    $T yview moveto 0.0
	}
    }
    return
}

# Experimental code to create a style that is used with drag-and-drop
proc DragStyleInit {} {

    set T [DemoList]

    set boxW 100
    set boxH 100
    set imgW 32
    set imgH 32

    $T element create DragStyleElemRect rect -fill #D0ffff -width $boxW -height $boxH
    $T element create DragStyleElemImg image -image big-file
    $T element create DragStyleElemTxt text -text DragImage!
    $T element create DragStyleElemTxtBg rect -fill white -outline black -outlinewidth 1

    $T style create DragStyle -orient vertical
    $T style elements DragStyle {DragStyleElemRect DragStyleElemImg DragStyleElemTxtBg DragStyleElemTxt}

    set cursorW 16
    $T style layout DragStyle DragStyleElemRect -detach yes

    set dx [expr {($boxW - $imgW) / 2}]
    set dy [expr {($boxH - $imgH) / 2}]
    $T style layout DragStyle DragStyleElemImg -detach yes -padx "$dx 0" -pady "$dy 0"

    set dx [expr {$boxW / 2 + $cursorW}]
    set dy $boxH
    $T style layout DragStyle DragStyleElemTxt -detach yes -padx "$dx 0" -pady "$dy 0"

    $T style layout DragStyle DragStyleElemTxtBg -union DragStyleElemTxt -ipadx 3 -ipady 2

    $T dragimage configure -style DragStyle

    set x [expr {$boxW / 2 - 0 * $cursorW / 2}]
    set y [expr {$boxH - $cursorW * 2/3}]
    $T dragimage stylehotspot $x $y

    return
}

# Configure the marquee for a modern transparent selection rectangle
# where transparent gradients are supported.
proc ConfigTransparentMarquee {T} {
    if {!$::NativeGradients} return
    if {![$T gradient native]} return
    if {[winfo depth $T] < 15} return

    set outline #3399ff
    set stops [list [list 0.0 #3399ff 0.3] [list 1.0 #3399ff 0.3]]

    $T gradient create G_marquee -stops $stops
    $T marquee configure -fill G_marquee -outline $outline
    return
}

#
# Demo: explorer files with Windows-7-like gradients
#
proc DemoExplorerDetailsWin7 {} {

    global Explorer

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 16} {
	set height 16 ; # small icon height
    }
    incr height 5

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode extended -xscrollincrement 20 -xscrollsmoothing yes \
	-scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50"

    $T configure -canvaspadx {12 0} -canvaspady {6 0}

    InitPics small-*

    #
    # Create columns
    #

    $T column create -text Name -tags {C0 name} -width 200 \
	-arrow up
    $T column create -text Size -tags size -justify right -width 60 \
	-arrowside left -arrowgravity right
    $T column create -text Type -tags type -width 120
    $T column create -text Modified -tags modified -width 120

    # Demonstration of per-state column options and configure "all"
    $T column configure all -background {gray90 active gray70 normal gray50 pressed}

    #
    # Create gradients
    #

    set steps [expr {($height - 5)/2}]
    $T gradient create G_mouseover       -steps $steps -stops {{0.0 white} {1.0 #ebf3fd}} -orient vertical
    $T gradient create G_selected_active -steps $steps -stops {{0.0 #dcebfc} {1.0 #c1dbfc}} -orient vertical
    $T gradient create G_selected        -steps $steps -stops {{0.0 #ebf4fe} {1.0 #cfe4fe}} -orient vertical
    $T gradient create G_focusout        -steps $steps -stops {{0.0 #f8f8f8} {1.0 #e5e5e5}} -orient vertical

    # With gdiplus this gives similar results
    # $T gradient configure G_mouseover -stops {{0.0 SystemHighlight 0.0} {1.0 SystemHighlight 0.1}} -orient vertical

    #
    # Create elements
    #

    if {$::shellicon} {
	$T element create elemImg shellicon -size small -useselected never
    } elseif {$::macBitmap} {
        $T element create elemImg bitmap
    } else {
	$T element create elemImg image -image small-folder
    }
    $T element create txtName text -lines 1
    $T element create txtType text -lines 1 -fill #6d6d6d
    $T element create txtSize text -datatype integer -format "%dKB" -lines 1 -fill #6d6d6d
    $T element create txtDate text -datatype time -format "%d/%m/%y %I:%M %p" -lines 1 -fill #6d6d6d

    $T state define mouseover
    $T state define openW
    $T state define openE
    $T state define openWE

    $T element create elemRectGradient rect \
	-fill [list	G_selected_active {selected mouseover} \
			G_focusout {selected !focus} \
			G_selected_active {selected active} \
			G_selected selected \
			G_mouseover mouseover]

    $T element create elemRectOutline rect -rx 1 \
	-open [list we openWE w openW e openE] \
	-outline [list #7da2ce {selected mouseover} \
		       #d9d9d9 {selected !focus} \
		       #7da2ce selected \
		       #7da2ce {active focus} \
		       #b8d6fb mouseover] -outlinewidth 1

    #
    # Create styles using the elements
    #

    # column 0: image + text
    set S [$T style create styName -orient horizontal]
    $T style elements $S {elemRectGradient elemRectOutline elemImg txtName}
    $T style layout $S elemRectGradient -detach yes -padx {2 0} -pady {2 3} -iexpand xy
    $T style layout $S elemRectOutline -detach yes -pady {0 1} -iexpand xy
    $T style layout $S elemImg -padx {6 2} -pady {2 3} -expand ns
    $T style layout $S txtName -pady {2 3} -squeeze x -expand ns

    # column 1: text
    set S [$T style create stySize]
    $T style elements $S {elemRectGradient elemRectOutline txtSize}
    $T style layout $S elemRectGradient -detach yes -padx 0 -pady {2 3} -iexpand xy
    $T style layout $S elemRectOutline -detach yes -pady {0 1} -iexpand xy
    $T style layout $S txtSize -padx 6 -pady {2 3} -squeeze x -expand ns

    # column 2: text
    set S [$T style create styType]
    $T style elements $S {elemRectGradient elemRectOutline txtType}
    $T style layout $S elemRectGradient -detach yes -padx 0 -pady {2 3} -iexpand xy
    $T style layout $S elemRectOutline -detach yes -pady {0 1} -iexpand xy
    $T style layout $S txtType -padx 6 -pady {2 3} -squeeze x -expand ns

    # column 3: text
    set S [$T style create styDate]
    $T style elements $S {elemRectGradient elemRectOutline txtDate}
    $T style layout $S elemRectGradient -detach yes -padx {0 2} -pady {2 3} -iexpand xy
    $T style layout $S elemRectOutline -detach yes -pady {0 1} -iexpand xy
    $T style layout $S txtDate -padx 6 -pady {2 3} -squeeze x -expand ns

    # List of lists: {column style element ...} specifying text elements
    # the user can edit.
    TreeCtrl::SetEditable $T {
	{name styName txtName}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on.
    TreeCtrl::SetSensitive $T {
	{name styName elemImg txtName}
	{size stySize txtSize}
	{type styType txtType}
	{modified styDate txtDate}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can select with the selection rectangle. Empty means
    # use the same list passed to TreeCtrl::SetSensitive.
    TreeCtrl::SetSensitiveMarquee $T {
	{name styName elemRectOutline elemImg txtName}
	{size stySize elemRectOutline txtSize}
	{type styType elemRectOutline txtType}
	{modified styDate elemRectOutline txtDate}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items.
    TreeCtrl::SetDragImage $T {
	{name styName elemImg txtName}
    }

    # During editing, hide the text
    $T state define edit
    $T style layout styName txtName -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item name styName size stySize type styType modified styDate
	$T item element configure $item \
	    name txtName -text [file tail $file] , \
	    type txtType -text "Folder" , \
	    modified txtDate -data [file mtime $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item \
		name elemImg -path $file
	}
	if {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item \
		name elemImg -bitmap [list $file]
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item name styName size stySize type styType modified styDate
	switch [file extension $file] {
	    .dll { set img small-dll }
	    .exe { set img small-exe }
	    .txt { set img small-txt }
	    default { set img small-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item \
		name elemImg -path $file + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	} elseif {$::macBitmap} {
	    ::tk::mac::iconBitmap $file 16 16 -file $file
	    $T item element configure $item \
		name elemImg -bitmap [list $file] + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	} else {
	    $T item element configure $item \
		name elemImg -image $img + txtName -text [file tail $file] , \
		size txtSize -data [expr {[file size $file] / 1024 + 1}] , \
		type txtType -text $type , \
		modified txtDate -data [file mtime $file]
	}
	$T item lastchild root $item
    }

    set scriptFollowup {
	DemoExplorerDetailsWin7_FixItemStyles $T
    }

    DemoExplorerAux $scriptDir $scriptFile $scriptFollowup
    DemoExplorerBindings true

    ConfigTransparentMarquee $T

    # Fix the display when a column is dragged
    $T notify bind $T <ColumnDrag-receive> {
	%T column move %C %b
	DemoExplorerDetailsWin7_FixItemStyles %T
    }

    # Fix the display when a column's visibility changes
    $T notify bind $T <DemoColumnVisibility> {
	DemoExplorerDetailsWin7_FixItemStyles %T
    }

    set ::Explorer(sortColumn) name
    set ::Explorer(sortColor) ""
    $T notify bind $T <Header-invoke> { ExplorerHeaderInvoke %T %C }

    bindtags $T [list $T DemoExplorerWin7 DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

proc DemoExplorerDetailsWin7_FixItemStyles {T} {
    foreach C [$T column id "visible !tail"] {
	if {[$T column compare $C == "first visible"]} {
	    set padx {2 0}
	    set state openE
	} elseif {[$T column compare $C == "last visible"]} {
	    set padx {0 2}
	    set state openW
	} else {
	    set padx {0 0}
	    set state openWE
	}
	switch -glob [$T column cget $C -tags] {
	    *name* {
		set style styName
		set padelem elemImg
		set padelemX {6 2}
	    }
	    size {
		set style stySize
	    }
	    type {
		set style styType
	    }
	    modified {
		set style styDate
	    }
	}
	$T item state forcolumn all $C [list !openW !openE !openWE $state]
	$T style layout $style elemRectGradient -padx $padx
    }
}

proc DemoExplorerLargeIconsWin7 {} {

    set T [DemoList]

    # Item height is 2 + 32 for icon + 4 pad + 3 lines of text + 3
    set fontHeight [font metrics [$T cget -font] -linespace]
    set itemHeight [expr {2 + 32 + 4 + $fontHeight * 3 + 3}]

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode extended -wrap window -orient horizontal \
	-itemwidth 74 -showheader no -yscrollsmoothing yes \
	-scrollmargin 16 -yscrolldelay "500 200"

    $T configure -canvaspadx {15 0} -canvaspady {6 0} -itemgapx 1 -itemgapy 1

    InitPics big-*

    #
    # Create columns
    #

    $T column create -tags C0

    #
    # Create gradients
    #

    set steps 8
    $T gradient create G_mouseover       -steps $steps -stops {{0.0 white} {1.0 #ebf3fd}} -orient vertical
    $T gradient create G_selected_active -steps $steps -stops {{0.0 #dcebfc} {1.0 #c1dbfc}} -orient vertical
    $T gradient create G_selected        -steps $steps -stops {{0.0 #ebf4fe} {1.0 #cfe4fe}} -orient vertical
    $T gradient create G_focusout        -steps $steps -stops {{0.0 #f8f8f8} {1.0 #e5e5e5}} -orient vertical

    #
    # Create elements
    #

    $T state define mouseover
    $T state define openW
    $T state define openE
    $T state define openWE

    $T element create elemRectGradient rect \
	-fill [list	G_selected_active {selected mouseover} \
			G_focusout {selected !focus} \
			G_selected_active {selected active} \
			G_selected selected \
			G_mouseover mouseover]

    $T element create elemRectOutline rect -rx 3 \
	-open [list we openWE w openW e openE] \
	-outline [list #7da2ce {selected mouseover} \
		       #d9d9d9 {selected !focus} \
		       #7da2ce selected \
		       #7da2ce {active focus} \
		       #b8d6fb mouseover] -outlinewidth 1

    if {$::shellicon} {
	$T element create elemImg shellicon -size large -useselect never
    } elseif {$::macBitmap} {
        $T element create elemImg bitmap
    } else {
	$T element create elemImg image -image big-folder
    }
    $T element create elemTxt text \
	-justify center -lines 3 -width 70 -wrap word

    #
    # Create styles using the elements
    #

    # image + text
    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemRectGradient elemRectOutline elemImg elemTxt}
    $T style layout $S elemRectGradient -union {elemImg elemTxt} -iexpand we
    $T style layout $S elemRectOutline -union elemRectGradient -ipadx 2 -ipady 2
    $T style layout $S elemImg -expand we
    $T style layout $S elemTxt -pady {4 0} -squeeze x -expand we

    # List of lists: {column style element ...} specifying text elements
    # the user can edit.
    TreeCtrl::SetEditable $T {
	{C0 STYLE elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can click on.
    TreeCtrl::SetSensitive $T {
	{C0 STYLE elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # the user can select with the selection rectangle. Empty means
    # use the same list passed to TreeCtrl::SetSensitive.
    TreeCtrl::SetSensitiveMarquee $T {
	{C0 STYLE elemRectOutline elemImg elemTxt}
    }

    # List of lists: {column style element ...} specifying elements
    # added to the drag image when dragging selected items.
    TreeCtrl::SetDragImage $T {
	{C0 STYLE elemImg elemTxt}
    }

    # During editing, hide the text and selection-rectangle elements.
    $T state define edit
    $T style layout STYLE elemTxt -draw {no edit}
    $T notify bind $T <Edit-begin> {
	%T item state set %I ~edit
    }
    $T notify bind $T <Edit-accept> {
	%T item element configure %I %C %E -text %t
    }
    $T notify bind $T <Edit-end> {
	%T item state set %I ~edit
    }

    #
    # Create items and assign styles
    #

    set scriptDir {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	$T item text $item C0 [file tail $file]
	if {$::shellicon} {
	    # The shellicon extension fails randomly (by putting GDB into the
	    # background!?) if the filename is not valid. MSDN says "relative
	    # paths are valid" but perhaps that is misinformation.
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    $T item element configure $item C0 \
		elemImg -path $file
	}
	if {$::macBitmap} {
	    if {$file eq ".."} { set file [file dirname $::Dir] }
	    ::tk::mac::iconBitmap $file 32 32 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file]
	}
	$T item lastchild root $item
    }

    set scriptFile {
	set item [$T item create -open no]
	$T item style set $item C0 STYLE
	switch [file extension $file] {
	    .dll { set img big-dll }
	    .exe { set img big-exe }
	    .txt { set img big-txt }
	    default { set img big-file }
	}
	set type [string toupper [file extension $file]]
	if {$type ne ""} {
	    set type "[string range $type 1 end] "
	}
	append type "File"
	if {$::shellicon} {
	    $T item element configure $item C0 \
		elemImg -path $file + \
		elemTxt -text [file tail $file]
	} elseif {$::macBitmap} {
	    ::tk::mac::iconBitmap $file 32 32 -file $file
	    $T item element configure $item C0 \
		elemImg -bitmap [list $file] + \
		elemTxt -text [file tail $file]
	} else {
	    $T item element configure $item C0 \
		elemImg -image [list $img] + \
		elemTxt -text [file tail $file]
	}
	$T item lastchild root $item
    }

    DemoExplorerAux $scriptDir $scriptFile
    DemoExplorerBindings true

    ConfigTransparentMarquee $T

    $T activate [$T item id "root firstchild"]

    $T notify bind $T <ActiveItem> {
	if {[%T item compare %p != root]} {
	    %T item element configure %p C0 elemTxt -lines {}
	}
	if {[%T item compare %c != root]} {
	    %T item element configure %c C0 elemTxt -lines 3
	}
    }
    $T item element configure active C0 elemTxt -lines 3

    bindtags $T [list $T DemoExplorerWin7 DemoExplorer TreeCtrlFileList TreeCtrl [winfo toplevel $T] all]

    return
}

# Win7: Handle <Motion> and <Leave> events.
proc ExplorerMotion {w x y} {
    global Explorer

    # Check for Win7 'mouseover' state
    if {[lsearch -exact [$w state names] mouseover] == -1} return

    if {[info exists TreeCtrl::Priv(buttonMode)] && $TreeCtrl::Priv(buttonMode) ne ""} {
	set x [set y -1]
    }

    set id [$w identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item" && [llength $id] > 4} {
	set item [lindex $id 1]
	if {$item ne $Explorer(prev)} {
	    if {$Explorer(prev) ne ""} {
		$w item state set $Explorer(prev) !mouseover
	    }
	    $w item state set $item mouseover
	    set Explorer(prev) $item
	}
	return
    }
    if {$Explorer(prev) ne ""} {
	if {[$w item id $Explorer(prev)] ne ""} {
	    $w item state set $Explorer(prev) !mouseover
	}
	set Explorer(prev) ""
    }
    return
}
