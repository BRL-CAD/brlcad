

package require sqlite3
package require Tkhtml 3.0

proc sourcefile {file} [string map              \
  [list %HV3_DIR% [file dirname [info script]]] \
{ 
  return [file join {%HV3_DIR%} $file] 
}]

source [sourcefile snit.tcl]
source [sourcefile hv3_widgets.tcl]
source [sourcefile hv3_notebook.tcl]
source [sourcefile hv3_db.tcl]
source [sourcefile hv3_home.tcl]
source [sourcefile hv3.tcl]
source [sourcefile hv3_prop.tcl]
source [sourcefile hv3_log.tcl]
source [sourcefile hv3_http.tcl]
source [sourcefile hv3_download.tcl]
source [sourcefile hv3_frameset.tcl]
source [sourcefile hv3_polipo.tcl]
source [sourcefile hv3_icons.tcl]
source [sourcefile hv3_history.tcl]
source [sourcefile hv3_bookmarks.tcl]
source [sourcefile hv3_bugreport.tcl]
source [sourcefile hv3_debug.tcl]
source [sourcefile hv3_dom.tcl]
source [sourcefile hv3_object.tcl]

#--------------------------------------------------------------------------
# Widget ::hv3::browser_frame
#
#     This mega widget is instantiated for each browser frame (a regular
#     html document has one frame, a <frameset> document may have more
#     than one). This widget is not considered reusable - it is designed
#     for the web browser only. The following application-specific
#     functionality is added to ::hv3::hv3:
#
#         * The -statusvar option
#         * The right-click menu
#         * Overrides the default -targetcmd supplied by ::hv3::hv3
#           to respect the "target" attribute of <a> and <form> elements.
#
#     For more detail on handling the "target" attribute, see HTML 4.01. 
#     In particular the following from appendix B.8:
# 
#         1. If the target name is a reserved word as described in the
#            normative text, apply it as described.
#         2. Otherwise, perform a depth-first search of the frame hierarchy 
#            in the window that contained the link. Use the first frame whose 
#            name is an exact match.
#         3. If no such frame was found in (2), apply step 2 to each window,
#            in a front-to-back ordering. Stop as soon as you encounter a frame
#            with exactly the same name.
#         4. If no such frame was found in (3), create a new window and 
#            assign it the target name.
#
#     Hv3 currently only implements steps 1 and 2.
#
namespace eval ::hv3::browser_frame {

  proc new {me browser args} {
    upvar #0 $me O

    # The name of this frame (as specified by the "name" attribute of 
    # the <frame> element).
    set O(-name) ""
    set O(oldname) ""

    # If this [::hv3::browser_frame] is used as a replacement object
    # for an <iframe> element, then this option is set to the Tkhtml3
    # node-handle for that <iframe> element.
    #
    set O(-iframe) ""

    set O(-statusvar) ""

    set O(myBrowser) $browser
    eval $me configure $args

    set O(myNodeList) ""                  ;# Current nodes under the pointer
    set O(myX) 0                          ;# Current location of pointer
    set O(myY) 0                          ;# Current location of pointer

    set O(myPositionId) ""                ;# See sub-command [positionid]

    # If "Copy Link Location" has been selected, store the selected text
    # (a URI) in set $O(myCopiedLinkLocation).
    set O(myCopiedLinkLocation) ""
 
    #set O(myHv3)      [::hv3::hv3 $O(win).hv3]
    #pack $O(myHv3) -expand true -fill both
    set O(myHv3) $O(hull)
    $O(myHv3) configure -frame $me

    ::hv3::the_visited_db init $O(myHv3)

    catch {$O(myHv3) configure -fonttable $::hv3::fontsize_table}

    # Create bindings for motion, right-click and middle-click.
    $O(myHv3) Subscribe motion [list $me motion]
    bind $O(win) <3>       [list $me rightclick %x %y %X %Y]
    #bind $O(win) <2>       [list $me goto_selection]
    bind $O(win) <2>       [list $me middleclick %x %y]

    # When the hyperlink menu "owns" the selection (happens after 
    # "Copy Link Location" is selected), invoke method 
    # [GetCopiedLinkLocation] with no arguments to retrieve it.

    # Register a handler command to handle <frameset>.
    set html [$O(myHv3) html]
    $html handler node frameset [list ::hv3::frameset_handler $me]

    # Register handler commands to handle <object> and kin.
    $html handler node object   [list hv3_object_handler $O(myHv3)]
    $html handler node embed    [list hv3_object_handler $O(myHv3)]

    $html handler node      iframe [list ::hv3::iframe_handler $me]
    $html handler attribute iframe [list ::hv3::iframe_attr_handler $me]

    # Add this object to the browsers frames list. It will be removed by
    # the destructor proc. Also override the default -targetcmd
    # option of the ::hv3::hv3 widget with our own version.
    if {$O(myBrowser) ne ""} {
      $O(myBrowser) add_frame $O(win)
    }
    $O(myHv3) configure -targetcmd [list $me Targetcmd]

    ::hv3::menu $O(win).hyperlinkmenu
    selection handle $O(win).hyperlinkmenu [list $me GetCopiedLinkLocation]
  }
  proc destroy {me} {
    upvar #0 $me O
    catch {$me ConfigureName -name ""}
    # Remove this object from the $theFrames list.
    catch {$O(myBrowser) del_frame $O(win)} msg
    catch {::destroy $O(win).hyperlinkmenu}
  }

