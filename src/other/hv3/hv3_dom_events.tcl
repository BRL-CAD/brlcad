namespace eval hv3 { set {version($Id$)} 1 }

#-------------------------------------------------------------------------
# DOM Level 2 Events.
#
# This file contains the Hv3 implementation of javascript events. Hv3
# attempts to be compatible with the both the W3C and Netscape models.
# Where these are incompatible, copy Safari. This file contains 
# implementations of the following DOM interfaces:
#
#     DocumentEvent    (mixed into the DOM Document object)
#     Event            (Event objects)
#     MutationEvent    (Mutation event objects)
#     UIEvent          (UI event objects)
#     MouseEvent       (Mouse event objects)
#
# And event object interfaces:
#
#                   Event
#                     |
#              +--------------+
#              |              |
#           UIEvent      MutationEvent
#              |
#              |
#          MouseEvent
#
# References:
# 
#   DOM:
#     http://www.w3.org/TR/DOM-Level-3-Events/
#
#   Gecko:
#     http://developer.mozilla.org/en/docs/DOM:event
#     http://developer.mozilla.org/en/docs/DOM:document.createEvent
#
#-------------------------------------------------------------------------

############################################################################
# DOCUMENTATION FOR EVENTS SUB-SYSTEM
#
if {$::hv3::dom::CREATE_DOM_DOCS} {
  namespace eval ::hv3::DOM::macros {
    proc ER {type target bubbles cancellable eventtype} {
      return "
        <TR><TD width=16ex>$type 
            <TD>$target 
            <TD width=1%>$bubbles 
            <TD width=1%>$cancellable 
            <TD width=1%><A href=$eventtype>$eventtype</A>
      "
    }
    proc ::hv3::DOM::docs::eventoverview {} [subst -novar { return {
    <HTML>
      <HEAD>
        <LINK rel=stylesheet href="home://dom/style.css">
        <STYLE>
          TABLE          { margin: 0 1cm; width: 100% }
          H1             { margin-left: auto; margin-right: auto }
          TH             { white-space: nowrap }
        </STYLE>
    
        <TITLE>Overview of Events</TITLE>
      </HEAD>
    
      <H1>Hv3 Events Reference</H1>
      <P>
        This document describes the way Hv3 dispatches events for handling
        by javascript programs embedded in web-pages. It also describes the
        <A>specific set of events supported by Hv3</A>.
      <P>
        In general, Hv3 supports those events specified by Chapter 18 of the 
        <A href="http://www.w3.org/TR/html401/">HTML 4.01 standard</A>. 
        The processing model for events is based on the W3C
        <A href="http://www.w3.org/TR/DOM-Level-2-Events/">DOM Level 2 Events
        </A> recomendation.
    
      <H2>Event Processing in Hv3</H2>
    
      <H2 name="types">Supported Event Types</H2>
    
      <H3>Document Events</H3>
    
      <TABLE border=1>
        <TR><TH>Event<TH>Target<TH>Bubbles<TH>Cancellable<TH>Event Object Type
        [ER   Load    Window 0 0 Event]
        [ER   Unload {} {} {} {}]
      </TABLE>
    
      <H3>Mouse Events</H3>
      <TABLE border=1>
        <TR><TH>Event<TH>Target<TH>Bubbles<TH>Cancellable<TH>Event Object Type
        [ER Click     {All elements and text} 1 1 MouseEvent]
        [ER Dblclick  {} {} {} {}]
        [ER Mousedown {All HTMLElement, Text} 1 1 MouseEvent]
        [ER Mouseup   {All HTMLElement, Text} 1 1 MouseEvent]
        [ER Mouseover {All HTMLElement, Text} 1 1 MouseEvent]
        [ER Mousemove {All HTMLElement, Text} 1 0 MouseEvent]
        [ER Mouseout  {All HTMLElement, Text} 1 1 MouseEvent]
      </TABLE>
    
      <H3>HTML Forms Related Events</H3>
      <TABLE border=1>
        <TR><TH>Event<TH>Target<TH>Bubbles<TH>Cancellable<TH>Event Object Type
        [ER Focus  {Form controls that take the focus} 0 0 Event]
        [ER Blur   {Form controls that take the focus} 0 0 Event]
        [ER Submit {FORM elements} 1 0 Event]
        [ER Reset  {FORM elements} 1 0 Event]
        [ER Select {Form controls that generate text fields} 1 0 Event]
        [ER Change {Form controls with state} 1 0 Event]
      </TABLE>
    
      <H3>Keyboard Events</H3>
      <TABLE border=1>
        <TR><TH>Event<TH>Target<TH>Bubbles<TH>Cancellable<TH>Event Object Type
        [ER Keypress  {} {} {} {}]
        [ER Keydown   {} {} {} {}]
        [ER Keyup     {} {} {} {}]
      </TABLE>
    
    </HTML>
    }}]
  }


  ::hv3::dom2::stateless Event {
  
    -- Constant value 1.
    dom_get CAPTURING_PHASE {}
    -- Constant value 2.
    dom_get AT_TARGET       {}
    -- Constant value 3.
    dom_get BUBBLING_PHASE  {}
  
    -- A string containing the type of the event. For example 
    -- <code>"click"</code> or <code>"load"</code>.
    dom_get type       {}

    -- Boolean value. Set to true if the event bubbles. See 
    -- [Ref eventoverview {the event overview}] for a table describing
    -- which events bubble and which do not.
    dom_get bubbles    {}

    -- Boolean value. Set to true if the event is cancelable. See 
    -- [Ref eventoverview {the event overview}] for a table describing
    -- which events are cancelable and which are not.
    dom_get cancelable {}
  
    -- This property is supposed to return a timestamp in milliseconds
    -- from the epoch. But the DOM spec notes that this information is not
    -- available on all systems, in which case the property should return 0. 
    -- Hv3 always returns 0.
    dom_get timestamp  {}
  
    dom_call_todo initEvent
  }
  
  ::hv3::dom2::stateless UIEvent {
    Inherit Event   {}
    dom_get view    {}
    dom_get detail  {}
    dom_call_todo initUIEvent 
  }
  ::hv3::dom2::stateless MouseEvent {
    Inherit UIEvent { }
  
    dom_call_todo initMouseEvent
  
    -- This attribute is populated for events of type 'mouseup', 'mousedown',
    -- 'click' and 'dblclick' to indicate which mouse button the event
    -- is related to. For mice configured for right-hand use, buttons are
    -- numbered from right to left, starting with zero (left-click sets
    -- this attribute to zero).
    dom_get button  {}
  
    -- X coordinate where the event occurred relative to the top left
    -- corner of the viewport.
    dom_get clientX {}

    -- Y coordinate where the event occurred relative to the top left
    -- corner of the viewport.
    dom_get clientY {}
  
    -- X coordinate where the event occurred relative to the screen
    -- coordinate system.
    dom_get screenX {}

    -- Y coordinate where the event occurred relative to the screen
    -- coordinate system.
    dom_get screenY {}
  
    -- Boolean attribute. True if the 'ctrl' key was pressed when the
    -- event was fired.
    dom_get ctrlKey  {}

    -- Boolean attribute. True if the 'shift' key was pressed when the
    -- event was fired.
    dom_get shiftKey {}

    -- Boolean attribute. True if the 'alt' key was pressed when the
    -- event was fired.
    dom_get altKey   {}

    -- Boolean attribute. True if the 'meta' key was pressed when the
    -- event was fired.
    dom_get metaKey  {}
  
    -- This attribute is populated for 'mouseover' and 'mouseout' events
    -- only. For a 'mouseover' event, this property is set to the document
    -- node that the pointer is exiting, if any. For 'mouseout', it is set to
    -- the document node that the pointer is exiting (if any).
    dom_get relatedTarget  {}
  
    -- This property is for Gecko compatibility only. It is always equal
    -- to the value of the <I>button</I> property plus one.
    dom_get which  { }
  }

  ::hv3::dom2::stateless MutationEvent {
    Inherit Event { }
  
    dom_call_todo initMutationEvent 
  
    -- Constant value 1.
    dom_get MODIFICATION {}
    -- Constant value 2.
    dom_get ADDITION     {}
    -- Constant value 3.
    dom_get REMOVAL      {}
  
    dom_todo relatedNode
    dom_todo prevValue
    dom_todo newValue
    dom_todo attrName
    dom_todo attrChange
  }

  namespace delete ::hv3::DOM::macros
}
# END OF DOCUMENTATION
############################################################################

