namespace eval hv3 { set {version($Id$)} 1 }

#--------------------------------------------------------------------------
# DOM Level 1 Core
#
# This file contains the Hv3 implementation of the DOM Level 1 Core. Where
# possible, Hv3 tries hard to be compatible with W3C and Gecko. Gecko
# is pretty much a clean super-set of W3C for this module.
#
#     DOMImplementation
#     Document
#     Node
#     NodeList
#     CharacterData
#     Element
#     Text
#
# Not implemented:
#     Attr
#     Comment
#     NamedNodeMap
#     DocumentFragment
# 
#-------------------------------------------------------------------------

#--------------------------------------------------------------------------
# The Node Prototype object
#
#     Currently, Hv3 implements the following types:
#
#         * Element nodes (1)        -> type HTMLElement
#         * Attribute nodes (2)      -> type Attr
#         * Text nodes (3)           -> type Text
#         * Document nodes (9)       -> type HTMLDocument
#
#     Probably also want to implement Document-fragment nodes at some
#     stage. And Comment too, but I bet we can get away without it :)
#    
namespace eval ::hv3::dom::code {
  set NODE_PROTOTYPE {
    # Required by XML and HTML applications:
    dom_get ELEMENT_NODE                {list number 1}
    dom_get ATTRIBUTE_NODE              {list number 2}
    dom_get TEXT_NODE                   {list number 3}
    dom_get COMMENT_NODE                {list number 8}
    dom_get DOCUMENT_NODE               {list number 9}
    dom_get DOCUMENT_FRAGMENT_NODE      {list number 11}
  
    # Required by XML applications only:
    dom_get CDATA_SECTION_NODE          {list number 4}
    dom_get ENTITY_REFERENCE_NODE       {list number 5}
    dom_get ENTITY_NODE                 {list number 6}
    dom_get PROCESSING_INSTRUCTION_NODE {list number 7}
    dom_get DOCUMENT_TYPE_NODE          {list number 10}
    dom_get NOTATION_NODE               {list number 12}
  }
  ::hv3::dom2::stateless NodePrototype %NODE_PROTOTYPE%
  set NODE_PROTOTYPE [list Inherit NodePrototype $NODE_PROTOTYPE]
}

