namespace eval hv3 { set {version($Id$)} 1 }

package require snit

# History state for a single browser window.
#
snit::type ::hv3::history_state {

  # Variable $myUri stores the document URI to fetch. This is the value
  # that will be displayed in the "location" entry field when the 
  # history state is loaded.
  #
  # Variable $myTitle stores the title of the page last seen at location
  # $myUri. These two variables are used in concert to determine the
  # displayed title of the history list entry (on the history menu).
  #
  variable myUri     ""
  variable myTitle   ""

  # Object member $myUri stores the URI of the top-level document loaded
  # into the applicable browser window. However if that document is a 
  # frameset document a URI is required for each frame in the (possibly
  # large) tree of frames. This array stores those URIs, indexed by
  # the frames "positionid" (see the [positionid] sub-command of class
  # ::hv3::browser_frame).
  #
  # Note that the top-level frame is included in this array (the index
  # of the top-level frame is always "0 0"). The code in this file uses
  # $myUri for display purposes (i.e. in the history menu) and 
  # $myFrameUri for reloading states.
  #
  variable myFrameUri -array [list]

  method Getset {var arglist} {
    if {[llength $arglist] > 0} {
      set $var [lindex $arglist 0]
    }
    return [set $var]
  }

  method uri        {args} { return [$self Getset myUri $args] }
  method title      {args} { return [$self Getset myTitle $args] }

  # Retrieve the URI associated with the frame $positionid.
  #
  method get_frameuri {positionid} {
    if {[info exists myFrameUri($positionid)]} {
      return [lindex $myFrameUri($positionid) 0]
    }
    return ""
  }

  method get_framedoc {positionid} {
    if {[info exists myFrameUri($positionid)]} {
      return [lindex $myFrameUri($positionid) 3]
    }
    return ""
  }

  method get_frameyview {positionid} {
    if {[info exists myFrameUri($positionid)]} {
      return [lindex $myFrameUri($positionid) 2]
    }
    return ""
  }

  # Set an entry in the $myFrameUri array.
  #
  method set_framestate {positionid uri xscroll yscroll handle} {
    if {$handle ne ""} {
      if {
          ![string match -nocase http* [$handle cget -uri]] || 
          ![$handle isFinished]
      } {
        set handle ""
      } else {
        $handle reference
      }
    }
    set current_handle [$self get_framedoc $positionid]
    if {$current_handle ne ""} {
      $current_handle release
    }
    set myFrameUri($positionid) [list $uri $xscroll $yscroll $handle]
  }

  # Clear the $myFrameUri array.
  #
  method clear_frameurilist {} {
    foreach {k v} [array get myFrameUri] {
      set handle [lindex $v 3]
      catch {$handle release}
    }
    array unset myFrameUri
  }

  destructor {
    $self clear_frameurilist
  }
}