# The $HTML_Events_List variable contains a list of HTML events 
# handled by this module. This is used at runtime by HTMLElement 
# objects. This list comes from chapter 18 ("Scripts") of HTML 4.01.
#
# Other notes from HTML 4.01:
#
# The "load" and "unload" events are valid for <BODY> and 
# <FRAMESET> elements only.
#
# Events "focus" and "blur" events are only valid for elements
# that accept keyboard focus. HTML 4.01 defines a list of these, but
# it is different from Hv3's implementation (example, <A href="...">
# constructions do not accept keyboard focus in Hv3).
#
# The "submit" and "reset" events apply only to <FORM> elements.
#
# The "select" event (occurs when a user selects some text in a field)
# may be used with <INPUT> and <TEXTAREA> elements.
#
# The "change" event applies to <INPUT>, <SELECT> and <TEXTAREA> elements.
#
# All other events "may be used with most elements".
#
set ::hv3::dom::HTML_Events_List [list]

# Events generated by the forms module. "submit" is generated correctly
# (and preventDefault() handled correctly). The others are a bit patchy.
#
lappend ::hv3::dom::HTML_Events_List    focus blur submit reset select change

# These events are currently only generated for form control elements
# that grab the keyboard focus (<INPUT> types "text" and "password", and
# <TEXT> elements). This is done by the forms module. Eventually this
# will have to change... 
#
lappend ::hv3::dom::HTML_Events_List    keypress keydown keyup

