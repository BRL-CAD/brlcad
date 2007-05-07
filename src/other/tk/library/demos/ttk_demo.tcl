#
# $Id$
#
# Tile widget set -- widget demo
#
package require Tk 8.5

eval destroy [winfo children .]		;# in case script is re-sourced

### Load auxilliary scripts.
#
variable demodir [file dirname [info script]]
lappend auto_path . $demodir

source [file join $demodir ttk_iconlib.tcl]
source [file join $demodir ttk_repeater.tcl]

# This forces an update of the available packages list.
# It's required for package names to find the themes in demos/themes/*.tcl
eval [package unknown] Tcl [package provide Tcl]

### Global options and bindings.
#
option add *Button.default normal
option add *Text.background white
option add *Entry.background white
option add *tearOff false

# See toolbutton.tcl.
#
option add *Toolbar.relief groove
option add *Toolbar.borderWidth 2
option add *Toolbar.Button.Pad 2
option add *Toolbar.Button.default disabled
option add *Toolbar*takeFocus 0

# ... for debugging:
bind all <ButtonPress-3> { set ::W %W }
bind all <Control-ButtonPress-3> { focus %W }

# Stealth feature:
#
if {![catch {package require Img 1.3}]} {
    bind all <Control-Shift-Alt-KeyPress-S> screenshot
    proc screenshot {} {
	image create photo ScreenShot -format window -data .
	bell
	# Gamma looks off if we use PNG ...
	# Looks even worse if we use GIF ...
	ScreenShot write screenshot.png -format png
	image delete ScreenShot
	bell
    }
}

### Global data.
#

# The descriptive names of the builtin themes:
#
set ::THEMELIST {
    default  	"Default"
    classic  	"Classic"
    alt      	"Revitalized"
    winnative	"Windows native"
    xpnative	"XP Native"
    aqua	"Aqua"
}
array set ::THEMES $THEMELIST;

# Add in any available loadable themes:
#
foreach name [ttk::themes] {
    if {![info exists ::THEMES($name)]} {
	lappend THEMELIST $name [set ::THEMES($name) [string totitle $name]]
    }
}

# Generate icons (see also: iconlib.tcl):
#
foreach {icon data} [array get ::ImgData]  {
    set ::ICON($icon) [image create photo -data $data]
}

variable ROOT "."
variable BASE [ttk::frame .base]
pack $BASE -side top -expand true -fill both

array set ::V {
    COMPOUND	top
    CONSOLE	0
    MENURADIO1	One
    PBMODE	determinate
    SELECTED 	1
    CHOICE	2
    SCALE	50
    VSCALE	0
}

### Utilities.
#

## foreachWidget varname widget script --
#	Execute $script with $varname set to each widget in the hierarchy.
#
proc foreachWidget {varname Q script} {
    upvar 1 $varname w
    while {[llength $Q]} {
    	set QN [list]
	foreach w $Q {
	    uplevel 1 $script
	    foreach child [winfo children $w] {
		lappend QN $child
	    }
	}
	set Q $QN
    }
}

## sbstub $sb -- stub -command option for a scrollbar.
#	Updates the scrollbar's position.
#
proc sbstub {sb cmd number {units units}} { sbstub.$cmd $sb $number $units }
proc sbstub.moveto {sb number _} { $sb set $number [expr {$number + 0.5}] }
proc sbstub.scroll {sb number units} {
    if {$units eq "pages"} {
    	set delta 0.2
    } else {
	set delta 0.05
    }
    set current [$sb get]
    set new0 [expr {[lindex $current 0] + $delta*$number}]
    set new1 [expr {[lindex $current 1] + $delta*$number}]
    $sb set $new0 $new1
}

## sbset $sb -- auto-hide scrollbar
#	Scrollable widget -[xy]scrollcommand prefix.
#	Sets the scrollbar, auto-hides/shows.
#	Scrollbar must be controlled by the grid geometry manager.
#
proc sbset {sb first last} {
    if {$first <= 0 && $last >= 1} {
    	grid remove $sb
    } else {
        grid $sb
    }
    $sb set $first $last
}

