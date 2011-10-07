namespace eval hv3 { set {version($Id$)} 1 }

package require snit
package require Tk

set ::hv3::toolkit Tk
catch {
#  package require tile
#  set ::hv3::toolkit Tile
}

#-------------------------------------------------------------------
# Font control:
#
#     Most of the ::hv3::** widgets use the named font 
#     "Hv3DefaultFont". The following two procs, [::hv3::UseHv3Font]
#     and [::hv3::SetFont] deal with configuring the widgets and
#     dynamically modifying the font when required.
#
proc ::hv3::UseHv3Font {widget} {
  $widget configure -font Hv3DefaultFont
}

# Basic wrapper widget-names used to abstract Tk and Tile:
#
#    ::hv3::scrollbar
#    ::hv3::button
#    ::hv3::entry
#    ::hv3::text
#    ::hv3::label
#    ::hv3::toolbutton

namespace eval ::hv3 {

  set toolkit "Tk"

  proc UseHv3Font {widget} {
    $widget configure -font Hv3DefaultFont
  }
  proc SetFont {font} {
    catch {font delete Hv3DefaultFont}
    catch {font delete Hv3DefaultBold}
    eval [linsert $font 0 font create Hv3DefaultFont]
    eval [linsert $font 0 font create Hv3DefaultBold -weight bold]

  
    # WARNING: Horrible, horrible action at a distance...
    catch {.notebook.header Redraw}
  }

  SetFont {-size 10}

  proc button {args} {
    if {$::hv3::toolkit eq "Tile"} {
      set w [eval [linsert $args 0 ::ttk::button]]
    } else {
      set w [eval [linsert $args 0 ::button]]
      $w configure -highlightthickness 0
      $w configure -pady 0 -borderwidth 1
      ::hv3::UseHv3Font $w
    }
    return $w
  }
  proc entry {args} {
    if {$::hv3::toolkit eq "Tile"} {
      set w [eval [linsert $args 0 ::ttk::entry]]
    } else {
      set w [eval [linsert $args 0 ::entry]]
      $w configure -highlightthickness 0
      $w configure -borderwidth 1
      $w configure -background white
      ::hv3::UseHv3Font $w
    }
    return $w
  }
  proc text {args} {
    set w [eval [linsert $args 0 ::text]]
    $w configure -highlightthickness 0
    return $w
  }
  
  proc label {args} {
    if {$::hv3::toolkit eq "Tile"} {
      set w [eval [linsert $args 0 ::ttk::label]]
    } else {
      set w [eval [linsert $args 0 ::label]]
      $w configure -highlightthickness 0
      ::hv3::UseHv3Font $w
    }
    return $w
  }

}

::snit::widget ::hv3::toolbutton {

  component myButton
  component myPopupLabel
  variable  myPopup

  variable myCallback ""

  constructor {args} {

    if {$::hv3::toolkit eq "Tile"} {
      set myButton [::ttk::button ${win}.button -style Toolbutton]
    } else {
      set myButton [::button ${win}.button -takefocus 0]

      # Configure Tk presentation options not required for Tile here.
      $myButton configure -highlightthickness 0
      $myButton configure -borderwidth 1
      $myButton configure -relief flat -overrelief raised
    }
    set top [winfo toplevel $myButton]
    if {$top eq "."} {set top ""}
    set myPopup ${top}.[string map {. _} $myButton]
    set myPopupLabel ${myPopup}.label
    frame $myPopup -bg black
    ::label $myPopupLabel -fg black -bg white
    ::hv3::UseHv3Font $myPopupLabel

    pack $myButton -expand true -fill both
    pack $myPopup.label -padx 1 -pady 1 -fill both -expand true

    $self configurelist $args

    bind $myButton <Enter> [list $self Enter]
    bind $myButton <Leave> [list $self Leave]
    bind $myButton <ButtonPress-1> +[list $self Leave]
  }

  destructor {
    destroy $myPopup
  }

  method Enter {} {
    after 600 [list $self Popup]
  }

  method Leave {} {
    after cancel [list $self Popup]
    place forget $myPopup
  }

  method Popup {} {
    set top [winfo toplevel $myButton]
    set x [expr [winfo rootx $myButton] - [winfo rootx $top]]
    set y [expr [winfo rooty $myButton] - [winfo rooty $top]]
    incr y [expr [winfo height $myButton]  / 2]
    if {$x < ([winfo width $top] / 2)} {
      incr x [expr [winfo width $myButton]]
      place $myPopup -anchor w -x $x -y $y
    } else {
      place $myPopup -anchor e -x $x -y $y
    }
  }

  delegate method * to myButton
  delegate option * to myButton

  delegate option -tooltip to myPopupLabel as -text
}

