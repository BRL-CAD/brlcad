namespace eval hv3 { set {version($Id$)} 1 }

# This file contains the implementation of three snit widgets:
#
#     ::hv3::notebook_header
#     ::hv3::notebook
#     ::hv3::tabset
#
# ::hv3::notebook_header is only used directly by code in this file.
#



#-------------------------------------------------------------------------
#
# ::hv3::notebook_header
#
#     This widget creates the notebook tabs. It does not manage any child
#     windows, all it does is draw the tabs.
#
# OPTIONS:
#
#   -selectcmd SCRIPT
#   -closecmd SCRIPT
#
# SUB-COMMANDS:
#
#   add_tab   IDENTIFIER
#   del_tab   IDENTIFIER
#   title     IDENTIFIER ?TITLE?
#   select    IDENTIFIER
#
snit::widget ::hv3::notebook_header {

  option -selectcmd -default [list] -configuremethod SetOption
  option -closecmd  -default [list] -configuremethod SetOption

  # The identifier of the currently selected tab (the value most recently 
  # passed to the [select] method).
  variable myCurrent ""

  # List of identifiers in display order from left to right.
  variable myIdList ""
  
  # Map from current identifiers to display titles.
  variable myIdMap -array [list]

  proc init_bitmaps {} {

    # Set up the two images used for the "close tab" buttons positioned
    # on the tabs themselves. The image data was created by me using an 
    # archaic tool called "bitmap" that was installed with fedora.
    #
    set BitmapData {
      #define closetab_width 14
      #define closetab_height 14
      static unsigned char closetab_bits[] = {
        0xff, 0x3f, 0x01, 0x20, 0x0d, 0x2c, 0x1d, 0x2e, 0x39, 0x27, 0xf1, 0x23,
        0xe1, 0x21, 0xe1, 0x21, 0xf1, 0x23, 0x39, 0x27, 0x1d, 0x2e, 0x0d, 0x2c,
        0x01, 0x20, 0xff, 0x3f
      };
    }
    image create bitmap notebook_close_img -data $BitmapData -background ""
    image create bitmap notebook_close_img2 -data $BitmapData -background red
  }

  constructor {args} {
    canvas ${win}.tabs
    ${win}.tabs configure -borderwidth 0
    ${win}.tabs configure -highlightthickness 0
    ${win}.tabs configure -selectborderwidth 0

    init_bitmaps

    $self configurelist $args

    bind ${win}.tabs <Configure> [list $self Redraw]
    place ${win}.tabs -relwidth 1.0 -relheight 1.0
    $hull configure -height 0
  }

  method add_tab {id} {
    if {[info exists myIdMap($id)]} { error "Already added id $id" }
    lappend myIdList $id
    set myIdMap($id) ""
    $self Redraw
  }

  method del_tab {id} {
    if {![info exists myIdMap($id)]} { error "No nothing about id $id" }
    unset myIdMap($id)
    set idx [lsearch $myIdList $id]
    set myIdList [lreplace $myIdList $idx $idx]
    if {$myCurrent eq $id} { set myCurrent "" }
    $self Redraw
  }

  method title {id args} {
    if {![info exists myIdMap($id)]} { error "No nothing about id $id" }
    if {[llength $args] > 1} { error "Wrong args to notebook_header.title" }
    if {[llength $args] == 1} {
      set zTitle [lindex $args 0]
      set myIdMap($id) $zTitle
      foreach {polygon text line} [${win}.tabs find withtag $id] break
      if {[info exists text]} {
        set bbox [${win}.tabs bbox $polygon]
        set font Hv3DefaultFont
        set iTextWidth [expr {
          [lindex $bbox 2] - [lindex $bbox 0] - 25 - [font measure $font ...]
        }]
        for {set n 0} {$n <= [string length $zTitle]} {incr n} {
          if {[font measure $font [string range $zTitle 0 $n]] > $iTextWidth} {
            break;
          }
        }

        set newtext [string range $zTitle 0 $n]
        if {$n+1 < [string length $zTitle]} {append newtext ...}
        ${win}.tabs itemconfigure $text -text $newtext
      }
    }
    return $myIdMap($id)
  }

  method select {id} {
    set myCurrent $id

    # Optimization: When there is only one tab in the list, do not bother
    # to configure any bindings or colors.
    if {[array size myIdMap]<=1} return
    
    if {$id ne "" && ![info exists myIdMap($id)]} { 
      error "Know nothing about id $id" 
    }
    foreach ident $myIdList {
      foreach {polygon text line} [${win}.tabs find withtag $ident] break
      if {![info exists polygon]} return
      set entercmd ""
      set leavecmd ""
      set clickcmd ""
      if {$ident eq $myCurrent} {
        ${win}.tabs itemconfigure $polygon -fill #d9d9d9
        ${win}.tabs itemconfigure $text -fill darkblue
        ${win}.tabs itemconfigure $line -fill #d9d9d9
      } else {
        ${win}.tabs itemconfigure $polygon -fill #c3c3c3
        ${win}.tabs itemconfigure $text -fill black
        ${win}.tabs itemconfigure $line -fill white
        set entercmd [list ${win}.tabs itemconfigure $polygon -fill #ececec]
        set leavecmd [list ${win}.tabs itemconfigure $polygon -fill #c3c3c3]
        set clickcmd [list $self Select $ident]
      }
      foreach i [list $polygon $text] {
        ${win}.tabs bind $i <Enter> $entercmd
        ${win}.tabs bind $i <Leave> $leavecmd
        ${win}.tabs bind $i <1> $clickcmd
      }
    }
  }

  destructor {
    after cancel [list $self RedrawCallback]
  }

  method Redraw {} {
    after cancel [list $self RedrawCallback]
    after idle   [list $self RedrawCallback]
  }

  method RedrawCallback {} {
    # Optimization: When there is only one tab in the list, do not bother
    # to draw anything. It won't be visible anyway.
    if {[array size myIdMap]<=1} {
      $hull configure -height 1
      return
    }

    set tab_height [expr [font metrics Hv3DefaultFont -linespace] * 1.5]
    $hull configure -height $tab_height

    set font Hv3DefaultFont

    set iPadding  2
    set iDiagonal 2
    set iButton   14
    set iCanvasWidth [expr [winfo width ${win}.tabs] - 2]
    set iThreeDots [font measure $font "..."]

    # Delete the existing canvas items. This proc draws everything 
    # from scratch.
    ${win}.tabs delete all

    # If the myIdList is empty, there are no tabs to draw.
    set myRedrawScheduled 0
    if {[llength $myIdList] == 0} return

    set iRemainingTabs [llength $myIdList]
    set iRemainingPixels $iCanvasWidth

    set yt [expr 0.5 * ($tab_height + [font metrics $font -linespace])]
    set x 1
    foreach ident $myIdList {

      set zTitle $myIdMap($ident)

      set  iTabWidth [expr $iRemainingPixels / $iRemainingTabs]
      incr iRemainingTabs -1
      incr iRemainingPixels [expr $iTabWidth * -1]

      set iTextWidth [expr                                            \
          $iTabWidth - $iButton - $iDiagonal * 2 - $iPadding * 2 - 1  \
          - $iThreeDots
      ]
      for {set n 0} {$n <= [string length $zTitle]} {incr n} {
        if {[font measure $font [string range $zTitle 0 $n]] > $iTextWidth} {
          break;
        }
      }
      if {$n <= [string length $zTitle]} {
        set zTitle "[string range $zTitle 0 [expr $n - 1]]..."
      }

      set x2 [expr $x + $iDiagonal]
      set x3 [expr $x + $iTabWidth - $iDiagonal - 1]
      set x4 [expr $x + $iTabWidth - 1]

      set y1 [expr $tab_height - 0]
      set y2 [expr $iDiagonal + 1]
      set y3 1

      set ximg [expr $x + $iTabWidth - $iDiagonal - $iButton - 1]
      set yimg [expr 1 + ($tab_height - $iButton) / 2]

      ${win}.tabs create polygon \
          $x $y1 $x $y2 $x2 $y3 $x3 $y3 $x4 $y2 $x4 $y1 -tags $ident

      ${win}.tabs create text [expr $x2 + $iPadding] $yt \
          -anchor sw -text $zTitle -font $font -tags $ident

      if {$options(-closecmd) ne ""} {
        $self CreateButton $ident $ximg $yimg $iButton
      }

      ${win}.tabs create line $x $y1 $x $y2 $x2 $y3 $x3 $y3 -fill white
      ${win}.tabs create line $x3 $y3 $x4 $y2 $x4 $y1 -fill black

      set yb [expr $y1 - 1]
      ${win}.tabs create line $x $yb [expr $x+$iTabWidth] $yb -tags $ident

      incr x $iTabWidth
    }

    $win select $myCurrent
  }

  method ButtonRelease {tag idx x y} {
    foreach {x1 y1 x2 y2} [${win}.tabs bbox $tag] {}
    if {$x1 <= $x && $x2 >= $x && $y1 <= $y && $y2 >= $y} {
      eval $options(-closecmd) $idx
    }
  }

  method CreateButton {idx x y size} {
    set c ${win}.tabs              ;# Canvas widget to draw on
    set tag [$c create image $x $y -anchor nw]
    $c itemconfigure $tag -image notebook_close_img
    $c bind $tag <Enter> [list $c itemconfigure $tag -image notebook_close_img2]
    $c bind $tag <Leave> [list $c itemconfigure $tag -image notebook_close_img]
    $c bind $tag <ButtonRelease-1> [list $self ButtonRelease $tag $idx %x %y]
  }

  method Select {idx} {
    if {$options(-selectcmd) ne ""} {
      eval $options(-selectcmd) $idx
    }
  }

  method SetOption {option value} {
    set options($option) $value
    $self Redraw
  }
}

#---------------------------------------------------------------------------
#
# ::hv3::notebook
#
#     Generic notebook widget.
#
# OPTIONS
#
#     -closecmd
#
# WIDGET COMMAND
#
#     $notebook add WIDGET ?TITLE? ?UPDATE?
#     $notebook select ?WIDGET?
#     $notebook tabs
#     $notebook title WIDGET ?NEW-TITLE?
#
#     <<NotebookTabChanged>>
#
snit::widget ::hv3::notebook {

  variable myWidgets [list]
  variable myCurrent -1

  delegate option * to hull

  component myHeader
  delegate option -closecmd to myHeader
  delegate method title to myHeader
  
  constructor {args} {
  
    # Create a tabs header to paint the tab control in.
    #
    set myHeader [::hv3::notebook_header ${win}.header]
    $myHeader configure -selectcmd [mymethod select]

    grid columnconfigure $win 0 -weight 1
    grid rowconfigure $win 1 -weight 1
    grid propagate $win 0
    grid $myHeader -row 0 -column 0 -sticky ew

    $self configurelist $args
  }

  destructor {
    after cancel [list event generate $win <<NotebookTabChanged>>]
  }

  # add WIDGET ?TITLE? ?UPDATE?
  # 
  #     Add a new widget to the set of tabbed windows.
  #
  method add {widget {zTitle ""} {update false}} {

    lappend myWidgets $widget

    bind $widget <Destroy> [list $self HandleDestroy $widget %W]

    if {$update} update
    $myHeader add_tab $widget
    $myHeader title $widget $zTitle
    if {[llength $myWidgets] == 1} {
      $myHeader select $widget
      $self Place $widget
    }
  }

  method Place {w} {
    lower $w
    grid $w -row 1 -column 0 -sticky nsew
    update
    update
    update
    foreach slave [grid slaves $win -row 1 -column 0] {
      if {$w eq $slave} continue
      grid forget $slave
    } 
  }

  method HandleDestroy {widget w} {
    catch {
      if {$widget eq $w} {$self Remove $widget}
    }
  }

  # Remove $widget from the set of tabbed windows. Regardless of
  # whether or not $widget is the current tab, a <<NotebookTabChanged>>
  # event is generated.
  #
  method Remove {widget} {

    set idx [lsearch $myWidgets $widget]
    if {$idx < 0} { error "$widget is not managed by $self" }

    grid forget $widget
    bind $widget <Destroy> ""

    set myWidgets [lreplace $myWidgets $idx $idx]
    $myHeader del_tab $widget

    if {$idx < $myCurrent || $myCurrent == [llength $myWidgets]} {
      incr myCurrent -1
    }
    if {$myCurrent >= 0} {
      set w [lindex $myWidgets $myCurrent]
      $self Place $w
      $myHeader select $w
    }

    after cancel [list event generate $win <<NotebookTabChanged>>]
    after idle   [list event generate $win <<NotebookTabChanged>>]
  }

  # select ?WIDGET?
  # 
  #     If an argument is provided, make that widget the current tab.
  #     Return the current tab widget (a copy of the argument if one
  #     was provided).
  #
  method select {{widget ""}} {
    if {$widget ne ""} {
      set idx [lsearch $myWidgets $widget]
      if {$idx < 0} { error "$widget is not managed by $self" }
      if {$myCurrent != $idx} {
        set myCurrent $idx
        $myHeader select $widget
        after cancel [list event generate $win <<NotebookTabChanged>>]
        after idle   [list event generate $win <<NotebookTabChanged>>]
        # raise $widget
        $self Place $widget
      }
    }
    return [lindex $myWidgets $myCurrent]
  }

  method tabs {} {
    return $myWidgets
  }
}

#---------------------------------------------------------------------------
#
# ::hv3::tabset
#
#     Main tabset widget for hv3 web browser.
#
# OPTIONS
#
#     -newcmd
#     -switchcmd
#
# WIDGET COMMAND
#
#     $notebook add ARGS
#     $notebook addbg ARGS
#     $notebook close
#     $notebook current
#     $notebook set_title WIDGET TITLE
#     $notebook get_title WIDGET
#     $notebook tabs
#
snit::widgetadaptor ::hv3::tabset {

  option -newcmd      -default ""
  option -switchcmd   -default ""

  variable myNextId 0
  variable myPendingTitle ""

  method Switchcmd {} {
    if {$options(-switchcmd) ne ""} {
      eval [linsert $options(-switchcmd) end [$self current]]
    }
  }

  method close {} {
    destroy [$self current]
  }

  constructor {args} {
    installhull [::hv3::notebook $win -width 700 -height 500]
    $self configurelist $args
    $hull configure -closecmd destroy
    bind $win <<NotebookTabChanged>> [list $self Switchcmd]
  }

  method Addcommon {switchto args} {
    set widget ${win}.tab_[incr myNextId]

    set myPendingTitle Blank
    eval [concat [linsert $options(-newcmd) 1 $widget] $args]
    $hull add $widget $myPendingTitle 1

    if {$switchto} {
      $hull select $widget
      $self Switchcmd
    }

    return $widget
  }

  method addbg {args} {
      eval [concat $self Addcommon 0 $args]
  }

  method add {args} {
      eval [concat $self Addcommon 1 $args]
  }

  method set_title {widget title} {
    if {[catch {$hull title $widget $title}]} {
      set myPendingTitle $title
    }
  }

  method get_title {widget} {
    if {[catch {set title [$hull title $widget]}]} {
      set title $myPendingTitle
    }
    return $title
  }

  method current {} { $hull select }
  method tabs    {} { $hull tabs }

  delegate method select to hull
}