## scrolled -- create a widget with attached scrollbars.
#
proc scrolled {class w args} {
    set sf "${w}_sf"

    frame $sf
    eval [linsert $args 0 $class $w]
    scrollbar $sf.hsb -orient horizontal -command [list $w xview]
    scrollbar $sf.vsb -orient vertical -command [list $w yview]

    configure.scrolled $sf $w
    return $sf
}

## ttk::scrolled -- create a widget with attached Ttk scrollbars.
#
proc ttk::scrolled {class w args} {
    set sf "${w}_sf"

    ttk::frame $sf
    eval [linsert $args 0 $class $w]
    ttk::scrollbar $sf.hsb -orient horizontal -command [list $w xview]
    ttk::scrollbar $sf.vsb -orient vertical -command [list $w yview]

    configure.scrolled $sf $w
    return $sf
}

## configure.scrolled -- common factor of [scrolled] and [ttk::scrolled]
#
proc configure.scrolled {sf w} {
    $w configure -xscrollcommand [list $sf.hsb set]
    $w configure -yscrollcommand [list $sf.vsb set]

    grid $w -in $sf -row 0 -column 0 -sticky nwse
    grid $sf.hsb -row 1 -column 0 -sticky we
    grid $sf.vsb -row 0 -column 1 -sticky ns

    grid columnconfigure $sf 0 -weight 1
    grid rowconfigure $sf 0 -weight 1
}

### Toolbars.
#
proc makeToolbars {} {
    set buttons [list open new save]
    set checkboxes [list bold italic]

    #
    # Ttk toolbar:
    #
    set tb [ttk::frame $::BASE.tbar_styled -class Toolbar]
    set i 0
    foreach icon $buttons {
	set b [ttk::button $tb.tb[incr i] \
	       -text $icon -image $::ICON($icon) -compound $::V(COMPOUND) \
	       -style Toolbutton]
	grid $b -row 0 -column $i -sticky news
    }
    ttk::separator $tb.sep -orient vertical
    grid $tb.sep -row 0 -column [incr i] -sticky news -padx 2 -pady 2
    foreach icon $checkboxes {
	set b [ttk::checkbutton $tb.cb[incr i] \
		-variable ::V($icon) \
	       -text $icon -image $::ICON($icon) -compound $::V(COMPOUND) \
	       -style Toolbutton]
	grid $b -row 0 -column $i -sticky news
    }

    ttk::menubutton $tb.compound \
    	-text "toolbar" -image $::ICON(file) -compound $::V(COMPOUND)
    $tb.compound configure -menu [makeCompoundMenu $tb.compound.menu]
    grid $tb.compound -row 0 -column [incr i] -sticky news

    grid columnconfigure $tb [incr i] -weight 1

    #
    # Standard toolbar:
    #
    set tb [frame $::BASE.tbar_orig -class Toolbar]
    set i 0
    foreach icon $buttons {
	set b [button $tb.tb[incr i] \
		  -text $icon -image $::ICON($icon) -compound $::V(COMPOUND) \
		  -relief flat -overrelief raised]
	grid $b -row 0 -column $i -sticky news
    }
    frame $tb.sep -borderwidth 1 -width 2 -relief sunken
    grid $tb.sep -row 0 -column [incr i] -sticky news  -padx 2 -pady 2
    foreach icon $checkboxes {
	set b [checkbutton $tb.cb[incr i] -variable ::V($icon) \
		  -text $icon -image $::ICON($icon) -compound $::V(COMPOUND) \
		  -indicatoron false \
		  -selectcolor {} \
		  -relief flat \
		  -overrelief raised \
		  -offrelief flat]
	grid $b -row 0 -column $i -sticky news
    }

    menubutton $tb.compound \
    	-text "toolbar" -image $::ICON(file) -compound $::V(COMPOUND) \
	-indicatoron true
    $tb.compound configure -menu [makeCompoundMenu $tb.compound.menu]
    grid $tb.compound -row 0 -column [incr i] -sticky news

    grid columnconfigure $tb [incr i] -weight 1
}

#
# Toolbar -compound control:
#
proc makeCompoundMenu {menu} {
    variable compoundStrings {text image none top bottom left right center}
    menu $menu
    foreach string $compoundStrings {
	$menu add radiobutton \
	    -label [string totitle $string] \
	    -variable ::V(COMPOUND) -value $string \
	    -command changeToolbars ;
    }
    return $menu
}

