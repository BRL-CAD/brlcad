
package require snit
package require Tk

# The ::hv3::framepair widget is used to pack two other widgets into a 
# frame with a draggable seperator between them. Widgets may be stacked
# horizontally or vertically.
#
snit::widget ::hv3::framepair {

  variable myLeftWidget
  variable myRightWidget
  variable myHandle

  option -fraction -default 0.5
  option -orient   -default "h" -readonly 1

  constructor {left right args} {
    $self configurelist $args

    set options(-orient) [string range $options(-orient) 0 0]
    if {$options(-orient) ne "h" && $options(-orient) ne "v"} {
      error "Bad value for -orient: should be horizontal or vertical"
    }

    puts [linsert $right 1 ${win}.right]
    set myLeftWidget  [eval [linsert $left 1 ${win}.left]] 
    set myRightWidget [eval [linsert $right 1 ${win}.right]]

    set myHandle [frame ${win}.handle]
    $myHandle configure -borderwidth 2 -relief raised

    bind $self <Configure> [mymethod place]

    if {$options(-orient) eq "h"} {
      bind $myHandle <B1-Motion> [mymethod drag %Y]
      $myHandle configure -cursor sb_v_double_arrow
    } else {
      bind $myHandle <B1-Motion> [mymethod drag %X]
      $myHandle configure -cursor sb_h_double_arrow
    }
    $self place
  }

  method drag {Y} {
    if {$options(-orient) eq "h"} {
      set H  [winfo height $self]
      set Y0 [winfo rooty $self]
    } else {
      set H  [winfo width $self]
      set Y0 [winfo rootx $self]
    }

    set options(-fraction) [expr {($Y-$Y0)/double($H)}]
    $self place 
  }

  method place {} {
    if {$options(-fraction) < 0.01} {set options(-fraction) 0.01}
    if {$options(-fraction) > 0.99} {set options(-fraction) 0.99}

    set i [expr {1.0 - $options(-fraction)}]
    set f $options(-fraction)
    if {$options(-orient) eq "h"} {
      place $myLeftWidget  -relheight $f -relwidth 1 -x 0 -y 0 -anchor nw
      place $myHandle      -rely $f -x 0 -anchor w -relwidth 1 -height 4
      place $myRightWidget -relheight $i -x 0 -rely 1 -anchor sw -relwidth 1
    } else {
      place $myLeftWidget  -relwidth $f -relheight 1
      place $myHandle      -relx $f -rely 0 -anchor n -relheight 1 -width 4
      place $myRightWidget -relwidth $i -anchor se -relx 1 -rely 1 -relheight 1
    }
  }

  method left   {} {return $myLeftWidget}
  method right  {} {return $myRightWidget}
  method top    {} {return $myLeftWidget}
  method bottom {} {return $myRightWidget}

  delegate option * to hull
}

snit::widget ::hv3::scrolled {
  component myWidget
  variable myVsb
  variable myHsb

  constructor {widget args} {
    set myWidget [eval [linsert $widget 1 ${win}.widget]]
    set myVsb  [scrollbar ${win}.vsb -orient vertical]
    set myHsb  [scrollbar ${win}.hsb -orient horizontal]

    grid configure $myWidget -column 0 -row 0 -sticky nsew
    grid columnconfigure $win 0 -weight 1
    grid rowconfigure    $win 0 -weight 1

    $self configurelist $args

    $myWidget configure -yscrollcommand [mymethod scrollcallback $myVsb]
    $myWidget configure -xscrollcommand [mymethod scrollcallback $myHsb]
    $myVsb  configure -command        [mymethod yview]
    $myHsb  configure -command        [mymethod xview]

    bindtags $myWidget [concat [bindtags $myWidget] $win]
  }

  method scrollcallback {scrollbar first last} {
    $scrollbar set $first $last
    set ismapped   [expr [winfo ismapped $scrollbar] ? 1 : 0]
    set isrequired [expr ($first == 0.0 && $last == 1.0) ? 0 : 1]
    if {$isrequired && !$ismapped} {
      switch [$scrollbar cget -orient] {
        vertical   {grid configure $scrollbar  -column 1 -row 0 -sticky ns}
        horizontal {grid configure $scrollbar  -column 0 -row 1 -sticky ew}
      }
    } elseif {$ismapped && !$isrequired} {
      grid forget $scrollbar
    }
  }

  method widget {} {return $myWidget}

  delegate option -width  to hull
  delegate option -height to hull
  delegate option *       to myWidget
  delegate method *       to myWidget
}

# Example:
#
#     frameset .frames \
#         hv3    -variable myTopWidget    -side top  -fraction 0.25 \
#         canvas -variable myTreeWidget   -side left -fraction 0.4 \
#         hv3    -variable myReportWidget
# 
proc frameset {win args} {
  set idx 0
  set frames [list]
  while {$idx < [llength $args]} {
    set cmd [lindex $args $idx]
    incr idx

    unset -nocomplain variable
    set side left
    set fraction 0.5

    set options [list -variable -side -fraction]
    while {                                                        \
      $idx < ([llength $args]-1) &&                                \
      [set opt [lsearch $options [lindex $args $idx]]] >= 0        \
    } {
      incr idx 
      set [string range [lindex $options $opt] 1 end] [lindex $args $idx]
      incr idx
    }

    if {![info exists variable]} {
      error "No -variable option supplied for widget $cmd"
    }
    if {$side ne "top" && $side ne "left"} {
      error "Bad value for -side option: should be \"left\" or \"top\""
    }
    if {0 == [string is double $fraction]} {
      error "Bad value for -fraction option: expected double got \"$fraction\""
    }

    lappend frames [list $cmd $variable $side $fraction]
  }

  puts $frames

  unset -nocomplain cmd
  for {set ii [expr [llength $frames] - 1]} {$ii >= 0} {incr ii -1} {
    foreach {wid variable side fraction} [lindex $frames $ii] {}
    if {[info exists cmd]} { 
      switch -- $side {
        top  {set o horizontal}
        left {set o vertical}
      }
      set cmd [list ::hv3::framepair $wid $cmd -fraction $fraction -orient $o]
    } else {
      set cmd $wid
    }
  }
  puts $cmd
  eval [linsert $cmd 1 $win]

  set framepair $win
  for {set ii 0} {$ii < [llength $frames]} {incr ii} {
    foreach {wid variable side fraction} [lindex $frames $ii] {}
    if {$ii == ([llength $frames]-1)} {
      uplevel [list set $variable $framepair]
    } else {
      uplevel [list set $variable [$framepair left]]
      set framepair [$framepair right]
    }
  }

  return $win
}

