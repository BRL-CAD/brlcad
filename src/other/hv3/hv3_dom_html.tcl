namespace eval hv3 { set {version($Id$)} 1 }

#--------------------------------------------------------------------------
# DOM Level 1 Html
#
# This file contains the Hv3 implementation of the DOM Level 1 Html. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
#
# Interfaces defined in this file:
#
#     HTMLDocument
#     HTMLCollection
#     HTMLElement
#       HTMLFormElement
#       plus a truckload of other HTML***Element interfaces to come.
#
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLDocument (Node -> Document -> HTMLDocument)
#
# DOM level 1 interface (- sign means it's missing) in Hv3.
#
#     HTMLDocument.write(string)
#     HTMLDocument.writeln(string)
#     HTMLDocument.getElementById(string)
#     HTMLDocument.forms[]
#     HTMLDocument.anchors[]
#     HTMLDocument.links[]
#     HTMLDocument.applets[]
#     HTMLDocument.body
#     HTMLDocument.cookie
#
set BaseList {DocumentEvent}
::hv3::dom2::stateless HTMLDocument {

  %NODE%
  %NODE_PROTOTYPE%
  %DOCUMENT%
  %DOCUMENTEVENT%

  # The "title" attribute is supposed to be read/write. But this one
  # is only read-only for the meantime.
  dom_get title {
    list string [$myHv3 title]
  }
  dom_put title val {
    puts "TODO: HTMLDocument.title (Put method)"
  }

  # Read-only attribute "domain".
  dom_get domain {
    set str [$myHv3 uri authority]
    if {$str eq ""} {
      list null
    } else {
      list string $str
    }
  }

  # Read-only attribute "URL".
  dom_get URL {
    list string [$myHv3 uri get]
  }

  dom_get referrer {
    list string [$myHv3 referrer]
  }

  dom_todo open
  dom_todo close

  # The document collections (DOM level 1)
  #
  #     HTMLDocument.images[] 
  #     HTMLDocument.forms[]
  #     HTMLDocument.anchors[]
  #     HTMLDocument.links[]
  #     HTMLDocument.applets[] 
  #
  # TODO: applets[] is supposed to contain "all the OBJECT elements that
  # include applets and APPLET (deprecated) elements in a document". Here
  # only the APPLET elements are collected.
  #
  dom_get images  { HTMLDocument_Collection $myDom $myHv3 img }
  dom_get forms   { HTMLDocument_Collection $myDom $myHv3 form }
  dom_get applet  { HTMLDocument_Collection $myDom $myHv3 applet }
  dom_get anchors { HTMLDocument_Collection $myDom $myHv3 {a[name]} }
  dom_get links   { HTMLDocument_Collection $myDom $myHv3 {area,a[href]} }

  #-------------------------------------------------------------------------
  # The HTMLDocument.write() and writeln() methods (DOM level 1)
  #
  dom_call -string write {THIS str} {
    catch { [$myHv3 html] write text $str } msg
    return ""
  }
  dom_call -string writeln {THIS str} {
    catch { [$myHv3 html] write text "$str\n" }
    return ""
  }

  #-------------------------------------------------------------------------
  # HTMLDocument.getElementById() method. (DOM level 1)
  #
  # This returns a single object (or NULL if an object of the specified
  # id cannot be found).
  #
  dom_call -string getElementById {THIS elementId} {
    set elementId [string map [list "\x22" "\x5C\x22"] $elementId]
    set selector [subst -nocommands {[id="$elementId"]}]
    set node [$myHv3 html search $selector -index 0]
    if {$node ne ""} {
      return [list object [::hv3::dom::wrapWidgetNode $myDom $node]]
    }
    return null
  }

  #-------------------------------------------------------------------------
  # HTMLDocument.getElementsByName() method. (DOM level 1)
  #
  # Return a NodeList of the elements whose "name" value is set to
  # the supplied argument. This is similar to the 
  # Document.getElementsByTagName() method in hv3_dom_core.tcl.
  #
  dom_call -string getElementsByName {THIS elementName} {
    set name [string map [list "\x22" "\x5C\x22"] $elementName]
    set selector [subst -nocommands {[name="$name"]}]
    set nl [list ::hv3::DOM::NodeListS $myDom [
      list [$myHv3 html] search $selector
    ]]
    list transient $nl
  }

  #-----------------------------------------------------------------------
  # The HTMLDocument.cookie property (DOM level 1)
  #
  # The cookie property is a strange modeling. Getting and putting the
  # property are not related in the usual way (the usual way: calling Get
  # returns the value stored by Put).
  #
  # When setting the cookies property, at most a single cookie is added
  # to the cookies database. 
  #
  # The implementations of the following get and put methods interface
  # directly with the ::hv3::the_cookie_manager object. Todo: Are there
  # security implications here (in concert with the location property 
  # perhaps)?
  #
  dom_get cookie {
    list string [::hv3::the_cookie_manager Cookie [$myHv3 uri get]]
  }
  dom_put -string cookie value {
    ::hv3::the_cookie_manager SetCookie [$myHv3 uri get] $value
  }

  #-----------------------------------------------------------------------
  # The HTMLDocument.body property (DOM level 1)
  #
  dom_get body {
    # set body [lindex [$myHv3 search body] 0]
    set body [lindex [[$myHv3 node] children] 1]
    list object [::hv3::dom::wrapWidgetNode $myDom $body]
  }

  #-----------------------------------------------------------------------
  # The "location" property (Gecko compatibility)
  #
  # Setting the value of the document.location property is equivalent
  # to calling "document.location.assign(VALUE)".
  #
  dom_get location { 
    set obj [list ::hv3::DOM::Location $myDom $myHv3]
    list object $obj
  }
  dom_put -string location value {
    Location_assign $myHv3 $value
  }

  #-------------------------------------------------------------------------
  # Handle unknown property requests.
  #
  # An unknown property may refer to certain types of document element
  # by either the "name" or "id" HTML attribute.
  #
  # 1: Have to find some reference for this behaviour...
  # 2: Maybe this is too inefficient. Maybe it should go to the 
  #    document.images and document.forms collections.
  #
  dom_get * {

    # Selectors to use to find document nodes.
    set nameselector [subst -nocommands {
      form[name="$property"],img[name="$property"]
    }]
    set idselector [subst -nocommands {
      form[id="$property"],img[id="$property"]
    }]
 
    foreach selector [list $nameselector $idselector] {
      set node [$myHv3 html search $selector -index 0]
      if {$node ne ""} {
        return [list object [::hv3::dom::wrapWidgetNode $myDom $node]]
      }
    }

    list
  }
}
namespace eval ::hv3::DOM {
  proc HTMLDocument_Collection {dom hv3 selector} {
    set cmd [list [$hv3 html] search $selector]
    list object [list ::hv3::DOM::HTMLCollectionS $dom $cmd] 
  }
}