proc changeToolbars {} {
    foreachWidget w [list $::BASE.tbar_styled $::BASE.tbar_orig] {
	catch { $w configure -compound $::V(COMPOUND) }
    }
}

makeToolbars

### Theme control panel.
#
proc makeThemeControl {c} {
    ttk::labelframe $c -text "Theme"
    foreach {theme name} $::THEMELIST {
	set b [ttk::radiobutton $c.s$theme -text $name \
		   -variable ::ttk::currentTheme -value $theme \
		   -command [list ttk::setTheme $theme]]
	pack $b -side top -expand false -fill x
	if {[lsearch -exact [package names] ttk::theme::$theme] == -1} {
	    $c.s$theme state disabled
	}
    }
    return $c
}
makeThemeControl $::BASE.control

### Notebook widget.
#
set nb [ttk::notebook $::BASE.nb]
ttk::notebook::enableTraversal $nb

### Main demo pane.
#
# Side-by comparison of Ttk vs. core widgets.
#


set pw [ttk::panedwindow $nb.client -orient horizontal]
$nb add $pw -text "Demo" -underline 0 -padding 6
set l [ttk::labelframe $pw.l -text "Themed" -padding 6 -underline 1]
set r [labelframe $pw.r -text "Standard" -padx 6 -pady 6]
$pw add $l -weight 1; $pw add $r -weight 1

## menubuttonMenu -- demo menu for menubutton widgets.
#
proc menubuttonMenu {menu} {
    menu $menu
    foreach dir {above below left right flush} {
	$menu add command -label [string totitle $dir] \
	    -command [list [winfo parent $menu] configure -direction $dir]
    }
    $menu add cascade -label "Submenu" -menu [set submenu [menu $menu.submenu]]
    $submenu add command -label "Subcommand 1"
    $submenu add command -label "Subcommand 2"
    $submenu add command -label "Subcommand 3"
    $menu add separator
    $menu add command -label "Quit"  -command [list destroy .]

    return $menu
}

## Main demo pane - themed widgets.
#
ttk::checkbutton $l.cb -text "Checkbutton" -variable ::V(SELECTED) -underline 2
ttk::radiobutton $l.rb1 -text "One" -variable ::V(CHOICE) -value 1 -underline 0
ttk::radiobutton $l.rb2 -text "Two" -variable ::V(CHOICE) -value 2
ttk::radiobutton $l.rb3 -text "Three" -variable ::V(CHOICE) -value 3 -under 0
ttk::button $l.button -text "Button" -underline 0

ttk::menubutton $l.mb -text "Menubutton" -underline 2
$l.mb configure -menu [menubuttonMenu $l.mb.menu]

set ::entryText "Entry widget"
ttk::entry $l.e -textvariable ::entryText
$l.e selection range 6 end 

set ltext [ttk::scrolled text $l.t -width 12 -height 5 -wrap none]

grid $l.cb  -sticky ew
grid $l.rb1 -sticky ew
grid $l.rb2 -sticky ew
grid $l.rb3 -sticky ew
grid $l.button -sticky ew -padx 2 -pady 2
grid $l.mb -sticky ew -padx 2 -pady 2
grid $l.e -sticky ew -padx 2 -pady 2
grid $ltext -sticky news

grid columnconfigure $l 0 -weight 1
grid rowconfigure    $l 7 -weight 1 ; # text widget (grid is a PITA)

## Main demo pane - core widgets.
#
checkbutton $r.cb -text "Checkbutton" -variable ::V(SELECTED) 
radiobutton $r.rb1 -text "One" -variable ::V(CHOICE) -value 1 
radiobutton $r.rb2 -text "Two" -variable ::V(CHOICE) -value 2 -underline 1
radiobutton $r.rb3 -text "Three" -variable ::V(CHOICE) -value 3
button $r.button -text "Button"
menubutton $r.mb -text "Menubutton" -underline 3 -takefocus 1
$r.mb configure -menu [menubuttonMenu $r.mb.menu]
# Add -indicatoron control:
set ::V(rmbIndicatoron) [$r.mb cget -indicatoron]
$r.mb.menu insert 0 checkbutton -label "Indicator?" \
    -variable ::V(rmbIndicatoron) \
    -command "$r.mb configure -indicatoron \$::V(rmbIndicatoron)" ;
