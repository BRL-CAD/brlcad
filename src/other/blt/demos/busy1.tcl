#!../src/bltwish

package require BLT
# --------------------------------------------------------------------------
# Starting with Tcl 8.x, the BLT commands are stored in their own 
# namespace called "blt".  The idea is to prevent name clashes with
# Tcl commands and variables from other packages, such as a "table"
# command in two different packages.  
#
# You can access the BLT commands in a couple of ways.  You can prefix
# all the BLT commands with the namespace qualifier "blt::"
#  
#    blt::graph .g
#    blt::table . .g -resize both
# 
# or you can import all the command into the global namespace.
#
#    namespace import blt::*
#    graph .g
#    table . .g -resize both
#
# --------------------------------------------------------------------------

if { $tcl_version >= 8.0 } {
    namespace import blt::*
    namespace import -force blt::tile::*
}
source scripts/demo.tcl

#
# Script to test the "busy" command.
# 

#
# General widget class resource attributes
#
option add *Button.padX 	10
option add *Button.padY 	2
option add *Scale.relief 	sunken
#option add *Scale.orient	horizontal
option add *Entry.relief 	sunken
option add *Frame.borderWidth 	2

set visual [winfo screenvisual .] 
if { $visual == "staticgray"  || $visual == "grayscale" } {
    set activeBg black
    set normalBg white
    set bitmapFg black
    set bitmapBg white
    option add *f1.background 		white
} else {
    set activeBg red
    set normalBg springgreen
    set bitmapFg blue
    set bitmapBg green
    option add *Button.background       khaki2
    option add *Button.activeBackground khaki1
    option add *Frame.background        khaki2
    option add *f2.tile		textureBg
#    option add *Button.tile		textureBg

    option add *releaseButton.background 		limegreen
    option add *releaseButton.activeBackground 	springgreen
    option add *releaseButton.foreground 		black

    option add *holdButton.background 		red
    option add *holdButton.activeBackground	pink
    option add *holdButton.foreground 		black
    option add *f1.background 		springgreen
}

#
# Instance specific widget options
#
option add *f1.relief 		sunken
option add *f1.background 	$normalBg
option add *testButton.text 	"Test"
option add *quitButton.text 	"Quit"
option add *newButton.text 	"New\nButton"
option add *holdButton.text 	"Hold"
option add *releaseButton.text 	"Release"
option add *buttonLabel.text	"Buttons"
option add *entryLabel.text	"Entries"
option add *scaleLabel.text	"Scales"
option add *textLabel.text	"Text"

bind keepRaised <Visibility> { raise %W } 

proc KeepRaised { w } {
    bindtags $w keepRaised
}

set file ./images/chalk.gif
image create photo textureBg -file $file

#
# This never gets used; it's reset by the Animate proc. It's 
# here to just demonstrate how to set busy window options via
# the host window path name
#
#option add *f1.busyCursor 	bogosity 

#
# Counter for new buttons created by the "New button" button
#
set numWin 0

#
# Create two frames. The top frame will be the host window for the
# busy window.  It'll contain widgets to test the effectiveness of
# the busy window.  The bottom frame will contain buttons to 
# control the testing.
#
frame .f1
frame .f2

#
# Create some widgets to test the busy window and its cursor
#
label .buttonLabel
button .testButton -command { 
    puts stdout "Not busy." 
}
button .quitButton -command { exit }
entry .entry 
scale .scale
text .text -width 20 -height 4

#
# The following buttons sit in the lower frame to control the demo
#
button .newButton -command {
    global numWin
    incr numWin
    set name button#${numWin}
    button .f1.$name -text "$name" \
	-command [list .f1 configure -bg blue]
    table .f1 \
	.f1.$name $numWin+3,0 -padx 10 -pady 10
}

button .holdButton -command {
    if { [busy isbusy .f1] == "" } {
        global activeBg
	.f1 configure -bg $activeBg
    }
    busy .f1 
    focus -force . 
}

button .releaseButton -command {
    if { [busy isbusy .f1] == ".f1" } {
        busy release .f1
    }
    global normalBg
    .f1 configure -bg $normalBg
}

#
# Notice that the widgets packed in .f1 and .f2 are not their children
#
table .f1 \
    0,0		.testButton \
    1,0		.scale		-fill y \
    0,1		.entry		-fill x \
    1,1		.text		-fill both \
    2,0		.quitButton	-cspan 2

table .f2 \
    0,0		.holdButton \
    0,1		.releaseButton  \
    0,2		.newButton

table configure .f1 \
    .testButton .scale .entry .quitButton -padx 10 -pady 10
table configure .f2 \
    .newButton .holdButton .releaseButton -padx 10 -pady 4 -reqwidth 1.i

table configure .f1 r0 r2 -resize none
table configure .f2 r* -resize none

#
# Finally, realize and map the top level window
#
table . \
    0,0		.f1		-fill both \
    1,0		.f2		-fill both

table configure . r1 -resize none

table configure .f1 c1 -weight 2.0

# Initialize a list of bitmap file names which make up the animated 
# fish cursor. The bitmap mask files have a "m" appended to them.

set bitmapList { 
    left left1 mid right1 right 
}

#
# Simple cursor animation routine: Uses the "after" command to 
# circulate through a list of cursors every 0.075 seconds. The
# first pass through the cursor list may appear sluggish because 
# the bitmaps have to be read from the disk.  Tk's cursor cache
# takes care of it afterwards.
#
proc StartAnimation { widget count } {
    global bitmapList
    set prefix bitmaps/fish/[lindex $bitmapList $count]
    set cursor [list @${prefix}.xbm ${prefix}m.xbm blue green ]
    busy configure $widget -cursor $cursor

    incr count
    set limit [llength $bitmapList]
    if { $count >= $limit } {
	set count 0
    }
    global afterId
    set afterId($widget) [after 125 StartAnimation $widget $count]
}

proc StopAnimation { widget } {    
    global afterId
    after cancel $afterId($widget)
}

proc TranslateBusy { window } {
    set widget [string trimright $window "_Busy"]
    if { $widget != "." } {
        set widget [string trimright $widget "."]
    }
    return $widget
}

if { [info exists tcl_platform] && $tcl_platform(platform) == "unix" } {
    bind Busy <Map> { 
	StartAnimation [TranslateBusy %W] 0
    }
    bind Busy <Unmap> { 
	StopAnimation  [TranslateBusy %W] 
    }
}

#
# For testing, allow the top level window to be resized 
#
wm min . 0 0

#
# Force the demo to stay raised
#
raise .
KeepRaised .