#-------------------------------------------------------------------------
# DOM Type HTMLElement (Node -> Element -> HTMLElement)
#
set HTMLElement {
  %NODE_PROTOTYPE%
  %NODE%
  %WIDGET_NODE%
  %ELEMENT%
  %HTMLELEMENT%
  %ELEMENTCSSINLINESTYLE%
}
set ::hv3::dom::code::HTMLELEMENT {
  
  element_attr id
  element_attr title
  element_attr lang
  element_attr dir
  element_attr className -attribute class

  # The above is all that is defined by DOM Html Level 1 for HTMLElement.
  # Everything below this point is for compatibility with legacy browsers.
  #
  #   See Mozilla docs: http://developer.mozilla.org/en/docs/DOM:element
  #
  dom_todo localName
  dom_todo namespaceURI
  dom_todo prefix
  dom_todo textContent

  #----------------------------------------------------------------------
  # The HTMLElement.innerHTML property. This is not part of any standard.
  # See reference for the equivalent mozilla property at:
  #
  #     http://developer.mozilla.org/en/docs/DOM:element.innerHTML
  #
  dom_get innerHTML { 
    set res [HTMLElement_getInnerHTML $myNode]
    set res
  }
  dom_put -string innerHTML val { 
    HTMLElement_putInnerHTML $myDom $myNode $val
  }

  #--------------------------------------------------------------------
  # The completely non-standard offsetParent, offsetTop and offsetLeft.
  # TODO: Ref.
  #
  dom_get offsetParent { 
    set N [HTMLElement_offsetParent $myNode]
    if {$N ne ""} { 
      list object [::hv3::dom::wrapWidgetNode $myDom $N] 
    } else {
      list null
    }
  }
  
  # Implementation of Gecko/DHTML compatibility properties for 
  # querying layout geometry:
  #
  #  offsetLeft offsetTop offsetHeight offsetWidth
  #
  #    offsetHeight and offsetWidth are the height and width of the
  #    generated box, including borders.
  #
  #    offsetLeft and offsetTop are the offsets of the generated box
  #    within the box generated by the element returned by DOM
  #    method HTMLElement.offsetParent().
  #
  #  clientLeft clientTop clientHeight clientWidth
  #
  #    clientHeight and clientWidth are the height and width of the
  #    box generated by the node, including padding but not borders.
  #    clientLeft is the width of the generated box left-border. clientTop 
  #    is the generated boxes top border.
  #
  #    Note that there are complications with boxes that generate their
  #    own scrollbars if the scrollbar is on the left or top side. Since
  #    hv3 never does this, it doesn't matter.
  #
  #  scrollLeft scrollTop scrollHeight scrollWidth
  #
  #    For most elements, scrollLeft and scrollTop are 0, and scrollHeight
  #    and scrollWidth are equal to clientHeight and clientWidth.
  #
  #    For elements that generate their own scrollable boxes, (i.e. with 
  #    'overflow' property set to "scroll") scrollHeight and scrollWidth
  #    are the height and width of the scrollable areas. scrollLeft/scrollTop
  #    are the current scroll offsets.
  #
  #    One of the <HTML> or <BODY> nodes returns values for these attributes
  #    corresponding to the scrollbars and scrollable region for the whole
  #    document. In firefox standards mode, it is the <HTML> element.
  #    In quirks mode, the <BODY>. In Hv3 it is always the <HTML> element.
  #
  #    BUG: For nodes other than the <HTML> node, values are always all 0.
  #
  dom_get offsetLeft { 
    list number [lindex [HTMLElement_offsetBox $myDom $myNode] 0]
  }
  dom_get offsetTop { 
    list number [lindex [HTMLElement_offsetBox $myDom $myNode] 1]
  }
  dom_get offsetHeight { 
    set bbox [HTMLElement_offsetBox $myDom $myNode]
    list number [expr {[lindex $bbox 3] - [lindex $bbox 1]}]
  }
  dom_get offsetWidth { 
    set bbox [HTMLElement_offsetBox $myDom $myNode]
    list number [expr {[lindex $bbox 2] - [lindex $bbox 0]}]
  }

  dom_get clientLeft {
    set bw [$myNode property border-left-width]
    list number [string range $bw 0 end-2]
  }
  dom_get clientTop {
    set bw [$myNode property border-top-width]
    list number [string range $bw 0 end-2]
  }
  dom_get clientHeight {
    set N $myNode
    set bbox [HTMLElement_nodeBox $myDom $N]
    set bt [string range [$N property border-top-width] 0 end-2]
    set bb [string range [$N property border-bottom-width] 0 end-2]
    list number [expr [lindex $bbox 3] - [lindex $bbox 1] - $bt - $bb]
  }
  dom_get clientWidth {
    set N $myNode
    set bbox [HTMLElement_nodeBox $myDom $N]
    set bt [string range [$N property border-left-width] 0 end-2]
    set bb [string range [$N property border-right-width] 0 end-2]
    list number [expr [lindex $bbox 2] - [lindex $bbox 0] - $bt - $bb]
  }

  # See comments above for what these are supposed to do.
  #
  dom_get scrollTop    { 
    list number [HTMLElement_scrollTop [$myNode html] $myNode]
  }
  dom_get scrollLeft   { 
    list number [HTMLElement_scrollLeft [$myNode html] $myNode]
  }
  dom_get scrollWidth  {
    list number [HTMLElement_scrollWidth [$myNode html] $myNode]
  }
  dom_get scrollHeight { 
    list number [HTMLElement_scrollHeight [$myNode html] $myNode]
  }

  dom_events {
    set R [list]
    foreach {k v} [$myNode attr] {
      if {[info exists ::hv3::DOM::HTMLElement_EventAttrArray($k)]} {
        lappend R $k $v
      }
    }
    return $R
  }
}

