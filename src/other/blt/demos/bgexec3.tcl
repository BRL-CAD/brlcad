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

bitmap define blt.0 {{40 40} {
    00 00 00 00 00 00 fc 07 00 00 00 04 08 00 00 00 04 04 00 00 00 e4 03 00
    00 00 64 fe 07 00 00 64 02 04 00 00 e4 03 04 00 00 64 7e 02 00 00 64 1a
    02 00 00 e4 1b 01 00 00 04 1a 01 00 00 04 1a 01 00 00 fc 1b 02 00 00 0c
    1a 02 00 00 0c 02 04 00 00 0c 02 f4 03 80 ed fe 07 04 e0 0c 00 20 09 10
    0c 00 00 12 10 0c 00 00 10 30 00 00 00 19 d0 03 00 00 14 b0 fe ff ff 1b
    50 55 55 55 0d e8 aa aa aa 16 e4 ff ff ff 2f f4 ff ff ff 27 d8 ae aa bd
    2d 6c 5f d5 67 1b bc f3 7f d0 36 f8 01 10 cc 1f e0 45 8e 92 0f b0 32 41
    43 0b d0 cf 3c 7c 0d b0 aa c2 ab 0a 60 55 55 55 05 c0 ff ab aa 03 00 00
    fe ff 00 00 00 00 00 00}
}

bitmap define blt.1 {{40 40} {
    00 00 00 00 00 00 fc 07 00 00 00 04 08 00 00 00 04 04 00 00 00 e4 ff 0f
    00 00 64 06 08 00 00 64 06 08 00 00 e4 ff 04 00 00 64 36 04 00 00 64 36
    02 00 00 e4 37 02 00 00 04 34 02 00 00 04 34 04 00 00 fc 35 04 00 00 0c
    04 08 00 00 0c 04 08 00 00 0c fc ef 03 80 ed 01 00 04 e0 0c 00 20 09 10
    0c 00 00 12 10 0c 00 00 10 30 00 00 00 19 d0 03 00 00 14 b0 fe ff ff 1b
    50 55 55 55 0d e8 aa aa aa 16 e4 ff ff ff 2f f4 ff ff ff 27 d8 ae aa bd
    2d 6c 5f d5 67 1b bc f3 7f d0 36 f8 01 10 cc 1f e0 45 8e 92 0f b0 32 41
    43 0b d0 cf 3c 7c 0d b0 aa c2 ab 0a 60 55 55 55 05 c0 ff ab aa 03 00 00
    fe ff 00 00 00 00 00 00}
}

bitmap define blt.2 {{40 40} {
    00 00 00 00 00 00 fc 0f 00 00 00 04 10 00 00 00 04 10 00 00 00 e4 fb 3f
    00 00 64 0e 20 00 00 64 0e 20 00 00 e4 fb 13 00 00 64 ce 10 00 00 64 ce
    08 00 00 e4 cb 08 00 00 04 c8 08 00 00 04 c8 10 00 00 fc cf 10 00 00 0c
    08 20 00 00 0c 08 20 00 00 0c f8 bf 03 80 ed 03 00 04 e0 0c 00 20 09 10
    0c 00 00 12 10 0c 00 00 10 30 00 00 00 19 d0 03 00 00 14 b0 fe ff ff 1b
    50 55 55 55 0d e8 aa aa aa 16 e4 ff ff ff 2f f4 ff ff ff 27 d8 ae aa bd
    2d 6c 5f d5 67 1b bc f3 7f d0 36 f8 01 10 cc 1f e0 45 8e 92 0f b0 32 41
    43 0b d0 cf 3c 7c 0d b0 aa c2 ab 0a 60 55 55 55 05 c0 ff ab aa 03 00 00
    fe ff 00 00 00 00 00 00}
}

bitmap define blt.3 {{40 40} {
    00 00 00 00 00 00 fc 0f 00 00 00 04 f0 ff 00 00 04 00 80 00 00 e4 03 80
    00 00 64 d6 4f 00 00 64 16 43 00 00 e4 13 23 00 00 64 16 23 00 00 64 16
    23 00 00 e4 13 43 00 00 04 70 43 00 00 04 00 80 00 00 fc 0f 80 00 00 0c
    f0 ff 00 00 0c 00 00 00 00 0c f8 ff 03 80 ed 07 00 04 e0 0c 00 20 09 10
    0c 00 00 12 10 0c 00 00 10 30 00 00 00 19 d0 03 00 00 14 b0 fe ff ff 1b
    50 55 55 55 0d e8 aa aa aa 16 e4 ff ff ff 2f f4 ff ff ff 27 d8 ae aa bd
    2d 6c 5f d5 67 1b bc f3 7f d0 36 f8 01 10 cc 1f e0 45 8e 92 0f b0 32 41
    43 0b d0 cf 3c 7c 0d b0 aa c2 ab 0a 60 55 55 55 05 c0 ff ab aa 03 00 00
    fe ff 00 00 00 00 00 00}
}

