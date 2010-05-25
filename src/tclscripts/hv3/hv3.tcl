catch {namespace eval hv3 { set {version($Id$)} 1 }}

# This file contains the mega-widget hv3::hv3 that is at the core
# of the Hv3 web browser implementation. An instance of this widget 
# displays a single HTML frame. Documentation for the published
# interface to this widget is found at:
#
#   http://tkhtml.tcl.tk/hv3_widget.html
#
# Other parts of the interface, used internally and by the Hv3
# web-browser, are documented in comments in this file. Eventually,
# the Hv3 web-browser will use the published interface only. But
# that is not the case yet.
#
#-------------------------------------------------------------------
#
# 
#
# Standard Functionality:
#
#     xview
#     yview
#     -xscrollcommand
#     -yscrollcommand
#     -width
#     -height
# 
# Widget Specific Options:
#
#     -requestcmd
#         If not an empty string, this option specifies a script to be
#         invoked for a GET or POST request. The script is invoked with a
#         download handle appended to it. See the description of class
#         ::hv3::request for a description.
#
#     -targetcmd
#         If not an empty string, this option specifies a script for
#         the widget to invoke when a hyperlink is clicked on or a form
#         submitted. The script is invoked with the node handle of the 
#         clicked hyper-link element appended. The script must return
#         the name of an hv3 widget to load the new document into. This
#         is intended to be used to implement frameset handling.
#
#     -isvisitedcmd
#         If not an empty string, this option specifies a script for
#         the widget to invoke to determine if a hyperlink node should
#         be styled with the :link or :visited pseudo-class. The
#         script is invoked with the node handle appended to it. If
#         true is returned, :visited is used, otherwise :link.
#
#     -fonttable
#         Delegated through to the html widget.
#
#     -locationvar
#         Set to the URI of the currently displayed document.
#
#     -scrollbarpolicy
#         This option may be set to either a boolean value or "auto". It
#         determines the visibility of the widget scrollbars. TODO: This
#         is now set internally by the value of the "overflow" property
#         on the root element. Maybe the option should be removed?
#
#
# Widget Sub-commands:
#
#     goto URI ?OPTIONS?
#         Load the content at the specified URI into the widget. 
#
#     stop
#         Cancel all pending downloads.
#
#     node        
#         Caching wrapper around html widget [node] command.
#
#     reset        
#         Wrapper around the html widget command of the same name. Also
#         resets all document related state stored by the mega-widget.
#
#     html        
#         Return the path of the underlying html widget. This should only
#         be used to determine paths for child widgets. Bypassing hv3 and
#         accessing the html widget interface directly may confuse hv3.
#
#     title        
#         Return the "title" of the currently loaded document.
#
#     location        
#         Return the location URI of the widget.
#
#     selected        
#         Return the currently selected text, or an empty string if no
#         text is currently selected.
#
#
# Widget Custom Events:
#
#     <<Goto>>
#         This event is generated whenever the goto method is called.
#
#     <<Complete>>
#         This event is generated once all of the resources required
#         to display a document have been loaded. This is analogous
#         to the Html "onload" event.
#
#     <<Location>>
#         This event is generated whenever the "location" is set.
#
#     <<SaveState>>
#         Generated whenever the widget state should be saved.

#
# The code in this file is partitioned into the following classes:
#
#     ::hv3::hv3
#     ::hv3::selectionmanager
#     ::hv3::dynamicmanager
#     ::hv3::hyperlinkmanager
#     ::hv3::mousemanager
#
# ::hv3::hv3 is, of course, the main mega-widget class. Class
# ::hv3::request is part of the public interface to ::hv3::hv3. A
# single instance of ::hv3::request represents a resource request made
# by the mega-widget package - for document, stylesheet, image or 
# object data.
#
# The three "manager" classes all implement the following interface. Each
# ::hv3::hv3 widget has exactly one of each manager class as a component.
# Further manager objects may be added in the future. Interface:
#
#     set manager [::hv3::XXXmanager $hv3]
#
#     $manager motion  X Y
#     $manager release X Y
#     $manager press   X Y
#
# The -targetcmd option of ::hv3::hv3 is delegated to the
# ::hv3::hyperlinkmanager component.
#
package require Tkhtml 3.0
package require snit

package provide hv3 0.1

if {[info commands ::hv3::make_constructor] eq ""} {
  source [file join [file dirname [info script]] hv3_encodings.tcl]
  source [file join [file dirname [info script]] hv3_util.tcl]
  source [file join [file dirname [info script]] hv3_form.tcl]
  source [file join [file dirname [info script]] hv3_request.tcl]
}
#source [file join [file dirname [info script]] hv3_request.tcl.bak]

#--------------------------------------------------------------------------
# Class ::hv3::hv3::mousemanager
#
#     This type contains code for the ::hv3::hv3 widget to manage 
#     dispatching mouse events that occur in the HTML widget to the 
#     rest of the application. The following HTML4 events are handled:
#
#     Pointer movement:
#         onmouseover
#         onmouseout
#         motion
#
#     Click-related events:
#         onmousedown
#         onmouseup
#         onclick
#
#     Currently, the following hv3 subsystems subscribe to one or more of
#     these events:
#
#         ::hv3::hyperlinkmanager
#             Click events, mouseover and mouseout on all nodes.
#
#         ::hv3::dynamicmanager
#             Events mouseover, mouseout, mousedown mouseup on all nodes.
#
#         ::hv3::formmanager
#             Click events (for clickable controls) on all nodes.
#
#         ::hv3::selectionmanager
#             motion
#
namespace eval ::hv3::hv3::mousemanager {

  proc new {me hv3} {
    upvar $me O

    set O(myHv3) $hv3
    set O(myHtml) [$hv3 html]

    # In browsers with no DOM support, the following option is set to
    # an empty string.
    #
    # If not set to an empty string, this option is set to the name
    # of the ::hv3::dom object to dispatch events too. The DOM 
    # is a special client because it may cancel the "default action"
    # of mouse-clicks (it may also cancel other events, but they are
    # dispatched by other sub-systems).
    #
    # Each time an event occurs, the following script is executed:
    #
    #     $O(-dom) mouseevent EVENT-TYPE NODE X Y ?OPTIONS?
    #
    # where OPTIONS are:
    #
    #     -button          INTEGER        (default 0)
    #     -detail          INTEGER        (default 0)
    #     -relatedtarget   NODE-HANDLE    (default "")
    #
    # the EVENT-TYPE parameter is one of:
    #
    #     "click", "mouseup", "mousedown", "mouseover" or "mouseout".
    #
    # NODE is the target leaf node and X and Y are the pointer coordinates
    # relative to the top-left of the html widget window.
    #
    # For "click" events, if the $O(-dom) script returns false, then
    # the "click" event is not dispatched to any subscribers (this happens
    # when some javascript calls the Event.preventDefault() method). If it
    # returns true, proceed as normal. Other event types ignore the return 
    # value of the $O(-dom) script.
    #
    set O(-dom) ""
  
    # This variable is set to the node-handle that the pointer is currently
    # hovered over. Used by code that dispatches the "mouseout", "mouseover"
    # and "mousemove" to the DOM.
    #
    set O(myCurrentDomNode) ""
  
    # The "top" node from the ${me}.hovernodes array. This is the node
    # that determines the pointer to display (via the CSS2 'cursor' 
    # property).
    #
    set O(myTopHoverNode) ""
  
    set O(myCursor) ""
    set O(myCursorWin) [$hv3 hull]

    # Database of callback scripts for each event type.
    #
    set O(scripts.onmouseover) ""
    set O(scripts.onmouseout) ""
    set O(scripts.onclick) ""
    set O(scripts.onmousedown) ""
    set O(scripts.onmouseup) ""
    set O(scripts.motion) ""

    # There are also two arrays that store lists of nodes currently "hovered"
    # over and "active". An entry in the correspondoing array indicates the
    # condition is true. The arrays are named:
    #
    #   ${me}.hovernodes
    #   ${me}.activenodes
    #
  
    set w [$hv3 win]
    bind $w <Motion>          "+[list $me Motion  %W %x %y]"
    bind $w <ButtonPress-1>   "+[list $me Press   %W %x %y]"
    bind $w <ButtonRelease-1> "+[list $me Release %W %x %y]"
  }


  proc subscribe {me event script} {
    upvar $me O

    # Check that the $event argument is Ok:
    if {![info exists O(scripts.$event)]} {
      error "No such mouse-event: $event"
    }

    # Append the script to the callback list.
    lappend O(scripts.$event) $script
  }

  proc reset {me} {
    upvar $me O
    array unset ${me}.activenodes
    array unset ${me}.hovernodes
    set O(myCurrentDomNode) ""
  }

  proc GenerateEvents {me eventlist} {
    upvar $me O
    foreach {event node} $eventlist {
      if {[info commands $node] ne ""} {
        foreach script $O(scripts.$event) {
          eval $script $node
        }
      }
    }
  }

  proc AdjustCoords {to W xvar yvar} {
    upvar $xvar x
    upvar $yvar y
    while {$W ne "" && $W ne $to} {
      incr x [winfo x $W]
      incr y [winfo y $W]
      set W [winfo parent $W]
    }
  }