# class ::hv3::history
#
# Options:
#     -gotocmd
#     -backbutton
#     -forwardbutton
#
# Methods:
#     locationvar
# 
snit::type ::hv3::history {
  # corresponding option exported by this class.
  #
  variable myLocationVar ""
  variable myTitleVarName ""

  variable myHv3 ""
  variable myBrowser ""

  # The following two variables store the history list
  variable myStateList [list]
  variable myStateIdx 0 

  variable myRadioVar 0 

  # Variables used when loading a history-state.
  variable myHistorySeek -1
  variable myIgnoreGotoHandler 0
  variable myCacheControl ""

  # Configuration options to attach this history object to a set of
  # widgets - back and forward buttons and a menu widget.
  #
  option -backbutton    -default ""
  option -forwardbutton -default ""
  option -locationentry -default ""

  # An option to set the script to invoke to goto a URI. The script is
  # evaluated with a single value appended - the URI to load.
  #
  option -gotocmd -default ""

  # Events:
  #     <<Goto>>
  #     <<SaveState>>
  #     <<Location>>
  #
  #     Also there is a trace on "titlevar" (set whenever a <title> node is
  #     parsed)
  #
  constructor {hv3 protocol browser args} {
    $hv3 configure -locationvar [myvar myLocationVar]
    $self configurelist $args

    trace add variable [$hv3 titlevar] write [list $self Locvarcmd]

    set myTitleVarName [$hv3 titlevar]
    set myHv3 $hv3
    set myBrowser $browser

    bind [$hv3 win] <<Location>>  +[list $self Locvarcmd]
    $self add_hv3 $hv3

    # Initialise the state-list to contain a single, unconfigured state.
    set myStateList [::hv3::history_state %AUTO%]
    set myStateIdx 0
  }

  destructor {
    trace remove variable $myTitleVarName write [list $self Locvarcmd]
    foreach state $myStateList {
      $state destroy
    }
  }

  method add_hv3 {hv3} {
    bind [$hv3 win] <<Goto>>      +[list $self GotoHandler]
    bind [$hv3 win] <<SaveState>> +[list $self SaveStateHandler $hv3]
  }

  method loadframe {frame} {
    if {$myHistorySeek >= 0} {
      set state [lindex $myStateList $myHistorySeek]
      set uri [$state get_frameuri [$frame positionid]]
      if {$uri ne ""} {
        incr myIgnoreGotoHandler
        $frame goto $uri -cachecontrol $myCacheControl
        incr myIgnoreGotoHandler -1
        return 1
      }
    }
    return 0
  }

  # Return the name of the variable configured as the -locationvar option
  # of the hv3 widget. This is provided so that other code can add
  # [trace] callbacks to the variable.
  method locationvar {} {return [myvar myLocationVar]}

  # This method is bound to the <<Goto>> event of the ::hv3::hv3 
  # widget associated with this history-list.
  #
  method GotoHandler {} {
    if {!$myIgnoreGotoHandler} {
      # We are not in "history" mode any more.
      set myHistorySeek -1
    }
  }

  # Invoked whenever our hv3 widget is reset (i.e. just before a new
  # document is loaded) or when moving to a different #fragment within
  # the same document. The current state of the widget should be copied 
  # into the history list.
  #
  method SaveStateHandler {hv3} {
    set state [lindex $myStateList $myStateIdx]

    # Update the current history-state record:
    if {$myHistorySeek != $myStateIdx} {
      $state clear_frameurilist
      foreach frame [$myBrowser get_frames] {
        set fhv3 [$frame hv3]
        set pos  [$frame positionid]
        $state set_framestate $pos   \
            [$fhv3 uri get]          \
            [lindex [$fhv3 xview] 0] \
            [lindex [$fhv3 yview] 0] \
            [$fhv3 documenthandle]
      }
    }

    if {$myHistorySeek >= 0} {
      set myStateIdx $myHistorySeek
      set myRadioVar $myStateIdx
    } else {
      # Add an empty state to the end of the history list. Set myStateIdx
      # and myRadioVar to the index of the new state in $myStateList.
      set myStateList [lrange $myStateList 0 $myStateIdx]
      incr myStateIdx
      set myRadioVar $myStateIdx
      lappend myStateList [::hv3::history_state %AUTO%]

      # If the widget that generated this event is not the main widget,
      # copy the URI and title from the previous state.
      if {$hv3 ne $myHv3 && $myStateIdx >= 1} {
        set prev [lindex $myStateList [expr $myStateIdx - 1]]
        set new [lindex $myStateList $myStateIdx]
        $new uri [$prev uri]
        $new title [$prev title]
      }
    }

    $self populatehistorymenu
  }

  # Invoked when the [$myHv3 titlevar] variable is modified.  are modified.
  # Update the current history-state record according to the new value.
  #
  method Locvarcmd {args} {
    set state [lindex $myStateList $myStateIdx]
    $state uri $myLocationVar
    set t [set [$myHv3 titlevar]]
    if {$t ne ""} {
      $state title $t
    }
    $self populatehistorymenu
  }

  method framegotohistory {state browser_frame} {
    set hv3 [$browser_frame hv3]

    set pos [$browser_frame positionid]
    set zUri [$state get_frameuri $pos]
    set handle [$state get_framedoc $pos]
    set zUriNow [$hv3 uri get]

    $hv3 seek_to_yview [$state get_frameyview $pos]
    $hv3 goto $zUri -cachecontrol $myCacheControl -history_handle $handle

    if {$zUriNow eq [$hv3 uri get]} {
      foreach frame [$myBrowser get_frames] {
        set fpos [$frame positionid]
        if {[llength [$frame positionid]] == [llength $pos]+1} {
          $self framegotohistory $state $frame
        }
      }
    }
  }

  # Load history state $idx into the browser window.
  #
  method gotohistory {idx} {
    set myHistorySeek $idx
    set state [lindex $myStateList $idx]

    incr myIgnoreGotoHandler 
    set myCacheControl relax-transparency
    set c $myCacheControl
    $self framegotohistory $state $myBrowser
    incr myIgnoreGotoHandler -1
  }

  method reload {} {
    set myHistorySeek $myStateIdx
    set state [lindex $myStateList $myHistorySeek]
    incr myIgnoreGotoHandler 
    set myCacheControl no-cache
    set c $myCacheControl
    eval [linsert $options(-gotocmd) end [$state uri] -cachecontrol $c]
    incr myIgnoreGotoHandler -1
  }

  # This method reconfigures the state of the -historymenu, -backbutton
  # and -forwardbutton to match the internal state of this object. To
  # summarize, it:
  #
  #     * Enables or disabled the -backbutton button
  #     * Enables or disabled the -forward button
  #     * Clears and repopulates the -historymenu menu
  #
  # This should be called whenever some element of internal state changes.
  # Possibly as an [after idle] background job though...
  #
  method populatehistorymenu {} {

    # Handles for the four widgets this object is controlling.
    set back $options(-backbutton)
    set forward $options(-forwardbutton)
    set locationentry $options(-locationentry)

    set myRadioVar $myStateIdx

    set backidx [expr $myStateIdx - 1]
    set backcmd [list $self gotohistory $backidx]
    if {$backidx >= 0} {
        bind Hv3HotKeys <Alt-Left> $backcmd
        if {$back ne ""} { $back configure -state normal -command $backcmd }
    } else {
        bind Hv3HotKeys <Alt-Left> ""
        if {$back ne ""} { $back configure -state disabled }
    }

    set fwdidx [expr $myStateIdx + 1]
    set fwdcmd [list $self gotohistory $fwdidx]
    if {$fwdidx < [llength $myStateList]} {
        bind Hv3HotKeys <Alt-Right> $fwdcmd
        if {$forward ne ""} { $forward configure -state normal -command $fwdcmd}
    } else {
        bind Hv3HotKeys <Alt-Right> ""
        if {$forward ne ""} { $forward configure -state disabled }
    }

    if {$locationentry ne ""} { 
        $locationentry delete 0 end
        $locationentry insert 0 $myLocationVar
    }
  }

  method populate_menu {menu} {
    $menu delete 0 end

    set backidx [expr $myStateIdx - 1]
    $menu add command -label Back -accelerator (Alt-Left) -state disabled
    if {$backidx >= 0} {
      $menu entryconfigure end -command [list $self gotohistory $backidx]
      $menu entryconfigure end -state normal
    }

    set fwdidx [expr $myStateIdx + 1]
    $menu add command -label Forward -accelerator (Alt-Right) -state disabled
    if {$fwdidx < [llength $myStateList]} {
      $menu entryconfigure end -command [list $self gotohistory $fwdidx]
      $menu entryconfigure end -state normal
    }

    $menu add separator

    set myRadioVar $myStateIdx

    set idx [expr [llength $myStateList] - 15]
    if {$idx < 0} {set idx 0}
    for {} {$idx < [llength $myStateList]} {incr idx} {
      set state [lindex $myStateList $idx]

      # Try to use the history-state "title" as the menu item label, 
      # but if this is an empty string, fall back to the URI.
      set caption [$state title]
      if {$caption eq ""} {set caption [$state uri]}

      $menu add radiobutton                       \
        -label $caption                           \
        -variable [myvar myRadioVar]              \
        -value    $idx                            \
        -command [list $self gotohistory $idx]
    }
  }
}