::hv3::dom2::stateless HTMLElement $HTMLElement

set HTMLElement "Inherit HTMLElement { $HTMLElement }"

namespace eval ::hv3::DOM {

  # Return the scrollable width and height of the window $html.
  #
  proc HTMLElement_scrollWidth {html node} {
    if {[$html node] ne $node} {return 0}
    foreach {x1 x2} [$html xview] {}
    set w [expr {double([winfo width $html])}]
    expr {int(0.5 + ($w/($x2-$x1)))}
  }

  proc HTMLElement_scrollHeight {html node} {
    if {[$html node] ne $node} {return 0}
    foreach {y1 y2} [$html yview] {}
    set h [expr {double([winfo height $html])}]
    expr {int(0.5 + ($h/($y2-$y1)))}
  }

  proc HTMLElement_scrollTop {html node} {
    foreach {y1 y2} [$html yview] {}
    expr {int(0.5 + $y1 * double([HTMLElement_scrollHeight $html $node]))}
  }

  proc HTMLElement_scrollLeft {html node} {
    foreach {x1 x2} [$html xview] {}
    expr {int(0.5 + $x1 * double([HTMLElement_scrollWidth $html $node]))}
  }

  proc HTMLElement_offsetParent {node} {
    for {set N [$node parent]} {$N ne ""} {set N [$N parent]} {
      set position [$N property position]
      if {$position ne "static"} break
    }
    return $N
  }

  proc HTMLElement_nodeBox {dom node} {
    set html [$node html]
    if {$node eq [$html node]} {
      set bbox [$html bbox]
    } else {
      set bbox [$html bbox $node]
    }
    if {$bbox eq ""} {set bbox [list 0 0 0 0]}
    return $bbox
  }

  proc HTMLElement_offsetBox {dom node} {
    set bbox [HTMLElement_nodeBox $dom $node]

    set parent [HTMLElement_offsetParent $node]
    if {$parent ne ""} {
      set bbox2 [HTMLElement_nodeBox $dom $parent]
      set x [lindex $bbox2 0]
      set y [lindex $bbox2 1]
      lset bbox 0 [expr {[lindex $bbox 0] - $x}]
      lset bbox 1 [expr {[lindex $bbox 1] - $y}]
      lset bbox 2 [expr {[lindex $bbox 2] - $x}]
      lset bbox 3 [expr {[lindex $bbox 3] - $y}]
    }
    
#puts "OFFSETBOX: $node $bbox (parent = $parent)"
    return $bbox
  }

  proc HTMLElement_getInnerHTML {node} {
    set str [WidgetNode_ChildrenToHtml $node]
    list string $str
  }

  proc HTMLElement_putInnerHTML {dom node newHtml} {

    # Remove the existing children.
    set children [$node children]
    $node remove $children

    # TODO: At this point it would be nice to delete the child nodes.
    # But this will cause problems if the javascript world has any
    # references to them. The solution should be the same as the
    # HTMLElement.removeChild() function.
    #
    ### foreach child $children {
    ###  $child destroy
    ### }

    # Insert the new descendants, created by parsing $newHtml.
    set htmlwidget [$node html]
    set children [$htmlwidget fragment $newHtml]
    $node insert $children
    return ""
  }
}