  proc configure-name {me} {
    update_parent_dom $me [[$me hv3 dom] see] 
  }
  proc update_parent_dom {me my_see} {
    upvar #0 $me O

    # This method is called when the "name" of attribute of this
    # frame is modified. If javascript is enabled we have to update
    # the properties on the parent window object (if any).

    set parent [$me parent_frame]
    if {$parent ne ""} {
      set parent_see [[$parent hv3 dom] see] 

      if {$parent_see ne ""} { 
        if {$O(oldname) ne ""} {
          $parent_see global $O(oldname) undefined
        }
        if {$my_see ne ""} {
          $parent_see global $O(-name) [list bridge $my_see]
        }
      }
    }

    set O(oldname) $O(-name)
  }

  proc Targetcmd {me node} {
    upvar #0 $me O
    set target [$node attr -default "" target]
    if {$target eq ""} {
      # If there is no target frame specified, see if a default
      # target was specified in a <base> tag i.e. <base target="_top">.
      set n [lindex [[$O(myHv3) html] search base] 0]
      if {$n ne ""} { set target [$n attr -default "" target] }
    }

    set theTopFrame [[lindex [$O(myBrowser) get_frames] 0] hv3]

    # Find the target frame widget.
    set widget $O(myHv3)
    switch -- $target {
      ""        { set widget $O(myHv3) }
      "_self"   { set widget $O(myHv3) }
      "_top"    { set widget $theTopFrame }

      "_parent" { 
        set w [winfo parent $O(myHv3)]
        while {$w ne "" && [lsearch [$O(myBrowser) get_frames] $w] < 0} {
          set w [winfo parent $w]
        }
        if {$w ne ""} {
          set widget [$w hv3]
        } else {
          set widget $theTopFrame
        }
      }

      # This is incorrect. The correct behaviour is to open a new
      # top-level window. But hv3 doesn't support this (and because 
      # reasonable people don't like new top-level windows) we load
      # the resource into the "_top" frame instead.
      "_blank"  { set widget $theTopFrame }

      default {
        # In html 4.01, an unknown frame should be handled the same
        # way as "_blank". So this next line of code implements the
        # same bug as described for "_blank" above.
        set widget $theTopFrame

        # TODO: The following should be a depth first search through the
        # frames in the list returned by [get_frames].
        #
        foreach f [$O(myBrowser) get_frames] {
          set n [$f cget -name]
          if {$n eq $target} {
            set widget [$f hv3]
            break
          }
        }
      }
    }

    return $widget
  }

  proc parent_frame {me} {
    upvar #0 $me O
    set frames [$O(myBrowser) get_frames]
    set w [winfo parent $O(win)]
    while {$w ne "" && [lsearch $frames $w] < 0} {
      set w [winfo parent $w]
    }
    return $w
  }
  proc top_frame {me } {
    upvar #0 $me O
    lindex [$O(myBrowser) get_frames] 0
  }
  proc child_frames {me } {
    upvar #0 $me O
    set ret [list]
    foreach c [$O(myBrowser) frames_tree $O(win)] {
      lappend ret [lindex $c 0]
    }
    set ret
  }

