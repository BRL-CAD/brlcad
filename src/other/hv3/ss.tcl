# @(#) $Id$
#
# This script implements the "ss" application.  "ss" implements
# a presentation slide-show based on HTML slides.
# 
wm title . {Tk Slide Show}
wm iconname . {SlideShow}

package require Tkhtml

# Attempt to load the HTML widget if it isn't already part
# of the interpreter
#
if {[info command html]==""} {
  foreach f {
    ./tkhtml.so
    /usr/lib/tkhtml.so
    /usr/local/lib/tkhtml.so
    ./tkhtml.dll
  } {
    if {[file exists $f]} {
      if {[catch {load $f Tkhtml}]==0} break
    }
  }
}

# Pick the initial filename from the command line
#
set HtmlTraceMask 0
set file {}
foreach a $argv {
  if {[regexp {^debug=} $a]} {
    scan $a "debug=0x%x" HtmlTraceMask
  } else {
    set file $a
  }
}

# These are images to use with the actual image specified in a
# "<img>" markup can't be found.
#
image create photo biggray -data {
    R0lGODdhPAA+APAAALi4uAAAACwAAAAAPAA+AAACQISPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9o3n+s73/g8MCofEovGITCqXzKbzCY1Kp9Sq9YrNFgsAO///
}
image create photo smgray -data {
    R0lGODdhOAAYAPAAALi4uAAAACwAAAAAOAAYAAACI4SPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9m0VADv/
}

# Build the half-size view of the page
#
frame .mbar -bd 2 -relief raised
pack .mbar -side top -fill x
menubutton .mbar.help -text File -underline 0 -menu .mbar.help.m
pack .mbar.help -side left -padx 5
set m [menu .mbar.help.m]
$m add command -label Open -underline 0 -command Load
$m add command -label {Full Screen} -underline 0 -command FullScreen
$m add command -label Refresh -underline 0 -command Refresh
$m add separator
$m add command -label Exit -underline 1 -command exit

frame .h
pack .h -side top -fill both -expand 1
html .h.h \
  -width 512 -height 384 \
  -yscrollcommand {.h.vsb set} \
  -xscrollcommand {.f2.hsb set} \
  -padx 5 \
  -pady 9 \
  -formcommand FormCmd \
  -imagecommand "ImageCmd 1" \
  -scriptcommand ScriptCmd \
  -appletcommand AppletCmd \
  -hyperlinkcommand HyperCmd \
  -fontcommand pickFont \
  -appletcommand {runApplet small} \
  -bg white -tablerelief raised
.h.h token handler meta "Meta .h.h"

if {$HtmlTraceMask} {
  .h.h config -tablerelief flat
}

# This routine is called to pick fonts for the half-size window.
#
proc pickFont {size attrs} { 
  # puts "FontCmd: $size $attrs"
  set a [expr {-1<[lsearch $attrs fixed]?{courier}:{charter}}]
  set b [expr {-1<[lsearch $attrs italic]?{italic}:{roman}}]
  set c [expr {-1<[lsearch $attrs bold]?{bold}:{normal}}]
  set d [expr {int(12*pow(1.2,$size-4))}]
  list $a $d $b $c
}

# This routine is called to pick fonts for the fullscreen view.
#
set baseFontSize 24
proc pickFontFS {size attrs} { 
  # puts "FontCmd: $size $attrs"
  set a [expr {-1<[lsearch $attrs fixed]?{courier}:{charter}}]
  set b [expr {-1<[lsearch $attrs italic]?{italic}:{roman}}]
  set c [expr {-1<[lsearch $attrs bold]?{bold}:{normal}}]
  global baseFontSize
  set d [expr {int($baseFontSize*pow(1.2,$size-4))}]
  list $a $d $b $c
} 

proc HyperCmd {args} {
   puts "HyperlinkCommand: $args"
}

