#!/bin/wish84.exe

# Copyright (c) 2002-2011 Tim Baker

set VERSION 2.4.1

package require Tk 8.4

set thisPlatform $::tcl_platform(platform)
if {$thisPlatform eq "unix" && [tk windowingsystem] eq "aqua"} {
    set thisPlatform "macosx"
}

switch -- [tk windowingsystem] {
    aqua { set thisPlatform "macosx" }
    classic { set thisPlatform "macintosh" }
    win32 { set thisPlatform "windows" }
    x11 { set thisPlatform "unix" }
}

proc Platform {args} {
    if {![llength $args]} { return $::thisPlatform }
    return [expr {[lsearch -exact $args $::thisPlatform] != -1}]
}

# Get full pathname to this file
set ScriptDir [file normalize [file dirname [info script]]]

# Command to create a full pathname in this file's directory
proc Path {args} {
    return [file normalize [eval [list file join $::ScriptDir] $args]]
}

# Create some photo images on demand
proc InitPics {args} {
    foreach pattern $args {
	if {[lsearch [image names] $pattern] == -1} {
	    foreach file [glob -directory [Path pics] $pattern.gif] {
		set imageName [file root [file tail $file]]
		# I created an image called "file", which clobbered the
		# original Tcl command "file". Then I got confused.
		if {[llength [info commands $imageName]]} {
		    error "don't want to create image called \"$imageName\""
		}
		image create photo $imageName -file $file

		# Hack -- Create a "selected" version too
		image create photo ${imageName}Sel
		${imageName}Sel copy $imageName
		imagetint ${imageName}Sel $::SystemHighlight 128
	    }
	}
    }
    return
}

# http://wiki.tcl.tk/1530
if {[info procs lassign] eq ""} {
    proc lassign {values args} {
	uplevel 1 [list foreach $args [linsert $values end {}] break]
	lrange $values [llength $args] end
    }
}

if {[catch {
    package require dbwin
}]} {
    proc dbwin {s} {
	puts [string trimright $s "\n"]
    }
}
proc dbwintrace {name1 name2 op} {
    dbwin $::dbwin
}
trace add variable ::dbwin write dbwintrace

# This gets called if 'package require' won't work during development.
proc LoadSharedLibrary {} {

    switch -- $::thisPlatform {
	macintosh {
	    set pattern treectrl*.shlb
	}
	macosx {
	    set pattern treectrl*.dylib
	}
	unix {
	    set pattern libtreectrl*[info sharedlibextension]*
	}
	windows {
	    set pattern treectrl*[info sharedlibextension]
	}
    }

    set SHLIB [glob -nocomplain -directory [Path ..] $pattern]
    if {[llength $SHLIB] != 1} {
	return 0
    }

    # When using configure/make, the "make demo" Makefile target sets the value of
    # the TREECTRL_LIBRARY environment variable which is used by tcl_findLibrary to
    # find our treectrl.tcl file. When *not* using configure/make, we set the value
    # of TREECTRL_LIBRARY and load the shared library manually. Note that
    # tcl_findLibrary is called by the Treectrl_Init() routine in C.
    set ::env(TREECTRL_LIBRARY) [Path .. library]

    load $SHLIB

    return 1
}

puts "demo.tcl: Tcl/Tk [info patchlevel] [winfo server .]"

# See if treectrl is already loaded for some reason
if {[llength [info commands treectrl]]} {
    puts "demo.tcl: using previously-loaded treectrl package v[package provide treectrl]"
    if {$VERSION ne [package provide treectrl]} {
	puts "demo.tcl: WARNING: expected v$VERSION"
    }

# For 'package require' to work with the development version, make sure the
# TCLLIBPATH and TREECTRL_LIBRARY environment variables are set by your
# Makefile/Jamfile/IDE etc.
} elseif {![catch {package require treectrl $VERSION} err]} {
    puts "demo.tcl: 'package require' succeeded"

} else {
    puts "demo.tcl: 'package require' failed: >>> $err <<<"

    if {[LoadSharedLibrary]} {
	puts "demo.tcl: loaded treectrl library by hand"

    } else {
	error "demo.tcl: can't load treectrl package"
    }
}

# Display path of shared library that was loaded
foreach list [info loaded] {
    set file [lindex $list 0]
    set pkg [lindex $list 1]
    if {$pkg ne "Treectrl"} continue
    puts "demo.tcl: using '$file'"
    break
}
if {[info exists env(TREECTRL_LIBRARY)]} {
    puts "demo.tcl: TREECTRL_LIBRARY=$env(TREECTRL_LIBRARY)"
} else {
    puts "demo.tcl: TREECTRL_LIBRARY undefined"
}
puts "demo.tcl: treectrl_library=$treectrl_library"

set tile 0
set tileFull 0 ; # 1 if using tile-aware treectrl
catch {
    if {[ttk::style layout TreeCtrl] ne ""} {
	set tile 1
	set tileFull 1
    }
}
if {$tile == 0} {
    catch {
	package require tile 0.7.8
	namespace export style
	namespace eval ::tile {
	    namespace export setTheme
	}
	namespace eval ::ttk {
	    namespace import ::style
	    namespace import ::tile::setTheme
	}
	set tile 1
    }
}
if {$tile} {
    # Don't import ttk::entry, it messes up the edit bindings, and I'm not
    # sure how to get/set the equivalent -borderwidth, -selectborderwidth
    # etc options of a TEntry.
    set entryCmd ::ttk::entry
    set buttonCmd ::ttk::button
    set checkbuttonCmd ::ttk::checkbutton
    set radiobuttonCmd ttk::radiobutton
    set scrollbarCmd ::ttk::scrollbar
    set scaleCmd ::ttk::scale
} else {
    set entryCmd ::entry
    set buttonCmd ::button
    set checkbuttonCmd ::checkbutton
    set radiobuttonCmd ::radiobutton
    set scrollbarCmd ::scrollbar
    set scaleCmd ::scale
}

option add *TreeCtrl.Background white
#option add *TreeCtrl.itemPrefix item
#option add *TreeCtrl.ColumnPrefix col

if {$tile} {
    set font TkDefaultFont
} else {
    switch -- $::thisPlatform {
	macintosh {
	    set font {Geneva 9}
	}
	macosx {
	    set font {{Lucida Grande} 13}
	}
	unix {
	    set font {Helvetica -12}
	}
	default {
	    # There is a bug on my Win98 box with Tk_MeasureChars() and
	    # MS Sans Serif 8.
	    set font {{MS Sans} 8}
	}
    }
}
array set fontInfo [font actual $font]
eval font create DemoFont [array get fontInfo]
option add *TreeCtrl.font DemoFont

array set fontInfo [font actual $font]
set fontInfo(-weight) bold
eval font create DemoFontBold [array get fontInfo]

array set fontInfo [font actual $font]
set fontInfo(-underline) 1
eval font create DemoFontUnderline [array get fontInfo]

proc SetDemoFontSize {size} {
    font configure DemoFont -size $size
    font configure DemoFontBold -size $size
    font configure DemoFontUnderline -size $size
    return
}
proc IncreaseFontSize {} {
    set size [font configure DemoFont -size]
    if {$size < 0} {
	incr size -1
    } else {
	incr size
    }
    SetDemoFontSize $size
    return
}
proc DecreaseFontSize {} {
    set size [font configure DemoFont -size]
    if {$size < 0} {
	incr size
    } else {
	incr size -1
    }
    SetDemoFontSize $size
    return
}

# Demo sources
foreach file {
    biglist
    bitmaps
    column-lock
    explorer
    firefox
    gradients
    gradients2
    gradients3
    headers
    help
    imovie
    layout
    mailwasher
    mycomputer
    outlook-folders
    outlook-newgroup
    random
    span
    table
    textvariable
    www-options
} {
    source [Path $file.tcl]
}

# Get default colors
set w [listbox .listbox]
set SystemButtonFace [$w cget -highlightbackground]
set SystemHighlight [$w cget -selectbackground]
set SystemHighlightText [$w cget -selectforeground]
destroy $w

if {$thisPlatform == "unix"} {
    # I hate that gray selection color
    set SystemHighlight #316ac5
    set SystemHighlightText White
}

proc MakeMenuBar {} {
    set m [menu .menubar]
    . configure -menu $m
    set m2 [menu $m.mFile -tearoff no]
    if {$::thisPlatform ne "unix" && [info commands console] ne ""} {
	console eval {
	    wm title . "TkTreeCtrl Console"
	    if {[info tclversion] == 8.4} {
		.console configure -font {Courier 9}
	    }
	    .console configure -height 8
#	    ::tk::ConsolePrompt
	    wm geometry . +0-100
	}
	$m2 add command -label "Console" -command {
	    if {[console eval {winfo ismapped .}]} {
		console hide
	    } else {
		console show
	    }
	}
    } else {
#	uplevel #0 source ~/Programming/console.tcl
    }
    $m2 add command -label "Event Browser" -command EventsWindow::ToggleWindowVisibility
    $m2 add command -label "Identify" -command IdentifyWindow::ToggleWindowVisibility
    $m2 add command -label "Style Editor" -command ToggleStyleEditorWindow
    $m2 add command -label "View Source" -command SourceWindow::ToggleWindowVisibility
    $m2 add command -label "Magnifier" -command LoupeWindow::ToggleWindowVisibility
    $m2 add separator
    $m2 add checkbutton -label "Native Gradients" -command ToggleNativeGradients \
	-variable ::NativeGradients
    $m2 add separator
    $m2 add command -label "Increase Font Size" -command IncreaseFontSize
    $m2 add command -label "Decrease Font Size" -command DecreaseFontSize
    switch -- [Platform] {
	macintosh -
	macosx {
	    $m add cascade -label "TkTreeCtrl" -menu $m2
	}
	unix -
	windows {
	    $m2 add separator
	    $m2 add command -label "Quit" -command exit
	    $m add cascade -label "File" -menu $m2
	}
    }

    if {$::tile} {
	set m2 [menu $m.mTheme -tearoff no]
	$m add cascade -label "Theme" -menu $m2
	foreach theme [lsort -dictionary [ttk::style theme names]] {
	    $m2 add radiobutton -label $theme -command [list ttk::setTheme $theme] \
		-variable ::DemoTheme -value $theme
	}
	$m2 add separator
	$m2 add command -label "Inspector" -command ThemeWindow::ToggleWindowVisibility
    }

    return
}

