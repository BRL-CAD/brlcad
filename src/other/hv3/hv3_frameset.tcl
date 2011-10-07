namespace eval hv3 { set {version($Id$)} 1 }

# This file contains code for implementing HTML frameset documents in Hv3. 
#
# Each <frameset> element is implemented by a single ::hv3::frameset
# widget. 
#

#
# Code structure:
#
#     ::hv3::frameset_handler
#     ::hv3::iframe_handler
#     ::hv3::get_markup
#

namespace eval hv3 {

  # Set this global variable to true to enable frameset-related
  # debugging output on stderr.
  set FRAMESET_DEBUG 0

  # Create a window name to use for a replaced object for node $node.
  # The first argument is the name of an ::hv3::browser_frame widget.
  #
  proc create_widget_name {node} {
    return [$node html].[string map {: _} $node]
  }

  proc multilength_to_list {multilength} {
    set ret [list]
    foreach elem [split $multilength ,] {
      lappend ret [string trim $elem]
    }
    return $ret
  }

  # frameset_handler
  #
  #     Tkhtml node-handler script for <frameset> elements. The first
  #     argument is the name of an ::hv3::browser_frame widget. The
  #     second argument is the <frameset> node.
  #
  proc frameset_handler {browser_frame node} {

    # Make sure this is not a nested <frameset> element. If it is, ignore
    # it. Only the outer <frameset> is managed by the hv3 widget.
    for {set N [$node parent]} {$N ne ""} {set N [$N parent]} {
      if {[$N tag] eq "frameset"} return
    }

    set win [create_widget_name $node]
    ::hv3::frameset $win $browser_frame $node

    # The 'display' property of <frameset> elements is set to "none" by
    # the default stylesheet. So replacing the <frameset> node with the new
    # ::hv3::frameset widget does not cause the html widget to map the
    # frameset widget. Instead we use standard Tk [grid] to put our
    # widget on top of the html widget.
    #
    # But we call [$node replace] anyway so that Tkhtml will call
    # our destructor when it loads a new resource into $browser_frame.
    #
    $node replace $win -deletecmd [list destroy $win]
    place $win -relheight 1.0 -relwidth 1.0
  }

  # iframe_handler
  #
  #     Tkhtml node-handler script for <iframe> elements. The first
  #     argument is the name of an ::hv3::browser_frame widget. The
  #     second argument is the <iframe> node.
  #
  proc iframe_handler {browser_frame node} {
    set hv3 [$browser_frame hv3]

    # Create an ::hv3::browser_frame to display the resource.
    set panel [create_widget_name $node]
    ::hv3::browser_frame $panel [$browser_frame browser]
    [[$panel hv3] html] configure -shrink 1

    # TODO: Should be properly configured...
    $panel configure -requestcmd [$browser_frame cget -requestcmd]
    $panel configure -statusvar  [$browser_frame cget -statusvar]
    $panel configure -name [$node attr -default "" name]
    $panel configure -iframe $node

    set hv3 [$panel hv3]

    set cfg [list ::hv3::iframe_configure $hv3 $node]
    set del [list destroy $panel]
    $node replace $panel -configurecmd $cfg -deletecmd $del
    ::hv3::iframe_configure $hv3 $node ""

    set history [[$browser_frame browser] history]
    if {0 == [$history loadframe $panel]} {
      # Retrieve the URI for the resource to display in this <IFRAME>
      set src [$node attribute -default "" src]
      if {$src ne "" && [string range $src 0 0] ne "#"} {
        set uri [[$browser_frame hv3] resolve_uri $src]
        $panel goto $uri
      }
    }
  }
  proc iframe_configure {hv3 node values} {
    set scrolling [$node attr -default auto scrolling]
    if {![string is boolean $scrolling]} { set scrolling auto }
    $hv3 configure -scrollbarpolicy $scrolling
  }
  proc iframe_attr_handler {browser_frame node attr value} {
    if {$attr eq "src"} {
      set uri [[$browser_frame hv3] resolve_uri $value]
      [$node replace] goto $uri
    }
  }

  # get_markup --
  #
  #     Utility procedure. Given a document node, return an equivalent
  #     fragment of html markup text.
  #
  proc get_markup {node} {
    set tag [$node tag]
    set ret [$node text]
    if {$tag ne ""} {
      set ret "<$tag"
      foreach {key val} [$node attr] {
        append ret " $key=\"$val\""
      }
      append ret ">"
      foreach child [$node children] {
        append ret [get_markup $child]
      }
      append ret "</$tag>"
    }
    return $ret
  }
}