#-------------------------------------------------------------------------
# DOM Type HTMLFormElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLFormElement $HTMLElement {

  # Various Get/Put string property/attributes.
  #
  element_attr name
  element_attr target
  element_attr method
  element_attr action
  element_attr acceptCharset -attribute acceptcharset
  element_attr enctype

  dom_get ** {
    set superset [subst -nocommands {
      lrange [::hv3::get_form_nodes $myNode] 1 end
    }]
    HTMLCollectionC_getUnknownProperty $myDom $superset $property
  }

  # The HTMLFormElement.elements array.
  #
  dom_get elements {
    set cmd [subst -nocommands {lrange [::hv3::get_form_nodes $myNode] 1 end}]
    list object [list ::hv3::DOM::HTMLCollectionC $myDom $cmd]
  }

  # Form control methods: submit() and reset().
  #
  dom_call submit {THIS} {
    set form [$myNode replace]
    $form submit ""
  }
  dom_call reset {THIS} {
    set form [$myNode replace]
    $form reset
  }

  # Unknown property handler. Delegate any unknown property requests to
  # the HTMLFormElement.elements object.
  #
  #dom_get * {
    #set obj [lindex [eval HTMLFormElement $myDom $myNode Get elements] 1]
    #eval $obj Get $property
  #}
}
# </HTMLFormElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLInputElement (extends HTMLElement)
#
#     <INPUT> elements. The really complex stuff for this object is 
#     handled by the forms manager code.
#
::hv3::dom2::stateless HTMLInputElement $HTMLElement {

  dom_todo defaultValue
  dom_todo readOnly

  element_attr accept
  element_attr accessKey -attribute accesskey
  element_attr align
  element_attr alt
  element_attr disabled
  element_attr name
  element_attr maxLength -attribute maxlength
  element_attr size
  element_attr src
  element_attr tabIndex  -attribute tabindex
  element_attr type -readonly
  element_attr useMap

  dom_get form { HTMLElement_get_form $myDom $myNode }

  # According to DOM HTML Level 1, the HTMLInputElement.defaultChecked
  # is the HTML element attribute, not the current value of the form
  # control. Setting this attribute sets both the value of the form
  # control and the HTML attribute.
  #
  dom_get defaultChecked { 
    set c [$myNode attr -default 0 checked]
    list boolean $c
  }
  dom_put -string defaultChecked C { 
    set F [$myNode replace]
    $F dom_checked $C
    $myNode attr checked $C
  }

  # The HTMLInputElement.checked attribute on the other hand is the
  # current state of the form control. Writing to it does not change
  # the attribute on the underlying HTML element.
  #
  dom_get checked { 
    set F [$myNode replace]
    list boolean [$F dom_checked]
  }
  dom_put -string checked C { 
    set F [$myNode replace]
    $F dom_checked $C
  }

  -- This property refers to current value of the corresponding form control 
  -- if the \"type\" property is one of \"Text\", \"File\" or \"Password\".
  -- In this case writing to this property sets the value of the form 
  -- control. Note that writing to this attribute is not permitted if the
  -- type is \"File\". This is a security measure. If this is attempted the
  -- write will silently fail.
  --
  -- If the \"type\"  property is other than those listed above, then this
  -- property refers the attribute on the underlying HTML element.
  --
  dom_get value {
    set SPECIAL [list text file password]
    set T [string tolower [$myNode attr -default text type]]
    if {[lsearch $SPECIAL $T]>=0} {
      set F [$myNode replace]
      list string [$F dom_value]
    } else {
      list string [$myNode attr -default "" value]
    }
  }
  dom_put -string value V { 
    set SPECIAL [list text password]
    set T [string tolower [$myNode attr -default text type]]
    if {$T eq "file"} {return}
    if {[lsearch $SPECIAL $T]>=0} {
      set F [$myNode replace]
      $F dom_value $V
    } else {
      $myNode attr value $V
    }
  }

  dom_call blur   {THIS} { [$myNode replace] dom_blur }
  dom_call focus  {THIS} { [$myNode replace] dom_focus }
  dom_call select {THIS} { [$myNode replace] dom_select }
  dom_call click  {THIS} { [$myNode replace] dom_click }

  dom_scope { HTMLElement_control_scope $myDom $myNode }
}
# </HTMLInputElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLSelectElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLSelectElement $HTMLElement {

  dom_get form { HTMLElement_get_form $myDom $myNode }

  dom_get type {
    # DOM Level 1 says: "This is the string "select-multiple" when the 
    # multiple attribute is true and the string "select-one" when false."
    # However since Hv3 does not support multiple-select controls, this
    # property is always set to "select-one".
    list string "select-one"
  }

  dom_get selectedIndex {
    list number [[$myNode replace] dom_selectionIndex]
  }
  dom_put -string selectedIndex value {
    [$myNode replace] dom_setSelectionIndex $value
  }

  dom_get value {
    # The value attribute is read-only for this element. It is set to
    # the string value that will be submitted by this control during
    # form submission.
    if {[::hv3::boolean_attr $myNode disabled 0]} {
      list string ""
    } else {
      list string [[$myNode replace] value]
    }
  }

  dom_get length {
    list number [llength [HTMLSelectElement_getOptions $myNode]]
  }

  dom_get options {
    list object [list ::hv3::DOM::HTMLOptionsCollection $myDom $myNode]
  }

  dom_get multiple {
    # In Hv3, this attribute is always 0. This is because Hv3 does not
    # support multiple-select controls. But maybe it should...
    list number 0
  }

  element_attr name
  element_attr size
  element_attr tabIndex -attribute tabindex

  # The "disabled" property.
  dom_get disabled { 
    list boolean [::hv3::boolean_attr $myNode disabled false] 
  }
  dom_put -string disabled val { 
    ::hv3::put_boolean_attr $myNode disabled $val
    [$myNode replace] treechanged
  }

  dom_call_todo add

  # void remove(in long index);
  #
  #     Remove the index'th option from the <select> node.
  #
  dom_call -string remove {THIS idx} {
    set options [HTMLSelectElement_getOptions $myNode]
    set o [lindex $options [expr {int($idx)}]] 
    if {$o ne ""} {
      $myNode remove $o
      [$myNode replace] treechanged
     
      # TODO: See HTMLElement.removeChild(). Same problem with memory
      # management of "possibly deleted" nodes occurs here.
      #
      ### $o destroy
    }
  }

  dom_call blur   {THIS} { [$myNode replace] dom_blur }
  dom_call focus  {THIS} { [$myNode replace] dom_focus }

  #--------------------------------------------------------------------
  # Non-standard stuff starts here.
  #
  dom_get * {
    set obj [lindex [HTMLSelectElement $myDom $myNode Get options] 1]
    eval $obj Get $property
  }

  dom_scope { HTMLElement_control_scope $myDom $myNode }
}
namespace eval ::hv3::DOM {
  proc HTMLSelectElement_getOptions {node} {
    # Note: This needs to be merged with code in hv3_form.tcl.
    set ret [list]
    foreach child [$node children] {
      if {[$child tag] == "option"} {lappend ret $child}
    }
    set ret
  }
  proc HTMLElement_get_form {dom node} {
    set f [lindex [::hv3::get_form_nodes $node] 0]
    if {$f eq ""} { 
      set f null
    } else {
      list object [::hv3::dom::wrapWidgetNode $dom $f]
    }
  }
  proc HTMLElement_control_scope {dom node} {
    set scope [list \
      [::hv3::dom::wrapWidgetNode $dom $node] [node_to_document $node]
    ]

    set f [lindex [::hv3::get_form_nodes $node] 0]
    if {$f ne ""} { 
      set scope [linsert $scope 1 [::hv3::dom::wrapWidgetNode $dom $f]]
    }
    return $scope
  }
}
# </HTMLSelectElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLTextAreaElement (extends HTMLElement)
#
#
#     http://api.kde.org/cvs-api/kdelibs-apidocs/khtml/html/classDOM_1_1HTMLTextAreaElement.html
#
::hv3::dom2::stateless HTMLTextAreaElement $HTMLElement {

  dom_todo defaultValue;
  dom_todo cols;

  element_attr accessKey -attribute accesskey
  element_attr name;
  element_attr tabIndex -attribute tabindex; 

  dom_get type { list string textarea }

  dom_todo disabled;
  dom_todo readOnly;
  dom_todo rows;
  dom_todo value;

  dom_get form { HTMLElement_get_form $myDom $myNode }

  dom_get value             { HTMLTextAreaElement_getValue $myNode }
  dom_put -string value val { HTMLTextAreaElement_putValue $myNode $val }

  dom_call focus  {THIS} { [$myNode replace] dom_focus }
  dom_call blur   {THIS} { [$myNode replace] dom_blur }
  dom_call select {THIS} { HTMLTextAreaElement_select $myNode }

  #-------------------------------------------------------------------------
  # The following are not part of the standard DOM. They are mozilla
  # extensions. KHTML implements them too. 
  #
  dom_get selectionEnd   { HTMLTextAreaElement_getSelection $myNode 1 }
  dom_get selectionStart { HTMLTextAreaElement_getSelection $myNode 0 }

  # I couldn't find any mozilla docs for this API, but it is used
  # all over the place. Hv3 sets the selected region of the text
  # widget to include all characters between $start and $end, which
  # must both be numbers (and are cast to integers). The insertion
  # cursor is set to the character just after the selected range.
  #
  dom_call -string setSelectionRange {THIS start end} {
    HTMLTextAreaElement_setSelectionRange $myNode $start $end
  }

  dom_scope { HTMLElement_control_scope $myDom $myNode }
}
namespace eval ::hv3::DOM {