namespace eval EventsWindow {}

proc EventsWindow::Init {} {
    set w [toplevel .events]
    wm withdraw $w
#    wm transient $w .
    wm title $w "TkTreeCtrl Events"

    set m [menu $w.menubar]
    $w configure -menu $m
    set m1 [menu $m.m1 -tearoff 0]
    $m1 add cascade -label "Static" -menu [menu $m1.m1 -tearoff 0]
    $m1 add cascade -label "Dynamic" -menu [menu $m1.m2 -tearoff 0]
    $m1 add command -label "Clear Window" -command "$w.f.t item delete all" \
	-accelerator Ctrl+X
    $m1 add command -label "Rebuild Menus" -command "EventsWindow::RebuildMenus $w.f.t $m"
    $m add cascade -label "Events" -menu $m1

    bind $w <Control-KeyPress-x> "$w.f.t item delete all"

    TreePlusScrollbarsInAFrame $w.f 1 1
    pack $w.f -expand yes -fill both

    set T $w.f.t

    $T configure -showheader no -showroot no -showrootlines no -height 300
    $T column create -tags C0
    $T configure -treecolumn C0

    $T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e2 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes
    $T element create e4 rect -fill blue -width 100 -height 2

    set S [$T style create s1]
    $T style elements $S {e3 e1}
    $T style layout $S e3 -union [list e1] -ipadx 1 -ipady {0 1}

    set S [$T style create s2]
    $T style elements $S {e3 e1 e2}
    $T style layout $S e1 -width 20 -sticky w
    $T style layout $S e3 -union [list e1 e2] -ipadx 1 -ipady {0 1}

    set S [$T style create s3]
    $T style elements $S {e4}

    $T column configure C0 -itemstyle s1

    RebuildMenus $T $m

    wm protocol $w WM_DELETE_WINDOW "EventsWindow::ToggleWindowVisibility"
    switch -- $::thisPlatform {
	macintosh -
	macosx {
	    wm geometry $w -40+40
	}
	default {
	    wm geometry $w -0+0
	}
    }

    return
}

proc EventsWindow::RebuildMenus {T m} {
    variable Priv
    foreach event [lsort -dictionary [[DemoList] notify eventnames]] {
	set details [lsort -dictionary [[DemoList] notify detailnames $event]]
	foreach detail $details {
	    set pattern <$event-$detail>
	    set linkage [[DemoList] notify linkage $pattern]
	    lappend patterns $pattern $linkage
	    lappend patterns2($linkage) $pattern
	}
	if {![llength $details]} {
	    set pattern <$event>
	    set linkage [[DemoList] notify linkage $pattern]
	    lappend patterns $pattern $linkage
	    lappend patterns2($linkage) $pattern
	}
    }

    $m.m1.m1 delete 0 end
    $m.m1.m2 delete 0 end
    set menu(static) $m.m1.m1
    set menu(dynamic) $m.m1.m2
    foreach {pattern linkage} $patterns {
	if {![info exists Priv(track,$pattern)]} {
	    set Priv(track,$pattern) 1
	}
	$menu($linkage) add checkbutton -label $pattern \
	    -variable ::EventsWindow::Priv(track,$pattern) \
	    -command [list EventsWindow::ToggleEvent $T $pattern]
    }
    foreach linkage {static dynamic} {
	$menu($linkage) add separator
	$menu($linkage) add command -label "Toggle All" \
	    -command [list EventsWindow::ToggleEvents $T $patterns2($linkage)]
    }

    set Priv(events) {}
    set Priv(afterId) ""
    foreach {pattern linkage} $patterns {
	[DemoList] notify bind $T $pattern {
	    EventsWindow::EventBinding %W %?
	}
    }
    return
}

proc EventsWindow::EventBinding {T charMap} {
    variable Priv
    lappend Priv(events) $charMap
    if {$Priv(afterId) eq ""} {
	set Priv(afterId) [after idle [list EventsWindow::RecordEvents $T]]
    }
    return
}

proc EventsWindow::RecordEvents {T} {
    variable Priv
    set Priv(afterId) ""
    set events $Priv(events)
    set Priv(events) {}
    if {![winfo ismapped .events]} return
    if {[$T item numchildren root] > 2000} {
	set N [expr {[$T item numchildren root] - 2000}]
	$T item delete "root firstchild" "root child $N"
    }
    if {0 && [$T item count] > 1} {
	set I [$T item create]
	$T item style set $I 0 s3
	$T item lastchild root $I
    }
    set open 1
    if {[llength $events] > 50} {
	set open 0
    }
    foreach list $events {
	RecordEvent $T $list $open
    }
    $T see "last visible"
    return
}

proc EventsWindow::RecordEvent {T list open} {
    set I [$T item create -open $open]
    array set map $list
    $T item text $I C0 $map(P)
    $T item lastchild root $I
    foreach {char value} $list {
	if {[string first $char "TWPed"] != -1} continue
	set I2 [$T item create]
	$T item style set $I2 C0 s2
	$T item element configure $I2 C0 e1 -text $char + e2 -text $value
	$T item lastchild $I $I2
	$T item configure $I -button yes
    }
    return
}

proc EventsWindow::ToggleWindowVisibility {} {
    set w .events
    if {![winfo exists $w]} {
	Init
    }
    if {[winfo ismapped $w]} {
	wm withdraw $w
    } else {
	wm deiconify $w
	raise $w
    }
    return
}

proc EventsWindow::ToggleEvent {T pattern} {
    variable Priv
    [DemoList] notify configure $T $pattern -active $Priv(track,$pattern)
    return
}

proc EventsWindow::ToggleEvents {T patterns} {
    variable Priv
    foreach pattern $patterns {
	set Priv(track,$pattern) [expr {!$Priv(track,$pattern)}]
	ToggleEvent $T $pattern
    }
    return
}

namespace eval IdentifyWindow {}

proc IdentifyWindow::Init {} {
    set w .identify
    toplevel $w
    wm withdraw $w
    wm title $w "TkTreeCtrl Identify"
    set wText $w.text
    text $wText -state disabled -width 70 -height 3 -font [[DemoList] cget -font]
    $wText tag configure tagBold -font DemoFontBold
    pack $wText -expand yes -fill both
    wm protocol $w WM_DELETE_WINDOW "IdentifyWindow::ToggleWindowVisibility"
    return
}

proc IdentifyWindow::Update {T x y} {
    set w .identify
    if {![winfo exists $w]} return
    if {![winfo ismapped $w]} return
    set wText $w.text
    $wText configure -state normal
    $wText delete 1.0 end
    set nearest [$T item id [list nearest $x $y]]
    $wText insert end "x=" tagBold "$x  " {} "y=" tagBold "$y  " {} "nearest=" tagBold $nearest\n
    $wText insert end "string: "
    foreach {key val} [$T identify $x $y] {
	$wText insert end $key tagBold " $val "
    }
    $wText insert end "\narray: "
    $T identify -array id $x $y
    switch -- $id(where) {
	"header" {
	    set keys [list where header column element side]
	}
	"item" {
	    set keys [list where item column element button line]
	}
	default {
	    set keys [array names id]
	}
    }
    foreach key $keys {
	set val $id($key)
	if {$val eq ""} {
	    set val "\"\""
	}
	$wText insert end $key tagBold " $val "
    }
    $wText configure -state disabled
    return
}

proc IdentifyWindow::ToggleWindowVisibility {} {
    set w .identify
    if {![winfo exists $w]} {
	Init
    }
    if {[winfo ismapped $w]} {
	wm withdraw $w
    } else {
	wm deiconify $w
	raise $w
    }
    return
}

namespace eval SourceWindow {}

proc SourceWindow::Init {} {
    set w [toplevel .source]
    wm withdraw $w
#    wm transient $w .
    set f [frame $w.f -borderwidth 0]
    if {[lsearch -exact [font names] TkFixedFont] != -1} {
	set font TkFixedFont
    } else {
	switch -- $::thisPlatform {
	    macintosh -
	    macosx {
		set font {Geneva 9}
	    }
	    unix {
		set font {Courier -12}
	    }
	    default {
		set font {Courier 9}
	    }
	}
    }
    text $f.t -font $font -tabs [font measure $font 12345678] -wrap none \
	-yscrollcommand "$f.sv set" -xscrollcommand "$f.sh set"
    $::scrollbarCmd $f.sv -orient vertical -command "$f.t yview"
    $::scrollbarCmd $f.sh -orient horizontal -command "$f.t xview"
    pack $f -expand yes -fill both
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1
    grid configure $f.t -row 0 -column 0 -sticky news
    grid configure $f.sh -row 1 -column 0 -sticky we
    grid configure $f.sv -row 0 -column 1 -sticky ns

    wm protocol $w WM_DELETE_WINDOW "SourceWindow::ToggleWindowVisibility"
    switch -- $::thisPlatform {
	macintosh -
	macosx {
	    wm geometry $w +0+30
	}
	default {
	    wm geometry $w -0+0
	}
    }

    return
}