  # This method returns the "position-id" of a frame, an id that is
  # used by the history sub-system when loading a historical state of
  # a frameset document.
  #
  proc positionid {me } {
    upvar #0 $me O
    if {$O(myPositionId) eq ""} {
      set w $O(win)
      while {[set p [winfo parent $w]] ne ""} {
        set class [winfo class $p]
        if {$class eq "Panedwindow"} {
          set a [lsearch [$p panes] $w]
          set w $p
          set p [winfo parent $p]
          set b [lsearch [$p panes] $w]

          set O(myPositionId) [linsert $O(myPositionId) 0 "${b}.${a}"]
        }
        if {$class eq "Html" && $O(myPositionId) eq ""} {
          set node $O(-iframe)
          set idx [lsearch [$p search iframe] $node]
          set O(myPositionId) [linsert $O(myPositionId) 0 iframe.${idx}]
        }
        set w $p
      }
      set O(myPositionId) [linsert $O(myPositionId) 0 0]
    }
    return $O(myPositionId)
  }

  proc middleclick {me x y} {
    upvar #0 $me O
    set node [lindex [$O(myHv3) node $x $y] 0]

    set href ""
    for {set N $node} {$N ne ""} {set N [$N parent]} {
      if {[$N tag] eq "a" && [catch {$N attr href}]==0} {
        set href [$O(myHv3) resolve_uri [$N attr href]]
      }
    }

    if {$href eq ""} {
      $me goto_selection
    } else {
      $me menu_select opentab $href
    }
  }

  # This callback is invoked when the user right-clicks on this 
  # widget. If the mouse cursor is currently hovering over a hyperlink, 
  # popup the hyperlink menu. Otherwise launch the tree browser.
  #
  # Arguments $x and $y are the the current cursor position relative to
  # this widget's window. $X and $Y are the same position relative to
  # the root window.
  #
  proc rightclick {me x y X Y} {
    upvar #0 $me O
    if {![info exists ::hv3::G]} return

    set m $O(win).hyperlinkmenu
    $m delete 0 end

    set nodelist [$O(myHv3) node $x $y]

    set a_href ""
    set img_src ""
    set select [$O(myHv3) selected]
    set leaf ""

    foreach leaf $nodelist {
      for {set N $leaf} {$N ne ""} {set N [$N parent]} {
        set tag [$N tag]

        if {$a_href eq "" && $tag eq "a"} {
          set a_href [$N attr -default "" href]
        }
        if {$img_src eq "" && $tag eq "img"} {
          set img_src [$N attr -default "" src]
        }

      }
    }

    if {$a_href ne ""}  {set a_href [$O(myHv3) resolve_uri $a_href]}
    if {$img_src ne ""} {set img_src [$O(myHv3) resolve_uri $img_src]}

    set MENU [list \
      a_href "Open Link"             [list $me menu_select open $a_href]     \
      a_href "Open Link in Bg Tab"   [list $me menu_select opentab $a_href]  \
      a_href "Download Link"         [list $me menu_select download $a_href] \
      a_href "Copy Link Location"    [list $me menu_select copy $a_href]     \
      a_href --                      ""                                        \
      img_src "View Image"           [list $me menu_select open $img_src]    \
      img_src "View Image in Bg Tab" [list $me menu_select opentab $img_src] \
      img_src "Download Image"       [list $me menu_select download $img_src]\
      img_src "Copy Image Location"  [list $me menu_select copy $img_src]    \
      img_src --                     ""                                        \
      select  "Copy Selected Text"   [list $me menu_select copy $select]     \
      select  --                     ""                                        \
      leaf    "Open Tree browser..." [list ::HtmlDebug::browse $O(myHv3) $leaf]   \
    ]

    foreach {var label cmd} $MENU {
      if {$var eq "" || [set $var] ne ""} {
        if {$label eq "--"} {
          $m add separator
        } else {
          $m add command -label $label -command $cmd
        }
      }
    }

    $::hv3::G(config) populate_hidegui_entry $m
    $m add separator

    # Add the "File", "Search", "View" and "Debug" menus.
    foreach sub [list File Search Options Debug History] {
      catch {
        set menu_widget $m.[string tolower $sub]
        gui_populate_menu $sub [::hv3::menu $menu_widget]
      }
      $m add cascade -label $sub -menu $menu_widget -underline 0
    }

    tk_popup $m $X $Y
  }