$r.mb.menu insert 1 separator

entry $r.e -textvariable ::entryText

set rtext [scrolled text $r.t -width 12 -height 5 -wrap none]

grid $r.cb -sticky ew
grid $r.rb1 -sticky ew
grid $r.rb2 -sticky ew
grid $r.rb3 -sticky ew
grid $r.button -sticky ew -padx 2 -pady 2
grid $r.mb -sticky ew -padx 2 -pady 2
grid $r.e -sticky ew -padx 2 -pady 2
grid $rtext -sticky news

grid columnconfigure $r 0 -weight 1
grid rowconfigure    $r 7 -weight 1 ; # text widget

#
# Add some text to the text boxes:
#

set cb $::BASE.tbar_orig.cb5
set txt "checkbutton $cb \\\n"
foreach copt [$cb configure] {
    if {[llength $copt] == 5} {
	append txt "  [lindex $copt 0] [lindex $copt 4] \\\n"
    }
}
append txt "  ;\n"

$l.t insert end $txt
$r.t insert end $txt

### Scales and sliders pane.
#
proc scales.pane {scales} {
    ttk::frame $scales

    ttk::panedwindow $scales.pw -orient horizontal
    set l [ttk::labelframe $scales.styled -text "Themed" -padding 6]
    set r [labelframe $scales.orig -text "Standard" -padx 6 -pady 6]

    ttk::scale $l.scale -orient horizontal -from 0 -to 100 -variable ::V(SCALE)
    ttk::scale $l.vscale -orient vertical -from 100 -to 0 -variable ::V(VSCALE)
    ttk::progressbar $l.progress -orient horizontal -maximum 100
    ttk::progressbar $l.vprogress -orient vertical -maximum 100
    if {1} {
	$l.scale configure -command [list $l.progress configure -value]
	$l.vscale configure -command [list $l.vprogress configure -value]
    } else {
	# This would also work, but the Tk scale widgets
	# in the right hand pane cause some interference when
	# in autoincrement/indeterminate mode.
	#
    	$l.progress configure -variable ::V(SCALE)
        $l.vprogress configure -variable ::V(VSCALE)
    }

    $l.scale set 50
    $l.vscale set 50

    ttk::label $l.lmode -text "Progress bar mode:"
    ttk::radiobutton $l.pbmode0 -variable ::V(PBMODE) \
    	-text determinate -value determinate -command [list pbMode $l]
    ttk::radiobutton $l.pbmode1 -variable ::V(PBMODE) \
    	-text indeterminate -value indeterminate -command [list pbMode $l]
    proc pbMode {l} {
	variable V
	$l.progress configure -mode $V(PBMODE)
	$l.vprogress configure -mode $V(PBMODE)
    }

    ttk::button $l.start -text "Start" -command [list pbStart $l]
    proc pbStart {l} {
	set ::V(PBMODE) indeterminate; pbMode $l
	$l.progress start 10
	$l.vprogress start
    }

    ttk::button $l.stop -text "Stop" -command [list pbStop $l]
    proc pbStop {l} {
    	$l.progress stop
    	$l.vprogress stop
    }

    grid $l.scale -columnspan 2 -sticky ew
    grid $l.progress -columnspan 2 -sticky ew
    grid $l.vscale $l.vprogress -sticky nws

    grid $l.lmode -sticky we -columnspan 2
    grid $l.pbmode0 -sticky we -columnspan 2
    grid $l.pbmode1 -sticky we -columnspan 2
    grid $l.start -sticky we -columnspan 2
    grid $l.stop  -sticky we -columnspan 2

    grid columnconfigure $l 0 -weight 1
    grid columnconfigure $l 1 -weight 1

    grid rowconfigure $l 99 -weight 1

    scale $r.scale -orient horizontal -from 0 -to 100 -variable ::V(SCALE)
    scale $r.vscale -orient vertical -from 100 -to 0 -variable ::V(VSCALE)
    grid $r.scale -sticky news
    grid $r.vscale -sticky nws

    grid rowconfigure $r 99 -weight 1
    grid columnconfigure $r 0 -weight 1

    ##
    $scales.pw add $l -weight 1
    $scales.pw add $r -weight 1
    pack $scales.pw -expand true -fill both

    return $scales
}
$nb add [scales.pane $nb.scales] -text Scales -sticky nwes -padding 6