proc SourceWindow::ShowSource {file} {
    wm title .source "TkTreeCtrl Source: $file"
    set path [Path $file]
    set t .source.f.t
    set chan [open $path]
    $t delete 1.0 end
    $t insert end [read $chan]
    $t mark set insert 1.0
    close $chan
    return
}

proc SourceWindow::ToggleWindowVisibility {} {
    set w .source
    if {[winfo ismapped $w]} {
	wm withdraw $w
    } else {
	wm deiconify $w
	raise $w
    }
    return
}

proc ToggleStyleEditorWindow {} {
    set w .styleEditor
    if {![winfo exists $w]} {
	source [Path style-editor.tcl]
	StyleEditor::Init [DemoList]
	StyleEditor::SetListOfStyles
    } elseif {[winfo ismapped $w]} {
	wm withdraw $w
    } else {
	wm deiconify $w
	raise $w
	StyleEditor::SetListOfStyles
    }
    return
}

namespace eval ThemeWindow {}
proc ThemeWindow::Init {} {
    set w [toplevel .theme]
    wm withdraw $w
#    wm transient $w .
    wm title $w "TkTreeCtrl Themes"

    set m [menu $w.menubar]
    $w configure -menu $m
    set m1 [menu $m.m1 -tearoff 0]
    $m1 add command -label "Set List" -command ThemeWindow::SetList
    $m add cascade -label "Theme" -menu $m1

    TreePlusScrollbarsInAFrame $w.f 1 1
    pack $w.f -expand yes -fill both

    set T $w.f.t

    $T configure -showheader no -showroot no -showrootlines no -height 300
    $T column create -tags C0
    $T configure -treecolumn C0

    $T element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $T element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes

    set S [$T style create s1]
    $T style elements $S {e3 e1}
    $T style layout $S e3 -union [list e1] -ipadx 1 -ipady {0 1}

    $T column configure C0 -itemstyle s1

    SetList

    wm protocol $w WM_DELETE_WINDOW "ThemeWindow::ToggleWindowVisibility"

    return
}

proc ThemeWindow::ToggleWindowVisibility {} {
    set w .theme
    if {![winfo exists $w]} {
	Init
    }
    if {[winfo ismapped $w]} {
	wm withdraw $w
    } else {
	wm deiconify $w
	raise $w
    }
    return
}

proc ThemeWindow::SetList {} {
    set w .theme
    set T $w.f.t

    $T item delete all
    #
    # Themes
    #
    foreach theme [lsort -dictionary [ttk::style theme names]] {
	set I [$T item create -button yes -open no -tags theme -parent root]
	$T item text $I C0 $theme
	ttk::style theme settings $theme {
	    set I2 [$T item create -button yes -open no -parent $I]
	    $T item text $I2 C0 ELEMENTS
	    #
	    # Elements
	    #
	    foreach element [lsort -dictionary [ttk::style element names]] {
		#
		# Element options
		#
		set options [ttk::style element options $element]
		set I3 [$T item create -button [llength $options] -open no -tags element -parent $I2]
		$T item text $I3 C0 $element
		foreach option [lsort -dictionary $options] {
		    set I4 [$T item create -open no -tags {element option} -parent $I3]
		    $T item text $I4 C0 $option
		}
	    }
	    #
	    # Styles
	    #
	    set I2 [$T item create -button yes -open no -parent $I]
	    $T item text $I2 C0 STYLES
	    set styles [list "."] ; # [ttk::style names] please!
	    foreach style [lsort -dictionary $styles] {
		#
		# Style options
		#
		set cfg [ttk::style configure $style]
		set I3 [$T item create -button [llength $cfg] -open no -tags style -parent $I2]
		$T item text $I3 C0 $style
		foreach {option value} $cfg {
		    set I4 [$T item create -open no -tags {style option} -parent $I3]
		    $T item text $I4 C0 "$option $value"
		}
	    }
	}
    }
    return
}

set ::NativeGradients 1
proc ToggleNativeGradients {} {
    [DemoList] gradient native $::NativeGradients
    dbwin "native gradients is now $::NativeGradients"
    return
}

SourceWindow::Init
MakeMenuBar

# http://wiki.tcl.tk/950
proc sbset {sb first last} {
    # Get infinite loop on X11
    if {$::thisPlatform ne "unix"} {
	if {$first <= 0 && $last >= 1} {
	    grid remove $sb
	} else {
	    grid $sb
	}
    }
    $sb set $first $last
    return
}

proc TreePlusScrollbarsInAFrame {f h v} {
    if {$::tileFull} {
	frame $f -borderwidth 0
    } else {
	frame $f -borderwidth 1 -relief sunken
    }
    treectrl $f.t -highlightthickness 0 -borderwidth 0
    if {[Platform unix]} {
	$f.t configure -headerfont [$f.t cget -font]
    }
    $f.t configure -xscrollincrement 20 -xscrollsmoothing 1
#    $f.t configure -itemprefix item# -columnprefix column#
    $f.t debug configure -enable no -display yes -erasecolor pink \
	-drawcolor orange -displaydelay 30 -textlayout 0 -data 0 -span 0
    if {$h} {
	$::scrollbarCmd $f.sh -orient horizontal -command "$f.t xview"
	#		$f.t configure -xscrollcommand "$f.sh set"
	$f.t notify bind $f.sh <Scroll-x> { sbset %W %l %u }
	bind $f.sh <ButtonPress-1> "focus $f.t"
    }
    if {$v} {
	$::scrollbarCmd $f.sv -orient vertical -command "$f.t yview"
	#		$f.t configure -yscrollcommand "$f.sv set"
	$f.t notify bind $f.sv <Scroll-y> { sbset %W %l %u }
	bind $f.sv <ButtonPress-1> "focus $f.t"
    }
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1
    grid configure $f.t -row 0 -column 0 -sticky news
    if {$h} {
	grid configure $f.sh -row 1 -column 0 -sticky we
    }
    if {$v} {
	grid configure $f.sv -row 0 -column 1 -sticky ns
    }

    bind $f.t <Control-Shift-ButtonPress-1> {
	TreeCtrl::MarqueeBegin %W %x %y
	set DebugExpose(x1) %x
	set DebugExpose(y1) %y
	break
    }
    bind $f.t <Control-Shift-Button1-Motion> {
	TreeCtrl::MarqueeUpdate %W %x %y
	set DebugExpose(x2) %x
	set DebugExpose(y2) %y
	break
    }
    bind $f.t <Control-Shift-ButtonRelease-1> {
	TreeCtrl::MarqueeEnd %W %x %y
	%W debug expose $DebugExpose(x1) $DebugExpose(y1) $DebugExpose(x2) $DebugExpose(y2)
	break
    }

    MakeListPopup $f.t
    MakeHeaderPopup $f.t

    switch -- $::thisPlatform {
	macintosh -
	macosx {
	    bind $f.t <Control-ButtonPress-1> {
		ShowPopup %W %x %y %X %Y
	    }
	    bind $f.t <ButtonPress-2> {
		ShowPopup %W %x %y %X %Y
	    }
	}
	unix -
	windows {
	    bind $f.t <ButtonPress-3> {
		ShowPopup %W %x %y %X %Y
	    }
	}
    }

    return
}

proc ShouldShowLines {T} {
    if {![$T cget -usetheme]} {
	return 1
    }
    switch -- [$T theme platform] {
	aqua -
	gtk {
	    return 0
	}
    }
    return 1
}