   # Called when an option has been selected on the hyper-link menu. The
   # argument identifies the specific option. May be one of:
   #
   #     open
   #     opentab
   #     download
   #     copy
   #
  proc menu_select {me option uri} {
    upvar #0 $me O
    switch -- $option {
      open { 
        set top_frame [lindex [$O(myBrowser) get_frames] 0]
        $top_frame goto $uri 
      }
      opentab { set new [.notebook addbg $uri] }
      download { $O(myBrowser) saveuri $uri }
      copy {
        set O(myCopiedLinkLocation) $uri
        selection own $O(win).hyperlinkmenu
        clipboard clear
        clipboard append $uri
      }

      default {
        error "Internal error"
      }
    }
  }

  proc GetCopiedLinkLocation {me args} {
    upvar #0 $me O
    return $O(myCopiedLinkLocation)
  }

  # Called when the user middle-clicks on the widget
  proc goto_selection {me} {
    upvar #0 $me O
    set theTopFrame [lindex [$O(myBrowser) get_frames] 0]
    $theTopFrame goto [selection get]
  }

  proc motion {me N x y} {
    upvar #0 $me O
    set O(myX) $x
    set O(myY) $y
    set O(myNodeList) $N
    $me update_statusvar
  }

  proc node_to_string {me node {hyperlink 1}} {
    upvar #0 $me O
    set value ""
    for {set n $node} {$n ne ""} {set n [$n parent]} {
      if {[info commands $n] eq ""} break
      set tag [$n tag]
      if {$tag eq ""} {
        set value [$n text]
      } elseif {$hyperlink && $tag eq "a" && [$n attr -default "" href] ne ""} {
        set value "hyper-link: [string trim [$n attr href]]"
        break
      } elseif {[set nid [$n attr -default "" id]] ne ""} {
        set value "<$tag id=$nid>$value"
      } else {
        set value "<$tag>$value"
      }
    }
    return $value
  }

  proc update_statusvar {me} {
    upvar #0 $me O
    if {$O(-statusvar) ne ""} {
      set str ""

      set status_mode browser
      catch { set status_mode $::hv3::G(status_mode) }

      switch -- $status_mode {
        browser-tree {
          set value [$me node_to_string [lindex $O(myNodeList) end]]
          set str "($O(myX) $O(myY)) $value"
        }
        browser {
          for {set n [lindex $O(myNodeList) end]} {$n ne ""} {set n [$n parent]} {
            if {[$n tag] eq "a" && [$n attr -default "" href] ne ""} {
              set str "hyper-link: [string trim [$n attr href]]"
              break
            }
          }
        }
      }

      if {$O(-statusvar) ne $str} {
        set $O(-statusvar) $str
      }
    }
  }
 
  #--------------------------------------------------------------------------
  # PUBLIC INTERFACE
  #--------------------------------------------------------------------------

  proc goto {me args} {
    upvar #0 $me O
    eval [concat $O(myHv3) goto $args]
    set O(myNodeList) ""
    $me update_statusvar
  }

  # Launch the tree browser
  proc browse {me} {
    upvar #0 $me O
    ::HtmlDebug::browse $O(myHv3) [$O(myHv3) node]
  }

  proc hv3 {me args} { 
    upvar #0 $me O
    if {[llength $args] == 0} {return $O(myHv3)}
    eval $O(myHv3) $args
  }
  proc browser {me} {
    upvar #0 $me O
    return $O(myBrowser) 
  }
  proc dom {me} { 
    upvar #0 $me O
    return [$O(myHv3) dom]
  }

  # The [isframeset] method returns true if this widget instance has
  # been used to parse a frameset document (widget instances may parse
  # either frameset or regular HTML documents).
  #
  proc isframeset {me} {
    upvar #0 $me O
    # When a <FRAMESET> tag is parsed, a node-handler in hv3_frameset.tcl
    # creates a widget to manage the frames and then uses [place] to 
    # map it on top of the html widget created by this ::hv3::browser_frame
    # widget. Todo: It would be better if this code was in the same file
    # as the node-handler, otherwise this test is a bit obscure.
    #
    set html [[$me hv3] html]
    set slaves [place slaves $html]
    set isFrameset 0
    if {[llength $slaves]>0} {
      set isFrameset [expr {[winfo class [lindex $slaves 0]] eq "Frameset"}]
    }
    return $isFrameset
  }

