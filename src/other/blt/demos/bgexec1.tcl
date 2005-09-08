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



#set animate(colors) { #ff8813 #ffaa13 #ffcc13 #ffff13 #ffcc13 #ffaa13 #ff8813 }
bitmap define blt.5 [bitmap data blt.3]
bitmap define blt.6 [bitmap data blt.2]
bitmap define blt.7 [bitmap data blt.1]


set interval 200

proc AnimateBitmap { index } {
    global interval afterId
    if { ![winfo exists .logo] } {
	return
    }
    if { $index >= 0 } {
	.logo configure -bitmap blt.$index
	incr index
	if { $index >= 7 } {
	    set index 0
	}
	set afterId [after $interval "AnimateBitmap $index"]
    }
}

set length 80

option add *text.yScrollCommand { .vscroll set }
option add *text.relief sunken
option add *text.width $length
option add *text.height 10
option add *text.borderWidth 2
option add *vscroll.command { .text yview }
option add *vscroll.minSlider 4p
option add *quit.command { exit }
option add *quit.text { quit }
option add *stop.command { set bgStatus {} }
option add *stop.text { stop }
option add *logo.relief sunken
option add *logo.padX 4
option add *title.text "Virtual Memory Statistics"
option add *title.font -*-Helvetica-Bold-R-*-*-14-*-*-*-*-*-*-*

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *text.background lightblue
    option add *text.foreground blue
    option add *quit.background red
    option add *quit.foreground white
    option add *stop.background yellow
    option add *stop.foreground navyblue
    option add *logo.background beige
    option add *logo.foreground brown 
}

# Create widgets
text .text 
scrollbar .vscroll 
button .quit
button .stop
label .logo 
label .title

# Layout widgets in table
table . \
    .title      0,0 -columnspan 4 \
    .text 	1,0 -columnspan 3 \
    .vscroll 	1,3 -fill y \
    .logo 	2,0 -anchor w -padx 10 -reqheight .6i -pady 4 \
    .stop 	2,1 \
    .quit  	2,2 

set buttonWidth 1i
table configure . c1 c2 -width 1i
table configure . c3 -resize none
table configure . .stop .quit -reqwidth $buttonWidth -anchor e
table configure . .title .text -fill both

wm min . 0 0

proc DisplayStats { data } {
    .text insert end "$data\n"
    set textlen [expr int([.text index end])]
    scan [.vscroll get] "%s %s %s %s" total window first last
    if { $textlen > $total } {
	.text yview [expr $textlen-$window]
    }
    update idletasks
}

set bgStatus {}

AnimateBitmap 0

#
# Pick a command that 
#    1) periodically writes output and
#    2) flushes output each time.
#
set command { vmstat 1 }
#set command { netstat -c }

catch { eval "bgexec bgStatus -onoutput DisplayStats $command" } 

# Turn off animation by canceling any pending after task.
if { [info exists afterId] } {
    after cancel $afterId
}