  # Mapping from CSS2 cursor type to Tk cursor type.
  #
  variable CURSORS
  array set CURSORS [list      \
      crosshair crosshair      \
      default   ""             \
      pointer   hand2          \
      move      fleur          \
      text      xterm          \
      wait      watch          \
      progress  box_spiral     \
      help      question_arrow \
  ]

  proc Motion {me W x y} {
    upvar $me O
    variable CURSORS

    if {$W eq ""} return
    AdjustCoords [$O(myHv3) html] $W x y

    # Figure out the node the cursor is currently hovering over. Todo:
    # When the cursor is over multiple nodes (because overlapping content
    # has been generated), maybe this should consider all overlapping nodes
    # as "hovered".
    set nodelist [lindex [$O(myHtml) node $x $y] end]
    
    # Handle the 'cursor' property.
    #
    set topnode [lindex $nodelist end]
    if {$topnode ne "" && $topnode ne $O(myTopHoverNode)} {

      set Cursor ""
      if {[$topnode tag] eq ""} {
        set Cursor xterm
        set topnode [$topnode parent]
      }
      set css2_cursor [$topnode property cursor]
      catch { set Cursor $CURSORS($css2_cursor) }

      if {$Cursor ne $O(myCursor)} {
        $O(myCursorWin) configure -cursor $Cursor
        set O(myCursor) $Cursor
      }
      set O(myTopHoverNode) $topnode
    }

    # Dispatch any DOM events in this order:
    #
    #     mouseout
    #     mouseover
    #     mousemotion
    #
    set N [lindex $nodelist end]
    if {$N eq ""} {set N [$O(myHv3) node]}

    if {$O(-dom) ne ""} {
      if {$N ne $O(myCurrentDomNode)} {
        $O(-dom) mouseevent mouseout $O(myCurrentDomNode) $x $y
        $O(-dom) mouseevent mouseover $N $x $y
        set O(myCurrentDomNode) $N
      }
      $O(-dom) mouseevent mousemove $N $x $y
    }

    foreach script $O(scripts.motion) {
      eval $script $N $x $y
    }

    # After the loop runs, hovernodes will contain the list of 
    # currently hovered nodes.
    array set hovernodes [list]

    # Events to generate:
    set events(onmouseout)  [list]
    set events(onmouseover) [list]

    foreach node $nodelist {
      if {[$node tag] eq ""} {set node [$node parent]}

      for {set n $node} {$n ne ""} {set n [$n parent]} {
        if {[info exists hovernodes($n)]} {
          break
        } else {
          if {[info exists ${me}.hovernodes($n)]} {
            unset ${me}.hovernodes($n)
          } else {
            lappend events(onmouseover) $n
          }
          set hovernodes($n) ""
        }
      }
    }
    set events(onmouseout)  [array names ${me}.hovernodes]

    array unset ${me}.hovernodes
    array set ${me}.hovernodes [array get hovernodes]

    set eventlist [list]
    foreach key [list onmouseover onmouseout] {
      foreach node $events($key) {
        lappend eventlist $key $node
      }
    }
    $me GenerateEvents $eventlist
  }

  proc Press {me W x y} {
    upvar $me O
    if {$W eq ""} return
    AdjustCoords [$O(myHv3) html] $W x y
    set N [lindex [$O(myHtml) node $x $y] end]
    if {$N ne ""} {
      if {[$N tag] eq ""} {set N [$N parent]}
    }
    if {$N eq ""} {set N [$O(myHv3) node]}

    # Dispatch the "mousedown" event to the DOM, if any.
    #
    set rc ""
    if {$O(-dom) ne ""} {
      set rc [$O(-dom) mouseevent mousedown $N $x $y]
    }

    # If the DOM implementation called preventDefault(), do 
    # not start selecting text. But every mouseclick should clear
    # the current selection, otherwise the browser window can get
    # into an annoying state.
    #
    if {$rc eq "prevent"} {
      $O(myHv3) theselectionmanager clear
    } else {
      $O(myHv3) theselectionmanager press $N $x $y
    }

    for {set n $N} {$n ne ""} {set n [$n parent]} {
      set ${me}.activenodes($n) 1
    }

    set eventlist [list]
    foreach node [array names ${me}.activenodes] {
      lappend eventlist onmousedown $node
    }
    $me GenerateEvents $eventlist
  }

  proc Release {me W x y} {
    upvar $me O
    if {$W eq ""} return
    AdjustCoords [$O(myHv3) html] $W x y
    set N [lindex [$O(myHtml) node $x $y] end]
    if {$N ne ""} {
      if {[$N tag] eq ""} {set N [$N parent]}
    }
    if {$N eq ""} {set N [$O(myHv3) node]}

    # Dispatch the "mouseup" event to the DOM, if any.
    #
    # In Tk, the equivalent of the "mouseup" (<ButtonRelease>) is always
    # dispatched to the same widget as the "mousedown" (<ButtonPress>). 
    # But in the DOM things are different - the event target for "mouseup"
    # depends on the current cursor location only.
    #
    if {$O(-dom) ne ""} {
      $O(-dom) mouseevent mouseup $N $x $y
    }

    # Check if the is a "click" event to dispatch to the DOM. If the
    # ::hv3::dom [mouseevent] method returns 0, then the click is
    # not sent to the other hv3 sub-systems (default action is cancelled).
    #
    set domrc ""
    if {$O(-dom) ne ""} {
      for {set n $N} {$n ne ""} {set n [$n parent]} {
        if {[info exists ${me}.activenodes($N)]} {
          set domrc [$O(-dom) mouseevent click $n $x $y]
          break
        }
      }
    }

    set eventlist [list]
    foreach node [array names ${me}.activenodes] {
      lappend eventlist onmouseup $node
    }
    
    if {$domrc ne "prevent"} {
      set onclick_nodes [list]
      for {set n $N} {$n ne ""} {set n [$n parent]} {
        if {[info exists ${me}.activenodes($n)]} {
          lappend onclick_nodes $n
        }
      }
      foreach node $onclick_nodes {
        lappend eventlist onclick $node
      }
    }

    $me GenerateEvents $eventlist

    array unset ${me}.activenodes
  }

  proc destroy me {
    array unset $me
    array unset ${me}.hovernodes
    array unset ${me}.activenodes
    rename $me {}
  }
}
::hv3::make_constructor ::hv3::hv3::mousemanager

#--------------------------------------------------------------------------
# ::hv3::hv3::selectionmanager
#
#     This type encapsulates the code that manages selecting text
#     in the html widget with the mouse.
#
namespace eval ::hv3::hv3::selectionmanager {

  proc new {me hv3} {
    upvar $me O

    # Variable myMode may take one of the following values:
    #
    #     "char"           -> Currently text selecting by character.
    #     "word"           -> Currently text selecting by word.
    #     "block"          -> Currently text selecting by block.
    #
    set O(myState) false             ;# True when left-button is held down
    set O(myMode) char
  
    # The ::hv3::hv3 widget.
    #
    set O(myHv3) $hv3
    set O(myHtml) [$hv3 html]
  
    set O(myFromNode) ""
    set O(myFromIdx) ""
  
    set O(myToNode) ""
    set O(myToIdx) ""
  
    set O(myIgnoreMotion) 0

    set w [$hv3 win]
    selection handle $w [list ::hv3::bg [list $me get_selection]]

    # bind $myHv3 <Motion>               "+[list $self motion %x %y]"
    # bind $myHv3 <ButtonPress-1>        "+[list $self press %x %y]"
    bind $w <Double-ButtonPress-1> "+[list $me doublepress %x %y]"
    bind $w <Triple-ButtonPress-1> "+[list $me triplepress %x %y]"
    bind $w <ButtonRelease-1>      "+[list $me release %x %y]"
  }

  # Clear the selection.
  #
  proc clear {me} {
    upvar $me O
    $O(myHtml) tag delete selection
    $O(myHtml) tag configure selection -foreground white -background darkgrey
    set O(myFromNode) ""
    set O(myToNode) ""
  }

  proc press {me N x y} {
    upvar $me O

    # Single click -> Select by character.
    clear $me
    set O(myState) true
    set O(myMode) char
    motion $me $N $x $y
  }

  # Given a node-handle/index pair identifying a character in the 
  # current document, return the index values for the start and end
  # of the word containing the character.
  #
  proc ToWord {node idx} {
    set t [$node text]
    set cidx [::tkhtml::charoffset $t $idx]
    set cidx1 [string wordstart $t $cidx]
    set cidx2 [string wordend $t $cidx]
    set idx1 [::tkhtml::byteoffset $t $cidx1]
    set idx2 [::tkhtml::byteoffset $t $cidx2]
    return [list $idx1 $idx2]
  }

  # Add the widget tag "selection" to the word containing the character
  # identified by the supplied node-handle/index pair.
  #
  proc TagWord {me node idx} {
    upvar $me O
    foreach {i1 i2} [ToWord $node $idx] {}
    $O(myHtml) tag add selection $node $i1 $node $i2
  }

  # Remove the widget tag "selection" to the word containing the character
  # identified by the supplied node-handle/index pair.
  #
  proc UntagWord {me node idx} {
    upvar $me O
    foreach {i1 i2} [ToWord $node $idx] {}
    $O(myHtml) tag remove selection $node $i1 $node $i2
  }