  set DelegateOption(-forcefontmetrics) myHv3
  set DelegateOption(-fonttable)        myHv3
  set DelegateOption(-fontscale)        myHv3
  set DelegateOption(-zoom)             myHv3
  set DelegateOption(-enableimages)     myHv3
  set DelegateOption(-enablejavascript) myHv3
  set DelegateOption(-dom)              myHv3
  set DelegateOption(-width)            myHv3
  set DelegateOption(-height)           myHv3
  set DelegateOption(-requestcmd)       myHv3
  set DelegateOption(-resetcmd)         myHv3
  set DelegateOption(-downloadcmd)      myHv3

  proc stop {me args} {
    upvar #0 $me O
    eval $O(myHv3) stop $args
  }
  proc titlevar {me args} {
    upvar #0 $me O
    eval $O(myHv3) titlevar $args
  }
  proc dumpforms {me args} {
    upvar #0 $me O
    eval $O(myHv3) dumpforms $args
  }
  proc javascriptlog {me args} {
    upvar #0 $me O
    eval $O(myHv3) javascriptlog $args
  }

  proc me {me} {
    return $me
  }
}
::hv3::make_constructor ::hv3::browser_frame ::hv3::hv3

# An instance of this widget represents a top-level browser frame (not
# a toplevel window - an html frame not contained in any frameset 
# document). These are the things managed by the notebook widget.
#
namespace eval ::hv3::browser {

  proc new {me args} {
    upvar #0 $me O

    # Initialize the global database connection if it has not already
    # been initialized. TODO: Remove the global variable.
    ::hv3::dbinit

    set O(-stopbutton) ""
    set O(-unsafe) 0

    # Variables passed to [$myProtocol configure -statusvar] and
    # the same option of $myMainFrame. Used to create the value for 
    # $myStatusVar.
    set O(myProtocolStatus) ""
    set O(myFrameStatus) ""

    set O(myStatusVar) ""
    set O(myLocationVar) ""

    # List of all ::hv3::browser_frame objects using this object as
    # their toplevel browser. 
    set O(myFrames) [list]

    # Create the protocol object.
    set O(myProtocol) [::hv3::protocol %AUTO%]

    # The main browser frame (always present).
    set O(myHistory) ""
    set O(myMainFrame) [::hv3::browser_frame $O(win).frame $me]
    #set O(myMainFrame) $O(hull)

    # The history sub-system.
    set hv3 [$O(myMainFrame) hv3]
    set O(myHistory) [::hv3::history %AUTO% $hv3 $O(myProtocol) $me]

    # The widget may be in one of two states - "pending" or "not pending".
    # "pending" state is when the browser is still waiting for one or more
    # downloads to finish before the document is correctly displayed. In
    # this mode the default cursor is an hourglass and the stop-button
    # widget is in normal state (stop button is clickable).
    #
    # Otherwise the default cursor is "" (system default) and the stop-button
    # widget is disabled.
    #
    set O(myIsPending) 0

    set psc [list $me ProtocolStatusChanged]
    trace add variable ${me}(myProtocolStatus) write $psc
    trace add variable ${me}(myFrameStatus) write [list $me Writestatus]
    $O(myMainFrame) configure -statusvar ${me}(myFrameStatus)
    $O(myProtocol)  configure -statusvar ${me}(myProtocolStatus)

    # Link in the "home:" and "about:" scheme handlers (from hv3_home.tcl)
    ::hv3::home_scheme_init [$O(myMainFrame) hv3] $O(myProtocol)
    ::hv3::cookies_scheme_init $O(myProtocol)
    ::hv3::download_scheme_init [$O(myMainFrame) hv3] $O(myProtocol)

    # Configure the history sub-system. TODO: Is this obsolete?
    $O(myHistory) configure -gotocmd [list $me goto]

    $O(myMainFrame) configure -requestcmd [list $O(myProtocol) requestcmd]

    # pack $O(myMainFrame) -expand true -fill both -side top
    grid $O(myMainFrame) -sticky nsew
    grid rowconfigure $O(win) 0 -weight 1
    grid columnconfigure $O(win) 0 -weight 1

    eval $me configure $args
  }