  proc HTMLTextAreaElement_setSelectionRange {node iStart iEnd} {
    set iStart [expr {int($iStart)}]
    set iEnd   [expr {int($iEnd)}]
    set t [[$node replace] get_text_widget]
    $t tag remove sel 0.0 end
    $t tag add sel "0.0 + $iStart c" "0.0 + $iEnd c"
    $t mark set insert "0.0 + $iEnd c"
    return ""
  }

  proc HTMLTextAreaElement_getSelection {node isEnd} {
    set t [[$node replace] get_text_widget]
    set sel [$t tag nextrange sel 0.0]
    if {$sel eq ""} {
      set ret [string length [$t get 0.0 insert]]
    } else {
      set ret [string length [$t get 0.0 [lindex $sel $isEnd]]]
    }
    list number $ret
  }

  proc HTMLTextAreaElement_select {node} {
    set t [[$node replace] get_text_widget]
    $t tag add sel 0.0 end
    list undefined
  }

  proc HTMLTextAreaElement_getValue {node} {
    list string [[$node replace] value] 
  }

  proc HTMLTextAreaElement_putValue {node str} {
    set t [[$node replace] get_text_widget]
    $t delete 0.0 end
    $t insert 0.0 $str
  }
}
# </HTMLTextAreaElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLButtonElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLButtonElement $HTMLElement {

  dom_todo disabled;

  dom_get form { HTMLElement_get_form $myDom $myNode }

  element_attr accessKey -attribute accesskey
  element_attr name;
  element_attr tabIndex -attribute tabindex;
  element_attr type;

  dom_todo value;

  dom_scope { HTMLElement_control_scope $myDom $myNode }
}
# </HTMLButtonElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLOptGroupElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLOptGroupElement $HTMLElement {
}
# </HTMLOptGroupElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLOptionElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLOptionElement $HTMLElement {

  dom_todo defaultSelected
  dom_todo index
  dom_todo disabled

  dom_get form {
    set select [HTMLOptionElement_getSelect $myNode]
    if {$select ne ""} {
      HTMLElement_get_form $myDom $select
    } else {
      set f null
    }
  }

  dom_get text {
    list string [HTMLOptionElement_getText $myNode]
  }
  dom_put -string text zText {
    set z [string map {< &lt; > &gt;} $zText]
    HTMLElement_putInnerHTML $myDom $myNode $z
    HTMLOptionElement_treechanged $myNode
  }

  # TODO: After writing this attribute, have to update data 
  # structures in the hv3_forms module.
  dom_get label {
    list string [HTMLOptionElement_getLabelOrValue $myNode label]
  }
  dom_put -string label v {
    $myNode attr label $v
  }

  dom_get selected {
    set select [HTMLOptionElement_getSelect $myNode]
    set idx [lsearch [HTMLSelectElement_getOptions $select] $myNode]
    if {$idx == [[$select replace] dom_selectionIndex]} {
      list boolean true
    } else {
      list boolean false
    }
  }
  dom_put -string selected v {
    set select [HTMLOptionElement_getSelect $myNode]
    set idx [lsearch [HTMLSelectElement_getOptions $select] $myNode]
    if {$v} {
      [$select replace] dom_setSelectionIndex $idx
    } else {
      [$select replace] dom_setSelectionIndex -1
    }
  }

  dom_get value {
    list string [HTMLOptionElement_getLabelOrValue $myNode value]
  }
  dom_put -string value v {
    # TODO: After writing this attribute, have to update data structures in
    # the hv3_forms module.
    $myNode attr value $v
  }
}
namespace eval ::hv3::DOM {
  proc HTMLOptionElement_getText {node} {
    set contents ""
    catch {
      set t [lindex [$node children] 0]
      set contents [$t text]
    }
    set contents
  }