# This routine is called to run an applet
#
proc runApplet {size w arglist} {
  global AppletArg
  catch {unset AppletArg}
  foreach {name value} $arglist {
    set AppletArg([string tolower $name]) $value
  }
  if {![info exists AppletArg(src)]} return
  set src [.h.h resolve $AppletArg(src)]
  set AppletArg(window) $w
  set AppletArg(fontsize) $size
  if {[catch {uplevel #0 "source $src"} msg]} {
    puts "Applet error: $msg"
  }
}

proc FormCmd {n cmd args} {
 # puts "FormCmd: $n $cmd $args"
 #  switch $cmd {
 #   select -
 #   textarea -
 #   input {
 #     set w [lindex $args 0]
 #     label $w -image smgray
 #   }
 # }
}
proc ImageCmd {hs args} {
  global OldImages Images
  set fn [lindex $args 0]
  if {[info exists OldImages($fn)]} {
    set Images($fn) $OldImages($fn)
    unset OldImages($fn)
    return $Images($fn)
  }
  if {[catch {image create photo -file $fn} img]} {
    if {$hs} {
      return smallgray
    } else {
      return biggray
    }
  }
  if {$hs} {
    set img2 [image create photo]
    $img2 copy $img -subsample 2 2
    image delete $img
    set img $img2
  }
  if {[image width $img]*[image height $img]>20000} {
    global BigImages
    set b [image create photo -width [image width $img] \
           -height [image height $img]]
    set BigImages($b) $img
    set img $b
    after idle "MoveBigImage $b"
  }
  set Images($fn) $img
  return $img
}
proc MoveBigImage b {
  global BigImages
  if {![info exists BigImages($b)]} return
  $b copy $BigImages($b)
  image delete $BigImages($b)
  unset BigImages($b)
}
  
proc ScriptCmd {args} {
  # puts "ScriptCmd: $args"
}
proc AppletCmd {w arglist} {
  # puts "AppletCmd: w=$w arglist=$arglist"
  # label $w -text "The Applet $w" -bd 2 -relief raised
}

# This binding fires when there is a click on a hyperlink
#
proc HrefBinding {w x y} {
  set new [$w href $x $y]
  # puts "link to [list $new]";
  if {$new!=""} {
    ProcessUrl $new
  }
}
bind HtmlClip <1> {KeyPress %W Down}
bind HtmlClip <3> {KeyPress %W Up}
bind HtmlClip <2> {KeyPress %w Down}

# Clicking button three on the small screen causes the full-screen view
# to appear.
#
# bind .h.h.x <3> FullScreen

# Handle all keypress events on the screen
#
bind HtmlClip <KeyPress> {KeyPress %W %K}
proc KeyPress {w keysym} {
  global hotkey key_block
  if {[info exists key_block]} return
  set key_block 1
  after 250 {catch {unset key_block}}
  if {[info exists hotkey($keysym)]} {
    ProcessUrl $hotkey($keysym)
  }
  switch -- $keysym {
    Escape {
      if {[winfo exists .fs]} {FullScreenOff} {FullScreen}
    }
  }
}


# Finish building the half-size screen
#
pack .h.h -side left -fill both -expand 1
scrollbar .h.vsb -orient vertical -command {.h.h yview}
pack .h.vsb -side left -fill y

frame .f2
pack .f2 -side top -fill x
frame .f2.sp -width [winfo reqwidth .h.vsb] -bd 2 -relief raised
pack .f2.sp -side right -fill y
scrollbar .f2.hsb -orient horizontal -command {.h.h xview}
pack .f2.hsb -side top -fill x

#proc FontCmd {args} {
#  puts "FontCmd: $args"
#  return {Times 12}
#}
#proc ResolverCmd {args} {
#  puts "Resolver: $args"
#  return [lindex $args 0]
#}

set lastDir [pwd]
proc Load {} {
  set filetypes {
    {{Html Files} {.html .htm}}
    {{All Files} *}
  }
  global lastDir htmltext
  set f [tk_getOpenFile -initialdir $lastDir -filetypes $filetypes]
  if {$f!=""} {
    LoadFile $f
    set lastDir [file dirname $f]
  }
}

# Clear the screen.
#
proc Clear {} {
  global Images OldImages hotkey
  if {[winfo exists .fs.h]} {set w .fs.h} {set w .h.h}
  $w clear
  catch {unset hotkey}
  ClearBigImages
  ClearOldImages
  foreach fn [array names Images] {
    set OldImages($fn) $Images($fn)
  }
  catch {unset Images}
}
proc ClearOldImages {} {
  global OldImages
  foreach fn [array names OldImages] {
    image delete $OldImages($fn)
  }
  catch {unset OldImages}
}
proc ClearBigImages {} {
  global BigImages
  foreach b [array names BigImages] {
    image delete $BigImages($b)
  }
  catch {unset BigImages}
}

# Read a file
#
proc ReadFile {name} {
  if {[catch {open $name r} fp]} {
    tk_messageBox -icon error -message $fp -type ok
    return {}
  } else {
    set r [read $fp [file size $name]]
    close $fp
    return $r
  }
}

# Process the given URL
#
proc ProcessUrl {url} {
  switch -glob -- $url {
    file:* {
      LoadFile [string range $url 5 end]
    }
    exec:* {
      regsub -all \\+ [string range $url 5 end] { } url
      eval exec $url &
    }
    default {
      LoadFile $url
    }
  }
}

# Load a file into the HTML widget
#
proc LoadFile {name} {
  set html [ReadFile $name]
  if {$html==""} return
  Clear
  global LastFile
  set LastFile $name
  if {[winfo exists .fs.h]} {set w .fs.h} {set w .h.h}
  $w config -base $name -cursor watch
  $w parse $html
  $w config -cursor top_left_arrow
  ClearOldImages
}

# Refresh the current file.
#
proc Refresh {} {
  global LastFile
  if {![info exists LastFile]} return
  LoadFile $LastFile
}

# This routine is called whenever a "<meta>" markup is seen.
#
proc Meta {w tag alist} {
  foreach {name value} $alist {
    set v($name) $value
  }
  if {[info exists v(key)] && [info exists v(href)]} {
    global hotkey
    set hotkey($v(key)) [$w resolve $v(href)]
  }
  if {[info exists v(next)]} {
    global hotkey
    set hotkey(Down) $v(next)
  }
  if {[info exists v(prev)]} {
    global hotkey
    set hotkey(Up) $v(next)
  }
  if {[info exists v(other)]} {
    global hotkey
    set hotkey(o) $v(other)
  }
}

# Go from full-screen mode back to window mode.
#
proc FullScreenOff {} {
  destroy .fs
  wm deiconify .
  update
  raise .
  focus .h.h.x
  Clear
  ClearOldImages
  Refresh 
}

# Go from window mode to full-screen mode.
#
proc FullScreen {} {
  if {[winfo exists .fs]} {
    wm deiconify .fs
    update
    raise .fs
    return
  }
  toplevel .fs
  wm overrideredirect .fs 1
  set w [winfo screenwidth .]
  set h [winfo screenheight .]
  wm geometry .fs ${w}x$h+0+0
  # bind .fs <3> FullScreenOff
  html .fs.h \
    -padx 5 \
    -pady 9 \
    -formcommand FormCmd \
    -imagecommand "ImageCmd 0" \
    -scriptcommand ScriptCmd \
    -appletcommand AppletCmd \
    -hyperlinkcommand HyperCmd \
    -bg white -tablerelief raised \
    -appletcommand {runApplet big} \
    -fontcommand pickFontFS \
    -cursor tcross
  pack .fs.h -fill both -expand 1
  .fs.h token handler meta "Meta .fs.h"
  Clear
  ClearOldImages
  Refresh
  update
  focus .fs.h.x
}
focus .h.h.x

# Load the file named on the command-line, if there is
# one.
#
update
if {$file!=""} {
  LoadFile $file
}
