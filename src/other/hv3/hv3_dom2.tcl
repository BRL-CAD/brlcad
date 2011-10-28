
#
# Contains the following:
#
#   Procs:
#       ::hv3::dom::get_inner_html
#       ::hv3::dom::set_inner_html
#
#   Types:
#       ::hv3::dom::HTMLDocument
#
#   Widgets:
#
#

#
# Implemented subset of DOM Level 1:
#
#     Node
#      +----- Document ---------- HTMLDocument
#      +----- CharacterData ----- Text
#      +----- Element  ---------- HTMLElement
#                                     +------- HTMLInputElement
#                                     +------- HTMLTextAreaElement
#                                     +------- HTMLFormElement
#                                     +------- HTMLImageElement
#                                     +------- HTMLAnchorElement
#
#     HTMLCollection
#

# Not implemented DOM Level 1:
#
#      DOMException
#      ExceptionCode
#      DOMImplementation
#      DocumentFragment
#      Attr
#      Comment
#
#      NodeList
#      NamedNodeMap
#





#------------------------------------------------------------------------
# http://www.w3.org/TR/DOM-Level-2-HTML/html.html
#
#
namespace eval ::hv3::dom {

  #----------------------------------------------------------------------
  # get_inner_html
  #
  #     This is a helper function for HTMLElement. The "inner-html" of
  #     element node $node is returned.
  #
  proc get_inner_html {node} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    set ret ""
    foreach child [$node children] {
      append ret [NodeToHtml $child]
    }
    return $ret
  }
  proc NodeToHtml {node} {
    set tag [$node tag]
    if {$tag eq ""} {
      return [$node text -pre]
    } else {
      set inner [get_inner_html $node]
      return "<$tag>$node</$tag>"
    }
  }


  #----------------------------------------------------------------------
  # set_inner_html
  #
  #     This is a helper function for HTMLElement. The "inner-html" of
  #     element node $node, owned by document frame $hv3, is set
  #     to $newHtml.
  #
  proc set_inner_html {hv3 node newHtml} {
    if {[$node tag] eq ""} {error "$node is not an HTMLElement"}

    # Destroy the existing children (and their descendants) of $node.
    set children [$node children]
    $node remove $children
    foreach child $children {
      $child destroy
    }

    # Insert the new descendants, created by parseing $newHtml.
    set children [[$hv3 html] fragment $newHtml]
    $node insert $children
  }
}