  proc ToBlock {me node idx} {
    upvar $me O
    set t [$O(myHtml) text text]
    set offset [$O(myHtml) text offset $node $idx]

    set start [string last "\n" $t $offset]
    if {$start < 0} {set start 0}
    set end   [string first "\n" $t $offset]
    if {$end < 0} {set end [string length $t]}

    set start_idx [$O(myHtml) text index $start]
    set end_idx   [$O(myHtml) text index $end]

    return [concat $start_idx $end_idx]
  }

  proc TagBlock {me node idx} {
    upvar $me O
    foreach {n1 i1 n2 i2} [ToBlock $me $node $idx] {}
    $O(myHtml) tag add selection $n1 $i1 $n2 $i2
  }
  proc UntagBlock {me node idx} {
    upvar $me O
    foreach {n1 i1 n2 i2} [ToBlock $me $node $idx] {}
    catch {$O(myHtml) tag remove selection $n1 $i1 $n2 $i2}
  }

  proc doublepress {me x y} {
    upvar $me O

    # Double click -> Select by word.
    clear $me
    set O(myMode) word
    set O(myState) true
    motion $me "" $x $y
  }

  proc triplepress {me x y} {
    upvar $me O

    # Triple click -> Select by block.
    clear $me
    set O(myMode) block
    set O(myState) true
    motion $me "" $x $y
  }

  proc release {me x y} {
    upvar $me O
    set O(myState) false
  }

  proc reset {me} {
    upvar $me O

    set O(myState) false

    # Unset the myFromNode variable, since the node handle it (may) refer 
    # to is now invalid. If this is not done, a future call to the [selected]
    # method of this object will cause an error by trying to use the
    # (now invalid) node-handle value in $myFromNode.
    set O(myFromNode) ""
    set O(myToNode) ""
  }

  proc motion {me N x y} {
    upvar $me O
    if {!$O(myState) || $O(myIgnoreMotion)} return

    set to [$O(myHtml) node -index $x $y]
    foreach {toNode toIdx} $to {}

    # $N containst the node-handle for the node that the cursor is
    # currently hovering over (according to the mousemanager component).
    # If $N is in a different stacking-context to the closest text, 
    # do not update the highlighted region in this event.
    #
    if {$N ne "" && [info exists toNode]} {
      if {[$N stacking] ne [$toNode stacking]} {
        set to ""
      }
    }

    if {[llength $to] > 0} {
  
      if {$O(myFromNode) eq ""} {
        set O(myFromNode) $toNode
        set O(myFromIdx) $toIdx
      }
  
      # This block is where the "selection" tag is added to the HTML 
      # widget (so that the selected text is highlighted). If some
      # javascript has been messing with the tree, then either or
      # both of $myFromNode and $myToNode may be orphaned or deleted.
      # If so, catch the exception and clear the selection.
      #
      set rc [catch {
        if {$O(myToNode) ne $toNode || $toIdx != $O(myToIdx)} {
          switch -- $O(myMode) {
            char {
              if {$O(myToNode) ne ""} {
                $O(myHtml) tag remove selection $O(myToNode) $O(myToIdx) $toNode $toIdx
              }
              $O(myHtml) tag add selection $O(myFromNode) $O(myFromIdx) $toNode $toIdx
              if {$O(myFromNode) ne $toNode || $O(myFromIdx) != $toIdx} {
                selection own [$O(myHv3) win]
              }
            }
    
            word {
              if {$O(myToNode) ne ""} {
                $O(myHtml) tag remove selection $O(myToNode) $O(myToIdx) $toNode $toIdx
                $me UntagWord $O(myToNode) $O(myToIdx)
              }
    
              $O(myHtml) tag add selection $O(myFromNode) $O(myFromIdx) $toNode $toIdx
              $me TagWord $toNode $toIdx
              $me TagWord $O(myFromNode) $O(myFromIdx)
              selection own [$O(myHv3) win]
            }
    
            block {
              set to_block2  [$me ToBlock $toNode $toIdx]
              set from_block [$me ToBlock $O(myFromNode) $O(myFromIdx)]
    
              if {$O(myToNode) ne ""} {
                set to_block [$me ToBlock $O(myToNode) $O(myToIdx)]
                $O(myHtml) tag remove selection $O(myToNode) $O(myToIdx) $toNode $toIdx
                eval $O(myHtml) tag remove selection $to_block
              }
    
              $O(myHtml) tag add selection $O(myFromNode) $O(myFromIdx) $toNode $toIdx
              eval $O(myHtml) tag add selection $to_block2
              eval $O(myHtml) tag add selection $from_block
              selection own [$O(myHv3) win]
            }
          }
    
          set O(myToNode) $toNode
          set O(myToIdx) $toIdx
        }
      } msg]

      if {$rc && [regexp {[^ ]+ is an orphan} $msg]} {
        $me clear
      }
    }

    set motioncmd ""
    set win [$O(myHv3) win]
    if {$y > [winfo height $win]} {
      set motioncmd [list yview scroll 1 units]
    } elseif {$y < 0} {
      set motioncmd [list yview scroll -1 units]
    } elseif {$x > [winfo width $win]} {
      set motioncmd [list xview scroll 1 units]
    } elseif {$x < 0} {
      set motioncmd [list xview scroll -1 units]
    }

    if {$motioncmd ne ""} {
      set O(myIgnoreMotion) 1
      eval $O(myHv3) $motioncmd
      after 20 [list $me ContinueMotion]
    }
  }

  proc ContinueMotion {me} {
    upvar $me O
    set win [$O(myHv3) win]
    set O(myIgnoreMotion) 0
    set x [expr [winfo pointerx $win] - [winfo rootx $win]]
    set y [expr [winfo pointery $win] - [winfo rooty $win]]
    set N [lindex [$O(myHv3) node $x $y] 0]
    $me motion $N $x $y
  }

  # get_selection OFFSET MAXCHARS
  #
  #     This command is invoked whenever the current selection is selected
  #     while it is owned by the html widget. The text of the selected
  #     region is returned.
  #
  proc get_selection {me offset maxChars} {
    upvar $me O
    set t [$O(myHv3) html text text]

    set n1 $O(myFromNode)
    set i1 $O(myFromIdx)
    set n2 $O(myToNode)
    set i2 $O(myToIdx)

    set stridx_a [$O(myHv3) html text offset $O(myFromNode) $O(myFromIdx)]
    set stridx_b [$O(myHv3) html text offset $O(myToNode) $O(myToIdx)]
    if {$stridx_a > $stridx_b} {
      foreach {stridx_a stridx_b} [list $stridx_b $stridx_a] {}
    }

    if {$O(myMode) eq "word"} {
      set stridx_a [string wordstart $t $stridx_a]
      set stridx_b [string wordend $t $stridx_b]
    }
    if {$O(myMode) eq "block"} {
      set stridx_a [string last "\n" $t $stridx_a]
      if {$stridx_a < 0} {set stridx_a 0}
      set stridx_b [string first "\n" $t $stridx_b]
      if {$stridx_b < 0} {set stridx_b [string length $t]}
    }
  
    set T [string range $t $stridx_a [expr $stridx_b - 1]]
    set T [string range $T $offset [expr $offset + $maxChars]]

    return $T
  }

  proc selected {me} {
    upvar $me O
    if {$O(myFromNode) eq ""} {return ""}
    return [$me get_selection 0 10000000]
  }

  proc destroy {me} {
    array unset $me
    rename $me {}
  }
}
::hv3::make_constructor ::hv3::hv3::selectionmanager
#
# End of ::hv3::hv3::selectionmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hv3::dynamicmanager
#
#     This class is responsible for setting the dynamic :hover flag on
#     document nodes in response to cursor movements. It may one day
#     be extended to handle :focus and :active, but it's not yet clear
#     exactly how these should be dealt with.
#
namespace eval ::hv3::hv3::dynamicmanager {

  proc new {me hv3} {
    $hv3 Subscribe onmouseover [list $me handle_mouseover]
    $hv3 Subscribe onmouseout  [list $me handle_mouseout]
    $hv3 Subscribe onmousedown [list $me handle_mousedown]
    $hv3 Subscribe onmouseup   [list $me handle_mouseup]
  }
  proc destroy {me} {
    uplevel #0 [list unset $me]
    rename $me ""
  }

  proc handle_mouseover {me node} { $node dynamic set hover }
  proc handle_mouseout {me node}  { $node dynamic clear hover }
  proc handle_mousedown {me node} { $node dynamic set active }
  proc handle_mouseup {me node}   { $node dynamic clear active }
}
::hv3::make_constructor ::hv3::hv3::dynamicmanager
#
# End of ::hv3::hv3::dynamicmanager
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Class ::hv3::hv3::hyperlinkmanager
#
# Each instance of the hv3 widget contains a single hyperlinkmanager as
# a component. The hyperlinkmanager takes care of:
#
#     * -targetcmd option and associate callbacks
#     * -isvisitedcmd option and associate callbacks
#     * Modifying the cursor to the hand shape when over a hyperlink
#     * Setting the :link or :visited dynamic condition on hyperlink 
#       elements (depending on the return value of -isvisitedcmd).
#
# This class installs a node handler for <a> elements. It also subscribes
# to the <Motion>, <ButtonPress-1> and <ButtonRelease-1> events on the
# associated hv3 widget.
#
namespace eval ::hv3::hv3::hyperlinkmanager {