# List of menu widgets used by ::hv3::menu and ::hv3::menu_color
#
set ::hv3::menu_list  [list]
set ::hv3::menu_style [list]

proc ::hv3::menu {args} {
  set w [eval [linsert $args 0 ::menu]]
  if {$::hv3::toolkit eq "Tile"} {
    lappend ::hv3::menu_list $w
    $w configure -borderwidth 1 -tearoff 0 -font TkDefaultFont
  } else {
    $w configure -borderwidth 1 -tearoff 0 -activeborderwidth 1
    ::hv3::UseHv3Font $w
  }
  return $w
}

proc ::hv3::menu_color {} {
  set fg  [style lookup Toolbutton -foreground]
  set afg [style lookup Toolbutton -foreground active]
  set bg  [style lookup Toolbutton -background]
  set abg [style lookup Toolbutton -background active]

  foreach w $::hv3::menu_list {
    catch {
      $w configure -fg $fg -bg $bg -activebackground $abg -activeforeground $afg
    }
  }
}

#---------------------------------------------------------------------------
# ::hv3::walkTree
# 
#     This proc is used for depth first traversal of the document tree 
#     headed by the argument node. 
#
#     Example:
#
#         ::hv3::walkTree [.html node] N {
#           puts "Type of node: [$N tag]"
#         }
#
#     If the body of the loop executes a [continue], then the current iteration
#     is terminated and the body is not executed for any of the current nodes
#     children. i.e. [continue] prevents descent of the tree.
#
proc ::hv3::walkTree {N varname body} {
  set level "#[expr [info level] - 1]"
  ::hv3::walkTree2 $N $body $varname $level
}
proc ::hv3::walkTree2 {N body varname level} {
  uplevel $level [list set $varname $N]
  set rc [catch {uplevel $level $body} msg] 
  switch $rc {
    0 {           ;# OK
      foreach n [$N children] {
        ::hv3::walkTree2 $n $body $varname $level
      }
    }
    1 {           ;# ERROR
      error $msg
    }
    2 {           ;# RETURN
      return $msg
    }
    3 {           ;# BREAK
      error "break from within ::hv3::walkTree"
    }
    4 {           ;# CONTINUE
      # Do nothing. Do not descend the tree.
    }
  }
}
#---------------------------------------------------------------------------

snit::widget ::hv3::googlewidget {

  option -getcmd  -default ""
  option -config  -default ""
  option -initial -default Google

  delegate option -borderwidth to hull
  delegate option -relief      to hull

  variable myEngine 

  constructor {args} {
    $self configurelist $args

    set myEngine $options(-initial)

    ::hv3::label $win.label -text "Search:"
    ::hv3::entry $win.entry -width 30
    ::hv3::button $win.close -text dismiss -command [list destroy $win]

    set w ${win}.menubutton
    menubutton $w -textvar [myvar myEngine] -indicatoron 1 -menu $w.menu
    ::hv3::menu $w.menu
    $w configure -borderwidth 1 -relief raised -pady 2
    ::hv3::UseHv3Font $w
    foreach {label uri} $options(-config) {
      $w.menu add radiobutton -label $label -variable [myvar myEngine]
    }

    pack $win.label -side left
    pack $win.entry -side left
    pack $w -side left
    pack $win.close -side right

    bind $win.entry <Return>       [list $self Search]

    # Propagate events that occur in the entry widget to the 
    # ::hv3::findwidget widget itself. This allows the calling script
    # to bind events without knowing the internal mega-widget structure.
    # For example, the hv3 app binds the <Escape> key to delete the
    # findwidget widget.
    #
    bindtags $win.entry [concat [bindtags $win.entry] $win]
  }

  method Search {} {
    array set a $options(-config)

    set search [::hv3::escape_string [${win}.entry get]]
    set query [format $a($myEngine) $search]

    if {$options(-getcmd) ne ""} {
      set script [linsert $options(-getcmd) end $query]
      eval $script
    }
  }
}

proc ::hv3::ComparePositionId {frame1 frame2} {
  return [string compare [$frame1 positionid] [$frame2 positionid]]
}