  proc HTMLOptionElement_getSelect {node} {
    set P [$node parent]
    while {$P ne ""} {
      if {[$P tag] eq "select"} {
        return $P
      }
      set P [$P parent]
    }
    return ""
  }

  proc HTMLOptionElement_treechanged {node} {
    set P [HTMLOptionElement_getSelect $node]
    if {$P ne ""} {
      [$P replace] treechanged
    }
  }

  proc HTMLOptionElement_getLabelOrValue {node attr} {
    # If the element has text content, this is used as the default
    # for both the label and value of the entry (used if the Html
    # attributes "value" and/or "label" are not defined.
    #
    # Note: This needs to be merged with code in hv3_form.tcl.
    set contents [HTMLOptionElement_getText $node]
    $node attribute -default $contents $attr
  }
}

# </HTMLOptionElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLLabelElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLLabelElement $HTMLElement {
}
# </HTMLLabelElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLFieldSetElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLFieldSetElement $HTMLElement {
}
# </HTMLFieldSetElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLLegendElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLLegendElement $HTMLElement {
}
# </HTMLLegendElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLTableElement (extends HTMLElement)
#
::hv3::dom2::stateless HTMLTableElement $HTMLElement {

  dom_todo caption
  dom_todo tHead
  dom_todo tFoot

  dom_todo rows

  dom_get tBodies {
    set cmd [list HTMLTableElement_getTBodies $myNode]
    list object [list ::hv3::DOM::HTMLCollectionC $myDom $cmd]
  }

  element_attr align
  element_attr bgColor -attribute bgcolor
  element_attr border
  element_attr cellPadding -attribute cellpadding
  element_attr cellSpacing -attribute cellspacing
  element_attr frame
  element_attr rules
  element_attr summary
  element_attr width

  dom_call_todo createTHead
  dom_call_todo deleteTHead
  dom_call_todo createTFoot
  dom_call_todo deleteTFoot
  dom_call_todo createCaption
  dom_call_todo deleteCaption
  dom_call_todo insertRow
  dom_call_todo deleteRow
}
namespace eval ::hv3::DOM {
  proc HTMLTableElement_getTBodies {node} {
    set tbodies [list] 
    foreach child [$node children] {
      if {[$child tag] eq "tbody"} { lappend tbodies $child }
    }
    set tbodies
  }
}
# </HTMLTableElement>
#-------------------------------------------------------------------------
#-------------------------------------------------------------------------
# DOM Type HTMLTableSectionElement (extends HTMLElement)
#
#     This DOM type is used for HTML elements <TFOOT>, <THEAD> and <TBODY>.
#
::hv3::dom2::stateless HTMLTableSectionElement $HTMLElement {

  element_attr align
  element_attr ch -attribute char
  element_attr chOff -attribute charoff
  element_attr vAlign -attribute valign

  dom_get rows {
    set cmd [list HTMLTableSectionElement_getRows $myNode]
    list object [list ::hv3::DOM::HTMLCollectionC $myDom $cmd]
  }

  dom_todo insertRow
  dom_todo deleteRow
}
namespace eval ::hv3::DOM {
  proc HTMLTableSectionElement_getRows {node} {
    set rows [list] 
    foreach child [$node children] {
      if {[$child tag] eq "tr"} { lappend rows $child }
    }
    set rows
  }
}