  proc new {me hv3 baseuri} {
    upvar $me O

    set O(myHv3) $hv3

    set O(myBaseUri) $baseuri

    set O(myLinkHoverCount) 0

    set O(-targetcmd) [list ::hv3::ReturnWithArgs $hv3]

    set O(-isvisitedcmd) [list ::hv3::ReturnWithArgs 0]

    configure-isvisitedcmd $me
    $O(myHv3) Subscribe onclick [list $me handle_onclick]
  }

  proc reset {me} {
    upvar $me O
    set O(myLinkHoverCount) 0
  }

  # This is the configure method for the -isvisitedcmd option. This
  # option configures a callback script that sets or clears the 'visited' 
  # and 'link' properties of an <a href="..."> element. This is a 
  # performance critical operation because it is called so many times.
  #
  proc configure-isvisitedcmd {me} {
    upvar $me O

    # Create a proc to use as the node-handler for <a> elements.
    #
    set P_NODE ${me}.a_node_handler
    catch {rename $P_NODE ""}
    set template [list \
      proc $P_NODE {node} {
        if {![catch {
          set uri [%BASEURI% resolve [$node attr href]]
        }]} {
          if {[%VISITEDCMD% $uri]} {
            $node dynamic set visited
          } else {
            $node dynamic set link
          }
        }
      }
    ]
    eval [::hv3::Expand $template \
        %BASEURI% $O(myBaseUri) %VISITEDCMD% $O(-isvisitedcmd)
    ]

    # Create a proc to use as the attribute-handler for <a> elements.
    #
    set P_ATTR ${me}.a_attr_handler
    catch {rename $P_ATTR ""}
    set template [list \
      proc $P_ATTR {node attr val} {
        if {$attr eq "href"} {
          if {![catch {
            set uri [%BASEURI% resolve $val]
          }]} {
            if {[%VISITEDCMD% $uri]} {
              $node dynamic set visited
            } else {
              $node dynamic set link
            }
          }
        }
      }
    ]
    eval [::hv3::Expand $template \
        %BASEURI% $O(myBaseUri) %VISITEDCMD% $O(-isvisitedcmd)
    ]

    $O(myHv3) html handler node a $P_NODE
    $O(myHv3) html handler attribute a $P_ATTR
  }

  # This method is called whenever an onclick event occurs. If the
  # node is an <A> with an "href" attribute that is not "#" or the
  # empty string, call the [goto] method of some hv3 widget to follow 
  # the hyperlink.
  #
  # The particular hv3 widget is located by evaluating the -targetcmd 
  # callback script. This allows the upper layer to implement frames,
  # links that open in new windows/tabs - all that irritating stuff :)
  #
  proc handle_onclick {me node} {
    upvar $me O
    if {[$node tag] eq "a"} {
      set href [$node attr -default "" href]
      if {$href ne "" && $href ne "#"} {
        set hv3 [eval [linsert $O(-targetcmd) end $node]]
        set href [$O(myBaseUri) resolve $href]
        after idle [list $hv3 goto $href -referer [$O(myHv3) location]]
      }
    }
  }

  proc destroy {me} {
    catch {rename ${me}.a_node_handler ""}
    catch {rename ${me}.a_attr_handler ""}
  }
}
::hv3::make_constructor ::hv3::hv3::hyperlinkmanager
#
# End of ::hv3::hv3::hyperlinkmanager
#--------------------------------------------------------------------------

namespace eval ::hv3::hv3::framelog {

  proc new {me hv3} {
    upvar $me O

    set O(myHv3) $hv3
    set O(myStyleErrors) {}
    set O(myHtmlDocument) {}
  }
  proc destroy {me} {
    uplevel #0 [list unset $me]
    rename $me ""
  }

  proc loghtml {me data} {
    upvar $me O
    if {![info exists ::hv3::log_source_option]} return
    if {$::hv3::log_source_option} {
      append O(myHtmlDocument) $data
    }
  }

  proc log {me id filename data parse_errors} {
    upvar $me O
    if {![info exists ::hv3::log_source_option]} return
    if {$::hv3::log_source_option} {
      lappend O(myStyleErrors) [list $id $filename $data $parse_errors]
    }
  }

  proc clear {me} {
    upvar $me O
    set O(myStyleErrors) ""
    set O(myHtmlDocument) ""
  }

  proc get {me args} {
    upvar $me O
    switch -- [lindex $args 0] {
      html { 
        return $O(myHtmlDocument)
      }

      css { 
        return $O(myStyleErrors)
      }
    }
  }
}
::hv3::make_constructor ::hv3::hv3::framelog

#--------------------------------------------------------------------------
# Class hv3 - the public widget class.
#
namespace eval ::hv3::hv3 {

  proc theselectionmanager {me args} {
    upvar #0 $me O
    eval $O(mySelectionManager) $args
  }
  proc log {me args} {
    upvar #0 $me O
    eval $O(myFrameLog) $args
  }
  proc uri {me args} {
    upvar #0 $me O
    eval $O(myUri) $args
  }

  proc Subscribe {me args} {
    upvar #0 $me O
    eval $O(myMouseManager) subscribe $args
  }
  proc selected {me args} {
    upvar #0 $me O
    eval $O(mySelectionManager) selected $args
  }

