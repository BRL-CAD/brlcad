namespace eval hv3 { set {version($Id$)} 1 }

#--------------------------------------------------------------------------
# Snit types in this file:
#
#     ::hv3::dom                       -- One object per js interpreter. 
#     ::hv3::dom::logscript            -- Logging data for debugging.
#

package require snit

#-------------------------------------------------------------------------
# Class ::hv3::dom
#
#   A single instance of this class is created for each javascript
#   enabled ::hv3::hv3 widget. The javascript module.
#
# SYNOPSYS
#
#     set dom [::hv3::dom %AUTO% $hv3]
#
#     $dom script ATTR SCRIPT
#
#     $dom javascript SCRIPT
#     $dom event EVENT NODE
#     $dom set_object_property {object property value}
#     $dom reset
#
#     $dom destroy
#
#     $dom javascriptlog               ;# DEBUGGING ONLY.
#
#     $dom configure -enable BOOLEAN
#
# DESCRIPTION
#
snit::type ::hv3::dom {

  # Javascript interpreter.
  variable mySee ""

  # Instance of [::hv3::hv3] that contains the window used as the
  # global object by this interpreter.
  variable myHv3 ""

  # Logging command.
  option -logcmd -default "" -configuremethod ConfigureLogcmd

  # Used to assign unique ids to each block of script evaluated. 
  # This is used by the script debugging gui.
  variable myNextCodeblockNumber 1

  constructor {hv3 args} {

    # Call [::hv3::enable_javascript] to make sure the hv3_dom_XXX.tcl
    # files have been loaded.
    ::hv3::enable_javascript

    set myHv3 $hv3
    set mySee [::see::interp [list ::hv3::DOM::Window $self $hv3]]

    $self configurelist $args

    set frame [$myHv3 cget -frame]
    if {$frame ne ""} {
      $frame update_parent_dom $mySee
    }
  }

  destructor { 
    catch { $mySee destroy }
  }

  # Invoked to set the value of the -logcmd option
  method ConfigureLogcmd {option value} {
    set options($option) $value
    if {$mySee ne ""} {
      $mySee log $value
    }
  }

  method InitWindowEvents {body} {
    set script ""
    foreach A {onload onunload} {
      catch {
        set V [$body attr $A]
        append script [subst {
          if (!window.$A) { window.$A = function(event) {$V} }
        }]
      }
    }
    $mySee eval -noresult $script
  }

  method NewFilename {} {
    return "$self.[incr myNextCodeblockNumber]"
  }
  
  # This method is called as a Tkhtml3 "script handler" for elements
  # of type <SCRIPT>. I.e. this should be registered with the html widget
  # as follows:
  #
  #     $htmlwidget handler script script [list $dom script]
  #
  # This is done externally, not by code within this type definition.
  # If scripting is not enabled in this browser, this method is a no-op.
  #
  method script {attr script} {
    $myHv3 html write wait
    array set a $attr
    if {[info exists a(src)]} {
      set fulluri [$myHv3 resolve_uri $a(src)]
      set handle [::hv3::request %AUTO%              \
          -uri         $fulluri                      \
          -mimetype    text/javascript               \
          -cachecontrol normal                       \
      ]
      if {[$myHv3 encoding] ne ""} {
        $handle configure -encoding [$myHv3 encoding]
      }
	  
      set fin [mymethod ScriptCallback $attr $handle]
      $handle configure -finscript $fin
      $myHv3 makerequest $handle
    } else {
      return [$self ScriptCallback $attr "" $script]
    }
    return ""
  }
  
  # If a <SCRIPT> element has a "src" attribute, then the [script]
  # method will have issued a GET request for it. This is the 
  # successful callback.
  #
  method ScriptCallback {attr downloadHandle script} {
    set title ""
    if {$downloadHandle ne ""} { 
      # Handle an HTTP redirect or a Location header:
      #
      if {[$myHv3 HandleLocation $downloadHandle]} return
      set title [$downloadHandle cget -uri]
      $downloadHandle release 
    }

    if {$title eq ""} {
      set attributes ""
      foreach {a v} $attr {
        append attributes " [::hv3::string::htmlize $a]="
        append attributes "\"[::hv3::string::htmlize $v]\""
      }
      set title "<script$attributes>"
    }

    if {[info exists ::hv3::reformat_scripts_option]} {
      if {$::hv3::reformat_scripts_option} {
        set script [string map {"\r\n" "\n"} $script]
        set script [string map {"\r" "\n"} $script]
        set script [::see::format $script]
      }
    }

    set name [$self NewFilename]
    set rc [catch {$mySee eval -noresult -file $name $script} msg]
    if {$rc} {puts "MSG: $msg"}

    $self Log $title $name $script $rc $msg
    $myHv3 html write continue
  }

  method javascript {script} {
    set name [$self NewFilename]
    set rc [catch {$mySee eval -file $name $script} msg]
    return $msg
  }

  # This method is called when one an event of type $event occurs on the
  # document node $node. Argument $event is one of the following:
  #
  #     onload
  #     onsubmit
  #     onchange
  #
  method event {event node} {
    if {$mySee eq ""} {return ""}

    # Strip off the "on", if present. i.e. "onsubmit" -> "submit"
    #
    if {[string first on $event]==0} {set event [string range $event 2 end]}

    if {$event eq "load"} {
      # The Hv3 layer passes the <BODY> node along with the onload
      # event, but DOM Level 0 browsers raise this event against
      # the Window object (the onload attribute of a <BODY> element
      # may be used to set the onload property of the corresponding
      # Window object).
      #
      # According to "DOM Level 2 Events", the load event does not
      # bubble and is not cancelable.
      #
      set fs [[$node html] search frameset -index 0]
      if {$fs ne ""} { 
        $self InitWindowEvents $fs
        return
      }
      $self InitWindowEvents $node

      set js_obj [::hv3::DOM::node_to_window $node]
    } else {
      set js_obj [::hv3::dom::wrapWidgetNode $self $node]
    }

    # Dispatch the event.
    set rc [catch {
      ::hv3::dom::dispatchHtmlEvent $self $event $js_obj
    } msg]

    # If an error occured, log it in the debugging window.
    #
    if {$rc} {
      set name [string map {blob error} [$self NewFilename]]
      $self EventLog "$event $node" $name "" $rc $msg
      set msg error
    }

    if {$event eq "load"} {
      $self DoFramesetLoadEvents $node
    }

    return $msg
  }

  method DispatchHtmlEvent {event js_obj} {
    # Dispatch the event.
    set rc [catch {
      ::hv3::dom::dispatchHtmlEvent $self $event $js_obj
    } msg]

    # If an error occured, log it in the debugging window.
    #
    if {$rc} {
      set objtype [lindex $js_obj 0]
      set name [string map {blob error} [$self NewFilename]]
      $self Log "$objtype $event event" $name "event-handler" $rc $msg
      set msg ""
    }

    set msg
  }

  method DoFramesetLoadEvents {node} {
    set frame [[winfo parent [$node html]] me]

    # If $frame is a replacement object for an <IFRAME> element, 
    # then fire the load event on the <IFRAME> element.
    if {[$frame cget -iframe] ne ""} {
      set iframe [$frame cget -iframe]
      set hv3 [[winfo parent [$iframe html]] hv3 me]

      set js_obj [::hv3::dom::wrapWidgetNode [$hv3 dom] $iframe]
      $hv3 dom DispatchHtmlEvent load $js_obj
      return
    }

    set p [$frame parent_frame]
    if {$p eq ""} return
    
    foreach c [$p child_frames] {
      if {0 == [[$c hv3] onload_fired]} {
        return
      }
    }

    set js_obj [::hv3::DOM::hv3_to_window [$p hv3]]

    # Dispatch the event.
    $self DispatchHtmlEvent load $js_obj
  }

  # This method is called by the ::hv3::mousemanager object to 
  # dispatch a mouse-event into DOM country. Argument $event
  # is one of the following:
  #
  #     onclick 
  #     onmouseout 
  #     onmouseover
  #     onmouseup 
  #     onmousedown 
  #     onmousemove 
  #     ondblclick
  #
  # Argument $node is the Tkhtml node that the mouse-event is 
  # to be dispatched against. $x and $y are the viewport coordinates
  # as returned by the Tk [bind] commands %x and %y directives. Any
  # addition arguments ($args) are passed through to 
  # [::hv3::dom::dispatchMouseEvent].
  #
  method mouseevent {event node x y args} {
    if {$mySee eq ""} {return 1}

    # This can happen if the node is deleted by a DOM event handler
    # invoked by the same logical GUI event as this DOM event.
    #
    if {"" eq [info commands $node]} {return 1}

    set Node [::hv3::dom::wrapWidgetNode $self $node]

    set rc [catch {
      $mySee node $Node
      ::hv3::dom::dispatchMouseEvent $self $event $Node $x $y $args
    } msg]
    if {$rc} {
      set name [string map {blob error} [$self NewFilename]]
      $self EventLog "$event $node" $name "" $rc $msg
      set msg "prevent"
    }
    set msg
  }

  method see {} { return $mySee }

  #------------------------------------------------------------------
  # Logging system follows.
  #
  variable myLogList ""

  method GetLog {} {return $myLogList}

  method Log {heading name script rc result} {
    if {![info exists ::hv3::log_source_option]} return
    if {!$::hv3::log_source_option} return

    set obj [::hv3::dom::logscript %AUTO% \
        -rc $rc -heading $heading -script $script -result $result -name $name
    ]
    lappend myLogList $obj
  }

  method EventLog {heading name script rc result} {
    if {![info exists ::hv3::log_source_option]} return
    if {!$::hv3::log_source_option} return

    puts "error: $result"
    set obj [::hv3::dom::logscript %AUTO% -isevent true \
        -rc $rc -heading $heading -script $script -result $result -name $name
    ]
    lappend myLogList $obj

    ::hv3::launch_console
    .console.console Errors javascript [list -1 $self $obj]
  }

  # Called by the tree-browser to get event-listener info for the
  # javascript object associated with the specified tkhtml node.
  #
  method eventdump {node} {
    if {$mySee eq ""} {return ""}
    $mySee events [::hv3::dom::wrapWidgetNode $self $node]
  }
}