snit::widget ::hv3::frameset {

  variable myNode                ;# <frameset> node
  variable myHv3                 ;# ::hv3::hv3 containing myNode
  variable myBrowserFrame        ;# ::hv3::browser_frame containing myHv3

  # A ::hv3::frameset widget is always stored internally as a 2 dimensional
  # grid of "panels" (represented in the document by either <frame> or
  # <frameset> elements). Each panel is an ::hv3::browser_frame widget. If 
  # the panel is a <frameset>, it manages a single ::hv3::frameset object.
  # Otherwise, if the panel is a <frame>, the ::hv3::hv3 is used to render 
  # the linked html document.
  #
  # The following two variables store Tcl lists containing the values
  # (implicit or explicit) from the "cols" and "rows" attributes of 
  # the <frameset element>. For example, for
  #
  #     <frameset cols="50%, 100, 30%">
  # 
  # this object stores
  #
  #     set myCols [list 50% 100 50%]
  #     set myRows [list 100%]
  #
  # Therefore, both the grid dimensions and configured sizes may be
  # derived from the following two elements of state.
  variable myCols
  variable myRows

  # The pannedwindow widget used to manage columns and the N pannedwindow
  # widgets used to manage rows. Widget $myColPan manages the widgets
  # in $myRowPanList, which in turn manage the ::hv3::hv3 widgets (the
  # "panels"). N is the number of entries in variable $myCols.
  variable myColPan
  variable myRowPanList

  # A list of the <frame> and <frameset> nodes managed by this frameset.
  variable myPanelNodeList
  variable myPanelFrameList

  delegate option -width to hull
  delegate option -height to hull

  # Create a new ::hv3::frameset widget. $browser_frame is the parent
  # ::hv3::browser_frame widget. $node is the document node representing
  # the <frameset> element to be replaced.
  constructor {browser_frame node} {

    set myNode $node
    set myHv3 [$browser_frame hv3]
    set myBrowserFrame $browser_frame
    
    set myRows [::hv3::multilength_to_list [$node attr -default "100%" rows]]
    set myCols [::hv3::multilength_to_list [$node attr -default "100%" cols]]

    # Find the <frame> and <frameset> children
    set myPanelNodeList [list]
    foreach child [$myNode children] {
      switch -- [$child tag] {
        frame {
          lappend myPanelNodeList $child
        }
        frameset {
          lappend myPanelNodeList $child
        }
      }
    }

    # Create the required panedwindow widgets. One to manage columns
    # and N to manage rows (where N is the number of columns).
    set myColPan [panedwindow ${win}.colpan -orient horizontal -bd 0] 
    $self ApplyFrameAttrs $myColPan $myNode

    set myRowPanList [list]
    for {set iCol 0} {$iCol < [llength $myCols]} {incr iCol} {

      # Create the row-pan, and populate it with ::hv3::browser_frame widgets.
      set pan ${myColPan}.pan_${iCol} 
      panedwindow $pan -orient vertical -bd 0
      for {set iRow 0} {$iRow < [llength $myRows]} {incr iRow} {
        set panel ${pan}.hv3_${iRow}

        ::hv3::browser_frame $panel [$myBrowserFrame browser]
        $panel configure -requestcmd       [$myHv3 cget -requestcmd]
        $panel configure -statusvar        [$myBrowserFrame cget -statusvar]

        $pan add $panel
        lappend myPanelFrameList $panel
      }

      lappend myRowPanList $pan
      $myColPan paneconfigure $pan -sticky nsew
      $myColPan add $pan
    }

    # Make sure the list of nodes and list of panels are the same
    # length. If either has any "extra" elements, discard them.
    #
    set len [llength $myPanelNodeList]
    if {[llength $myPanelFrameList] < $len} {
      set len [llength $myPanelFrameList]
    }
    incr len -1
    set myPanelNodeList [lrange $myPanelNodeList 0 $len]
    set myPanelFrameList [lrange $myPanelFrameList 0 $len]

    # Populate the hv3 widgets used for each "panel".
    foreach pnode $myPanelNodeList pframe $myPanelFrameList {
      set phv3 [$pframe hv3]
      set pan [winfo parent $pframe]
      if {![info exists divwidth($pan)]} {set divwidth($pan) 2}

      if {$pnode eq "" || $phv3 eq ""} break
      switch -- [$pnode tag] {
        frame {

          # Handle the "scrolling" option on the <frame> element. If
          # this option is present and set to "yes" or "no", then this
          # sets the scrollbar-policy of the hv3 widget regardless of
          # the value of 'overflow' computed for the <body> node. If
          # the option is not present or is set to "auto", the
          # scrollbar-policy of the hv3 widget is set dynamically by 
          # the 'overflow' property on the <body> and <html> nodes.
          #
          switch -- [string tolower [$pnode attr -default auto scrolling]] {
            yes     { $phv3 configure -scrollbarpolicy 1 }
            no      { $phv3 configure -scrollbarpolicy 0 }
            default { $phv3 configure -scrollbarpolicy auto }
          }

          # Handle the "noresize" and "frameborder" options. This is 
          # obviously buggy, but handles a surprising number of cases well.
          #
          if {0 == [catch {$pnode attr frameborder} frameborder]} {
            switch -- [string tolower $frameborder] {
              0       { set divwidth($pan) 0 }
              1       { set divwidth($pan) 2 }
              no      { set divwidth($pan) 0 }
              yes     { set divwidth($pan) 2 }
              default { set divwidth($pan) 2 }
            }
          }
          if {0 == [catch {$pnode attr noresize}] && $divwidth($pan) > 1} {
            set divwidth($pan) 1
          }

          # First, see if the history system wants to set the URI for
          # the new frame. This happens during history-seek only. Otherwise,
          # load the document specified by the "src" attribute into
          # the new frame.
          set history [[$myBrowserFrame browser] history]
          if {0 == [$history loadframe $pframe]} {
            set uri [$pnode attr -default "" src]
            if {$uri ne ""} {
              set uri [$myHv3 resolve_uri $uri]
              $phv3 goto $uri
            }
          }
          $pframe configure -name [$pnode attr -default "" name]
        }
        frameset {
	  # For a frameset, we need to create the equivalent HTML document.
          set doc "<html>[::hv3::get_markup $pnode]</html>"
          $phv3 seturi [$myHv3 resolve_uri "internal"]
          $phv3 html parse -final $doc
        }
      }
    }

    foreach pan [array names divwidth] {
      switch $divwidth($pan) {
        0 {
          $pan configure -handlesize 0
          $pan configure -showhandle 0
          $pan configure -sashwidth 0
          $pan configure -sashpad 0
        }
        1 {
          $pan configure -handlesize 0
          $pan configure -showhandle 0
          $pan configure -sashwidth 2
          $pan configure -sashpad 0
        }
      }
    }

    pack $myColPan -fill both -expand true
    $myHv3 configure -scrollbarpolicy false

    bind [$myHv3 html] <Configure> [mymethod Sizecallback]
    after idle [list $self Sizecallback]
  }

  destructor {
    $myHv3 configure -scrollbarpolicy auto
    bind [$myHv3 html] <Configure> ""
    after cancel [list $self Sizecallback]
  }

  method ApplyFrameAttrs {pan node} {
    if {0 == [catch {$node attr frameborder} frameborder]} {
      if {$frameborder eq "0"} {
        $pan configure -handlesize 0
        $pan configure -showhandle 0
        $pan configure -sashwidth 0
        $pan configure -sashpad 0
      }
    }
  }

  method framename {n} {
    return ${myPanedwindow}.frame_$n
  }

  method Sizecallback {} {
    set w [winfo width [$myHv3 html]]
    set h [winfo height [$myHv3 html]]
    if {$::hv3::FRAMESET_DEBUG} {
      puts stderr "FRAMESET: Sizecallback $w $h"
    }

    # Configure the size of the column-pan to match the parent window
    if {[$myColPan cget -height] != $h || [$myColPan cget -width] != $w} {
      $myColPan configure -height $h -width $w
    
      # Configure the column-pan
      $self Configuresize $myColPan $w $myCols
   
      # Configure each of the row-pans
      foreach rowpan $myRowPanList {
        $self Configuresize $rowpan $h $myRows
      }
    }
  }

  # This method is called to configure the sizes of the panels in a 
  # single panedwindow widget. Arguments are:
  # 
  #     widget      - The name of the panedwindow widget
  #     w           - Total width available.
  #     h           - Total height available.
  #     multilength - Tcl list of HTML lengths, one for each panel. Each HTML
  #                   length may take the form "<number>%", "<number>*" or
  #                   "<number>".
  #
  method Configuresize {widget pixels multilength} {

    set pixel_lengths [list]
    foreach mlength $multilength {
      set val -1
      if {$mlength eq "*"} {
        set val -1
      } elseif {[regexp {([[:digit:]]+)([%*]?)} $mlength dummy number type]} {
        switch -- $type {
          % {
            catch {set val [expr int(double($number) * $pixels / 100.0)]}
          }
          * {
            catch {set val "[expr -1 * int($number)]"}
          }
          default {
            catch {set val [expr int($number)]}
          }
        }
      }
      lappend pixel_lengths $val
    }

    set exact 0
    set prop 0
    foreach len $pixel_lengths {
      if {$len > 0} {
        incr exact $len
      } else {
        incr prop [expr $len * -1]
      }
    }

    set per 0.0
    if {$exact < $pixels} {
      if {$prop > 0} {
        set per [expr (double($exact) - double($pixels)) / double($prop)]
      }
      set red 0.0
    } else {
      set red [expr (double($exact) - double($pixels)) / double($exact)]
    }

    set pos 0
    set N 0
    foreach len $pixel_lengths {
      set pane [lindex [$widget panes] $N]
      set sz [expr {($len > 0) ? int($len * (1.0 - $red)) : int($per * $len)}]
      lappend sizes $sz

      incr pos $sz
      if {[$widget cget -orient] eq "horizontal"} {
        catch { $widget sash place $N $pos 0 }
      } else {
        catch { $widget sash place $N 0 $pos }
      }

      incr N
    }

    if {$::hv3::FRAMESET_DEBUG} {
      puts stderr "FRAMESET: Divide $pixels according to \"$multilength\""
      puts stderr "FRAMESET: -> $sizes"
    }
  }
}