  set TextWrapper {
    <html>
      <style>
        body {background-color: #c3c3c3}
        pre  {
          margin: 20px 30px; 
          background-color: #d9d9d9; 
          background-color: white;
          padding: 5px;
          border: 1px solid;
          border-color: #828282 #ffffff #ffffff #828282;
        }
      </style>
    <pre>
  }

  proc new {me args} {
    upvar #0 $me O
    set win $O(win)
	   
    # The scrolled html widget.
    # set O(myHtml) [::hv3::scrolled html $win.html]
    set O(myHtml) $O(hull)
    set O(html) [html $me]
    catch {::hv3::profile::instrument [$O(myHtml) widget]}

    # Current location and base URIs. The default URI is "blank://".
    set O(myUri)  [::tkhtml::uri home://blank/]
    set O(myBase) [::tkhtml::uri home://blank/]

    # Component objects.
    set O(myMouseManager)     [mousemanager       %AUTO% $me]
    set O(myHyperlinkManager) [hyperlinkmanager   %AUTO% $me $O(myBase)]
    set O(mySelectionManager) [selectionmanager   %AUTO% $me]
    set O(myDynamicManager)   [dynamicmanager     %AUTO% $me]
    set O(myFormManager)      [::hv3::formmanager %AUTO% $me]
    set O(myFrameLog)         [framelog           %AUTO% $me]

    set O(-storevisitedcmd) ""

    set O(myStorevisitedDone) 0
    set O(-historydoccmd) ""

    # The option to display images (default true).
    set O(-enableimages) 1

    # The option to execute javascript (default false). 
    #
    # When javascript is enabled, the O(myDom) variable is set to the name of
    # an object of type [::hv3::dom]. When it is not enabled, O(myDom) is
    # an empty string.
    #
    # When the -enablejavascript option is changed from true to false,
    # the O(myDom) object is deleted (and O(myDom) set to the empty 
    # string). But the dom object is not created immediately when 
    # -enablejavascript is changed from false to true. Instead, we
    # wait until the next time the hv3 widget is reset.
    #
    set O(-enablejavascript) 0
    set O(myDom) ""

    set O(-scrollbarpolicy) auto

    set O(-locationvar) ""
    set O(-downloadcmd) ""
    set O(-requestcmd) ""

    set O(-frame) ""

    # Full text of referrer URI, if any.
    #
    # Note that the DOM attribute HTMLDocument.referrer has a double-r,
    # but the name of the HTTP header, "Referer", has only one.
    #
    set O(myReferrer) ""
  
    # Used to assign internal stylesheet ids.
    set O(myStyleCount) 0
  
    # This variable may be set to "unknown", "quirks" or "standards".
    set O(myQuirksmode) unknown
  
    set O(myFirstReset) 1
  
    # Current value to set the -cachecontrol option of download handles to.
    #
    set O(myCacheControl) normal
  
    # This variable stores the current type of resource being displayed.
    # When valid, it is set to one of the following:
    #
    #     * html
    #     * image
    #
    # Otherwise, it is set to an empty string, indicating that the resource
    # has been requested, but has not yet arrived.
    #
    set O(myMimetype) ""
  
    # This variable is only used when ($O(myMimetype) eq "image"). It stores
    # the data for the image about to be displayed. Once the image
    # has finished downloading, the data in this variable is loaded into
    # a Tk image and this variable reset to "".
    #
    set O(myImageData) ""
  
    # If this variable is not set to the empty string, it is the id of an
    # [after] event that will refresh the current document (i.e from a 
    # Refresh header or <meta type=http-equiv> markup). This scheduled 
    # event should be cancelled when the [reset] method is called.
    #
    # There should only be one Refresh event scheduled at any one time.
    # The [Refresh] method, which calls [after] to schedule the events,
    # cancels any pending event before scheduling a new one.
    #
    set O(myRefreshEventId) ""
  
    # This boolean variable is set to zero until the first call to [goto].
    # Before that point it is safe to change the values of the -enableimages
    # option without reloading the document.
    #
    set O(myGotoCalled) 0
  
    # This boolean variable is set after the DOM "onload" event is fired.
    # It is cleared by the [reset] method.
    set O(myOnloadFired) 0
  
    set O(myFragmentSeek) ""
  
    # The ::hv3::request object used to retrieve the main document.
    #
    set O(myDocumentHandle) ""
  
    # List of handle objects that should be released after the page has
    # loaded. This is part of the hack to work around the polipo bug.
    #
    set O(myShelvedHandles) [list]
  
    # List of all active download handles.
    #
    set O(myActiveHandles) [list]
  
    set O(myTitleVar) ""

    $O(myMouseManager) subscribe motion [list $O(mySelectionManager) motion]

    $O(myFormManager) configure -getcmd  [list $me Formcmd get]
    $O(myFormManager) configure -postcmd [list $me Formcmd post]

    # Attach an image callback to the html widget. Store images as 
    # pixmaps only when possible to save memory.
    $O(myHtml) configure -imagecmd [list $me Imagecmd] -imagepixmapify 1

    # Register node handlers to deal with the various elements
    # that may appear in the document <head>. In html, the <head> section
    # may contain the following elements:
    #
    #     <script>, <style>, <meta>, <link>, <object>, <base>, <title>
    #
    # All except <title> are handled by code in ::hv3::hv3. Note that the
    # handler for <object> is the same whether the element is located in
    # the head or body of the html document.
    #
    $O(myHtml) handler node   link     [list $me link_node_handler]
    $O(myHtml) handler node   base     [list $me base_node_handler]
    $O(myHtml) handler node   meta     [list $me meta_node_handler]
    $O(myHtml) handler node   title    [list $me title_node_handler]
    $O(myHtml) handler script style    [list $me style_script_handler]
    $O(myHtml) handler script script   [list ::hv3::ignore_script]

    # Register handler commands to handle <body>.
    $O(myHtml) handler node body   [list $me body_node_handler]

    bind $win <Configure>  [list $me goto_fragment]
    #bind [html $me].document <Visibility> [list $me VisibilityChange %s]

    eval $me configure $args
  }

  # Destructor. This is called automatically when the window is destroyed.
  #
  proc destroy {me} {
    upvar #0 $me O

    # Cancel any and all pending downloads.
    #
    $me stop
    catch {$O(myDocumentHandle) release}

    # Destroy the components. We don't need to destroy the scrolled
    # html component because it is a Tk widget - it is automatically
    # destroyed when it's parent widget is.
    catch { $O(mySelectionManager) destroy }
    catch { $O(myDynamicManager)   destroy }
    catch { $O(myHyperlinkManager) destroy }
    catch { $O(myUri)              destroy }
    catch { $O(myFormManager)      destroy }
    catch { $O(myMouseManager)     destroy }
    catch { $O(myBase)             destroy }
    catch { $O(myDom)              destroy }

    # Cancel any refresh-event that may be pending.
    if {$O(myRefreshEventId) ne ""} {
      after cancel $O(myRefreshEventId)
      set O(myRefreshEventId) ""
    }

    unset $me
    rename $me {}
  }

  proc VisibilityChange {me state} {
    upvar #0 $me O

    switch -- $state {
      VisibilityUnobscured {
        set enablelayout 1
      }
      VisibilityPartiallyObscured {
        set enablelayout 1
      }
      VisibilityFullyObscured {
        set enablelayout 0
      }
    }
    if {[$O(myHtml) cget -enablelayout] != $enablelayout} {
      $O(myHtml) configure -enablelayout $enablelayout
    }
  }

  # Return the location URI of the widget.
  #
  proc location {me} { 
    upvar #0 $me O
    return [$O(myUri) get] 
  }

  # Return the referrer URI of the widget.
  #
  proc referrer {me} { 
    upvar #0 $me O
    return $O(myReferrer) 
  }

  proc Forget {me handle} {
    upvar #0 $me O
    set idx [lsearch $O(myActiveHandles) $handle]
    set O(myActiveHandles) [lreplace $O(myActiveHandles) $idx $idx]
  }

  # The argument download-handle contains a configured request. This 
  # method initiates the request. 
  #
  # This method is used by hv3 and it's component objects (i.e. code in
  # hv3_object_handler). Also the dom code, for XMLHTTPRequest.
  #
  proc makerequest {me downloadHandle} {            # PRIVATE
    upvar #0 $me O

    lappend O(myActiveHandles) $downloadHandle
    $downloadHandle finish_hook [list $me Forget $downloadHandle]

    # Execute the -requestcmd script. Fail the download and raise
    # an exception if an error occurs during script evaluation.
    set cmd [concat $O(-requestcmd) [list $downloadHandle]]
    set rc [catch $cmd errmsg]
    if {$rc} {
      #set einfo $::errorInfo
      #error $errmsg $einfo
      puts "Error in -requestcmd [$downloadHandle cget -uri]: $errmsg"
      catch {$downloadHandle destroy}
    }
  }

  # Based on the current contents of instance variable $O(myUri), set the
  # variable identified by the -locationvar option, if any.
  #
  proc SetLocationVar {me } {
    upvar #0 $me O
    if {$O(-locationvar) ne ""} {
      uplevel #0 [list set $O(-locationvar) [$O(myUri) get]]
    }
    event generate $O(win) <<Location>>
  }

  proc MightBeComplete {me } {
    upvar #0 $me O
    if {[llength $O(myActiveHandles)] == 0} {
      event generate $O(win) <<Complete>>

      # There are no outstanding HTTP transactions. So fire
      # the DOM "onload" event.
      if {$O(myDom) ne "" && !$O(myOnloadFired)} {
        set O(myOnloadFired) 1
        set bodynode [$O(myHtml) search body]
	# Workaround. Currently meta reload causes empty completion.
	# XXX: Check this again!
	if {[llength $bodynode]} {
          $O(myDom) event load [lindex $bodynode 0]
	}
      }
    }
  }

  proc onload_fired {me } { 
    upvar #0 $me O
    return $O(myOnloadFired) 
  }

  # PUBLIC METHOD.
  #
  proc resolve_uri {me uri} {
    upvar #0 $me O
    if {$uri eq ""} {
      set ret "[$O(myBase) scheme]://[$O(myBase) authority][$O(myBase) path]"
    } else {
      set ret [$O(myBase) resolve $uri]
    }
    return $ret
  }

  # This proc is registered as the -imagecmd script for the Html widget.
  # The argument is the URI of the image required.
  #
  # This proc creates a Tk image immediately. It also kicks off a fetch 
  # request to obtain the image data. When the fetch request is complete,
  # the contents of the Tk image are set to the returned data in proc 
  # ::hv3::imageCallback.
  #
  proc Imagecmd {me uri} {
    upvar #0 $me O

    # Massage the URI a bit. Trim whitespace from either end.
    set uri [string trim $uri]

    if {[string match replace:* $uri]} {
        set img [string range $uri 8 end]
        return $img
    }
    set name [image create photo]

    if {$uri ne ""} {
      set full_uri [$me resolve_uri $uri]
    
      # Create and execute a download request. For now, "expect" a mime-type
      # of image/gif. This should be enough to tell the protocol handler to
      # expect a binary file (of course, this is not correct, the real
      # default mime-type might be some other kind of image).
      set handle [::hv3::request %AUTO%                \
          -uri          $full_uri                      \
          -mimetype     image/gif                      \
          -cachecontrol $O(myCacheControl)             \
          -cacheable    0                              \
      ]
      $handle configure -finscript [list $me Imagecallback $handle $name]
      $me makerequest $handle
    }

    # Return a list of two elements - the image name and the image
    # destructor script. See tkhtml(n) for details.
    return [list $name [list image delete $name]]
  }

  # This method is called to handle the "Location" header for all requests
  # except requests for the main document (see the [Refresh] method for
  # these). If there is a Location method, then the handle object is
  # destroyed, a new one dispatched and 1 returned. Otherwise 0 is returned.
  #
  proc HandleLocation {me handle} {
    upvar #0 $me O
    # Check for a "Location" header. TODO: Handling Location
    # should be done in one common location for everything except 
    # the main document. The main document is a bit different...
    # or is it?
    set location ""
    foreach {header value} [$handle cget -header] {
      if {[string equal -nocase $header "Location"]} {
        set location $value
      }
    }

    if {$location ne ""} {
      set finscript [$handle cget -finscript]
      $handle release
      set full_location [$me resolve_uri $location]
      set handle2 [::hv3::request $handle               \
          -uri          $full_location                   \
          -mimetype     image/gif                        \
          -cachecontrol $O(myCacheControl)                  \
      ]
      $handle2 configure -finscript $finscript
      $me makerequest $handle2
      return 1
    }
    return 0
  }

  # This proc is called when an image requested by the -imagecmd callback
  # ([imagecmd]) has finished downloading. The first argument is the name of
  # a Tk image. The second argument is the downloaded data (presumably a
  # binary image format like gif). This proc sets the named Tk image to
  # contain the downloaded data.
  #
  proc Imagecallback {me handle name data} {
    upvar #0 $me O
    if {0 == [$me HandleLocation $handle]} {
      # If the image data is invalid, it is not an error. Possibly hv3
      # should log a warning - if it had a warning system....
      catch { $name configure -data $data }
      $handle release
    }
  }

  # Request the resource located at URI $full_uri and treat it as
  # a stylesheet. The parent stylesheet id is $parent_id. This
  # method is used for stylesheets obtained by either HTML <link> 
  # elements or CSS "@import {...}" directives.
  #
  proc Requeststyle {me parent_id full_uri} {
    upvar #0 $me O
    set id        ${parent_id}.[format %.4d [incr O(myStyleCount)]]
    set importcmd [list $me Requeststyle $id]
    set urlcmd    [list ::hv3::ss_resolve_uri $full_uri]
    append id .9999

    set handle [::hv3::request %AUTO%               \
        -uri         $full_uri                      \
        -mimetype    text/css                       \
        -cachecontrol $O(myCacheControl)            \
        -cacheable 1                                \
    ]
    $handle configure -finscript [
        list $me Finishstyle $handle $id $importcmd $urlcmd
    ]
    $me makerequest $handle
  }

  # Callback invoked when a stylesheet request has finished. Made
  # from method Requeststyle above.
  #
  proc Finishstyle {me handle id importcmd urlcmd data} {
    upvar #0 $me O
    if {0 == [$me HandleLocation $handle]} {
      set full_id "$id.[$handle cget -uri]"
      $O(html) style             \
          -id $full_id           \
          -importcmd $importcmd  \
          -urlcmd $urlcmd        \
          -errorvar parse_errors \
          $data

      $O(myFrameLog) log $full_id [$handle cget -uri] $data $parse_errors

      $me goto_fragment
      $me MightBeComplete
      $handle release
    }
  }

  # Node handler script for <meta> tags.
  #
  proc meta_node_handler {me node} {
    upvar #0 $me O
    set httpequiv [string tolower [$node attr -default "" http-equiv]]
    set content   [$node attr -default "" content]

    switch -- $httpequiv {
      refresh {
        $me Refresh $content
      }

      content-type {
        foreach {a b enc} [::hv3::string::parseContentType $content] {}
        if {
           ![$O(myDocumentHandle) cget -hastransportencoding] &&
           ![::hv3::encoding_isequal $enc [$me encoding]]
        } {
          # This occurs when a document contains a <meta> element that
          # specifies a character encoding and the document was 
          # delivered without a transport-layer encoding (Content-Type
          # header). We need to start reparse the document from scratch
          # using the new encoding.
          #
          # We need to be careful to work around a polipo bug here: If
          # there are more than two requests for a single resource
          # to a single polipo process, and one of the requests is 
          # cancelled, then the other (still active) request is truncated
          # by polipo. The polipo developers acknowledge that this is
          # a bug, but as it doesn't come up very often in normal polipo
          # usage it is not likely to be fixed soon.
          #
          # It's a problem for Hv3 because if the following [reset] cancels
          # any requests, then when reparsing the same document with a
          # different encoding the same resources are requested, we are 
          # likely to trigger this bug.
          #
          puts "INFO: This page triggers meta enc reload"
          
          # For all active handles except the document handle, configure
          # the -incrscript as a no-op, and have the finscript simply 
          # release the handle reference. This means the polipo bug will
          # not be triggered.
          foreach h $O(myActiveHandles) {
            if {$h ne $O(myDocumentHandle)} {
              set fin [list ::hv3::release_handle $h]
              $h configure -incrscript "" -finscript $fin
            }
          }

          $me InternalReset
          $O(myDocumentHandle) configure -encoding $enc
          $me HtmlCallback                 \
              $O(myDocumentHandle)              \
              [$O(myDocumentHandle) isFinished] \
              [$O(myDocumentHandle) data]
        }
      }
    }
  }

  # Return the default encoding that should be used for 
  # javascript and CSS resources.
  proc encoding {me} {
    upvar #0 $me O
    if {$O(myDocumentHandle) eq ""} { 
      return [encoding system] 
    }
    return [$O(myDocumentHandle) encoding]
  }

  # This method is called to handle "Refresh" and "Location" headers
  # delivered as part of the response to a request for a document to
  # display in the main window. Refresh headers specified as 
  # <meta type=http-equiv> markup are also handled. The $content argument
  # contains a the content portion of the Request header, for example:
  #
  #     "5 ; URL=http://www.news.com"
  #
  # (wait 5 seconds before loading the page www.news.com).
  #
  # In the case of Location headers, a synthetic Refresh content header is
  # constructed to pass to this method.
  #
  # Returns 1 if immediate refresh (seconds = 0) is requested.
  #
  proc Refresh {me content} {
    upvar #0 $me O
    # Use a regular expression to extract the URI and number of seconds
    # from the header content. Then dequote the URI string.
    set uri ""
    set re {([[:digit:]]+) *; *[Uu][Rr][Ll] *= *([^ ]+)}
    regexp $re $content -> seconds uri
    regexp {[^\"\']+} $uri uri                  ;# Primitive dequote

    if {$uri ne ""} {
      if {$O(myRefreshEventId) ne ""} {
          after cancel $O(myRefreshEventId)
      }
      set cmd [list $me RefreshEvent $uri]
      set O(myRefreshEventId) [after [expr {$seconds*1000}] $cmd]

      # puts "Parse of content for http-equiv refresh successful! ($uri)"

      return [expr {$seconds == 0}]
    } else {
      # puts "Parse of content for http-equiv refresh failed..."
      return 0
    }
  }

  proc RefreshEvent {me uri} {
    upvar #0 $me O
    set O(myRefreshEventId) ""
    $me goto $uri -nosave
  }

  # System for handling <title> elements. This object exports
  # a method [titlevar] that returns a globally valid variable name
  # to a variable used to store the string that should be displayed as the
  # "title" of this document. The idea is that the caller add a trace
  # to that variable.
  #
  proc title_node_handler {me node} {
    upvar #0 $me O
    set val ""
    foreach child [$node children] {
      append val [$child text]
    }
    set O(myTitleVar) $val
  }
  proc titlevar {me}    {
    return ::${me}(myTitleVar)
  }
  proc title {me} {
    upvar #0 $me O
    return $O(myTitleVar)
  }

  # Node handler script for <body> tags. The purpose of this handler
  # and the [body_style_handler] method immediately below it is
  # to handle the 'overflow' property on the document root element.
  # 
  proc body_node_handler {me node} {
    upvar #0 $me O
    $node replace dumO(my) -stylecmd [list $me body_style_handler $node]
  }
  proc body_style_handler {me bodynode} {
    upvar #0 $me O

    if {$O(-scrollbarpolicy) ne "auto"} {
      $O(myHtml) configure -scrollbarpolicy $O(-scrollbarpolicy)
      return
    }

    set htmlnode [$bodynode parent]
    set overflow [$htmlnode property overflow]

    # Variable $overflow now holds the value of the 'overflow' property
    # on the root element (the <html> tag). If this value is not "visible",
    # then the value is used to govern the viewport scrollbars. If it is
    # visible, then use the value of 'overflow' on the <body> element.
    # See section 11.1.1 of CSS2.1 for details.
    #
    if {$overflow eq "visible"} {
      set overflow [$bodynode property overflow]
    }
    switch -- $overflow {
      visible { $O(myHtml) configure -scrollbarpolicy auto }
      auto    { $O(myHtml) configure -scrollbarpolicy auto }
      hidden  { $O(myHtml) configure -scrollbarpolicy 0 }
      scroll  { $O(myHtml) configure -scrollbarpolicy 1 }
      default {
        puts stderr "Hv3 is confused: <body> has \"overflow:$overflow\"."
        $O(myHtml) configure -scrollbarpolicy auto
      }
    }
  }

  # Node handler script for <link> tags.
  #
  proc link_node_handler {me node} {
    upvar #0 $me O
    set rel  [string tolower [$node attr -default "" rel]]
    set href [string trim [$node attr -default "" href]]
    set media [string tolower [$node attr -default all media]]
    if {
        [string match *stylesheet* $rel] &&
        ![string match *alternat* $rel] &&
        $href ne "" && 
        [regexp all|screen $media]
    } {
      set full_uri [$me resolve_uri $href]
      $me Requeststyle author $full_uri
    }
  }

  # Node handler script for <base> tags.
  #
  proc base_node_handler {me node} {
    upvar #0 $me O
    # Technically, a <base> tag is required to specify an absolute URI.
    # If a relative URI is specified, hv3 resolves it relative to the
    # current location URI. This is not standards compliant (a relative URI
    # is technically illegal), but seems like a reasonable idea.
    $O(myBase) load [$node attr -default "" href]
  }

  # Script handler for <style> tags.
  #
  proc style_script_handler {me attr script} {
    upvar #0 $me O
    array set attributes $attr
    if {[info exists attributes(media)]} {
      if {0 == [regexp all|screen $attributes(media)]} return ""
    }

    set id        author.[format %.4d [incr O(myStyleCount)]]
    set importcmd [list $me Requeststyle $id]
    set urlcmd    [list $me resolve_uri]
    append id ".9999.<style>"
    $O(html) style -id $id     \
        -importcmd $importcmd  \
        -urlcmd $urlcmd        \
        -errorvar parse_errors \
        $script

    $O(myFrameLog) log $id "<style> block $O(myStyleCount)" $script $parse_errors

    return ""
  }

  proc goto_fragment {me } {
    upvar #0 $me O
    switch -- [llength $O(myFragmentSeek)] {
      0 { # Do nothing }
      1 {
        $O(myHtml) yview moveto [lindex $O(myFragmentSeek) 0]
      }
      2 {
        set fragment [lindex $O(myFragmentSeek) 1]
        set selector [format {[name="%s"]} $fragment]
        set goto_node [lindex [$O(myHtml) search $selector] 0]

        # If there was no node with the name attribute set to the fragment,
        # search for a node with the id attribute set to the fragment.
        if {$goto_node eq ""} {
          set selector [format {[id="%s"]} $fragment]
          set goto_node [lindex [$O(myHtml) search $selector] 0]
        }
  
        if {$goto_node ne ""} {
          $O(myHtml) yview $goto_node
        }
      }
    }
  }

  proc seek_to_fragment {me fragment} {
    upvar #0 $me O

    # A fragment was specified as part of the URI that has just started
    # loading. Set O(myFragmentSeek) to the fragment name. Each time some
    # more of the document or a stylesheet loads, the [goto_fragment]
    # method will try to align the vertical scrollbar so that the 
    # named fragment is at the top of the view.
    #
    # If and when the user manually scrolls the viewport, the 
    # O(myFragmentSeek) variable is cleared. This is so we don't wrest
    # control of the vertical scrollbar after the user has manually
    # positioned it.
    #
    $O(myHtml) take_control [list set ::${me}(myFragmentSeek) ""]
    if {$fragment ne ""} {
      set O(myFragmentSeek) [list # $fragment]
    }
  }

  proc seek_to_yview {me moveto} {
    upvar #0 $me O
    $O(myHtml) take_control [list set ::${me}(myFragmentSeek) ""]
    set O(myFragmentSeek) $moveto
  }

  proc documenthandle {me } {
    upvar #0 $me O
    return $O(myDocumentHandle)
  }

  proc documentcallback {me handle referrer savestate final data} {
    upvar #0 $me O

    if {$O(myMimetype) eq ""} {
  
      # TODO: Real mimetype parser...
      set mimetype  [string tolower [string trim [$handle cget -mimetype]]]
      foreach {major minor} [split $mimetype /] {}

      switch -- $major {
        text {
          if {[lsearch [list html xml xhtml] $minor]>=0} {
            set q [::hv3::configure_doctype_mode $O(myHtml) $data isXHTML]
            $me reset $savestate
            set O(myQuirksmode) $q
            if {$isXHTML} { $O(myHtml) configure -parsemode xhtml } \
            else          { $O(myHtml) configure -parsemode html }
            set O(myMimetype) html
          } else {
            # Plain text mode.
            $me reset $savestate
            $O(myHtml) parse $::hv3::hv3::TextWrapper
            set O(myMimetype) text
	  }
        }
  
        image {
          set O(myImageData) ""
          $me reset $savestate
          set O(myMimetype) image
        }
      }

      # If there is a "Location" or "Refresh" header, handle it now.
      set refreshheader ""
      foreach {name value} [$handle cget -header] {
        switch -- [string tolower $name] {
          location {
            set refreshheader "0 ; URL=$value"
          }
          refresh {
            set refreshheader $value
          }
        }
      }

      set isImmediateRefresh [$me Refresh $refreshheader]
  
      if {!$isImmediateRefresh && $O(myMimetype) eq ""} {
        # Neither text nor an image. This is the upper layers problem.
        if {$O(-downloadcmd) ne ""} {
          # Remove the download handle from the list of handles to cancel
          # if [$hv3 stop] is invoked (when the user clicks the "stop" button
          # we don't want to cancel pending save-file operations).
          $me Forget $handle
          eval [linsert $O(-downloadcmd) end $handle $data $final]
        } else {
          $handle release
          set sheepish "Don't know how to handle \"$mimetype\""
          tk_dialog .apology "Sheepish apology" $sheepish 0 OK
        }
        return
      }

      $O(myUri)  load [$handle cget -uri]
      $O(myBase) load [$O(myUri) get]
      $me SetLocationVar

      if {$isImmediateRefresh} {
        $handle release
        return
      }

      set O(myReferrer) $referrer
  
      if {$O(myCacheControl) ne "relax-transparency"} {
        $me seek_to_fragment [$O(myUri) fragment]
      }

      set O(myStyleCount) 0
    }

    if {$O(myDocumentHandle) ne $handle} {
      if {$O(myDocumentHandle) ne ""} {
        $O(myDocumentHandle) release
      }
      set O(myDocumentHandle) $handle
    }

    switch -- $O(myMimetype) {
      text  {$me TextCallback $handle $final $data}
      html  {$me HtmlCallback $handle $final $data}
      image {$me ImageCallback $handle $final $data}
    }


    if {$final} {
      if {$O(myStorevisitedDone) == 0 && $O(-storevisitedcmd) ne ""} {
        set O(myStorevisitedDone) 1
        eval $O(-storevisitedcmd) 1
      }
      $me MightBeComplete
    }
  }

  proc TextCallback {me handle isFinal data} {
    upvar #0 $me O
    set z [string map {< &lt; > &gt;} $data]
    if {$isFinal} {
	$O(myHtml) parse -final $data
    } else {
	$O(myHtml) parse $data
    }
  }

  proc HtmlCallback {me handle isFinal data} {
    upvar #0 $me O
    $O(myFrameLog) loghtml $data
    if {$isFinal} {
	$O(html) parse -final $data
    } else {
	$O(html) parse $data
    }
    $me goto_fragment
  }

  proc ImageCallback {me handle isFinal data} {
    upvar #0 $me O
    append O(myImageData) $data
    if {$isFinal} {
      set img [image create photo -data $O(myImageData)]
      set O(myImageData) ""
      set imagecmd [$O(myHtml) cget -imagecmd]
      $O(myHtml) configure -imagecmd [list ::hv3::ReturnWithArgs $img]
      $O(myHtml) parse -final { <img src="unused"> }
      $O(myHtml) _force
      $O(myHtml) configure -imagecmd $imagecmd
    }
  }

  proc Formcmd {me method node uri querytype encdata} {
    upvar #0 $me O
    set cmd [linsert [$me cget -targetcmd] end $node]
    [eval $cmd] Formcmd2 $method $uri $querytype $encdata
  }

  proc Formcmd2 {me method uri querytype encdata} {
    upvar #0 $me O
    puts "Formcmd $method $uri $querytype $encdata"

    set uri_obj [::tkhtml::uri [$me resolve_uri $uri]]

    event generate $O(win) <<Goto>>

    set handle [::hv3::request %AUTO% -mimetype text/html]
    set O(myMimetype) ""
    set referer [$me uri get]
    $handle configure                                       \
        -incrscript [list $me documentcallback $handle $referer 1 0] \
        -finscript  [list $me documentcallback $handle $referer 1 1] \
        -requestheader [list Referer $referer]              \

    if {$method eq "post"} {
      $handle configure -uri [$uri_obj get] -postdata $encdata
      $handle configure -enctype $querytype
      $handle configure -cachecontrol normal
    } else {
      $uri_obj load "?$encdata"
      $handle configure -uri [$uri_obj get]
      $handle configure -cachecontrol $O(myCacheControl)
    }
    $uri_obj destroy
    $me makerequest $handle

    # Grab the keyboard focus for this widget. This is so that after
    # the form is submitted the arrow keys and PgUp/PgDown can be used
    # to scroll the main display.
    #
    focus [$me html]
  }

  proc seturi {me uri} {
    upvar #0 $me O
    $O(myUri) load $uri
    $O(myBase) load [$O(myUri) get]
  }

  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE TO HV3 WIDGET STARTS HERE:
  #
  #     Method              Delegate
  # --------------------------------------------
  #     goto                N/A
  #     xview               $O(myHtml)
  #     yview               $O(myHtml)
  #     html                N/A
  #     hull                N/A
  #   

  proc dom {me} { 
    upvar #0 $me O
    if {$O(myDom) eq ""} { return ::hv3::ignore_script }
    return $O(myDom)
  }

  #--------------------------------------------------------------------
  # Load the URI specified as an argument into the main browser window.
  # This method has the following syntax:
  #
  #     $hv3 goto URI ?OPTIONS?
  #
  # Where supported options are:
  #
  #     -cachecontrol "normal"|"relax-transparency"|"no-cache"
  #     -nosave
  #     -referer URI
  #     -history_handle  DOWNLOAD-HANDLE
  #
  # The -cachecontrol option (default "normal") specifies the value 
  # that will be used for all ::hv3::request objects issued as a 
  # result of this load URI operation.
  #
  # Normally, a <<SaveState>> event is generated. If -nosave is specified, 
  # this is suppressed.
  # 
  proc goto {me uri args} {
    upvar #0 $me O

    set O(myGotoCalled) 1

    # Process the argument switches. Local variable $cachecontrol
    # is set to the effective value of the -cachecontrol option.
    # Local boolean var $savestate is true unless the -nogoto
    # option is specified.
    set savestate 1
    set cachecontrol normal
    set referer ""
    set history_handle ""

    for {set iArg 0} {$iArg < [llength $args]} {incr iArg} {
      switch -- [lindex $args $iArg] {
        -cachecontrol {
          incr iArg
          set cachecontrol [lindex $args $iArg]
        }
        -referer {
          incr iArg
          set referer [lindex $args $iArg]
        }
        -nosave {
          set savestate 0
        }
        -history_handle {
          incr iArg
          set history_handle [lindex $args $iArg]
        }
        default {
          error "Bad option \"[lindex $args $iArg]\" to \[::hv3::hv3 goto\]"
        }
      }
    }

    # Special case. If this URI begins with "javascript:" (case independent),
    # pass it to the current running DOM implementation instead of loading
    # anything into the current browser.
    if {[string match -nocase javascript:* $uri]} {
      if {$O(myDom) ne ""} {
        $O(myDom) javascript [string range $uri 11 end]
      }
      return
    }

    set O(myCacheControl) $cachecontrol

    set current_uri [$O(myUri) get_no_fragment]
    set uri_obj [::tkhtml::uri [$me resolve_uri $uri]]
    set full_uri [$uri_obj get_no_fragment]
    set fragment [$uri_obj fragment]

    # Generate the <<Goto>> event.
    event generate $O(win) <<Goto>>

    if {$full_uri eq $current_uri && $cachecontrol ne "no-cache"} {
      # Save the current state in the history system. This ensures
      # that back/forward controls work when navigating between
      # different sections of the same document.
      if {$savestate} {
        event generate $O(win) <<SaveState>>
      }
      $O(myUri) load $uri

      # If the cache-mode is "relax-transparency", then the history 
      # system is controlling this document load. It has already called
      # [seek_to_yview] to provide a seek offset.
      if {$cachecontrol ne "relax-transparency"} {
        if {$fragment eq ""} {
          $me seek_to_yview 0.0
        } else {
          $me seek_to_fragment $fragment
        }
      }
      $me goto_fragment

      $me SetLocationVar
      return [$O(myUri) get]
    }

    # Abandon any pending requests
    if {$O(myStorevisitedDone) == 0 && $O(-storevisitedcmd) ne ""} {
      set O(myStorevisitedDone) 1
      eval $O(-storevisitedcmd) $savestate
    }
    $me stop
    set O(myMimetype) ""

    if {$history_handle eq ""} {
      # Base the expected type on the extension of the filename in the
      # URI, if any. If we can't figure out an expected type, assume
      # text/html. The protocol handler may override this anyway.
      set mimetype text/html
      set path [$uri_obj path]
      if {[regexp {\.([A-Za-z0-9]+)$} $path dumO(my) ext]} {
        switch -- [string tolower $ext] {
  	jpg  { set mimetype image/jpeg }
          jpeg { set mimetype image/jpeg }
          gif  { set mimetype image/gif  }
          png  { set mimetype image/png  }
          gz   { set mimetype application/gzip  }
          gzip { set mimetype application/gzip  }
          zip  { set mimetype application/gzip  }
          kit  { set mimetype application/binary }
        }
      }
  
      # Create a download request for this resource. We expect an html
      # document, but at this juncture the URI may legitimately refer
      # to kind of resource.
      #
      set handle [::hv3::request %AUTO%              \
          -uri         [$uri_obj get]                \
          -mimetype    $mimetype                     \
          -cachecontrol $O(myCacheControl)           \
          -hv3          $me                          \
      ]
      $handle configure                                                        \
        -incrscript [list $me documentcallback $handle $referer $savestate 0]\
        -finscript  [list $me documentcallback $handle $referer $savestate 1] 
      if {$referer ne ""} {
        $handle configure -requestheader [list Referer $referer]
      }
  
      $me makerequest $handle
    } else {
      # The history system has supplied the data to load into the widget.
      # Use $history_handle instead of creating a new request.
      #
      $history_handle reference
      $me documentcallback $history_handle $referer $savestate 1 [
        $history_handle data
      ]
      $me goto_fragment
    }
    $uri_obj destroy
  }

  # Abandon all currently pending downloads. This method is 
  # part of the public interface.
  #
  proc stop {me } {
    upvar #0 $me O

    foreach dl $O(myActiveHandles) { 
      if {$dl eq $O(myDocumentHandle)} {
        set O(myDocumentHandle) ""
      }
      $dl release 
    }

    if {$O(myStorevisitedDone) == 0 && $O(-storevisitedcmd) ne ""} {
      set O(myStorevisitedDone) 1
      eval $O(-storevisitedcmd) 1
    }
  }

  proc InternalReset {me } {
    upvar #0 $me O

    $O(myFrameLog) clear

    foreach m [list \
        $O(myMouseManager) $O(myFormManager)          \
        $O(mySelectionManager) $O(myHyperlinkManager) \
    ] {
      if {$m ne ""} {$m reset}
    }
    $O(html) reset
    $O(myHtml) configure -scrollbarpolicy $O(-scrollbarpolicy)

    catch {$O(myDom) destroy}
    if {$O(-enablejavascript)} {
      set O(myDom) [::hv3::dom %AUTO% $me]
      $O(myHtml) handler script script   [list $O(myDom) script]
      $O(myHtml) handler script noscript ::hv3::ignore_script
    } else {
      set O(myDom) ""
      $O(myHtml) handler script script   ::hv3::ignore_script
      $O(myHtml) handler script noscript {}
    }
    $O(myMouseManager) configure -dom $O(myDom)
  }

  proc reset {me isSaveState} {
    upvar #0 $me O

    # Clear the "onload-event-fired" flag
    set O(myOnloadFired) 0
    set O(myStorevisitedDone) 0

    # Cancel any pending "Refresh" event.
    if {$O(myRefreshEventId) ne ""} {
      after cancel $O(myRefreshEventId)
      set O(myRefreshEventId) ""
    }

    # Generate the <<Reset>> and <<SaveState> events.
    if {!$O(myFirstReset) && $isSaveState} {
      event generate $O(win) <<SaveState>>
    }
    set O(myFirstReset) 0

    set O(myTitleVar) ""
    set O(myQuirksmode) unknown

    $me InternalReset
  }

  proc configure-enableimages {me} {
    upvar #0 $me O

    # The -enableimages switch. If false, configure an empty string
    # as the html widget's -imagecmd option. If true, configure the
    # same option to call the [Imagecmd] method of this mega-widget.
    #
    # We used to reload the frame contents here. But it turns out
    # that is really inconvenient. If the user wants to reload the
    # document the reload button is right there anyway.
    #
    if {$O(-enableimages)} {
      $O(myHtml) configure -imagecmd [list $me Imagecmd]
    } else {
      $O(myHtml) configure -imagecmd ""
    }
  }

  proc configure-enablejavascript {me} {
    upvar #0 $me O
    if {!$O(-enablejavascript)} {
      catch {$O(myDom) destroy}
      set O(myDom) ""
      $O(myHtml) handler script script   ::hv3::ignore_script
      $O(myHtml) handler script noscript {}
      $O(myMouseManager) configure -dom ""
    }
  }

  proc pending {me}  {
    upvar #0 $me O
    return [llength $O(myActiveHandles)]
  }
  proc html {me args}     { 
    upvar #0 $me O
    if {[llength $args]>0} {
      eval [$O(myHtml) widget] $args
    } else {
      $O(myHtml) widget
    }
  }
  proc hull {me}     { 
    upvar #0 $me O
    return $O(hull)
  }
  proc win {me} {
    upvar #0 $me O
    return $O(win)
  }
  proc me {me} { return $me }

  proc yview {me args} {
    upvar #0 $me O
    eval $O(html) yview $args
  }
  proc xview {me args} {
    upvar #0 $me O
    eval $O(html) xview $args
  }

  proc javascriptlog {me args} {
    upvar #0 $me O
    if {$O(-dom) ne ""} {
      eval $O(-dom) javascriptlog $args
    }
  }

  #proc unknown {method me args} {
    # puts "UNKNOWN: $me $method $args"
    #upvar #0 $me O
    #uplevel 3 [list eval $O(myHtml) $method $args]
  #}
  #namespace unknown unknown

  proc node {me args} { 
    upvar #0 $me O
    eval $O(myHtml) node $args
  }

  set DelegateOption(-isvisitedcmd) myHyperlinkManager
  set DelegateOption(-targetcmd) myHyperlinkManager

  # Standard scrollbar and geometry stuff is delegated to the html widget
  #set DelegateOption(-xscrollcommand) myHtml
  #set DelegateOption(-yscrollcommand) myHtml
  set DelegateOption(-width) myHtml
  set DelegateOption(-height) myHtml

  # Display configuration options implemented entirely by the html widget
  set DelegateOption(-fonttable) myHtml
  set DelegateOption(-fontscale) myHtml
  set DelegateOption(-zoom) myHtml
  set DelegateOption(-forcefontmetrics) myHtml
}

::hv3::make_constructor ::hv3::hv3 [list ::hv3::scrolled html]

proc ::hv3::release_handle {handle args} {
  $handle release
}

proc ::hv3::ignore_script {args} {}

# This proc is passed as the -urlcmd option to the [style] method of the
# Tkhtml3 widget. Returns the full-uri formed by resolving $rel relative
# to $base.
#
proc ::hv3::ss_resolve_uri {base rel} {
  set b [::tkhtml::uri $base]
  set ret [$b resolve $rel]
  $b destroy
  set ret
}

bind Html <Tab>       [list ::hv3::forms::tab %W]
bind Html <Shift-Tab> [list ::hv3::forms::tab %W]

proc ::hv3::bg {script args} {
  set eval [concat $script $args]
  set rc [catch [list uplevel $eval] result]
  if {$rc} {
    set cmd [list bgerror $result]
    set error [list $::errorInfo $::errorCode]
    after idle [list foreach {::errorInfo ::errorCode} $error $cmd]
    set ::errorInfo ""
    return ""
  }
  return $result
}

proc ::hv3::ReturnWithArgs {retval args} {
  return $retval
}