### Combobox demo pane.
#
proc combobox.pane {cbf} {
    ttk::frame $cbf
    set values [list abc def ghi jkl mno pqr stu vwx yz]
    pack \
	[ttk::combobox $cbf.cb1 -values $values -textvariable ::COMBO] \
	[ttk::combobox $cbf.cb2 -values $values -textvariable ::COMBO ] \
    -side top -padx 2 -pady 2 -expand false -fill x;
    $cbf.cb2 configure -state readonly
    $cbf.cb1 current 3
    return $cbf
}
$nb add [combobox.pane $nb.combos] -text "Combobox" -underline 7

### Treeview widget demo pane.
#
proc tree.pane {w} {
    ttk::frame $w
    ttk::scrollbar $w.vsb -command [list $w.t yview]
    ttk::treeview $w.t -columns [list Class] \
	-padding 4 \
	-yscrollcommand [list sbset $w.vsb] 

    grid $w.t $w.vsb -sticky nwse
    grid columnconfigure $w 0 -weight 1
    grid rowconfigure $w 0 -weight 1
    grid propagate $w 0

    #
    # Add initial tree node: 
    # Later nodes will be added in <<TreeviewOpen>> binding.
    #
    $w.t insert {} 0 -id . -text "Main Window" -open 0 \
	-values [list [winfo class .]]
    $w.t heading \#0 -text "Widget"
    $w.t heading Class -text "Class"
    bind $w.t <<TreeviewOpen>> [list fillTree $w.t]

    return $w
}

# fillTree -- <<TreeviewOpen>> binding for tree widget. 
#
proc fillTree {tv} {
    set id [$tv focus]
    if {![winfo exists $id]} {
	$tv delete $id
	return
    }

    #
    # Replace tree item children with current list of child windows.
    #
    $tv delete [$tv children $id]
    set children [winfo children $id]
    foreach child $children {
	$tv insert $id end -id $child -text [winfo name $child] -open 0 \
	    -values [list [winfo class $child]]
	if {[llength [winfo children $child]]} {
	    # insert dummy child to show [+] indicator
	    $tv insert $child end
	}
    }
}

if {[llength [info commands ttk::treeview]]} {
    $nb add [tree.pane $nb.tree] -text "Tree" -sticky news
}

### Other demos.
#
$nb add [ttk::frame $nb.others] -text "Others" -underline 4

set Timers(StateMonitor) {}
set Timers(FocusMonitor) {}

set others $::BASE.nb.others

ttk::label $others.m -justify left -wraplength 300
bind ShowDescription <Enter> { $BASE.nb.others.m configure -text $Desc(%W) }
bind ShowDescription <Leave> { $BASE.nb.others.m configure -text "" }

foreach {command label description} {
    trackStates "Widget states..." 
    "Display/modify widget state bits"

    scrollbarResizeDemo  "Scrollbar resize behavior..."
    "Shows how Ttk and standard scrollbars differ when they're sized too large"

    trackFocus "Track keyboard focus..." 
    "Display the name of the widget that currently has focus"

    repeatDemo "Repeating buttons"
    "Demonstrates custom classes (see demos/repeater.tcl)"

} {
    set b [ttk::button $others.$command -text $label -command $command]
    set Desc($b) $description
    bindtags $b [lreplace [bindtags $b] end 0 ShowDescription]

    pack $b -side top -expand false -fill x -padx 6 -pady 6
}

pack $others.m -side bottom -expand true -fill both


### Scrollbar resize demo.
#
proc scrollbarResizeDemo {} {
    set t .scrollbars
    destroy $t
    toplevel $t ; wm geometry $t 200x200
    frame $t.f -height 200
    grid \
	[ttk::scrollbar $t.f.tsb -command [list sbstub $t.f.tsb]] \
	[scrollbar $t.f.sb -command [list sbstub $t.f.sb]] \
    -sticky news

    $t.f.sb set 0 0.5	;# prevent backwards-compatibility mode for old SB

    grid columnconfigure $t.f 0 -weight 1
    grid columnconfigure $t.f 1 -weight 1
    grid rowconfigure $t.f 0 -weight 1
    pack $t.f -expand true -fill both
}