#-------------------------------------------------------------------------
# ::hv3::findwidget
#
#     This snit widget encapsulates the "Find in page..." functionality.
#
#     Two tags may be added to the html widget(s):
#
#         findwidget                      (all search hits)
#         findwidgetcurrent               (the current search hit)
#
#
snit::widget ::hv3::findwidget {
  variable myBrowser          ;# The ::hv3::browser widget

  variable myNocaseVar 1      ;# Variable for the "Case insensitive" checkbox 
  variable myEntryVar  ""     ;# Variable for the entry widget
  variable myCaptionVar ""    ;# Variable for the label widget
  
  variable myCurrentHit -1
  variable myCurrentList ""

  delegate option -borderwidth to hull
  delegate option -relief      to hull

  constructor {browser args} {
    set myBrowser $browser

    ::hv3::label $win.label -text "Search for text:"
    ::hv3::entry $win.entry -width 30
    ::hv3::label $win.num_results -textvar [myvar myCaptionVar]

    checkbutton $win.check_nocase -variable [myvar myNocaseVar] -pady 0
    ::hv3::label $win.check_nocase_label -text "Case Insensitive"

    ::hv3::button $win.close -text dismiss -command [list destroy $win]
 
    $win.entry configure -textvar [myvar myEntryVar]
    trace add variable [myvar myEntryVar] write [list $self DynamicUpdate]
    trace add variable [myvar myNocaseVar] write [list $self DynamicUpdate]

    bind $win.entry <Return>       [list $self Return 1]
    bind $win.entry <Shift-Return> [list $self Return -1]
    focus $win.entry

    # Propagate events that occur in the entry widget to the 
    # ::hv3::findwidget widget itself. This allows the calling script
    # to bind events without knowing the internal mega-widget structure.
    # For example, the hv3 app binds the <Escape> key to delete the
    # findwidget widget.
    #
    bindtags $win.entry [concat [bindtags $win.entry] $win]

    pack $win.label -side left
    pack $win.entry -side left
    pack $win.check_nocase -side left
    pack $win.check_nocase_label -side left
    pack $win.close -side right
    pack $win.num_results -side right -fill x
  }

  method Hv3List {} {
    if {[catch {$myBrowser get_frames} msg]} {
      return $myBrowser
    } else {
      set frames [$myBrowser get_frames]
    }

    # Filter the $frames list to exclude frameset documents.
    set frames2 ""
    foreach f $frames {
      if {![$f isframeset]} {
        lappend frames2 $f
      }
    }

    # Sort the frames list in [positionid] order
    set frames3 [lsort -command ::hv3::ComparePositionId $frames2]

    set ret [list]
    foreach f $frames3 {
      lappend ret [$f hv3]
    }
    return $ret
  }

  method lazymoveto {hv3 n1 i1 n2 i2} {
    set nodebbox [$hv3 html text bbox $n1 $i1 $n2 $i2]
    set docbbox  [$hv3 html bbox]

    set docheight "[lindex $docbbox 3].0"

    set ntop    [expr ([lindex $nodebbox 1].0 - 30.0) / $docheight]
    set nbottom [expr ([lindex $nodebbox 3].0 + 30.0) / $docheight]
 
    set sheight [expr [winfo height [$hv3 html]].0 / $docheight]
    set stop    [lindex [$hv3 yview] 0]
    set sbottom [expr $stop + $sheight]


    if {$ntop < $stop} {
      $hv3 yview moveto $ntop
    } elseif {$nbottom > $sbottom} {
      $hv3 yview moveto [expr $nbottom - $sheight]
    }
  }

  # Dynamic update proc.
  method UpdateDisplay {nMaxHighlight} {

    set nMatch 0      ;# Total number of matches
    set nHighlight 0  ;# Total number of highlighted matches
    set matches [list]

    # Get the list of hv3 widgets that (currently) make up this browser
    # display. There is usually only 1, but may be more in the case of
    # frameset documents.
    #
    set hv3list [$self Hv3List]

    # Delete any instances of our two tags - "findwidget" and
    # "findwidgetcurrent". Clear the caption.
    #
    foreach hv3 $hv3list {
      $hv3 html tag delete findwidget
      $hv3 html tag delete findwidgetcurrent
    }
    set myCaptionVar ""

    # Figure out what we're looking for. If there is nothing entered 
    # in the entry field, return early.
    set searchtext $myEntryVar
    if {$myNocaseVar} {
      set searchtext [string tolower $searchtext]
    }
    if {[string length $searchtext] == 0} return

    foreach hv3 $hv3list {
      set doctext [$hv3 html text text]
      if {$myNocaseVar} {
        set doctext [string tolower $doctext]
      }

      set iFin 0
      set lMatch [list]

      while {[set iStart [string first $searchtext $doctext $iFin]] >= 0} {
        set iFin [expr $iStart + [string length $searchtext]]
        lappend lMatch $iStart $iFin
        incr nMatch
        if {$nMatch == $nMaxHighlight} { set nMatch "many" ; break }
      }

      set lMatch [lrange $lMatch 0 [expr ($nMaxHighlight - $nHighlight)*2 - 1]]
      incr nHighlight [expr [llength $lMatch] / 2]
      if {[llength $lMatch] > 0} {
        lappend matches $hv3 [eval [concat $hv3 html text index $lMatch]]
      }
    }

    set myCaptionVar "(highlighted $nHighlight of $nMatch hits)"

    foreach {hv3 matchlist} $matches {
      foreach {n1 i1 n2 i2} $matchlist {
        $hv3 html tag add findwidget $n1 $i1 $n2 $i2
      }
      $hv3 html tag configure findwidget -bg purple -fg white
      $self lazymoveto $hv3                         \
            [lindex $matchlist 0] [lindex $matchlist 1] \
            [lindex $matchlist 2] [lindex $matchlist 3]
    }

    set myCurrentList $matches
  }

  method DynamicUpdate {args} {
    set myCurrentHit -1
    $self UpdateDisplay 42
  }
  
  method Escape {} {
    # destroy $win
  }
  method Return {dir} {

    set previousHit $myCurrentHit
    if {$myCurrentHit < 0} {
      $self UpdateDisplay 100000
    } 
    incr myCurrentHit $dir

    set nTotalHit 0
    foreach {hv3 matchlist} $myCurrentList {
      incr nTotalHit [expr [llength $matchlist] / 4]
    }

    if {$myCurrentHit < 0 || $nTotalHit <= $myCurrentHit} {
      tk_messageBox -message "The text you entered was not found" -type ok
      incr myCurrentHit [expr -1 * $dir]
      return
    }
    set myCaptionVar "Hit [expr $myCurrentHit + 1] / $nTotalHit"

    set hv3 ""
    foreach {hv3 n1 i1 n2 i2} [$self GetHit $previousHit] { }
    catch {$hv3 html tag delete findwidgetcurrent}

    set hv3 ""
    foreach {hv3 n1 i1 n2 i2} [$self GetHit $myCurrentHit] { }
    $self lazymoveto $hv3 $n1 $i1 $n2 $i2
    $hv3 html tag add findwidgetcurrent $n1 $i1 $n2 $i2
    $hv3 html tag configure findwidgetcurrent -bg black -fg yellow
  }

  method GetHit {iIdx} {
    set nSofar 0
    foreach {hv3 matchlist} $myCurrentList {
      set nThis [expr [llength $matchlist] / 4]
      if {($nThis + $nSofar) > $iIdx} {
        return [concat $hv3 [lrange $matchlist \
                [expr ($iIdx-$nSofar)*4] [expr ($iIdx-$nSofar)*4+3]
        ]]
      }
      incr nSofar $nThis
    }
    return ""
  }

  destructor {
    # Delete any tags added to the hv3 widget. Do this inside a [catch]
    # block, as it may be that the hv3 widget has itself already been
    # destroyed.
    foreach hv3 [$self Hv3List] {
      catch {
        $hv3 html tag delete findwidget
        $hv3 html tag delete findwidgetcurrent
      }
    }
    trace remove variable [myvar myEntryVar] write [list $self UpdateDisplay]
    trace remove variable [myvar myNocaseVar] write [list $self UpdateDisplay]
  }
}

snit::widget ::hv3::stylereport {
  hulltype toplevel

  variable myHtml ""

  constructor {html} {
    set hv3 ${win}.hv3
    set myHtml $html
    set hv3 ${win}.hv3

    ::hv3::hv3 $hv3
    $hv3 configure -requestcmd [list $self Requestcmd] -width 600 -height 400

    # Create an ::hv3::findwidget so that the report is searchable.
    # In this case the findwidget should always be visible, so remove
    # the <Escape> binding that normally destroys the widget.
    ::hv3::findwidget ${win}.find $hv3
    bind ${win} <Escape> [list destroy $win]
    
    pack ${win}.find -side bottom -fill x
    pack $hv3 -fill both -expand true
  }
  
  method update {} {
    set hv3 ${win}.hv3
    $hv3 reset
    $hv3 goto report:
  }

  method Requestcmd {downloadHandle} {
    $downloadHandle append "<html><body>[$myHtml _stylereport]"
    $downloadHandle finish
  }
}