# Events generated by hv3.tcl (based on Tk widget binding events). At
# present the "dblclick" event is not generated. Everything else
# works.
#
lappend ::hv3::dom::HTML_Events_List    mouseover mousemove mouseout 
lappend ::hv3::dom::HTML_Events_List    mousedown mouseup 
lappend ::hv3::dom::HTML_Events_List    click dblclick 

# Events generated by hv3.tcl (based on protocol events). At the
# moment the "unload" event is never generated. The "load" event 
# is generated for the <BODY> node only (but see the [event] method
# of the ::hv3::dom object for a clarification - sometimes it is
# generated against the Window object).
#
lappend ::hv3::dom::HTML_Events_List    load unload

# Set up the HTMLElement_EventAttrArray array. This is used
# at runtime while compiling legacy event listeners (i.e. "onclick") 
# to javascript functions.
foreach E $::hv3::dom::HTML_Events_List {
  set ::hv3::DOM::HTMLElement_EventAttrArray(on${E}) 1
}

#-------------------------------------------------------------------------
# DocumentEvent (DOM Level 2 Events)
#
#     This interface is mixed into the Document object. It provides
#     a single method, used to create a new event object:
#
#         createEvent()
#
set ::hv3::dom::code::DOCUMENTEVENT {

  # The DocumentEvent.createEvent() method. The argument (specified as
  # type DOMString in the spec) should be one of the following:
  #
  #     "HTMLEvents"
  #     "UIEvents"
  #     "MouseEvents"
  #     "MutationEvents"
  #     "Events"
  #
  dom_call -string createEvent {THIS eventType} {
    if {![info exists ::hv3::DOM::EventGroup($eventType)]} {
      error {DOMException HIERACHY_REQUEST_ERR}
    }
    append arrayvar ::hv3::DOM::ea [incr ::hv3::dom::next_array]
    list transient [list $::hv3::DOM::EventGroup($eventType) $myDom $arrayvar]
  }
  set ::hv3::DOM::EventGroup(HTMLEvents)     ::hv3::DOM::Event
  set ::hv3::DOM::EventGroup(Events)         ::hv3::DOM::Event
  set ::hv3::DOM::EventGroup(MouseEvent)     ::hv3::DOM::MouseEvent
  set ::hv3::DOM::EventGroup(UIEvents)       ::hv3::DOM::UIEvent
  set ::hv3::DOM::EventGroup(MutationEvents) ::hv3::DOM::MutationEvent
}