snit::widget ::hv3::scrolledlistbox {
  component myVsb
  component myListbox

  delegate method * to myListbox
  delegate option * to myListbox

  constructor {args} {

    set myVsb [::hv3::scrollbar ${win}.scrollbar]
    set myListbox [listbox ${win}.listbox]
    $myVsb configure -command [list $myListbox yview]
    $myListbox configure -yscrollcommand [list $myVsb set]

    pack $myVsb -side right -fill y
    pack $myListbox -fill both -expand yes
    bindtags $myListbox [concat [bindtags $myListbox] $win]

    $self configurelist $args
  }
}

snit::widget ::hv3::locationentry {

  component myEntry             ;# The entry widget
  component myButton            ;# The button that activates the drop-down list

  variable myListbox    ""      ;# listbox widget (::hv3::scrolledlistbox)
  variable myListboxVar [list]  ;# -listvariable for listbox widget

  # Command to invoke when the location-entry widget is "activated" (i.e.
  # when the browser is supposed to load the contents as a URI). At
  # present this happens when the user presses enter.
  option -command -default ""

  delegate option * to myEntry
  delegate method * to myEntry

  constructor {args} {
    set myEntry [::hv3::entry ${win}.entry]
    $myEntry configure -background white

    # Compatibility binding for other browser users: Double-clicking on the
    # text in the location bar selects the entire entry widget, not just a
    # single word. Users like this because they can double-click on the
    # location toolbar, then type a new URI or copy the entire current
    # URI to the clipboard.
    #
    # Double-click followed by dragging the mouse still selects text
    # by word (standard Tk entry widget behaviour).
    #
    # Todo: Not completely sold on this idea... Simply removing the 
    # following two lines is safe to do if required.
    #
    set cmd [list after idle [list $myEntry selection range 0 end]]
    bind $myEntry <Double-1> $cmd
 
    set myButtonImage [image create bitmap -data {
      #define v_width 8
      #define v_height 4
      static unsigned char v_bits[] = { 0xff, 0x7e, 0x3c, 0x18 };
    }]
    set myButton [button ${win}.button -image $myButtonImage -width 12]
    $myButton configure -command [list $self ButtonPress] -takefocus 0

    pack $myButton -side right -fill y
    pack $myEntry -fill both -expand true

    $myEntry configure -borderwidth 0 -highlightthickness 0
    $myButton configure -borderwidth 1 -highlightthickness 0
    $hull configure -borderwidth 1 -relief sunken -background white

    # Create the listbox for the drop-down list. This is a child of
    # the same top-level as the ::hv3::locationentry widget...
    #
    set top [winfo toplevel $win]
    if {$top eq "."} {set top ""}
    set myListbox $top.[string map {. _} ${win}]
    ::hv3::scrolledlistbox $myListbox -takefocus 0

    # Any button-press anywhere in the GUI folds up the drop-down menu.
    bind [winfo toplevel $win] <ButtonPress> +[list $self AnyButtonPress %W]

    bind $myEntry <KeyPress>        +[list $self KeyPress]
    bind $myEntry <KeyPress-Return> +[list $self KeyPressReturn]
    bind $myEntry <KeyPress-Down>   +[list $self KeyPressDown]
    bind $myEntry <KeyPress-Tab>    +noop
    bind $myEntry <KeyPress-Escape> gui_escape

    $myListbox configure -listvariable [myvar myListboxVar]
    $myListbox configure -background white
    $myListbox configure -font [$myEntry cget -font]
    $myListbox configure -highlightthickness 0
    $myListbox configure -borderwidth 1

    # bind $myListbox.listbox <<ListboxSelect>> [list $self ListboxSelect]
    bind $myListbox.listbox <KeyPress-Return> [list $self ListboxReturn]
    bind $myListbox.listbox <1>   [list $self ListboxPress %y]

    $self configurelist $args
  }

  method AnyButtonPress {w} {
    if {
      [winfo ismapped $myListbox] &&
      $w ne $myButton && 
      $w ne $myEntry && 
      0 == [string match ${myListbox}* $w]
    } {
      place forget $myListbox
    }
  }

  # Configured -command callback for the button widget.
  #
  method ButtonPress {} {
    if {[winfo ismapped $myListbox]} {
      $self CloseDropdown
    } else {
      $self OpenDropdown *
    }
  }

  # Binding for <KeyPress-Return> events that occur in the entry widget.
  #
  method KeyPressReturn {} {

    set current [$myEntry get]
    set final ""

    # The string $current now holds whatever the user typed into the
    # location entry toolbar. This can be one of four things:
    #
    # 1. A fully qualified URI,
    # 2. An http URI missing the initial "http://" string, 
    # 3. A file URI missing the initial "file://" string, or
    # 4. A term or phrase to search the web for.
    #
    if {[regexp "Search the web for \"(.*)\"" $current -> newval]} {
      # Case 4 (special version), search the web with google.
      set newval [::hv3::escape_string $newval]
      set final "http://www.google.com/search?q=$newval"
    } elseif {[string match *:/* $current] || [string match *: $current]} {
      # Case 1, a fully qualified URI. Do nothing.
    } elseif {[string range $current 0 0] eq "/"} {
      # Case 3, an implicit file URI
      set final "file://${current}"
    } elseif {[string match {*[/.]*} $current]} {
      # Case 2, an implicit http URI
      set final "http://${current}"
    } else {
      # Case 4, search the web with google.
      set newval [::hv3::escape_string $current]
      set final "http://www.google.com/search?q=$newval"
    }

    if {$final ne ""} {
      $myEntry delete 0 end
      $myEntry insert 0 $final
    }

    if {$options(-command) ne ""} {
      eval $options(-command)
    }
    $self CloseDropdown
  }

  method KeyPressDown {} {
    if {[winfo ismapped $myListbox]} {
      focus $myListbox.listbox
      $myListbox activate 0
      $myListbox selection set 0 0
    }
  }
  method KeyPress {} {
    after idle [list $self AfterKeyPress]
  }
  method AfterKeyPress {} {
    $self OpenDropdown [$myEntry get]
  }

  method ListboxReturn {} {
    set str [$myListbox get active]
    $myEntry delete 0 end
    $myEntry insert 0 $str
    $self KeyPressReturn
  }
  method ListboxPress {y} {
    set str [$myListbox get [$myListbox nearest $y]]
    $myEntry delete 0 end
    $myEntry insert 0 $str
    $self KeyPressReturn
  }

  method CloseDropdown {} {
    place forget $myListbox
  }

  method OpenDropdown {pattern} {

    set matchpattern *
    if {$pattern ne "*"} {
      set matchpattern *${pattern}*
    }
    set myListboxVar [::hv3::the_visited_db keys $matchpattern]

    if {$pattern ne "*" && ![string match *.* $pattern] } {
      set search "Search the web for \"$pattern\""
      set myListboxVar [linsert $myListboxVar 0 $search]
    }

    if {[llength $myListboxVar] == 0 && $pattern ne "*"} {
      $self CloseDropdown
      return
    }

    if {[llength $myListboxVar] > 4} {
      $myListbox configure -height 5
    } else {
      $myListbox configure -height -1
    }

    set t [winfo toplevel $win]
    set x [expr [winfo rootx $win] - [winfo rootx $t]]
    set y [expr [winfo rooty $win] + [winfo height $win] - [winfo rooty $t]]
    set w [winfo width $win]

    place $myListbox -x $x -y $y -width $w
    raise $myListbox
  }

  method escape {} {
    $self CloseDropdown
    $self selection range 0 0
  }
}

namespace eval ::hv3::httpcache {
  proc new {me} {
    set O(handles) [list]
  }
  proc destroy {me} {
    upvar #0 $me O
    foreach h $O(handles) {
      $h release
    }
    rename $me ""
    unset $me
  }
  proc add {me handle} {
    upvar #0 $me O
    lappend O(handles) $handle

    set key cache.[$handle cget -uri]
    if {[info exists O($key)]} {
      set idx [lsearch $O(handles) $O($key)]
      set O(handles) [lreplace $O(handles) $idx $idx]
    }
    set O($key) $handle

    $handle reference
    if {[llength $O(handles)]>=25} {
      foreach h [lrange $O(handles) 0 4] {
        unset O(cache.[$h cget -uri])
        $h release
      }
      set O(handles) [lrange $O(handles) 5 end]
    }
  }
  proc query {me handle} {
    upvar #0 $me O
    set uri [$handle cget -uri]
    if {[info exists O(cache.$uri)]} {
      set h $O(cache.$uri)
      set idx [lsearch $O(handles) $h]
      set O(handles) [lreplace $O(handles) $idx $idx]
      lappend O(handles) $h
      $handle finish [$h rawdata] 
      return 1
    }
    return 0
  }
}
::hv3::make_constructor ::hv3::httpcache
::hv3::httpcache ::hv3::the_httpcache

proc noop {args} {}

