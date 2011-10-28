#
# This script implements the "hv" application.  Type "hv FILE" to
# view FILE as HTML.
#
# This application is used for testing the HTML widget.  It can
# also server as an example of how to use the HTML widget.
# 
# @(#) $Id$
#
wm title . {HTML File Viewer}
wm iconname . {HV}

# Make sure the html widget is loaded into
# our interpreter
#
if {[info command html]==""} {
  if {[catch {package require Tkhtml} error]} {
    foreach f {
      ./libTkhtml*.so
      ../libTkhtml*.so
      /usr/lib/libTkhtml*.so
      /usr/local/lib/libTkhtml*.so
      ./tkhtml.dll
    } {
      if {[set f [lindex [glob -nocomplain $f] end]] != ""} {
        if {[catch {load $f Tkhtml}]==0} break
      }
    }
  }
}

# The HtmlTraceMask only works if the widget was compiled with
# the -DDEBUG=1 command-line option.  "file" is the name of the
# first HTML file to be loaded.
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

# These images are used in place of GIFs or of form elements
#
image create photo biggray -data {
    R0lGODdhPAA+APAAALi4uAAAACwAAAAAPAA+AAACQISPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9o3n+s73/g8MCofEovGITCqXzKbzCY1Kp9Sq9YrNFgsAO///
}
image create photo smgray -data {
    R0lGODdhOAAYAPAAALi4uAAAACwAAAAAOAAYAAACI4SPqcvtD6OctNqLs968+w+G4kiW5omm
    6sq27gvH8kzX9m0VADv/
}
image create photo nogifbig -data {
    R0lGODdhJAAkAPEAAACQkADQ0PgAAAAAACwAAAAAJAAkAAACmISPqcsQD6OcdJqKM71PeK15
    AsSJH0iZY1CqqKSurfsGsex08XuTuU7L9HywHWZILAaVJssvgoREk5PolFo1XrHZ29IZ8oo0
    HKEYVDYbyc/jFhz2otvdcyZdF68qeKh2DZd3AtS0QWcDSDgWKJXY+MXS9qY4+JA2+Vho+YPp
    FzSjiTIEWslDQ1rDhPOY2sXVOgeb2kBbu1AAADv/
}
image create photo nogifsm -data {
    R0lGODdhEAAQAPEAAACQkADQ0PgAAAAAACwAAAAAEAAQAAACNISPacHtD4IQz80QJ60as25d
    3idKZdR0IIOm2ta0Lhw/Lz2S1JqvK8ozbTKlEIVYceWSjwIAO///
}

# Construct the main window
#
frame .mbar -bd 2 -relief raised
pack .mbar -side top -fill x
menubutton .mbar.file -text File -underline 0 -menu .mbar.file.m
pack .mbar.file -side left -padx 5
set m [menu .mbar.file.m]
$m add command -label Open -underline 0 -command Load
$m add command -label Refresh -underline 0 -command Refresh
$m add separator
$m add command -label Exit -underline 1 -command exit
menubutton .mbar.view -text View -underline 0 -menu .mbar.view.m
pack .mbar.view -side left -padx 5
set m [menu .mbar.view.m]
set underlineHyper 0
$m add checkbutton -label {Underline Hyperlinks} -variable underlineHyper
trace variable underlineHyper w ChangeUnderline
proc ChangeUnderline args {
  global underlineHyper
  .h.h config -underlinehyperlinks $underlineHyper
}
set showTableStruct 0
$m add checkbutton -label {Show Table Structure} -variable showTableStruct
trace variable showTableStruct w ShowTableStruct
proc ShowTableStruct args {
  global showTableStruct HtmlTraceMask
  if {$showTableStruct} {
    set HtmlTraceMask [expr {$HtmlTraceMask|0x8}]
    .h.h config -tablerelief flat
  } else {
    set HtmlTraceMask [expr {$HtmlTraceMask&~0x8}]
    .h.h config -tablerelief raised
  }
  Refresh
}
set showImages 1
$m add checkbutton -label {Show Images} -variable showImages
trace variable showImages w Refresh

# Construct the main HTML viewer
#
frame .h
pack .h -side top -fill both -expand 1
html .h.h \
  -yscrollcommand {.h.vsb set} \
  -xscrollcommand {.f2.hsb set} \
  -padx 5 \
  -pady 9 \
  -formcommand FormCmd \
  -imagecommand ImageCmd \
  -scriptcommand ScriptCmd \
  -appletcommand AppletCmd \
  -underlinehyperlinks 0 \
  -bg white -tablerelief raised