bitmap define blt.4 {{40 40} {
    00 00 00 00 00 00 fc ff ff 03 00 04 00 00 02 00 04 00 00 02 00 e4 33 3f
    01 00 64 36 0c 01 00 64 36 8c 00 00 e4 33 8c 00 00 64 36 8c 00 00 64 36
    0c 01 00 e4 f3 0d 01 00 04 00 00 02 00 04 00 00 02 00 fc ff ff 03 00 0c
    00 00 00 00 0c 00 00 00 00 0c f8 ff 03 80 ed 07 00 04 e0 0c 00 20 09 10
    0c 00 00 12 10 0c 00 00 10 30 00 00 00 19 d0 03 00 00 14 b0 fe ff ff 1b
    50 55 55 55 0d e8 aa aa aa 16 e4 ff ff ff 2f f4 ff ff ff 27 d8 ae aa bd
    2d 6c 5f d5 67 1b bc f3 7f d0 36 f8 01 10 cc 1f e0 45 8e 92 0f b0 32 41
    43 0b d0 cf 3c 7c 0d b0 aa c2 ab 0a 60 55 55 55 05 c0 ff ab aa 03 00 00
    fe ff 00 00 00 00 00 00}
}

set program ../src/bltwish
if { [info exists tcl_platform ] } {
    puts stderr $tcl_platform(platform)
    if { $tcl_platform(platform) == "windows" } {
        set shells [glob C:/Program\ Files/Tcl/bin/tclsh8*.exe ]
  	set program [lindex $shells 0]
    }
}
if { ![file executable $program] } {
    error "Can't execute $program"
}
set command [list $program scripts/bgtest.tcl]
set animate(index) -1
set animate(interval) 200
#set animate(colors) { #ff8813 #ffaa13 #ffcc13 #ffff13 #ffcc13 #ffaa13 #ff8813 }
bitmap define blt.5 [bitmap data blt.3]
bitmap define blt.6 [bitmap data blt.2]
bitmap define blt.7 [bitmap data blt.1]

proc Animate {} {
    global animate
    if { [info commands .logo] != ".logo" } {
	set animate(index) 0
	return
    }
    if { $animate(index) >= 0 } {
	.logo configure -bitmap blt.$animate(index) 
	incr animate(index)
	if { $animate(index) >= 7 } {
	    set animate(index) 0
	}
	after $animate(interval) Animate
    }
}


proc InsertText { string tag } {
    .text insert end "$tag: " "" $string $tag
    set textlen [expr int([.text index end])]
    scan [.vscroll get] "%s %s %s %s" total window first last
    if { $textlen > $total } {
	.text yview [expr $textlen-$window]
    }
    update idletasks
    update
}

proc DisplayOutput { data } {
    InsertText "$data\n" stdout
}

proc DisplayErrors { data } {
    InsertText "$data\n" stderr
}

set length 80

option add *text.yScrollCommand { .vscroll set }
option add *text.relief sunken
option add *text.width 20
option add *text.height 10
option add *text.height 10
option add *text.borderWidth 2
option add *vscroll.command { .text yview }
option add *vscroll.minSlider 4p
option add *stop.command { set results {} }
option add *stop.text { stop }
option add *logo.relief sunken
option add *logo.padX 4
option add *title.text "Catching stdout and stderr"
option add *title.font -*-Helvetica-Bold-R-*-*-14-*-*-*-*-*-*-*

set visual [winfo screenvisual .] 
if { [string match *color $visual] } {
    option add *text.background white
    option add *text.foreground blue
    option add *stop.background yellow
    option add *stop.activeBackground yellow2
    option add *stop.foreground navyblue
    option add *start.activeBackground green2
    option add *start.background green
    option add *start.foreground navyblue
    option add *logo.background beige
    option add *logo.foreground brown 
}

proc Start { command } {
    global results animate
    .text delete 1.0 end
    if { $animate(index) < 0 } {
        set results {}
        set animate(index) 0
        eval "bgexec results -error barney -output fred -killsignal SIGINT \
	    -onoutput DisplayOutput -onerror DisplayErrors -linebuffered no \
		$command &"
        Animate
    }
}

proc Stop { } {
    global results animate
    set results {}
    set animate(index) -1
}

# Create widgets
text .text 
.text tag configure stdout -font { Courier-Bold 14 } -foreground green2
.text tag configure stderr -font  { Courier 14 } -foreground red2

scrollbar .vscroll 
button .start -text "Start" -command [list Start $command]
button .stop -text "Stop" -command Stop
label .logo  -bitmap blt.0
label .title

# Layout widgets in table
table . \
    .title      0,0 -columnspan 4 \
    .text 	1,0 -columnspan 3 \
    .vscroll 	1,3 -fill y \
    .logo 	2,0 -anchor w -padx 10 -reqheight .6i -pady 4 \
    .start 	2,1 \
    .stop 	2,2 

set buttonWidth 1i
table configure . c1 c2 -width 1i
table configure . c3 r0 r2 -resize none
table configure . .start .stop -reqwidth $buttonWidth -anchor e
table configure . .title .text -fill both

wm min . 0 0