proc MakeMainWindow {} {

    wm title . "TkTreeCtrl Demo"

    switch -- $::thisPlatform {
	macintosh -
	macosx {
	    wm geometry . +6+30
	}
	default {
	    wm geometry . +0+0
	}
    }

    panedwindow .pw2 -orient horizontal -borderwidth 0 -sashwidth 6
    panedwindow .pw1 -orient vertical -borderwidth 0 -sashwidth 6

    # Tree + scrollbar: demos
    TreePlusScrollbarsInAFrame .f1 1 1
    .f1.t configure -showbuttons no -showlines no -showroot no -height 100
    .f1.t column create -text "List of Demos" -expand yes -button no -tags C0
    .f1.t configure -treecolumn C0

    # Tree + scrollbar: styles + elements in list
    TreePlusScrollbarsInAFrame .f4 1 1
    .f4.t configure -showlines [ShouldShowLines .f4.t] -showroot no -height 140
    .f4.t column create -text "Elements and Styles" -expand yes -button no -tags C0
    .f4.t configure -treecolumn C0

    # Tree + scrollbar: styles + elements in selected item
    TreePlusScrollbarsInAFrame .f3 1 1
    .f3.t configure -showlines [ShouldShowLines .f3.t] -showroot no
    .f3.t column create -text "Styles in Item" -expand yes -button no -tags C0
    .f3.t configure -treecolumn C0

    .pw1 add .f1 .f4 .f3 -height 150

    # Frame on right
    frame .f2

    # Tree + scrollbars
    TreePlusScrollbarsInAFrame .f2.f1 1 1
    [DemoList] configure -indent 19

    # Give it a big border to debug drawing
    if {!$::tileFull} {
	[DemoList] configure -borderwidth 6 -relief ridge -highlightthickness 3
    }

    grid columnconfigure .f2 0 -weight 1
    grid rowconfigure .f2 0 -weight 1
    grid configure .f2.f1 -row 0 -column 0 -sticky news -pady 0

    # Window to display result of "T identify"
    bind TagIdentify <Motion> {
	if {"%W" ne [DemoList]} {
	    set x [expr {%X - [winfo rootx [DemoList]]}]
	    set y [expr {%Y - [winfo rooty [DemoList]]}]
	} else {
	    set x %x
	    set y %y
	}
	IdentifyWindow::Update [DemoList] $x $y
    }
    AddBindTag [DemoList] TagIdentify

    .pw2 add .pw1 -width 200
    .pw2 add .f2 -width 450

    pack .pw2 -expand yes -fill both

    bind [DemoList] <g> {
	set NativeGradients [expr {!$NativeGradients}]
	ToggleNativeGradients
    }

    ###
    # A treectrl widget can generate the following built-in events:
    # <ActiveItem> called when the active item changes
    # <Collapse-before> called before an item is closed
    # <Collapse-after> called after an item is closed
    # <Expand-before> called before an item is opened
    # <Expand-after> called after an item is opened
    # <ItemDelete> called before items are deleted
    # <Scroll-x> called when horizontal scroll position changes
    # <Scroll-y> called when vertical scroll position changes
    # <Selection> called when items are added to or removed from the selection
    #
    # The application programmer can define custom events to be
    # generated by the "notify generate" command. The following events
    # are generated by the library scripts.

    [DemoList] notify install <Header-invoke>
    [DemoList] notify install <Header-state>

    [DemoList] notify install <ColumnDrag-begin>
    [DemoList] notify install <ColumnDrag-end>
    [DemoList] notify install <ColumnDrag-indicator>
    [DemoList] notify install <ColumnDrag-receive>

    [DemoList] notify install <Drag-begin>
    [DemoList] notify install <Drag-end>
    [DemoList] notify install <Drag-receive>

    [DemoList] notify install <Edit-begin>
    [DemoList] notify install <Edit-end>
    [DemoList] notify install <Edit-accept>
    ###

    # This event is generated when a column's visibility is changed by
    # the context menu.
    [DemoList] notify install <DemoColumnVisibility>

    return
}

proc DemoList {} {
    return .f2.f1.t
}
proc demolist args { # console-friendly version
    uplevel .f2.f1.t $args
}

proc MakeListPopup {T} {
    set m [menu $T.mTree -tearoff no]

    set m2 [menu $m.mCollapse -tearoff no]
    $m add cascade -label Collapse -menu $m2

    set m2 [menu $m.mExpand -tearoff no]
    $m add cascade -label Expand -menu $m2

    set m2 [menu $m.mBgImg -tearoff no]
    $m2 add radiobutton -label none -variable Popup(bgimg) -value none \
        -command {$Popup(T) configure -backgroundimage ""}
    $m2 add radiobutton -label feather -variable Popup(bgimg) -value feather \
        -command {$Popup(T) configure -bgimage $Popup(bgimg) -bgimageopaque no}
    $m2 add radiobutton -label sky -variable Popup(bgimg) -value sky \
        -command {$Popup(T) configure -bgimage $Popup(bgimg) -bgimageopaque yes}
    $m2 add separator
    set m3 [menu $m2.mBgImgAnchor -tearoff no]
    foreach anchor {nw n ne w center e sw s se} {
	$m3 add radiobutton -label $anchor -variable Popup(bgimganchor) \
	    -value $anchor \
	    -command {$Popup(T) configure -bgimageanchor $Popup(bgimganchor)}
    }
    $m2 add cascade -label "Anchor" -menu $m3
    $m2 add separator
    $m2 add checkbutton -label "Opaque" -variable Popup(bgimgopaque) \
	-command {$Popup(T) configure -bgimageopaque $Popup(bgimgopaque)}
    $m2 add separator
    $m2 add checkbutton -label "Scroll X" -variable Popup(bgimgscrollx) \
	-onvalue x -offvalue "" -command {$Popup(T) configure -bgimagescroll $Popup(bgimgscrollx)$Popup(bgimgscrolly)}
    $m2 add checkbutton -label "Scroll Y" -variable Popup(bgimgscrolly) \
	-onvalue y -offvalue "" -command {$Popup(T) configure -bgimagescroll $Popup(bgimgscrollx)$Popup(bgimgscrolly)}
    $m2 add separator
    $m2 add checkbutton -label "Tile X" -variable Popup(bgimgtilex) \
	-onvalue x -offvalue "" -command {$Popup(T) configure -bgimagetile $Popup(bgimgtilex)$Popup(bgimgtiley)}
    $m2 add checkbutton -label "Tile Y" -variable Popup(bgimgtiley) \
	-onvalue y -offvalue "" -command {$Popup(T) configure -bgimagetile $Popup(bgimgtilex)$Popup(bgimgtiley)}
    $m add cascade -label "Background Image" -menu $m2

    set m2 [menu $m.mBgMode -tearoff no]
    foreach value {column order ordervisible row} {
        $m2 add radiobutton -label $value -variable Popup(bgmode) -value $value \
	    -command {$Popup(T) configure -backgroundmode $Popup(bgmode)}
    }
    $m add cascade -label "Background Mode" -menu $m2

    $m add checkbutton -label "Button Tracking" -variable Popup(buttontracking) \
	-command {$Popup(T) configure -buttontracking $Popup(buttontracking)}

    set m2 [menu $m.mColumns -tearoff no]
    $m add cascade -label "Columns" -menu $m2

    set m2 [menu $m.mHeaders -tearoff no]
    $m add cascade -label "Headers" -menu $m2

    set m2 [menu $m.mColumnResizeMode -tearoff no]
    $m2 add radiobutton -label proxy -variable Popup(columnresizemode) -value proxy \
	-command {$Popup(T) configure -columnresizemode $Popup(columnresizemode)}
    $m2 add radiobutton -label realtime -variable Popup(columnresizemode) -value realtime \
	-command {$Popup(T) configure -columnresizemode $Popup(columnresizemode)}
    $m add cascade -label "Column Resize Mode" -menu $m2

    set m2 [menu $m.mDebug -tearoff no]
    $m2 add checkbutton -label Data -variable Popup(debug,data) \
	-command {$Popup(T) debug configure -data $Popup(debug,data)}
    $m2 add checkbutton -label Display -variable Popup(debug,display) \
	-command {$Popup(T) debug configure -display $Popup(debug,display)}
    $m2 add checkbutton -label Span -variable Popup(debug,span) \
	-command {$Popup(T) debug configure -span $Popup(debug,span)}
    $m2 add checkbutton -label "Text Layout" -variable Popup(debug,textlayout) \
	-command {$Popup(T) debug configure -textlayout $Popup(debug,textlayout)}
    $m2 add separator
    set m3 [menu $m2.mDelay -tearoff no]
    foreach n {10 20 30 40 50 60 70 80 90 100} {
	$m3 add radiobutton -label $n -variable Popup(debug,displaydelay) -value $n \
	    -command {$Popup(T) debug configure -displaydelay $Popup(debug,displaydelay)}
    }
    $m2 add cascade -label "Display Delay" -menu $m3
    $m2 add separator
    $m2 add checkbutton -label Enable -variable Popup(debug,enable) \
	-command {$Popup(T) debug configure -enable $Popup(debug,enable)}
    $m add cascade -label Debug -menu $m2
if 0 {
    set m2 [menu $m.mBuffer -tearoff no]
    $m2 add radiobutton -label "none" -variable Popup(doublebuffer) -value none \
	-command {$Popup(T) configure -doublebuffer $Popup(doublebuffer)}
    $m2 add radiobutton -label "item" -variable Popup(doublebuffer) -value item \
	-command {$Popup(T) configure -doublebuffer $Popup(doublebuffer)}
    $m2 add radiobutton -label "window" -variable Popup(doublebuffer) -value window \
	-command {$Popup(T) configure -doublebuffer $Popup(doublebuffer)}
    $m add cascade -label Buffering -menu $m2
}
    set m2 [menu $m.mItemWrap -tearoff no]
    $m add cascade -label "Item Wrap" -menu $m2

    set m2 [menu $m.mLineStyle -tearoff no]
    $m2 add radiobutton -label "dot" -variable Popup(linestyle) -value dot \
	-command {$Popup(T) configure -linestyle $Popup(linestyle)}
    $m2 add radiobutton -label "solid" -variable Popup(linestyle) -value solid \
	-command {$Popup(T) configure -linestyle $Popup(linestyle)}
    $m add cascade -label "Line style" -menu $m2

    set m2 [menu $m.mOrient -tearoff no]
    $m2 add radiobutton -label "Horizontal" -variable Popup(orient) -value horizontal \
	-command {$Popup(T) configure -orient $Popup(orient)}
    $m2 add radiobutton -label "Vertical" -variable Popup(orient) -value vertical \
	-command {$Popup(T) configure -orient $Popup(orient)}
    $m add cascade -label Orient -menu $m2

    set m2 [menu $m.mSmoothing -tearoff no]
    $m2 add checkbutton -label X -variable Popup(xscrollsmoothing) \
        -command {$Popup(T) configure -xscrollsmoothing $Popup(xscrollsmoothing)}
    $m2 add checkbutton -label Y -variable Popup(yscrollsmoothing) \
        -command {$Popup(T) configure -yscrollsmoothing $Popup(yscrollsmoothing)}
    $m add cascade -label "Scroll Smoothing" -menu $m2

    set m2 [menu $m.mSelectMode -tearoff no]
    foreach mode [list browse extended multiple single] {
	$m2 add radiobutton -label $mode -variable Popup(selectmode) -value $mode \
	    -command {$Popup(T) configure -selectmode $Popup(selectmode)}
    }
    $m add cascade -label Selectmode -menu $m2

    set m2 [menu $m.mShow -tearoff no]
    $m2 add checkbutton -label "Buttons" -variable Popup(showbuttons) \
	-command {$Popup(T) configure -showbuttons $Popup(showbuttons)}
    $m2 add checkbutton -label "Header" -variable Popup(showheader) \
	-command {$Popup(T) configure -showheader $Popup(showheader)}
    $m2 add checkbutton -label "Lines" -variable Popup(showlines) \
	-command {$Popup(T) configure -showlines $Popup(showlines)}
    $m2 add checkbutton -label "Root" -variable Popup(showroot) \
	-command {$Popup(T) configure -showroot $Popup(showroot)}
    $m2 add checkbutton -label "Root Button" -variable Popup(showrootbutton) \
	-command {$Popup(T) configure -showrootbutton $Popup(showrootbutton)}
    $m2 add checkbutton -label "Root Child Buttons" -variable Popup(showrootchildbuttons) \
	-command {$Popup(T) configure -showrootchildbuttons $Popup(showrootchildbuttons)}
    $m2 add checkbutton -label "Root Child Lines" -variable Popup(showrootlines) \
	-command {$Popup(T) configure -showrootlines $Popup(showrootlines)}
    $m add cascade -label Show -menu $m2

    set m2 [menu $m.mSpan -tearoff no]
    $m add cascade -label Span -menu $m2

    $m add checkbutton -label "Use Theme" -variable Popup(usetheme) \
	-command {$Popup(T) configure -usetheme $Popup(usetheme)}

    return
}