# If the tracemask is not 0, then draw the outline of all
# tables as a blank line, not a 3D relief.
#
if {$HtmlTraceMask} {
  .h.h config -tablerelief flat
}

# A font chooser routine.
#
# .h.h config -fontcommand pickFont
proc pickFont {size attrs} { 
  puts "FontCmd: $size $attrs"
  set a [expr {-1<[lsearch $attrs fixed]?{courier}:{charter}}]
  set b [expr {-1<[lsearch $attrs italic]?{italic}:{roman}}]
  set c [expr {-1<[lsearch $attrs bold]?{bold}:{normal}}]
  set d [expr {int(12*pow(1.2,$size-4))}]
  list $a $d $b $c
} 

# This routine is called for each form element
#
proc FormCmd {n cmd style args} {
  # puts "FormCmd: $n $cmd $args"
  switch $cmd {
    select -
    textarea -
    input {
      set w [lindex $args 0]
      label $w -image nogifsm
    }
  }
}

# This routine is called for every <IMG> markup
#
# proc ImageCmd {args} {
# puts "image: $args"
#   set fn [lindex $args 0]
#   if {[catch {image create photo -file $fn} img]} {
#     return nogifsm
#   } else {
#    global Images
#    set Images($img) 1
#    return $img
#  }
#}
proc ImageCmd {args} {
  global OldImages Images showImages
  if {!$showImages} {
    return smgray
  }
  set fn [lindex $args 0]
  if {[info exists OldImages($fn)]} {
    set Images($fn) $OldImages($fn)
    unset OldImages($fn)
    return $Images($fn)
  }
  if {[catch {image create photo -file $fn} img]} {
    return smgray
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
  update
}


# This routine is called for every <SCRIPT> markup
#
proc ScriptCmd {args} {
  # puts "ScriptCmd: $args"
}

# This routine is called for every <APPLET> markup
#
proc AppletCmd {w arglist} {
  # puts "AppletCmd: w=$w arglist=$arglist"
  label $w -text "The Applet $w" -bd 2 -relief raised
}
namespace eval tkhtml {
    array set Priv {}
}

# This procedure is called when the user clicks on a hyperlink.
# See the "bind .h.h.x" below for the binding that invokes this
# procedure
#
proc HrefBinding {x y} {
  # koba & dg marking text
  .h.h selection clear
  set ::tkhtml::Priv(mark) $x,$y
  set list [.h.h href $x $y]
  if {![llength $list]} {return}
  foreach {new target} $list break
  if {$new!=""} {
    global LastFile
    set pattern $LastFile#
    set len [string length $pattern]
    incr len -1
    if {[string range $new 0 $len]==$pattern} {
      incr len
      .h.h yview [string range $new $len end]
    } else {
      LoadFile $new
    }
  }
}
bind .h.h.x <1> {HrefBinding %x %y}
# marking text with the mouse and copying to the clipboard just with tkhtml2.0 working
bind .h.h.x <B1-Motion> {
    %W selection set @$::tkhtml::Priv(mark) @%x,%y
    clipboard clear
    # avoid tkhtml0.0 errors 
    # anyone can fix this for tkhtml0.0
    catch {
        clipboard append [selection get]
    }
}

# Pack the HTML widget into the main screen.
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

# This procedure is called when the user selects the File/Open
# menu option.
#
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

# Load a file into the HTML widget
#
proc LoadFile {name} {
  set html [ReadFile $name]
  if {$html==""} return
  Clear
  global LastFile
  set LastFile $name
   .h.h config -base $name
  .h.h parse $html
  ClearOldImages
}

# Refresh the current file.
#
proc Refresh {args} {
  global LastFile
  if {![info exists LastFile]} return
  LoadFile $LastFile
}

# If an arguent was specified, read it into the HTML widget.
#
update
if {$file!=""} {
  LoadFile $file
}


# This binding changes the cursor when the mouse move over
# top of a hyperlink.
#
bind HtmlClip <Motion> {
  set parent [winfo parent %W]
  set url [$parent href %x %y] 
  if {[string length $url] > 0} {
    $parent configure -cursor hand2
  } else {
    $parent configure -cursor {}
  }
}