### Track focus demo.
#
proc trackFocus {} {
    global Focus
    set t .focus
    destroy $t
    toplevel $t 
    wm title $t "Keyboard focus"
    set i 0
    foreach {label variable} {
	"Focus widget:" Focus(Widget)
	"Class:" Focus(WidgetClass)
	"Next:"  Focus(WidgetNext)
	"Grab:"  Focus(Grab)
	"Status:"  Focus(GrabStatus)
    } {
	grid [ttk::label $t.l$i -text $label -anchor e] \
	     [ttk::label $t.v$i -textvariable $variable \
		-width 40 -anchor w -relief groove] \
	-sticky ew;
	incr i
    }
    grid columnconfigure $t 1 -weight 1
    grid rowconfigure $t $i -weight 1

    bind $t <Destroy> {after cancel $Timers(FocusMonitor)}
    FocusMonitor
}

proc FocusMonitor {} {
    global Focus

    set Focus(Widget) [focus]
    if {$::Focus(Widget) ne ""} {
	set Focus(WidgetClass) [winfo class $Focus(Widget)]
	set Focus(WidgetNext) [tk_focusNext $Focus(Widget)]
    } else {
	set Focus(WidgetClass) [set Focus(WidgetNext) ""]
    }

    set Focus(Grab) [grab current]
    if {$Focus(Grab) ne ""} {
	set Focus(GrabStatus) [grab status $Focus(Grab)]
    } else {
	set Focus(GrabStatus) ""
    }

    set ::Timers(FocusMonitor) [after 200 FocusMonitor]
}

### Widget states demo.
#
variable Widget .tbar_styled.tb1

bind all <Control-Shift-ButtonPress-1> { TrackWidget %W ; break }

proc TrackWidget {w} {
    set ::Widget $w ;
    if {[winfo exists .states]} {
	UpdateStates
    } else {
    	trackStates 
    }
}

variable states [list \
    active disabled focus pressed selected readonly \
    background alternate invalid]

proc trackStates {} {
    variable states
    set t .states
    destroy $t; toplevel $t ; wm title $t "Widget states"

    set tf [ttk::frame $t.f] ; pack $tf -expand true -fill both

    ttk::label $tf.info -text "Press Control-Shift-Button-1 on any widget"

    ttk::label $tf.lw -text "Widget:" -anchor e -relief groove
    ttk::label $tf.w -textvariable ::Widget -anchor w -relief groove

    grid $tf.info - -sticky ew -padx 6 -pady 6
    grid $tf.lw $tf.w -sticky ew

    foreach state $states {
	ttk::checkbutton $tf.s$state \
	    -text $state \
	    -variable ::State($state) \
	    -command [list ChangeState $state] ;
	grid x $tf.s$state -sticky nsew
    }

    grid columnconfigure $tf 1 -weight 1

    grid x [ttk::frame $tf.cmd] -sticky nse
    grid x \
    	[ttk::button $tf.cmd.close -text Close -command [list destroy $t]] \
    	 -padx 4 -pady {6 4};
    grid columnconfigure $tf.cmd 0 -weight 1

    bind $t <KeyPress-Escape> [list event generate $tf.cmd.close <<Invoke>>]
    bind $t <Destroy> { after cancel $::Timers(StateMonitor) }
    StateMonitor
}

proc StateMonitor {} {
    if {$::Widget ne ""} { UpdateStates }
    set ::Timers(StateMonitor) [after 200 StateMonitor]
}

proc UpdateStates {}  {
    variable states
    variable State
    variable Widget

    foreach state $states {
	if {[catch {set State($state) [$Widget instate $state]}]} {
	    # Not a Ttk widget:
	    .states.f.s$state state disabled
	} else {
	    .states.f.s$state state !disabled
	}
    }
}

proc ChangeState {state} {
    variable State
    variable Widget
    if {$Widget ne ""} {
	if {$State($state)} { 
	    $Widget state $state
	} else {
	    $Widget state !$state
	}
    }
}

### Repeating button demo.
#