proc MakeHeaderPopup {T} {
    set m [menu $T.mColumn -tearoff no]

    ### Header

    set m1 [menu $m.mHeader -tearoff no]
    $m add cascade -label "Header" -menu $m1

    $m1 add checkbutton -label "Visible" -variable Popup(header,visible) \
	-command [list eval $T header configure \$Popup(header) -visible \$Popup(header,visible)]

    set m2 [menu $m1.mDnD -tearoff no]
    $m1 add cascade -label "Drag and Drop" -menu $m2
    $m2 add checkbutton -label "Draw" -variable Popup(header,drag,draw) \
	-command [list eval $T header dragconfigure \$Popup(header) -draw \$Popup(header,drag,draw)]
    $m2 add checkbutton -label "Enable" -variable Popup(header,drag,enable) \
	-command [list eval $T header dragconfigure \$Popup(header) -enable \$Popup(header,drag,enable)]

    ### Header column

    set m1 [menu $m.mHeaderColumn -tearoff no]
    $m add cascade -label "Header Column" -menu $m1

    set m2 [menu $m1.mArrow -tearoff no]
    $m1 add cascade -label Arrow -menu $m2
    $m2 add radiobutton -label "None" -variable Popup(arrow) -value none \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrow none}
    $m2 add radiobutton -label "Up" -variable Popup(arrow) -value up \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrow up}
    $m2 add radiobutton -label "Down" -variable Popup(arrow) -value down \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrow down}
    $m2 add separator
    $m2 add radiobutton -label "Side Left" -variable Popup(arrow,side) -value left \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrowside left}
    $m2 add radiobutton -label "Side Right" -variable Popup(arrow,side) -value right \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrowside right}
    $m2 add separator
    $m2 add radiobutton -label "Gravity Left" -variable Popup(arrow,gravity) -value left \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrowgravity left}
    $m2 add radiobutton -label "Gravity Right" -variable Popup(arrow,gravity) -value right \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -arrowgravity right}

    $m1 add checkbutton -label "Button" -variable Popup(button) \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -button $Popup(button)}

    set m2 [menu $m1.mJustify -tearoff no]
    $m1 add cascade -label "Justify" -menu $m2
    $m2 add radiobutton -label "Left" -variable Popup(header,justify) -value left \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -justify left}
    $m2 add radiobutton -label "Center" -variable Popup(header,justify) -value center \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -justify center}
    $m2 add radiobutton -label "Right" -variable Popup(header,justify) -value right \
	-command {$Popup(T) header configure $Popup(header) $Popup(column) -justify right}

    set m2 [menu $m1.mSpan -tearoff no]
    $m1 add cascade -label Span -menu $m2

    ### Tree column
    $m add command -label "Column"

    return
}

proc MakeHeaderSubmenu {T H parentMenu} {

    ### Header

    set m1 [menu $parentMenu.mHeader$H -tearoff no]

    $m1 add checkbutton -label "Visible" -variable Popup(header,visible,$H) \
	-command [list eval $T header configure $H -visible \$Popup(header,visible,$H)]

    return $m1
}

proc MakeColumnSubmenu {T C parentMenu {menuName ""}} {

    ### Tree-column
if 1 {
    if {$menuName ne ""} {
	set m1 [menu $parentMenu.mColumn$menuName -tearoff no]
    } else {
	set m1 [menu $parentMenu.mColumn$C -tearoff no]
    }
} else {
    set m1 $parentMenu.mColumn$C
    $m1 delete 0 end
}
    $m1 add checkbutton -label "Expand" -variable Popup(column,expand,$C) \
	-command [list eval $T column configure $C -expand \$Popup(column,expand,$C)]

    set m2 [menu $m1.mItemJustify -tearoff no]
    $m1 add cascade -label "Item Justify" -menu $m2
    $m2 add radiobutton -label "Left" -variable Popup(column,itemjustify,$C) -value left \
	-command [list $T column configure $C -itemjustify left]
    $m2 add radiobutton -label "Center" -variable Popup(column,itemjustify,$C) -value center \
	-command [list $T column configure $C -itemjustify center]
    $m2 add radiobutton -label "Right" -variable Popup(column,itemjustify,$C) -value right \
	-command [list $T column configure $C -itemjustify right]
    $m2 add radiobutton -label "Unspecified" -variable Popup(column,itemjustify,$C) -value none \
	-command [list $T column configure $C -itemjustify {}]

    set m2 [menu $m1.mJustify -tearoff no]
    $m1 add cascade -label "Justify" -menu $m2
    $m2 add radiobutton -label "Left" -variable Popup(column,justify,$C) -value left \
	-command [list $T column configure $C -justify left]
    $m2 add radiobutton -label "Center" -variable Popup(column,justify,$C) -value center \
	-command [list $T column configure $C -justify center]
    $m2 add radiobutton -label "Right" -variable Popup(column,justify,$C) -value right \
	-command [list $T column configure $C -justify right]

    set m2 [menu $m1.mLock -tearoff no]
    $m1 add cascade -label Lock -menu $m2
    $m2 add radiobutton -label "Left" -variable Popup(column,lock,$C) -value left \
	-command [list $T column configure $C -lock left]
    $m2 add radiobutton -label "None" -variable Popup(column,lock,$C) -value none \
	-command [list $T column configure $C -lock none]
    $m2 add radiobutton -label "Right" -variable Popup(column,lock,$C) -value right \
	-command [list $T column configure $C -lock right]

    $m1 add checkbutton -label "Resize" -variable Popup(column,resize,$C) \
	-command [list eval $T column configure $C -resize \$Popup(column,resize,$C)]
    $m1 add checkbutton -label "Squeeze" -variable Popup(column,squeeze,$C) \
	-command [list eval $T column configure $C -squeeze \$Popup(column,squeeze,$C)]
    $m1 add checkbutton -label "Tree Column" -variable Popup(column,treecolumn,$C) \
	-command [list eval $T configure -treecolumn "\[expr {\$Popup(column,treecolumn,$C) ? $C : {}}\]"]
    $m1 add checkbutton -label "Visible" -variable Popup(column,visible,$C) \
	-command [list eval $T column configure $C -visible \$Popup(column,visible,$C) \; \
	    TreeCtrl::TryEvent $T DemoColumnVisibility {} [list C $C] ]

    return $m1
}

proc AddBindTag {w tag} {

    if {[lsearch -exact [bindtags $w] $tag] == -1} {
	bindtags $w [concat [bindtags $w] $tag]
    }
    foreach child [winfo children $w] {
	AddBindTag $child $tag
    }
    return
}

MakeMainWindow

InitPics sky feather