  proc destroy {me} {
    upvar #0 $me O
    if {$O(myProtocol) ne ""} { $O(myProtocol) destroy }
    if {$O(myHistory) ne ""}  { $O(myHistory) destroy }
  }

  proc statusvar {me} { 
    return ${me}(myStatusVar)
  }
  proc titlevar {me args} { 
    upvar #0 $me O
    eval $O(myMainFrame) titlevar $args
  }

  # This method is called to activate the download-manager to download
  # the specified URI ($uri) to the local file-system.
  #
  proc saveuri {me uri} {
    upvar #0 $me O

    set handle [::hv3::request %AUTO%              \
        -uri         $uri                           \
        -mimetype    application/gzip               \
    ]
    $handle configure \
        -incrscript [list ::hv3::the_download_manager savehandle "" $handle] \
        -finscript  [list ::hv3::the_download_manager savehandle "" $handle]

    $O(myProtocol) requestcmd $handle
  }

  # Interface used by code in class ::hv3::browser_frame for frame management.
  #
  proc add_frame {me frame} {
    upvar #0 $me O

    lappend O(myFrames) $frame
    if {$O(myHistory) ne ""} {
      $O(myHistory) add_hv3 [$frame hv3]
    }
    set HTML [[$frame hv3] html]
    bind $HTML <1>               [list focus %W]
    bind $HTML <KeyPress-slash>  [list $me Find]
    bindtags $HTML [concat Hv3HotKeys $me [bindtags $HTML]]

    set cmd [list ::hv3::the_download_manager savehandle $O(myProtocol)]
    $frame configure -downloadcmd $cmd

    catch {$::hv3::G(config) configureframe $frame}
  }
  proc del_frame {me frame} {
    upvar #0 $me O
    set idx [lsearch $O(myFrames) $frame]
    if {$idx >= 0} {
      set O(myFrames) [lreplace $O(myFrames) $idx $idx]
    }
  }
  proc get_frames {me} {
    upvar #0 $me O
    return $O(myFrames)
  }

  # Return a list describing the current structure of the frameset 
  # displayed by this browser.
  #
  proc frames_tree {me {head {}}} {
    upvar #0 $me O
    set ret ""

    array set A {}
    foreach f [lsort $O(myFrames)] {
      set p [$f parent_frame]
      lappend A($p) $f
      if {![info exists A($f)]} {set A($f) [list]}
    }

    foreach f [concat [lsort -decreasing $O(myFrames)] [list {}]] {
      set new [list]
      foreach child $A($f) {
        lappend new [list $child $A($child)]
      }
      set A($f) $new
    }
    
    set A($head)
  }

  # This method is called by a [trace variable ... write] hook attached
  # to the O(myProtocolStatus) variable. Set O(myStatusVar).
  proc Writestatus {me args} {
    upvar #0 $me O
    set protocolstatus Done
    if {[llength $O(myProtocolStatus)] > 0} {
      foreach {nWaiting nProgress nPercent} $O(myProtocolStatus) break
      set protocolstatus "$nWaiting waiting, $nProgress progress  ($nPercent%)"
    }
    set O(myStatusVar) "$protocolstatus    $O(myFrameStatus)"
  }

  proc ProtocolStatusChanged {me args} {
    upvar #0 $me O
    $me pendingcmd [llength $O(myProtocolStatus)]
    $me Writestatus
  }

  proc set_frame_status {me text} {
    upvar #0 $me O
    set O(myFrameStatus) $text
  }

  proc pendingcmd {me isPending} {
    upvar #0 $me O
    if {$O(-stopbutton) ne "" && $O(myIsPending) != $isPending} {
      if {$isPending} { 
        $O(hull) configure -cursor watch
        $O(-stopbutton) configure        \
            -command [list $O(myMainFrame) stop]  \
            -image hv3_stopimg                 \
            -tooltip "Stop Current Download"
      } else {
        $O(hull) configure -cursor ""
        $O(-stopbutton) configure        \
            -command [list $me reload] \
            -image hv3_reloadimg               \
            -tooltip "Reload Current Document"
      }
    }
    set O(myIsPending) $isPending
  }