proc repeatDemo {} {
    set top .repeatDemo
    if {![catch { wm deiconify $top ; raise $top }]} { return }
    toplevel $top
    wm title $top "Repeating button"
    keynav::enableMnemonics $top

    set f [ttk::frame .repeatDemo.f]
    ttk::button $f.b -class Repeater -text "Press and hold" \
    	-command [list $f.p step]
    ttk::progressbar $f.p -orient horizontal -maximum 10

    ttk::separator $f.sep -orient horizontal
    set cmd [ttk::frame $f.cmd]
    pack \
    	[ttk::button $cmd.close -text Close -command [list destroy $top]]  \
    -side right -padx 6;

    pack $f.cmd -side bottom -expand false -fill x -padx 6 -pady 6
    pack $f.sep -side bottom -expand false -fill x -padx 6 -pady 6
    pack $f.b -side left -expand false -fill none -padx 6 -pady 6
    pack $f.p -side right -expand true -fill x -padx 6 -pady 6

    $f.b configure -underline 0
    $cmd.close configure -underline 0
    bind $top <KeyPress-Escape> [list event generate $cmd.close <<Invoke>>]

    pack $f -expand true -fill both
}


### Command box.
#
set cmd [ttk::frame $::BASE.command]
ttk::button $cmd.close -text Close -underline 0 -command [list destroy .]
ttk::button $cmd.help -text Help -command showHelp

proc showHelp {} {
    if {![winfo exists .helpDialog]} {
	lappend detail "Tk version $::tk_version"
	lappend detail "Ttk library: $::ttk::library"
	ttk::dialog .helpDialog -type ok -icon info \
	    -message "Ttk demo" -detail [join $detail \n]
    }
}

grid x $cmd.close $cmd.help -pady 6 -padx 6
grid columnconfigure $cmd 0 -weight 1

## Status bar (to demonstrate size grip)
#
set statusbar [ttk::frame $BASE.statusbar]
pack [ttk::sizegrip $statusbar.grip] -side right -anchor se

## Accelerators:
#
bind $::ROOT <KeyPress-Escape>	[list event generate $cmd.close <<Invoke>>]
bind $::ROOT <<Help>>		[list event generate $cmd.help <<Invoke>>]
keynav::enableMnemonics $::ROOT
keynav::defaultButton $cmd.help

### Menubar.
#
set menu [menu $::BASE.menu]
$::ROOT configure -menu $menu
$menu add cascade -label "File" -underline 0 -menu [menu $menu.file]
$menu.file add command -label "Open" -underline 0 \
    -compound left -image $::ICON(open)
$menu.file add command -label "Save" -underline 0 \
    -compound left -image $::ICON(save)
$menu.file add separator
$menu.file add checkbutton -label "Checkbox" -underline 0 \
    -variable ::V(SELECTED)
$menu.file add cascade -label "Choices" -underline 1 \
    -menu [menu $menu.file.choices]
foreach {label value} {One 1 Two 2 Three 3} {
    $menu.file.choices add radiobutton \
    	-label $label -variable ::V(CHOICE) -value $value
}

$menu.file insert end separator
if {[tk windowingsystem] ne "x11"} {
    $menu.file insert end checkbutton -label Console -underline 5 \
	-variable ::V(CONSOLE) -command toggleconsole
    proc toggleconsole {} {
	if {$::V(CONSOLE)} {console show} else {console hide}
    }
}
$menu.file add command -label "Exit" -underline 1 \
    -command [list event generate $cmd.close <<Invoke>>]

# Add Theme menu.
#
proc makeThemeMenu {menu} {
    menu $menu
    foreach {theme name} $::THEMELIST {
	$menu add radiobutton -label $name \
	    -variable ::ttk::currentTheme -value $theme \
	    -command [list ttk::setTheme $theme]
	if {[lsearch -exact [package names] ttk::theme::$theme] == -1} {
	    $menu entryconfigure end -state disabled
	}
    }
    return $menu
}

$menu add cascade -label "Theme" -underline 3 -menu [makeThemeMenu $menu.theme]

### Main window layout.
#

pack $BASE.statusbar -side bottom -expand false -fill x
pack $BASE.command -side bottom -expand false -fill x
pack $BASE.tbar_styled -side top -expand false -fill x
pack $BASE.tbar_orig -side top -expand false -fill x
pack $BASE.control -side left -expand false -fill y -padx 6 -pady 6
pack $BASE.nb -side left -expand true -fill both -padx 6 -pady 6

wm title $ROOT "Ttk demo"
wm iconname $ROOT "Ttk demo"
update; wm deiconify $ROOT