#-----------------------------------------------------------------------
# ::hv3::dom::logscript
#
#     An object type to store the results of executing a block of
#     javascript or firing a javascript event.
#
snit::type ::hv3::dom::logscript {
  option -rc      -default ""
  option -heading -default "" 
  option -script  -default "" 
  option -result  -default "" 
  option -name    -default "" 
  option -isevent -default 0 
}

#-----------------------------------------------------------------------
# Pull in the object definitions.
#
proc ::hv3::dom_init {{init_docs 0}} {
  set ::hv3::dom::CREATE_DOM_DOCS 0
  if {$init_docs} {set ::hv3::dom::CREATE_DOM_DOCS 1}

  if {
    $::hv3::dom::CREATE_DOM_DOCS == 0 && [info commands ::see::interp] eq ""
  } return

  uplevel #0 {

    foreach f [list \
      hv3_dom_compiler.tcl \
      hv3_dom_containers.tcl \
      hv3_dom_core.tcl \
      hv3_dom_style.tcl \
      hv3_dom_events.tcl \
      hv3_dom_html.tcl \
      hv3_dom_ns.tcl \
      hv3_dom_xmlhttp.tcl \
    ] {
      source [file join $::hv3::scriptdir $f]
    }
  }
}

#-----------------------------------------------------------------------
# Initialise the scripting environment. This tries to load the javascript
# interpreter library. If it fails, then we have a scriptless browser. 
# The test for whether or not the browser is script-enabled is:
#
#     if {[info commands ::see::interp] ne ""} {
#         puts "We have scripting :)"
#     } else {
#         puts "No scripting here. Probably better that way."
#     }
#
catch { load [file join tclsee0.1 libTclsee.so] }
catch { load [file join tclsee0.1 libTclsee.dll] }
catch { package require Tclsee }

set ::hv3::scriptdir [file dirname [info script]]
set ::hv3::dom_init_has_run 0
proc ::hv3::enable_javascript {} {
  if {!$::hv3::dom_init_has_run} {
    ::hv3::dom_init
    set ::hv3::dom_init_has_run 1
  }
}

namespace eval ::hv3::DOM {
  # Given an html-widget node-handle, return the corresponding 
  # ::hv3::DOM::HTMLDocument object. i.e. the thing needed for
  # the Node.ownerDocument javascript property of an HTMLElement or
  # Text Node.
  #
  proc node_to_document {node} {
    set hv3 [[winfo parent [$node html]] hv3 me]
    list ::hv3::DOM::HTMLDocument [$hv3 dom] $hv3
  }

  proc node_to_window {node} {
    set hv3 [[winfo parent [$node html]] hv3 me]
    list ::hv3::DOM::Window [$hv3 dom] $hv3
  }

  proc hv3_to_window {hv3} {
    list ::hv3::DOM::Window [$hv3 dom] $hv3
  }
}