  proc configure-stopbutton {me} {
    upvar #0 $me O
    set val $O(myIsPending)
    set O(myIsPending) -1
    $me pendingcmd $val
  }

  # Escape --
  #
  #     This method is called when the <Escape> key sequence is seen.
  #     Get rid of the "find-text" widget, if it is currently visible.
  #
  proc escape {me} {
    upvar #0 $me O
    catch { ::destroy $O(win).findwidget }
  }

  proc packwidget {me w {row 1}} {
    upvar #0 $me O
    grid $w -column 0 -row $row -sticky nsew
    bind $w <Destroy> [list catch [list focus [[$O(myMainFrame) hv3] html]]]
  }

  # Find --
  #
  #     This method is called when the "find-text" widget is summoned.
  #     Currently this can happen when the users:
  #
  #         * Presses "control-f",
  #         * Presses "/", or
  #         * Selects the "Edit->Find Text" pull-down menu command.
  #
  proc Find {me } {
    upvar #0 $me O

    set fdname $O(win).findwidget
    set initval ""
    if {[llength [info commands $fdname]] > 0} {
      set initval [${fdname}.entry get]
      ::destroy $fdname
    }
  
    ::hv3::findwidget $fdname $me

    $me packwidget $fdname
    $fdname configure -borderwidth 1 -relief raised

    # Bind up, down, next and prior key-press events to scroll the
    # main hv3 widget. This means you can use the keyboard to scroll
    # window (vertically) without shifting focus from the 
    # find-as-you-type box.
    #
    set hv3 [$me hv3]
    bind ${fdname} <KeyPress-Up>    [list $hv3 yview scroll -1 units]
    bind ${fdname} <KeyPress-Down>  [list $hv3 yview scroll  1 units]
    bind ${fdname} <KeyPress-Next>  [list $hv3 yview scroll  1 pages]
    bind ${fdname} <KeyPress-Prior> [list $hv3 yview scroll -1 pages]

    # When the findwidget is destroyed, return focus to the html widget. 
    bind ${fdname} <KeyPress-Escape> gui_escape

    ${fdname}.entry insert 0 $initval
    focus ${fdname}.entry
  }

  # ProtocolGui --
  #
  #     This method is called when the "toggle-protocol-gui" control
  #     (implemented externally) is manipulated. The argument must
  #     be one of the following strings:
  #
  #       "show"            (display gui)
  #       "hide"            (hide gui)
  #       "toggle"          (display if hidden, hide if displayed)
  #
  proc ProtocolGui {me cmd} {
    upvar #0 $me O
    set name $O(win).protocolgui
    set exists [winfo exists $name]

    switch -- $cmd {
      show   {if {$exists} return}
      hide   {if {!$exists} return}
      toggle {
        set cmd "show"
        if {$exists} {set cmd "hide"}
      }

      default { error "Bad arg" }
    }

    if {$cmd eq "hide"} {
      ::destroy $name
    } else {
      $O(myProtocol) gui $name
      $me packwidget $name 2
    }
  }

  proc history {me } {
    upvar #0 $me O
    return $O(myHistory)
  }

  proc reload {me } {
    upvar #0 $me O
    $O(myHistory) reload
  }

  proc populate_history_menu {me args} {
    upvar #0 $me O
    eval $O(myHistory) populate_menu $args
  }
  proc populatehistorymenu {me args} {
    upvar #0 $me O
    eval $O(myHistory) populatehistorymenu $args
  }
  proc locationvar {me args} {
    upvar #0 $me O
    eval $O(myHistory) locationvar $args
  }
  proc debug_cookies {me args} {
    upvar #0 $me O
    eval $O(myProtocol) debug_cookies $args
  }

  set DelegateOption(-backbutton)    myHistory
  set DelegateOption(-forwardbutton) myHistory
  set DelegateOption(-locationentry) myHistory
  set DelegateOption(*) myMainFrame

  proc unknown {method me args} {
    # puts "UNKNOWN: $me $method $args"
    upvar #0 $me O
    uplevel 3 [list eval $O(myMainFrame) $method $args]
  }
  namespace unknown unknown
}

::hv3::make_constructor ::hv3::browser

set ::hv3::maindir [file dirname [info script]] 