proc ShowPopup {T x y X Y} {
    global Popup
    set Popup(T) $T
    $T identify -array id $x $y
    if {$id(where) ne ""} {
	if {$id(where) eq "header"} {
	    set H $id(header)
	    set C $id(column)
	    set Popup(header) $H
	    set Popup(column) $C
	    set Popup(arrow) [$T header cget $H $C -arrow]
	    set Popup(arrow,side) [$T header cget $H $C -arrowside]
	    set Popup(arrow,gravity) [$T header cget $H $C -arrowgravity]
	    set Popup(button) [$T header cget $H $C -button]
	    set Popup(header,justify) [$T header cget $H $C -justify]
	    set Popup(header,visible) [$T header cget $H -visible]

	    set Popup(header,drag,draw) [$T header dragcget $H -draw]
	    set Popup(header,drag,enable) [$T header dragcget $H -enable]

	    set Popup(column,expand,$C) [$T column cget $C -expand]
	    set Popup(column,resize,$C) [$T column cget $C -resize]
	    set Popup(column,squeeze,$C) [$T column cget $C -squeeze]
	    set Popup(column,itemjustify,$C) [$T column cget $C -itemjustify]
	    if {$Popup(column,itemjustify,$C) eq ""} { set Popup(column,itemjustify) none }
	    set Popup(column,justify,$C) [$T column cget $C -justify]
	    set Popup(column,lock,$C) [$T column cget $C -lock]
	    set Popup(column,treecolumn,$C) [expr {[$T column id tree] eq $C}]
	    $T.mColumn delete "Column"
	    destroy $T.mColumn.mColumnX
	    set m1 [MakeColumnSubmenu $T $C $T.mColumn "X"]
	    $T.mColumn add cascade -label "Column" -menu $m1

	    set m $T.mColumn.mHeaderColumn.mSpan
	    $m delete 0 end
	    if {[$T column compare $C == tail]} {
		$m add checkbutton -label 1 -variable Popup(span)
		set Popup(span) 1
	    } else {
		set lock [$T column cget $C -lock]
		set last [expr {[$T column order "last lock $lock"] - [$T column order $C] + 1}]
		for {set i 1} {$i <= $last} {incr i} {
		    set break [expr {!(($i - 1) % 20)}]
		    $m add radiobutton -label $i -command "$T header span $H $C $i" \
			-variable Popup(span) -value $i -columnbreak $break
		}
		set Popup(span) [$T header span $H $C]
	    }

	    tk_popup $T.mColumn $X $Y
	    return
	}
    }
    set menu $T.mTree
    set m $menu.mCollapse
    $m delete 0 end
    $m add command -label "All" -command {$Popup(T) item collapse all}
    if {$id(where) eq "item"} {
	set item $id(item)
	$m add command -label "Item $item" -command "$T item collapse $item"
	$m add command -label "Item $item (recurse)" -command "$T item collapse $item -recurse"
    }
    set m $menu.mExpand
    $m delete 0 end
    $m add command -label "All" -command {$Popup(T) item expand all}
    if {$id(where) eq "item"} {
	set item $id(item)
	$m add command -label "Item $item" -command "$T item expand $item"
	$m add command -label "Item $item (recurse)" -command "$T item expand $item -recurse"
    }
    foreach option {data display displaydelay enable span textlayout} {
	set Popup(debug,$option) [$T debug cget -$option]
    }
    set Popup(bgimg) [$T cget -backgroundimage]
    set Popup(bgimganchor) [$T cget -bgimageanchor]
    set Popup(bgimgopaque) [$T cget -bgimageopaque]
    set Popup(bgimgscrollx) [string trim [$T cget -bgimagescroll] y]
    set Popup(bgimgscrolly) [string trim [$T cget -bgimagescroll] x]
    set Popup(bgimgtilex) [string trim [$T cget -bgimagetile] y]
    set Popup(bgimgtiley) [string trim [$T cget -bgimagetile] x]
    if {$Popup(bgimg) eq ""} { set Popup(bgimg) none }
    set Popup(bgmode) [$T cget -backgroundmode]
    set Popup(buttontracking) [$T cget -buttontracking]
    set Popup(columnresizemode) [$T cget -columnresizemode]
    set Popup(doublebuffer) [$T cget -doublebuffer]
    set Popup(linestyle) [$T cget -linestyle]
    set Popup(orient) [$T cget -orient]
    set Popup(selectmode) [$T cget -selectmode]
    set Popup(xscrollsmoothing) [$T cget -xscrollsmoothing]
    set Popup(yscrollsmoothing) [$T cget -yscrollsmoothing]
    set Popup(showbuttons) [$T cget -showbuttons]
    set Popup(showheader) [$T cget -showheader]
    set Popup(showlines) [$T cget -showlines]
    set Popup(showroot) [$T cget -showroot]
    set Popup(showrootbutton) [$T cget -showrootbutton]
    set Popup(showrootchildbuttons) [$T cget -showrootchildbuttons]
    set Popup(showrootlines) [$T cget -showrootlines]

    set m $menu.mColumns
    eval destroy [winfo children $m]
    $m delete 0 end
    foreach C [$T column list] {
	set break [expr {!([$T column order $C] % 20)}]
	set m1 [MakeColumnSubmenu $T $C $m]
#	set m1 [menu $m.mColumn$C -postcommand [list PostColumnSubmenu $T $C $m]]
	$m add cascade -menu $m1 -columnbreak $break \
	    -label "Column $C \"[$T column cget $C -text]\" \[[$T column cget $C -image]\]"

	set Popup(column,expand,$C) [$T column cget $C -expand]
	set Popup(column,justify,$C) [$T column cget $C -justify]
	set Popup(column,itemjustify,$C) [$T column cget $C -itemjustify]
	if {$Popup(column,itemjustify,$C) eq ""} { set Popup(column,itemjustify,$C) none }
	set Popup(column,lock,$C) [$T column cget $C -lock]
	set Popup(column,squeeze,$C) [$T column cget $C -squeeze]
	set Popup(column,visible,$C) [$T column cget $C -visible]
	set Popup(treecolumn,$C) no
	if {[$T column id tree] ne ""} {
	    set Popup(treecolumn,$C) [$T column compare [$T column id tree] == $C]
	}
    }

    set m $menu.mHeaders
    eval destroy [winfo children $m]
    $m delete 0 end
    foreach H [$T header id all] {
	set m1 [MakeHeaderSubmenu $T $H $m]
	$m add cascade -menu $m1 -label "Header $H"
	set Popup(header,visible,$H) [$T header cget $H -visible]
    }

    set m $menu.mItemWrap
    $m delete 0 end
    $m add command -label "All Off" -command {$Popup(T) item configure all -wrap off}
    $m add command -label "All On" -command {$Popup(T) item configure all -wrap on}
    if {$id(where) eq "item"} {
	set item $id(item)
	if {[$T item cget $item -wrap]} {
	    $m add command -label "Item $item Off" -command "$T item configure $item -wrap off"
	} else {
	    $m add command -label "Item $item On" -command "$T item configure $item -wrap on"
	}
    }

    set m $menu.mSpan
    $m delete 0 end
    if {$id(where) eq "item" && $id(column) ne ""} {
	set item $id(item)
	set column $id(column)
	set lock [$T column cget $column -lock]
	for {set i 1} {$i <= [$T column order "last lock $lock"] - [$T column order $column] + 1} {incr i} {
	    set break [expr {!(($i - 1) % 20)}]
	    $m add radiobutton -label $i -command "$T item span $item $column $i" \
		-variable Popup(span) -value $i -columnbreak $break
	}
	set Popup(span) [$T item span $item $column]
    } else {
	$m add command -label "no item column" -state disabled
    }

    set Popup(usetheme) [$T cget -usetheme]
    tk_popup $menu $X $Y
    return
}

# Allow "scan" bindings
if {$::thisPlatform eq "windows"} {
    bind [DemoList] <Control-ButtonPress-3> { }
}

#
# List of demos
#
proc InitDemoList {} {
    global DemoCmd
    global DemoFile

    set t .f1.t
    $t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
    $t element create e2 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	-showfocus yes
    $t style create s1
    $t style elements s1 {e2 e1}
    # Tk listbox has linespace + 1 height
    $t style layout s1 e2 -union [list e1] -ipadx 2 -ipady {0 1} -iexpand e

    $t column configure C0 -itemstyle s1

    #	"Picture Catalog" DemoPictureCatalog
    #	"Picture Catalog 2" DemoPictureCatalog2
    #	"Folder Contents (Vertical)" DemoExplorerFilesV
    foreach {label command file} [list \
	"Random $::RandomN Items" DemoRandom random.tcl \
	"Random $::RandomN Items, Button Images" DemoRandom2 random.tcl \
	"Outlook Express (Folders)" DemoOutlookFolders outlook-folders.tcl \
	"Outlook Express (Newsgroup)" DemoOutlookNewsgroup outlook-newgroup.tcl \
	"Explorer (Details, Win98)" DemoExplorerDetails explorer.tcl \
	"Explorer (Details, Win7)" DemoExplorerDetailsWin7 explorer.tcl \
	"Explorer (List)" DemoExplorerList explorer.tcl \
	"Explorer (Large icons, Win98)" DemoExplorerLargeIcons explorer.tcl \
	"Explorer (Large icons, Win7)" DemoExplorerLargeIconsWin7 explorer.tcl \
	"Explorer (Small icons)" DemoExplorerSmallIcons explorer.tcl \
	"Internet Options" DemoInternetOptions www-options.tcl \
	"Help Contents" DemoHelpContents help.tcl \
	"Layout" DemoLayout layout.tcl \
	"MailWasher" DemoMailWasher mailwasher.tcl \
	"Bitmaps" DemoBitmaps bitmaps.tcl \
	"iMovie" DemoIMovie imovie.tcl \
	"iMovie (Wrap)" DemoIMovieWrap imovie.tcl \
	"Firefox Privacy" DemoFirefoxPrivacy firefox.tcl \
	"Textvariable" DemoTextvariable textvariable.tcl \
	"Big List" DemoBigList biglist.tcl \
	"Column Spanning" DemoSpan span.tcl \
	"My Computer" DemoMyComputer mycomputer.tcl \
	"Column Locking" DemoColumnLock column-lock.tcl \
	"Gradients" DemoGradients gradients.tcl \
	"Gradients II" DemoGradients2 gradients2.tcl \
	"Gradients III" DemoGradients3 gradients3.tcl \
	"Headers" DemoHeaders headers.tcl \
	"Table" DemoTable table.tcl \
	] {
	set item [$t item create]
	$t item lastchild root $item
	#		$t item style set $item C0 s1
	$t item text $item C0 $label
	set DemoCmd($item) $command
	set DemoFile($item) $file
    }
    $t yview moveto 0.0
    return
}