# Recognised mouse event types.
#
#     Mapping is from the event-type to the value of the "cancelable"
#     property of the DOM MouseEvent object.
#
set ::hv3::dom::MouseEventType(click)     1
set ::hv3::dom::MouseEventType(mousedown) 1
set ::hv3::dom::MouseEventType(mouseup)   1
set ::hv3::dom::MouseEventType(mouseover) 1
set ::hv3::dom::MouseEventType(mousemove) 0
set ::hv3::dom::MouseEventType(mouseout)  1


# Recognised HTML event types.
#
#     Mapping is from the event-type to the value of the "bubbles" and
#     "cancelable" property of the DOM Event object.
#
set ::hv3::dom::HtmlEventType(load)     [list 0 0]
set ::hv3::dom::HtmlEventType(submit)   [list 0 1]
set ::hv3::dom::HtmlEventType(change)   [list 1 1]

set ::hv3::dom::HtmlEventType(keyup)    [list 1 0]
set ::hv3::dom::HtmlEventType(keydown)  [list 1 0]
set ::hv3::dom::HtmlEventType(keypress) [list 1 0]

set ::hv3::dom::HtmlEventType(focus)    [list 0 0]
set ::hv3::dom::HtmlEventType(blur)     [list 0 0]

namespace eval ::hv3::dom {

  # dispatchMouseEvent --
  #
  #     $dom         -> the ::hv3::dom object
  #     $eventtype   -> One of the above event types, e.g. "click".
  #     $EventTarget -> The DOM object implementing the EventTarget interface
  #     $x, $y       -> Widget coordinates for the event
  #
  proc dispatchMouseEvent {dom type js_obj x y extra} {
    set isCancelable $::hv3::dom::MouseEventType($type)

    # Dispatch an event of type MouseEvent. The properties are broken into
    # the following blocks:
    #
    #   DOM Event
    #   DOM UIEvent
    #   DOM MouseEvent
    #   Gecko compatibility
    #
    Dispatch [$dom see] $js_obj [list                    \
      CAPTURING_PHASE {number 1}                         \
      AT_TARGET       {number 2}                         \
      BUBBLING_PHASE  {number 3}                         \
      type            [list string $type]                \
      bubbles         {boolean 1}                        \
      cancelable      [list boolean $isCancelable]       \
      timestamp       {number 0}                         \
\
      view            undefined                          \
      detail          undefined                          \
\
      altKey          [list boolean 0]                   \
      button          [list number 0]                    \
      clientX         [list number $x]                   \
      clientY         [list number $y]                   \
      ctrlKey         [list boolean 0]                   \
      metaKey         [list boolean 0]                   \
      relatedTarget   undefined                          \
      screenX         undefined                          \
      screenY         undefined                          \
      shiftKey        [list boolean 0]                   \
\
      which           [list number 1]                    \
    ]
  }
    
  
  # dispatchHtmlEvent --
  #
  #     $dom     -> the ::hv3::dom object.
  #     $type    -> The event type (see list below).
  #     $js_obj  -> The DOM object with the EventTarget interface mixed in.
  #
  #     Dispatch one of the following events ($type):
  #
  #       load
  #       submit
  #       change
  #
  proc ::hv3::dom::dispatchHtmlEvent {dom type js_obj} {
    foreach {bubbles isCancelable} $::hv3::dom::HtmlEventType($type) {}
 
    Dispatch [$dom see] $js_obj [list                       \
      CAPTURING_PHASE {number 1}                            \
      AT_TARGET       {number 2}                            \
      BUBBLING_PHASE  {number 3}                            \
      type            [list string  $type]                  \
      bubbles         [list boolean $bubbles]               \
      cancelable      [list boolean $isCancelable]          \
      timestamp       {number 0}                            \
    ]
  }

  # Dispatch --
  #
  proc Dispatch {see js_obj event_obj} {
    foreach {isHandled isPrevented} [$see dispatch $js_obj $event_obj] {}
    if {$isPrevented} {return "prevent"}
    if {$isHandled}   {return "handled"}
    return ""
  }
}