#--------------------------------------------------------------------------
# This block contains default implementations of the methods and
# attributes in the Node interface (DOM Level 1 Core).
#
set ::hv3::dom::code::NODE {

  # These must both be overridden.
  dom_todo nodeName
  dom_todo nodeType

  # Node.nodeValue is null of all nodes except ATTRIBUTE and TEXT.
  # Also, technically CDATA_SECTION, COMMENT and PROCESSING_INSTRUCTION,
  # but these are not implemented in Hv3.
  #
  dom_get nodeValue       {list null}

  # Node.attributes is null for all nodes types except ELEMENT
  #
  dom_get attributes      {list null}

  # Default implementation is good enough for DOCUMENT nodes.
  #
  dom_get previousSibling {list null}
  dom_get nextSibling     {list null}

  -- Always null for this object.
  dom_get parentNode {list cache null}

  dom_get firstChild {list null}
  dom_get lastChild  {list null}

  # Default implementation of childNodes() returns an empty NodeList 
  # containing no Nodes.
  #
  dom_get childNodes {
    list object [list ::hv3::DOM::NodeListC $myDom list]
  }
  dom_call hasChildNodes {THIS} {list boolean false}

  dom_todo ownerDocument

  dom_call insertBefore {THIS newChild refChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call replaceChild {THIS newChild oldChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call removeChild {THIS oldChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }
  dom_call appendChild {THIS newChild} {
    error "DOMException HIERACHY_REQUEST_ERR"
  }

  # Method to clone the node. Spec indicates that it is optional to
  # support this on for DOCUMENT nodes, hence the exception.
  #
  dom_call -string cloneNode {THIS isDeep} {
    error "DOMException NOT_SUPPORTED_ERR"
  }
}

::hv3::dom2::stateless Implementation {

  dom_call -string hasFeature {THIS feature version} {
    set feature [string tolower $feature]
    set version [string tolower $version]

    set FeatureList { 
      html        1.0 
      css         2.0 
      mouseevents 2.0 
    }
    array set f $FeatureList

    if {[info exists f($feature)]} {
      list boolean true
    } else {
      list boolean false
    }
  }
}

set ::hv3::dom::code::DOCUMENT {

  # The ::hv3::hv3 widget containing the document this DOM object
  # represents.
  #
  dom_parameter myHv3

  -- Always Node.DOCUMENT_NODE (integer value 9).
  dom_get nodeType {list number 9}

  -- Always the literal string \"#document\".
  dom_get nodeName {list string #document}

  -- The Document node always has exactly one child: the &lt\;HTML&gt\; element
  -- of the document tree. This property always contains a [Ref NodeList] 
  -- collection containing that element.
  dom_get childNodes {
    set cmd [list $myHv3 node]
    list object [list ::hv3::DOM::NodeListC $myDom $cmd]
  }

  -- Return the root element of the document tree (an object of class
  -- [Ref HTMLHtmlElement]).
  dom_get firstChild {
    list object [::hv3::dom::wrapWidgetNode $myDom [$myHv3 node]]
  }

  -- Return the root element of the document tree (an object of class
  -- [Ref HTMLHtmlElement]).
  dom_get lastChild  {
    list object [::hv3::dom::wrapWidgetNode $myDom [$myHv3 node]]
  }

  -- The document node always has exactly one child node. So this property
  -- is always set to true.
  dom_call hasChildNodes {THIS} {list boolean true}

  dom_call_todo insertBefore
  dom_call_todo replaceChild
  dom_call_todo removeChild 
  dom_call_todo appendChild  

  -- For a Document node, the ownerDocument is null.
  dom_get ownerDocument {list null}

  # End of Node interface overrides.
  #---------------------------------

  dom_todo doctype

  -- Reference to the [Ref Implementation] object.
  dom_get implementation {
    list object [list ::hv3::DOM::Implementation $myDom]
  }

  -- This property is always set to the &lt\;HTML&gt\; element (an object of
  -- class [Ref HTMLHtmlElement]).
  dom_get documentElement {
    list object [::hv3::dom::wrapWidgetNode $myDom [$myHv3 node]]
  }

  # Constructors:
  #
  #     createElement()
  #     createTextNode()
  #     createDocumentFragment()     (todo)
  #     createComment()              (todo)
  #     createAttribute()            (todo)
  #     createEntityReference()      (todo)
  #
  dom_call -string createElement {THIS tagname} {
    set node [$myHv3 html fragment "<$tagname>"]
    if {$node eq ""} {error "DOMException NOT_SUPPORTED_ERR"}
    list object [::hv3::dom::wrapWidgetNode $myDom $node]
  }
  dom_call -string createTextNode {THIS data} {
    if {$data eq ""} {
      # Special case - The [fragment] API usually parses an empty string
      # to an empty fragment. So create a text node with text "X", then 
      # set the text to an empty string.
      set node [$myHv3 html fragment X]
      $node text set ""
    } else {
      set escaped [string map {< &lt; > &gt;} $data]
      set node [$myHv3 html fragment $escaped]
    }
    list object [::hv3::dom::wrapWidgetNode $myDom $node]
  }
  dom_call_todo createDocumentFragment
  dom_call_todo createComment
  dom_call_todo createAttribute
  dom_call_todo createEntityReference

  #-------------------------------------------------------------------------
  # The Document.getElementsByTagName() method (DOM level 1).
  #
  dom_call -string getElementsByTagName {THIS tag} { 
    # TODO: Should check that $tag is a valid tag name. Otherwise, one day
    # someone is going to pass ".id" and wonder why all the elements with
    # the "class" attribute set to "id" are returned.
    #
    set html $myHv3
    catch {set html [$myHv3 html]}
    set nl [list ::hv3::DOM::NodeListS $myDom [list $html search $tag]]
    list transient $nl
  }
}

#-------------------------------------------------------------------------
# This is not a DOM type. It contains code that is common to the
# DOM types "HTMLElement" and "Text". These are both wrappers around
# html widget node-handles, hence the commonality. The following 
# parts of the Node interface are implemented here:
#
#     Node.parentNode
#     Node.previousSibling
#     Node.nextSibling
#     Node.ownerDocument
#
set ::hv3::dom::code::WIDGET_NODE {
  dom_parameter myNode

  -- Retrieve the parent element. If this node is the root of the 
  -- document tree, return the associated window.document object.
  -- Return null if there is no parent element (can happen if this
  -- node has been removed from the document tree.
  dom_get parentNode {
    # Note that there is another version of this function in the
    # implementation of HTMLHtmlElement. Since the root of the document
    # tree is always an <HTML> element, we can deal with possibly having
    # to return the HTMLDocument object there.
    set parent [$myNode parent]
    if {$parent ne ""} {
      list node [::hv3::dom::wrapWidgetNode $myDom $parent]
    } else {
      list null
    }
  }

  # Retrieve the left and right sibling nodes.
  #
  dom_get previousSibling {WidgetNode_Sibling $myDom $myNode -1}
  dom_get nextSibling     {WidgetNode_Sibling $myDom $myNode +1}

  dom_call -string cloneNode {THIS isDeep} {
 
    # To clone a node, first obtain the serialized HTML representation.
    # Then parse it using the [widget fragment] API. The result is the
    # cloned DOM node.
    #
    set zHtml [WidgetNode_ToHtml $myNode $isDeep]
    if {$zHtml eq ""} {
      set clone [[$myNode html] fragment .]
      $clone text set ""
    } else {
      set clone [[$myNode html] fragment $zHtml]
    }
    
    list object [::hv3::dom::wrapWidgetNode $myDom $clone]
  }

  dom_get ownerDocument { 
    list object [node_to_document $myNode]
  }
}
namespace eval ::hv3::DOM {

  proc WidgetNode_ToHtml {node isDeep} {
    set tag [$node tag]
    if {$tag eq ""} {
      append ret [$node text -pre]
    } else {
      append ret "<$tag"
      foreach {zKey zVal} [$node attribute] {
        set zEscaped [string map [list "\x22" "\x5C\x22"] $zVal]
        append ret " $zKey=\"$zEscaped\""
      }
      append ret ">"
      if {$isDeep} {
        append ret [WidgetNode_ChildrenToHtml $node]
      }
      append ret "</$tag>"
    }
  }

  proc WidgetNode_ChildrenToHtml {elem} {
    set ret ""
    foreach child [$elem children] {
      append ret [WidgetNode_ToHtml $child 1]
    }
    return $ret
  }


  proc WidgetNode_Sibling {dom node dir} {
    set ret null
    set parent [$node parent]
    if {$parent ne ""} {
      set siblings [$parent children]
      set idx [lsearch $siblings $node]
      incr idx $dir
      if {$idx >= 0 && $idx < [llength $siblings]} {
        set ret [list object [::hv3::dom::wrapWidgetNode $dom [lindex $siblings $idx]]]
      }
    }
    set ret
  }
}

#-------------------------------------------------------------------------
# DOM Type Element (Node -> Element)
#
#     This object is never actually instantiated. HTMLElement (and other,
#     element-specific types) are instantiated instead.
#
set BaseList {ElementCSSInlineStyle}
set ::hv3::dom::code::ELEMENT {

  # Override parts of the Node interface.
  #
  dom_get nodeType {list number 1}           ;#     Node.ELEMENT_NODE -> 1
  dom_get nodeName {list string [string toupper [$myNode tag]]}

  dom_get childNodes {
    list object [list ::hv3::DOM::NodeListC $myDom [list $myNode children]]
  }

  dom_call insertBefore {THIS newChild refChild}  {
    # TODO: Arg checking and correct error messages (excptions).

    set new [GetNodeFromObj [lindex $newChild 1]]

    set ref ""
    if {[lindex $refChild 0] eq "object"} {
      set ref [GetNodeFromObj [lindex $refChild 1]]
    }

    if {$ref ne ""} {
      $myNode insert -before $ref $new
    } else {
      $myNode insert $new
    }

    # Return value is a reference to the object just inserted 
    # as a new child node.
    set newChild
  }

  dom_call appendChild {THIS newChild} {
    # TODO: Arg checking and correct error messages (excptions).

    set new [GetNodeFromObj [lindex $newChild 1]]
    $myNode insert $new

    # Return value is a reference to the object just inserted 
    # as a new child node.
    set newChild
  }

  dom_call removeChild {THIS oldChild} {
    # TODO: Arg checking and correct error messages (excptions).

    set old [GetNodeFromObj [lindex $oldChild 1]]
    $myNode remove $old

    # Return value is a reference to the node just removed. 
    #
    # TODO: At the Tkhtml widget level, the node is now an 
    # orphan. What we should be doing is telling the javascript 
    # interpreter that the object is now eligible for finalization.
    # The finalizer can safely delete the orphaned node object. 
    #
    set oldChild
  }

  dom_call replaceChild {THIS newChild oldChild} {
    # TODO: Arg checking and correct error messages (excptions).

    set new [GetNodeFromObj [lindex $newChild 1]]
    set old [GetNodeFromObj [lindex $oldChild 1]]

    $myNode insert -before $old $new
    $myNode remove $old

    # TODO: Same memory management problem as removeChild().
    set oldChild
  }

  dom_get childNodes {
    set cmd [list $myNode children]
    list object [list ::hv3::DOM::NodeListC $myDom $cmd]
  }
  dom_get lastChild {
    set f [lindex [$myNode children] end]
    if {$f eq ""} {
      list null
    } else {
      list object [::hv3::dom::wrapWidgetNode $myDom $f]
    }
  }
  dom_get firstChild {
    set f [lindex [$myNode children] 0]
    if {$f eq ""} {
      list null
    } else {
      list object [::hv3::dom::wrapWidgetNode $myDom $f]
    }
  }


  # End of Node interface overrides.
  #---------------------------------

  # Element.tagName
  #
  #     DOM Level 1 HTML section 2.5.3 specifically says that the string
  #     returned for the tag-name property be in upper-case. Tkhtml3 should
  #     probably be altered to match this.
  #
  dom_get tagName {
    list string [string toupper [$myNode tag]]
  }

  dom_call -string getAttribute {THIS attr} {
    Element_getAttributeString $myNode $attr ""
  }
  dom_call -string setAttribute {THIS attr v} {
    Element_putAttributeString $myNode $attr $v
  }

  dom_call -string removeAttribute {THIS attr} {
    Element_putAttributeString $myNode $attr ""
  }

  dom_call_todo getAttributeNode
  dom_call_todo setAttributeNode
  dom_call_todo removeAttributeNode

  dom_call -string getElementsByTagName {THIS tagname} {
    set htmlwidget [$myNode html]
    set nl [list ::hv3::DOM::NodeListS $myDom [
      list $htmlwidget search $tagname -root $myNode
    ]]
    list transient $nl
  }

  # normalize()
  #
  #     Coalesce adjacent text nodes in the sub-tree rooted at this
  #     element. In Hv3, for each string of adjacent text nodes, the
  #     contents of the first is modified, and each adjacent text
  #     node removed from the tree.
  #
  dom_call normalize {THIS} {
    Element_normalize $myNode
  }

  # Introduced in Core DOM Level 2:
  #
  dom_call -string hasAttribute {THIS attr} {
    set rc [catch {$myNode attribute $attr}]
    list boolean [expr {$rc ? 0 : 1}]
  }
}

namespace eval ::hv3::DOM {
  proc Element_getAttributeString {node name def} {
    list string [$node attribute -default $def $name]
  }

  proc Element_putAttributeString {node name val} {
    $node attribute $name $val
    return ""
  }

  proc Element_normalize {node} {
    set T ""
    foreach child [$node children] {
      if {[$child tag] eq ""} {
        if {$T eq ""} {
          set T $child
        } else {
          set t [$T text -pre]
          append t [$child text -pre]
          $T text set $t
          $node remove $child
        }
      } else {
        Element_normalize $child
        set T ""
      }
    }
  }

  # Assuming $js_obj is a javascript object that implements WidgetNode,
  # return the corresponding Tkhtml node handle. Return an empty string
  # if $js_obj does not not implement WidgetNode.
  #
  proc GetNodeFromObj {js_obj} {
    # set idx [lsearch -exact [info args [lindex $js_obj 0]] myNode]
    # lindex $js_obj [expr $idx+1]
    lindex $js_obj 2
  }
}

# element_attr --
#
#     This command can be used in the body of DOM objects that mixin class
#     Element.
#
#     element_attr NAME ?OPTIONS?
#
#         -attribute ATTRIBUTE-NAME          (default NAME)
#         -readonly                          (make the attribute readonly)
#
namespace eval ::hv3::dom::compiler {

  proc element_attr {name args} {

    set readonly 0
    set boolean 0
    set attribute $name

    # Process the arguments to [element_attr]:
    for {set ii 0} {$ii < [llength $args]} {incr ii} {
      set s [lindex $args $ii]
      switch -- $s {
        -attribute {
          incr ii
          set attribute [lindex $args $ii]
        }
        -readonly {
          set readonly 1
        }
        default {
          error "Bad option to element_attr: $s"
        }
      }
    }

    # The Get code.
    dom_get $name [subst -novariables {
      Element_getAttributeString $myNode [set attribute] ""
    }]

    # Create the Put method (unless the -readonly switch was passed).
    if {!$readonly} {
      dom_put -string $name val [subst -novariables {
        Element_putAttributeString $myNode [set attribute] $val
      }]
    }
  }
}

foreach ns [list ::hv3::dom2::compiler2 ::hv3::dom2::doccompiler] {
  namespace eval $ns {
  
    proc element_attr {name args} {
  
      set readonly 0
      set attribute $name
  
      # Process the arguments to [element_attr]:
      for {set ii 0} {$ii < [llength $args]} {incr ii} {
        set s [lindex $args $ii]
        switch -- $s {
          -attribute {
            incr ii
            set attribute [lindex $args $ii]
          }
          -readonly {
            set readonly 1
          }
          default {
            error "Bad option to element_attr: $s"
          }
        }
      }
  
      # The Get code.
      dom_get $name [subst -novariables {
        Element_getAttributeString $myNode [set attribute] ""
      }]
  
      # Create the Put method (unless the -readonly switch was passed).
      if {!$readonly} {
        dom_put -string $name val [subst -novariables {
          Element_putAttributeString $myNode [set attribute] $val
        }]
      }
    }
  }
}

namespace eval ::hv3::DOM {
  proc CharacterData_replaceData {node offset count arg} {
    set nOffset [expr {int($offset)}]
    set nCount  [expr {int($count)}]
    set idx     [expr {$nOffset + $nCount}]
    set text [$node text -pre]
    set    out [string range $text 0 [expr {$nOffset-1}]]
    append out $arg
    append out [string range $text $idx end]
    $node text set $out
  }
}

#-------------------------------------------------------------------------
# DOM Type Text (Node -> CharacterData -> Text)
#
::hv3::dom2::stateless Text {

  %NODE%
  %WIDGET_NODE%
  %NODE_PROTOTYPE%

  # The "data" property is a get/set on the contents of this text node.
  #
  dom_get data { list string [$myNode text -pre] }
  dom_put -string data newText { $myNode text set $newText }

  # Read-only "length" property.
  #
  dom_get length { list number [string length [$myNode text -pre]] }

  # The 5 functions specified by DOM:
  #
  #   substringData(offset, count)
  #   appendData(arg)
  #   insertData(offset, arg)
  #   deleteData(offset, count)
  #   replaceData(offset, count arg)
  #
  # The appendData(), insertData() and deleteData() are all implemented
  # as special cases of replaceData().
  #
  dom_call -string substringData {THIS offset count} {
    set nOffset [expr {int($offset)}]
    set nCount  [expr {int($count)}]
    set idx2 [expr {$nOffset + $nCount - 1}]
    list string [string range [$myNode text -pre] $nOffset $idx2]
  }
  dom_call -string appendData {THIS arg} {
    set nChar [string length [$myNode text -pre]]
    CharacterData_replaceData $myNode $nChar 0 $arg
  }
  dom_call -string insertData {THIS offset arg} {
    CharacterData_replaceData $myNode $offset 0 $arg
  }
  dom_call -string deleteData {THIS offset count} {
    CharacterData_replaceData $myNode $offset $count ""
  }
  dom_call -string replaceData {THIS offset count arg} {
    CharacterData_replaceData $myNode $offset $count $arg
  }

  # Override parts of the Node interface.
  #
  dom_get nodeType  {list number 3}           ;#     Node.TEXT_NODE -> 3
  dom_get nodeName  {list string #text}

  # nodeValue is read/write for a Text node.
  #
  dom_get nodeValue {list string [$myNode text -pre] }
  dom_put -string nodeValue newText { $myNode text set $newText }

  # End of Node interface overrides.
  #---------------------------------

  # splitText(offset)
  #
  dom_call -string splitText {THIS offset} {
    set nOffset [expr {int($offset)}]
    set t [$myNode text -pre]

    if {$nOffset > [string length $t]} {
      error "DOMException INDEX_SIZE_ERR"
    }

    set z1 [string range $t 0 [expr {$nOffset-1}]]
    set z2 [string range $t $nOffset end]

    set html   [$myNode html]
    set parent [$myNode parent]

    $myNode text set $z1
    set z2 [string map {< &lt; > &gt;} $z2]

    if {$z2 eq ""} {
      set new [$html fragment .]
      $new text set ""
    } else {
      set new [$html fragment $z2]
    }
    $parent insert -after $myNode $new
    list object [::hv3::dom::wrapWidgetNode $myDom $new]
  }
}