InitDemoList

proc TimerStart {} {
    if {[info tclversion] < 8.5} {
	return [set ::gStartTime [clock clicks -milliseconds]]
    }
    return [set ::gStartTime [clock microseconds]]
}

proc TimerStop {{startTime ""}} {
    if {[info tclversion] < 8.5} {
	set endTime [clock clicks -milliseconds]
	if {$startTime eq ""} { set startTime $::gStartTime }
	return [format "%.2g" [expr {($endTime - $startTime) / 1000.0}]]
    }
    set endTime [clock microseconds]
    if {$startTime eq ""} { set startTime $::gStartTime }
    return [format "%.2g" [expr {($endTime - $startTime) / 1000000.0}]]
}

proc DemoSet {namespace file} {
    DemoClear
    TimerStart
    uplevel #0 ${namespace}::Init [DemoList]
    dbwin "set list in [TimerStop] seconds\n"
    [DemoList] xview moveto 0
    [DemoList] yview moveto 0
    update
    DisplayStylesInList
    SourceWindow::ShowSource $file
    catch {
	if {[winfo ismapped .styleEditor]} {
	    StyleEditor::SetListOfStyles
	}
    }
    AddBindTag [DemoList] TagIdentify
    return
}

.f1.t notify bind .f1.t <Selection> {
    if {%c == 1} {
	set item [%T selection get 0]
	DemoSet $DemoCmd($item) $DemoFile($item)
    }
}

proc DisplayStylesInList {} {

    set T [DemoList]
    set t .f4.t

    # Create elements and styles the first time this is called
    if {[llength [$t style names]] == 0} {
	$t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
	$t element create e2 text -fill [list $::SystemHighlightText {selected focus} "" {selected !focus} blue {}]
	$t element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	    -showfocus yes

	$t style create s1
	$t style elements s1 {e3 e1}
	$t style layout s1 e3 -union [list e1] -ipadx 1 -ipady {0 1}

	$t style create s2
	$t style elements s2 {e3 e1 e2}
	$t style layout s2 e1 -padx {0 4}
	$t style layout s2 e3 -union [list e1 e2] -ipadx 1 -ipady {0 1}
    }

    # Clear the list
    $t item delete all

    # One item for each element in the demo list
    foreach elem [lsort -dictionary [$T element names]] {
	set item [$t item create -button yes -open no]
	$t item style set $item C0 s1
	$t item text $item C0 "Element $elem ([$T element type $elem])"

	# One item for each configuration option for this element
	foreach list [$T element configure $elem] {
	    lassign $list name x y default current
	    set item2 [$t item create]
	    if {[string equal $default $current]} {
		$t item style set $item2 C0 s1
		$t item element configure $item2 C0 e1 -text [list $name $current]
	    } else {
		$t item style set $item2 C0 s2
		$t item element configure $item2 C0 e1 -text $name + e2 -text [list $current]
	    }
	    $t item lastchild $item $item2
	}
	$t item lastchild root $item
    }

    # One item for each style in the demo list
    foreach style [lsort -dictionary [$T style names]] {
	set item [$t item create -button yes -open no]
	$t item style set $item C0 s1
	$t item text $item C0 "Style $style"

	# One item for each element in the style
	foreach elem [$T style elements $style] {
	    set item2 [$t item create -button yes -open no]
	    $t item style set $item2 C0 s1
	    $t item text $item2 C0 "Element $elem ([$T element type $elem])"

	    # One item for each layout option for this element in this style
	    foreach {option value} [$T style layout $style $elem] {
		set item3 [$t item create]
		#				$t item hasbutton $item3 no
		$t item style set $item3 C0 s1
		$t item text $item3 C0 [list $option $value]
		$t item lastchild $item2 $item3
	    }
	    $t item lastchild $item $item2
	}
	$t item lastchild root $item
    }

    $t xview moveto 0
    $t yview moveto 0
    return
}

proc DisplayStylesInItem {item} {

    set T [DemoList]
    set t .f3.t
    $t column configure C0 -text "Styles in item [$T item id $item]"

    # Create elements and styles the first time this is called
    if {[llength [$t style names]] == 0} {
	$t element create e1 text -fill [list $::SystemHighlightText {selected focus}]
	$t element create e2 text -fill [list $::SystemHighlightText {selected focus} "" {selected !focus} blue {}]
	$t element create e3 rect -fill [list $::SystemHighlight {selected focus} gray {selected !focus}] \
	    -showfocus yes

	$t style create s1
	$t style elements s1 {e3 e1}
	$t style layout s1 e3 -union [list e1] -ipadx {1 2} -ipady {0 1}

	$t style create s2
	$t style elements s2 {e3 e1 e2}
	$t style layout s2 e1 -padx {0 4}
	$t style layout s2 e3 -union [list e1 e2] -ipadx 1 -ipady {0 1}
    }

    # Clear the list
    $t item delete all

    # One item for each item-column
    foreach style [$T item style set $item] column [$T column list] {
	set item2 [$t item create -open no]
	$t item style set $item2 C0 s1
	if {$style ne ""} {
	    $t item element configure $item2 C0 e1 \
		-text "Column $column: Style $style"
	} else {
	    $t item element configure $item2 C0 e1 \
		-text "Column $column: no style"
	}

	# One item for each element in this style
	if {[string length $style]} {
	    set button 0
	    foreach elem [$T item style elements $item $column] {
		set button 1
		set item3 [$t item create -button yes -open no]
		$t item style set $item3 C0 s1
		$t item element configure $item3 C0 e1 \
		    -text "Element $elem ([$T element type $elem])"

		# One item for each configuration option in this element
		foreach list [$T item element configure $item $column $elem] {
		    lassign $list name x y default current
		    set item4 [$t item create]
		    set masterDefault [$T element cget $elem $name]
		    set sameAsMaster [string equal $masterDefault $current]
		    if {!$sameAsMaster && ![string length $current]} {
			set sameAsMaster 1
			set current $masterDefault
		    }

		    if {$sameAsMaster} {
			$t item style set $item4 C0 s1
			$t item element configure $item4 C0 e1 -text "$name [list $current]"
		    } else {
			$t item style set $item4 C0 s2
			$t item element configure $item4 C0 e1 -text $name + e2 -text [list $current]
		    }
		    $t item lastchild $item3 $item4
		}
		$t item lastchild $item2 $item3
	    }
	    if {$button} {
		$t item configure $item2 -button yes
	    }
	}
	$t item lastchild root $item2
    }
    $t xview moveto 0
    $t yview moveto 0

    return
}

# When one item is selected in the demo list, display the styles in that item.
# See DemoClear for why the tag "DontDelete" is used.
set DisplayStylesInItem(item) ""
set MouseIsDown 0
bind [DemoList] <ButtonPress-1> {
    set MouseIsDown 1
}
bind [DemoList] <ButtonRelease-1> {
    set MouseIsDown 0
    if {$DisplayStylesInItem(item) ne ""} {
	DisplayStylesInItem $DisplayStylesInItem(item)
	set DisplayStylesInItem(item) ""
    }
}
[DemoList] notify bind DontDelete <Selection> {
    if {%c == 1} {
	if {$MouseIsDown} {
	    set DisplayStylesInItem(item) [%T selection get 0]
	} else {
	    DisplayStylesInItem [%T selection get 0]
	}
    }
}

# Move columns when ColumnDrag-receive is generated.
# See DemoClear for why the tag "DontDelete" is used.
[DemoList] notify bind DontDelete <ColumnDrag-receive> {
    %T column move %C %b
}

proc DemoClear {} {

    set T [DemoList]

    # Delete all the items (except the root item, it never gets deleted).
    $T item delete all

    # Delete all the headers (except the first header, it never gets deleted).
    $T header delete all

    # Clear all bindings on the demo list added by the previous demo.
    # The bindings are removed from the tag $T only. For those
    # bindings that should not be deleted we use the tag DontDelete.
    # DontDelete is not a special name it just needs to be different
    # than $T.
    $T notify unbind $T

    # Clear all run-time states
    eval $T header state undefine [$T header state names]
    eval $T item state undefine [$T item state names]

    # Clear the styles-in-item list
    .f3.t item delete all

    # Delete columns in demo list
    $T column delete all

    # Delete all styles in demo list
    eval $T style delete [$T style names]

    # Delete all elements in demo list
    eval $T element delete [$T element names]

    # Delete -window windows
    foreach child [winfo children $T] {
	if {[string equal $child $T.mTree] || [string equal $child $T.mColumn]} continue
	destroy $child
    }

    # Restore defaults to marquee
    $T marquee configure -fill {} -outline {} -outlinewidth 1

    # Delete gradients
    eval $T gradient delete [$T gradient names]

    $T item configure root -button no -wrap no
    $T item expand root

    # Restore header defaults
    foreach spec [$T header configure 0] {
	if {[llength $spec] == 2} continue
	lassign $spec name x y default current
	$T header configure all $name $default
    }

    # Restore some happy defaults to the demo list
    foreach spec [$T configure] {
	if {[llength $spec] == 2} continue
	lassign $spec name x y default current
	$T configure $name $default
    }
    $T configure -background white
    $T configure -borderwidth [expr {$::tileFull ? 0 : 6}]
    $T configure -font DemoFont
    if {[Platform unix]} {
	$T configure -headerfont DemoFont
    }
    $T configure -highlightthickness [expr {$::tileFull ? 0 : 3}]
    $T configure -relief ridge

    switch -- [$T theme platform] {
	visualstyles {
	    $T theme setwindowtheme ""
	}
    }

    # Restore defaults to the tail column
    foreach spec [$T column configure tail] {
	if {[llength $spec] == 2} continue
	lassign $spec name x y default current
	$T column configure tail $name $default
    }

    # Enable drag-and-drop column reordering. This also requires the
    # <ColumnDrag> event be installed.
    $T header dragconfigure -enable yes
    $T header dragconfigure all -enable yes -draw yes

    # Re-active the column drag-and-drop binding in case the previous demo
    # deactivated it.
    $T notify configure DontDelete <ColumnDrag-receive> -active yes

    # Restore default bindings to the demo list
    bindtags $T [list $T TreeCtrl [winfo toplevel $T] all DisplayStylesInItemBindTag]

    catch {destroy $T.entry}
    catch {destroy $T.text}

    return
}