# </HTMLTableSectionElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLTableRowElement (extends HTMLElement)
#
#     This DOM type is used for HTML <TR> elements.
#
::hv3::dom2::stateless HTMLTableRowElement $HTMLElement {

  dom_todo rowIndex
  dom_todo sectionRowIndex

  dom_get cells {
    set cmd [list HTMLTableRowElement_getCells $myNode]
    list object [list ::hv3::DOM::HTMLCollectionC $myDom $cmd]
  }

  element_attr align
  element_attr bgColor -attribute bgcolor
  element_attr ch -attribute char
  element_attr chOff -attribute charoff
  element_attr vAlign -attribute valign

  dom_todo insertCell
  dom_todo deleteCell
}
namespace eval ::hv3::DOM {
  proc HTMLTableRowElement_getCells {node} {
    set cells [list] 
    foreach child [$node children] {
      set tag [$child tag]
      if {$tag eq "td" || $tag eq "th"} {lappend cells $child}
    }
    set cells
  }
}
# </HTMLTableRowElement>
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# DOM Type HTMLAnchorElement (extends HTMLElement)
#
#     This DOM type is used for HTML <A> elements.
#
::hv3::dom2::stateless HTMLAnchorElement $HTMLElement {

  element_attr accessKey -attribute accesskey
  element_attr charset
  element_attr coords
  element_attr href
  element_attr hreflang
  element_attr name
  element_attr rel
  element_attr shape
  element_attr tabIndex -attribute tabindex
  element_attr target
  element_attr type

  # Hv3 does not support keyboard focus on <A> elements yet. So these
  # two calls are no-ops for now.
  #
  dom_call focus {THIS} {list}
  dom_call blur {THIS} {list}
}

# </HTMLAnchorElement>
#-------------------------------------------------------------------------

::hv3::dom2::stateless HTMLImageElement $HTMLElement {

  element_attr lowSrc -attribute lowsrc
  element_attr name
  element_attr align
  element_attr alt
  element_attr border
  element_attr height
  element_attr hspace

  dom_get isMap { list boolean [::hv3::boolean_attr $myNode ismap false] }
  dom_put -string isMap val { ::hv3::put_boolean_attr $myNode ismap $val }

  element_attr longDesc -attribute longdesc;
  element_attr src;
  element_attr useMap -attribute usemap;
  element_attr vspace;
  element_attr width;
};

::hv3::dom2::stateless HTMLIFrameElement $HTMLElement {

  element_attr align
  element_attr frameBorder   -attribute frameborder
  element_attr height
  element_attr longDesc      -attribute longdesc
  element_attr marginHeight  -attribute marginheight
  element_attr marginWidth   -attribute marginwidth
  element_attr name
  element_attr scrolling
  element_attr src
  element_attr width
}