#-------------------------------------------------------------------------
# Snit class for the "document" object.
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
#     HTMLDocument.body[]
#     HTMLDocument.cookie
#
snit::type ::hv3::dom::HTMLDocument {

  variable myHv3

  js_init {dom hv3} {
    set myHv3 $hv3
  }

  #-------------------------------------------------------------------------
  # The HTMLDocument.write() and writeln() methods (DOM level 1)
  #
  js_scall write {THIS str} {
    catch { [$myHv3 html] write text $str }
    return ""
  }
  js_scall writeln {THIS str} {
    $self call_write $THIS "$str\n"
  }

  #-------------------------------------------------------------------------
  # HTMLDocument.getElementById() method. (DOM level 1)
  #
  # This returns a single object (or NULL if an object of the specified
  # id cannot be found).
  #
  js_scall getElementById {THIS elementId} {
    set node [lindex [$myHv3 search "#$elementId"] 0]
    if {$node ne ""} {
      return [list object [[$myHv3 dom] node_to_dom $node]]
    }
    return null
  }

  #-------------------------------------------------------------------------
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
  js_getobject images  { 
    hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 img 
  }
  js_getobject forms { 
    hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 form 
  }
  js_getobject anchors { 
    hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 {a[name]} 
  }
  js_getobject links { 
    hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 {area,a[href]} 
  }
  js_getobject applets { 
    hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 applet 
  }

  js_scall getElementsByTagName {THIS tag} { 
    set obj [hv3::dom::HTMLCollection %AUTO% [$self dom] $myHv3 $tag]
    $obj configure -finalizable 1
    list object $obj
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
  js_get cookie {
    list string [::hv3::the_cookie_manager Cookie [$myHv3 uri get]]
  }
  js_put cookie value {
    set str [[$self see] tostring $value]
    ::hv3::the_cookie_manager SetCookie [$myHv3 uri get] $str
  }

  #-----------------------------------------------------------------------
  # The "location" property (Gecko compatibility)
  #
  # Setting the value of the document.location property is equivalent
  # to calling "document.location.assign(VALUE)".
  #
  js_getobject location { ::hv3::dom::Location %AUTO% [$self dom] $myHv3 }
  js_put location value { 
    set location [lindex [$self Get location] 1]
    set assign [lindex [$location Get assign] 1]
    $assign Call THIS $value
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
  js_get * {

    # Allowable element types.
    set tags [list form img]

    # Selectors to use to find document nodes.
    set nameselector [subst -nocommands {[name="$property"]}]
    set idselector   [subst -nocommands {[id="$property"]}]
 
    foreach selector [list $nameselector $idselector] {
      set node [lindex [$myHv3 search $selector] 0]
      if {$node ne "" && [lsearch $tags [$node tag]] >= 0} {
        return [list object [[$myHv3 dom] node_to_dom $node]]
      }
    }

    return ""
  }

  js_finish {}
}

#-------------------------------------------------------------------------
# ::hv3::dom::HTMLElement
#
# DOM class: (Node -> Element -> HTMLElement)
#
# Supports the following interface:
#
#      Element.nodeName
#      Element.nodeValue
#      Element.nodeType
#      Element.parentNode
#      Element.childNodes
#      Element.firstChild
#      Element.lastChild
#      Element.previousSibling
#      Element.nextSibling
#      Element.attributes
#      Element.ownerDocument
#
# And the nonstandard:
#
#      HTMLElement.innerHTML
#      HTMLElement.style
#
namespace eval ::hv3::dom {

  proc snit_type {type args} {
    uplevel ::snit::type $type [list [join $args]]
  }

  snit_type HTMLElement {
    variable myNode ""
    variable myHv3 ""

    js_init {dom hv3 node} {
      set myNode $node
      set myHv3 $hv3
    }

  # Insert the event property code. Defined in file hv3_dom3.tcl.
  #
  } $DOM0Events_ElementCode {

    js_get tagName { 
      list string [string toupper [$myNode tag]]
    }
  
    js_getobject style { ::hv3::dom::InlineStyle %AUTO% [$self dom] $myNode }
  
    # Get/Put functions for the attributes of $myNode:
    #
    method GetBooleanAttribute {prop} {
      set bool [$myNode attribute -default 0 $prop]
      if {![catch {expr $bool}]} {
        return [list boolean [expr {$bool ? 1 : 0}]]
      } else {
        return [list boolean 1]
      }
    }
    method PutBooleanAttribute {prop value} {
      $myNode attribute $prop [lindex $value 1]
    }
  
    #-------------------------------------------------------------------
    # The following string attributes are common to all elements:
    #
    #     HTMLElement.id
    #     HTMLElement.title
    #     HTMLElement.lang
    #     HTMLElement.dir
    #     HTMLElement.className
    #
    js_getput_attribute id        id
    js_getput_attribute title     title
    js_getput_attribute lang      lang
    js_getput_attribute dir       dir
    js_getput_attribute className class

    js_get nodeType { list number 1 ;# 1 -> ELEMENT_NODE }

    js_getobject childNodes { ::hv3::dom::NodeList %AUTO% [$self dom] $myNode }
  
    #-------------------------------------------------------------------
    # Get and set the innerHTML property. The implmenetation of this
    # is in hv3_dom2.tcl.
    #
    js_get innerHTML { list string [::hv3::dom::get_inner_html $myNode] }
    js_put innerHTML {value} { 
      set code [[$self see] tostring $value ]
      ::hv3::dom::set_inner_html $myHv3 $myNode $code
    }
  
    js_finish {}
  
    method node {} {return $myNode}
  }
}
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# ::hv3::dom::NodeList
#
namespace eval ::hv3::dom {
  ::snit::type NodeList {
    variable myParent ""

    js_init {dom parent} {
      set myParent $parent
    }

    js_get length {
      list number [llength [$myParent children]]
    }

    js_scall item {THIS idx} {
      set i [format %.0f $idx]
      set child [lindex [$myParent children] $i]
      list object [[$self dom] node_to_dom $child]
    }

    js_finish {}
  }
}
#-------------------------------------------------------------------------

#-------------------------------------------------------------------------
# ::hv3::dom::XMLHttpRequest
#
#     Hv3 will eventually feature a fully-featured XMLHttpRequest object,
#     similar to that described here:
#     
#         http://www.w3.org/TR/XMLHttpRequest/
#
#     For now, this is a partial implementation to make the
#     tests/browsertest.tcl framework work.
#
namespace eval ::hv3::dom {
  ::snit::type XMLHttpRequest {

    variable myHv3

    variable myUri ""
    variable myRequestHandle ""

    js_init {dom hv3} {
      set myHv3 $hv3
    }

    #-------------------------------------------------------------------
    # XMLHttpRequest.readyState
    #
    variable myReadyState Uninitialized
    js_get readyState {
      switch -- $myReadyState {
        Uninitialized {list number 0}
        Open          {list number 1}
        Sent          {list number 2}
        Receiving     {list number 3}
        Loaded        {list number 4}
        default       {error "Bad myReadyState value: $myReadyState"}
      }
    }

    js_scall open {THIS http_method uri args} {
      if {$myReadyState ne "Uninitialized"} {
        error "Cannot call XMLHttpRequest.open() in state $myReadyState"
      }
      set myUri [$myHv3 resolve_uri $uri]
      set myReadyState Open
      return ""
    }

    js_scall send {THIS args} {
      if {$myReadyState ne "Open"} {
        error "Cannot call XMLHttpRequest.open() in state $myReadyState"
      }

      set myRequestHandle [::hv3::download %AUTO%] 
      $myRequestHandle configure -uri $myUri
      $myRequestHandle configure -finscript [mymethod RequestFinished]
      $myHv3 makerequest $myRequestHandle

      set myReadyState Sent
      return ""
    }

    variable isFinalized 0
    method Finalize {} {
      set isFinalized 1
      if {$myReadyState ne "Sent" && $myReadyState ne "Receiving"} {
        $self destroy
      }
    }
    method RequestFinished {data} {
      if {$isFinalized} {$self destroy}
      $myRequestHandle destroy
    }

    js_finish {
    }
  }
}
#-------------------------------------------------------------------------