#
# Demo: Picture catalog
#
proc DemoPictureCatalog {} {

    set T [DemoList]

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode multiple -orient horizontal -wrap window \
	-yscrollincrement 50 -showheader no

    $T column create

    $T element create elemTxt text -fill {SystemHighlightText {selected focus}}
    $T element create elemSelTxt rect -fill {SystemHighlight {selected focus}}
    $T element create elemSelImg rect -outline {SystemHighlight {selected focus}} \
	-outlinewidth 4
    $T element create elemImg rect -fill gray -width 80 -height 120

    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemSelImg elemImg elemSelTxt elemTxt}
    $T style layout $S elemSelImg -union elemImg -ipadx 6 -ipady 6
    $T style layout $S elemSelTxt -union elemTxt
    $T style layout $S elemImg -pady {0 6}

    for {set i 1} {$i <= 10} {incr i} {
	set I [$T item create]
	$T item style set $I 0 $S
	$T item text $I 0 "Picture #$i"
	$T item lastchild root $I
    }

    return
}

#
# Demo: Picture catalog
#
proc DemoPictureCatalog2 {} {

    set T [DemoList]

    $T configure -showroot no -showbuttons no -showlines no \
	-selectmode multiple -orient horizontal -wrap window \
	-yscrollincrement 50 -showheader no

    $T column create

    $T element create elemTxt text -fill {SystemHighlightText {selected focus}} \
	-justify left -wrap word -lines 3
    $T element create elemSelTxt rect -fill {SystemHighlight {selected focus}}
    $T element create elemSelImg rect -outline {SystemHighlight {selected focus}} \
	-outlinewidth 4
    $T element create elemImg rect -fill gray

    set S [$T style create STYLE -orient vertical]
    $T style elements $S {elemSelImg elemImg elemSelTxt elemTxt}
    $T style layout $S elemSelImg -union elemImg \
	-ipadx 6 -ipady 6
    $T style layout $S elemSelTxt -union elemTxt
    $T style layout $S elemImg -pady {0 6}
    $T style layout $S elemImg -expand n
    $T style layout $S elemTxt -expand s

    for {set i 1} {$i <= 10} {incr i} {
	set I [$T item create]
	$T item style set $I 0 $S
	$T item text $I 0 "This is\nPicture\n#$i"
	$T item element configure $I 0 elemImg -width [expr int(20 + rand() * 80)] \
	    -height [expr int(20 + rand() * 120)]
	$T item lastchild root $I
    }

    return
}




proc CursorWindow {} {
    set w .cursors
    if {[winfo exists $w]} {
	destroy $w
    }
    toplevel $w
    set c [canvas $w.canvas -background white -width [expr {50 * 10}] \
	       -highlightthickness 0 -borderwidth 0]
    pack $c -expand yes -fill both
    set cursors {
	X_cursor
	arrow
	based_arrow_down
	based_arrow_up
	boat
	bogosity
	bottom_left_corner
	bottom_right_corner
	bottom_side
	bottom_tee
	box_spiral
	center_ptr
	circle
	clock
	coffee_mug
	cross
	cross_reverse
	crosshair
	diamond_cross
	dot
	dotbox
	double_arrow
	draft_large
	draft_small
	draped_box
	exchange
	fleur
	gobbler
	gumby
	hand1
	hand2
	heart
	icon
	iron_cross
	left_ptr
	left_side
	left_tee
	leftbutton
	ll_angle
	lr_angle
	man
	middlebutton
	mouse
	pencil
	pirate
	plus
	question_arrow
	right_ptr
	right_side
	right_tee
	rightbutton
	rtl_logo
	sailboat
	sb_down_arrow
	sb_h_double_arrow
	sb_left_arrow
	sb_right_arrow
	sb_up_arrow
	sb_v_double_arrow
	shuttle
	sizing
	spider
	spraycan
	star
	target
	tcross
	top_left_arrow
	top_left_corner
	top_right_corner
	top_side
	top_tee
	trek
	ul_angle
	umbrella
	ur_angle
	watch
	xterm
    }
    set col 0
    set row 0
    foreach cursor $cursors {
	set x [expr {$col * 50}]
	set y [expr {$row * 40}]
	$c create rectangle $x $y [expr {$x + 50}] [expr {$y + 40}] \
	    -fill gray90 -outline black -width 2 -tags $cursor.rect
	$c create text [expr {$x + 50 / 2}] [expr {$y + 4}] -text $cursor \
	    -anchor n -width 42 -tags $cursor.text
	if {[incr col] == 10} {
	    set col 0
	    incr row
	}
	$c bind $cursor.rect <Enter> "
			$c configure -cursor $cursor
			$c itemconfigure $cursor.rect -fill linen
		"
	$c bind $cursor.rect <Leave> "
			$c configure -cursor {}
			$c itemconfigure $cursor.rect -fill gray90
		"
	$c bind $cursor.text <Enter> "
			$c configure -cursor $cursor
		"
	$c bind $cursor.text <Leave> "
			$c configure -cursor {}
		"
    }
    $c configure -height [expr {($row + 1) * 40}]
    return
}

# A little screen magnifier
if {[llength [info commands loupe]]} {

    namespace eval LoupeWindow {
	variable Priv
	set Priv(zoom) 2
	set Priv(x) 0
	set Priv(y) 0
	set Priv(auto) 1
	set Priv(afterId) ""
	set Priv(image) ::LoupeWindow::Image
	set Priv(delay) 500
    }

    proc LoupeWindow::After {} {
	variable Priv
	set x [winfo pointerx .]
	set y [winfo pointery .]
	if {$Priv(auto) || ($Priv(x) != $x) || ($Priv(y) != $y)} {
	    set w [image width $Priv(image)]
	    set h [image height $Priv(image)]
	    loupe $Priv(image) $x $y $w $h $Priv(zoom)
	    set Priv(x) $x
	    set Priv(y) $y
	}
	set Priv(afterId) [after $Priv(delay) LoupeWindow::After]
	return
    }

    proc LoupeWindow::Init {} {
	variable Priv
	set w [toplevel .loupe]
	wm title $w "TreeCtrl Magnifier"
	wm withdraw $w
	if {[Platform macintosh macosx]} {
	    wm geometry $w +6+30
	} else {
	    wm geometry $w -0+0
	}
	image create photo $Priv(image) -width 280 -height 150
	pack [label $w.label -image $Priv(image) -borderwidth 1 -relief sunken] \
	    -expand yes -fill both

	set f [frame $w.zoom -borderwidth 0]
	radiobutton $f.r1 -text "1x" -variable ::LoupeWindow::Priv(zoom) -value 1
	radiobutton $f.r2 -text "2x" -variable ::LoupeWindow::Priv(zoom) -value 2
	radiobutton $f.r4 -text "4x" -variable ::LoupeWindow::Priv(zoom) -value 4
	radiobutton $f.r8 -text "8x" -variable ::LoupeWindow::Priv(zoom) -value 8
	pack $f.r1 $f.r2 $f.r4 $f.r8 -side left
	pack $f -side bottom -anchor center

	# Resize the image with the window
	bind LoupeWindow <Configure> {
	    LoupeWindow::ResizeImage %w %h
	}
	bindtags $w.label [concat [bindtags .loupe] LoupeWindow]

	wm protocol $w WM_DELETE_WINDOW "LoupeWindow::ToggleWindowVisibility"
	return
    }

    proc LoupeWindow::ResizeImage {w h} {
	variable Priv
	set w [expr {$w - 2}]
	set h [expr {$h - 2}]
	if {$w != [$Priv(image) cget -width] ||
	    $h != [$Priv(image) cget -height]} {
	    $Priv(image) configure -width $w -height $h
	    loupe $Priv(image) $Priv(x) $Priv(y) $w $h $Priv(zoom)
	}
	return
    }

    proc LoupeWindow::ToggleWindowVisibility {} {
	variable Priv
	set w .loupe
	if {![winfo exists $w]} {
	    LoupeWindow::Init
	}
	if {[winfo ismapped $w]} {
	    after cancel $Priv(afterId)
	    wm withdraw $w
	} else {
	    After
	    wm deiconify $w
	    raise $w
	}
	return
    }
}

proc RandomPerfTest {} {
    set ::RandomN 15000
    DemoSet DemoRandom random.tcl
    [DemoList] item expand all
    [DemoList] style layout styFolder elemTxtName -squeeze x
    [DemoList] style layout styFile elemTxtName -squeeze x
    [DemoList] elem conf elemTxtName -lines 1
    update
    puts [time {[DemoList] colu conf 0 -width 160 ; update}]
    return
}