::hv3::dom2::stateless HTMLUListElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLOListElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLDListElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLDirectoryElement $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLMenuElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLLIElement        $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLDivElement       $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLParagraphElement $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLHeadingElement   $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLQuoteElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLPreElement       $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLBRElement        $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLBaseFontElement  $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLFontElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLHRElement        $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLModElement       $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLObjectElement    $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLParamElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLAppletElement    $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLMapElement       $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLAreaElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLScriptElement    $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLFrameSetElement  $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLFrameElement     $HTMLElement { #TODO }

::hv3::dom2::stateless HTMLTableCaptionElement $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLTableColElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLTableCellElement    $HTMLElement { #TODO }

::hv3::dom2::stateless HTMLHtmlElement $HTMLElement { 
  Inherit HTMLElement {
    dom_get parentNode {
      if {$myNode eq [[$myNode html] node]} {
        # TODO: Maybe this is not quite cacheable... But caching it saves
        # calling this code for every single event propagation....
        list cache object [node_to_document $myNode]
      } else {
        list null
      }
    }
  }
}
::hv3::dom2::stateless HTMLHeadElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLLinkElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLTitleElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLMetaElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLBaseElement      $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLStyleElement     $HTMLElement { #TODO }
::hv3::dom2::stateless HTMLBodyElement      $HTMLElement { #TODO }

#-------------------------------------------------------------------------
# Element/Text Node Factory:
#
#     This block implements a factory method called by the ::hv3::dom
#     object to transform html-widget node handles into DOM objects.
#
namespace eval ::hv3::dom {

  ::variable TagToNodeTypeMap

  # The TagToNodeTypeMap table contains mappings for all HTMLElement
  # types in HTML DOM Level 1 except for the following:
  #
  #   HTMLIsIndexElement
  #
  # This is because the <isindex> element is never part of the tree
  # in Hv3 (it is transformed to other forms elements first).
  #

  array set TagToNodeTypeMap {
    ""       ::hv3::DOM::Text
    a        ::hv3::DOM::HTMLAnchorElement
    img      ::hv3::DOM::HTMLImageElement
    iframe   ::hv3::DOM::HTMLIFrameElement
  }

  # HTML Forms related objects:
  array set TagToNodeTypeMap {
    form     ::hv3::DOM::HTMLFormElement
    button   ::hv3::DOM::HTMLButtonElement
    input    ::hv3::DOM::HTMLInputElement
    select   ::hv3::DOM::HTMLSelectElement
    textarea ::hv3::DOM::HTMLTextAreaElement
    optgroup ::hv3::DOM::HTMLOptGroupElement
    option   ::hv3::DOM::HTMLOptionElement
    label    ::hv3::DOM::HTMLLabelElement
    fieldset ::hv3::DOM::HTMLFieldSetElement
    legend   ::hv3::DOM::HTMLLegendElement
  }

  # HTML Tables related objects:
  array set TagToNodeTypeMap {
    table    ::hv3::DOM::HTMLTableElement
    tbody    ::hv3::DOM::HTMLTableSectionElement
    tfoot    ::hv3::DOM::HTMLTableSectionElement
    thead    ::hv3::DOM::HTMLTableSectionElement
    tr       ::hv3::DOM::HTMLTableRowElement
    caption  ::hv3::DOM::HTMLTableCaptionElement
    col      ::hv3::DOM::HTMLTableColElement
    th       ::hv3::DOM::HTMLTableCellElement
    td       ::hv3::DOM::HTMLTableCellElement
  }

  # HTML specific objects:
  array set TagToNodeTypeMap {
    html     ::hv3::DOM::HTMLHtmlElement 
    head     ::hv3::DOM::HTMLHeadElement
    link     ::hv3::DOM::HTMLLinkElement
    title    ::hv3::DOM::HTMLTitleElement
    meta     ::hv3::DOM::HTMLMetaElement 
    base     ::hv3::DOM::HTMLBaseElement
    style    ::hv3::DOM::HTMLStyleElement
    body     ::hv3::DOM::HTMLBodyElement
  }

  # Other:
  array set TagToNodeTypeMap {
    ul          ::hv3::DOM::HTMLUListElement 
    ol          ::hv3::DOM::HTMLOListElement 
    dl          ::hv3::DOM::HTMLDListElement 
    dir         ::hv3::DOM::HTMLDirectoryElement 
    menu        ::hv3::DOM::HTMLMenuElement 
    li          ::hv3::DOM::HTMLLIElement 
    Div         ::hv3::DOM::HTMLDivElement 
    p           ::hv3::DOM::HTMLParagraphElement 
    h1          ::hv3::DOM::HTMLHeadingElement 
    h2          ::hv3::DOM::HTMLHeadingElement 
    h3          ::hv3::DOM::HTMLHeadingElement 
    h4          ::hv3::DOM::HTMLHeadingElement 
    h5          ::hv3::DOM::HTMLHeadingElement 
    h6          ::hv3::DOM::HTMLHeadingElement 
    q           ::hv3::DOM::HTMLQuoteElement 
    blockquote  ::hv3::DOM::HTMLQuoteElement 
    pre         ::hv3::DOM::HTMLPreElement
    br          ::hv3::DOM::HTMLBRElement
    basefont    ::hv3::DOM::HTMLBaseFontElement
    font        ::hv3::DOM::HTMLFontElement
    hr          ::hv3::DOM::HTMLHRElement
    mod         ::hv3::DOM::HTMLModElement
    object      ::hv3::DOM::HTMLObjectElement
    param       ::hv3::DOM::HTMLParamElement
    applet      ::hv3::DOM::HTMLAppletElement
    map         ::hv3::DOM::HTMLMapElement
    area        ::hv3::DOM::HTMLAreaElement
    script      ::hv3::DOM::HTMLScriptElement
    frameSet    ::hv3::DOM::HTMLFrameSetElement
    frame       ::hv3::DOM::HTMLFrameElement
  }

  # Create a DOM HTMLElement or Text object in DOM $dom (type ::hv3::dom)
  # wrapped around the html-widget $node.
  #
  proc wrapWidgetNode {dom node} {
    ::variable TagToNodeTypeMap
    set tag [$node tag]
    set objtype ::hv3::DOM::HTMLElement
    catch { set objtype $TagToNodeTypeMap($tag) }
    list $objtype $dom $node
  }
}
#-------------------------------------------------------------------------
