namespace eval hv3 { set {version($Id$)} 1 }

# This file contains the implementation of the two DOM specific
# container objects:
#
#     NodeList               (DOM Core 1)
#     HTMLCollection         (DOM HTML 1)
#
# Both are implemented as stateless js objects. There are two 
# implementations of NodeList - NodeListC (commands) and NodeListS
# (selectors). Also HTMLCollectionC and HTMLCollectionS
#


#-------------------------------------------------------------------------
# DOM class: (HTMLCollection)
#
# Supports the following javascript interface:
#
#     length
#     item(index)
#     namedItem(name)
#
# Also, a request for any property with a numeric name is mapped to a call
# to the item() method. A request for any property with a non-numeric name
# maps to a call to namedItem(). Hence, javascript references like:
#
#     collection[1]
#     collection["name"]
#     collection.name
#
# work as expected.
#
::hv3::dom2::stateless HTMLCollectionC {

  # There are several variations on the role this object may play in
  # DOM level 1 Html:
  #
  #     HTMLDocument.getElementsByName()
  #
  #     HTMLDocument.images
  #     HTMLDocument.applets
  #     HTMLDocument.links
  #     HTMLDocument.forms
  #     HTMLDocument.anchors
  #
  #     HTMLFormElement.elements
  #     HTMLMapElement.areas
  #
  #     HTMLTableElement.rows
  #     HTMLTableElement.tBodies
  #     HTMLTableSectionElement.rows
  #     HTMLTableRowElement.cells
  #
  dom_parameter nodelistcmd

  # HTMLCollection.length
  #
  dom_get length {
    list number [llength [eval $nodelistcmd]]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    HTMLCollectionC_item $myDom $nodelistcmd $index
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    HTMLCollectionC_namedItem $myDom $nodelistcmd $name
  }

  # Handle an attempt to retrieve an unknown property.
  #
  dom_get * {
    HTMLCollectionC_getUnknownProperty $myDom $nodelistcmd $property
  }
}

namespace eval ::hv3::DOM {
  proc HTMLCollectionC_getNodeHandlesByName {supersetcmd name} {
    set nodelist [eval $supersetcmd]
    set ret [list]
    foreach node $nodelist {
      if {[$node attr -default "" id] eq $name || 
        [$node attr -default "" name] eq $name} {
        lappend ret $node
      }
    }
    return $ret
  }

  proc HTMLCollectionC_getUnknownProperty {dom supersetcmd property} {

    # If $property looks like a number, treat it as an index into the list
    # of widget nodes. 
    #
    # Otherwise look for nodes with the "name" or "id" attribute set 
    # to the queried attribute name. If a single node is found, return
    # it directly. If more than one have matching names or ids, a NodeList
    # containing the matches is returned.
    #
    if {[string is double $property]} {
      set res [HTMLCollectionC_item $dom $supersetcmd $property]
    } else {
      set res [
        HTMLCollectionC_getNodeHandlesByName $supersetcmd $property
      ]
      set nRet [llength $res]
      if {$nRet==0} {
        set res ""
      } elseif {$nRet==1} {
        set res [list object [::hv3::dom::wrapWidgetNode $dom [lindex $res 0]]]
      } else {
        set getnodes [namespace code [list \
          HTMLCollectionC_getNodeHandlesByName $supersetcmd $property
        ]]
        set obj [list ::hv3::DOM::NodeListC $dom $getnodes]
        set res [list object $obj]
      }
    }

    return $res
  }
  
  proc HTMLCollectionC_item {dom nodelistcmd index} {
    set idx [format %.0f $index]
    set ret null
    set node [lindex [eval $nodelistcmd] $idx]
    if {$node ne ""} {
      set ret [list object [::hv3::dom::wrapWidgetNode $dom $node]]
    }
    set ret
  }
  
  proc HTMLCollectionC_namedItem {dom nodelistcmd name} {
    set nodelist [eval $nodelistcmd]
    
    foreach node $nodelist {
      if {[$node attr -default "" id] eq $name} {
        set domobj [::hv3::dom::wrapWidgetNode $dom $node]
        return [list object $domobj]
      }
    }
    
    foreach node $nodelist {
      if {[$node attr -default "" name] eq $name} {
        set domobj [::hv3::dom::wrapWidgetNode $dom $node]
        return [list object $domobj]
      }
    }
    
    return null
  }
}

::hv3::dom2::stateless HTMLCollectionS {

  # This is set to a search command like ".html search p" (to find
  # all <P> elements in the system).
  #
  dom_parameter mySearchCmd

  # HTMLCollection.length
  #
  dom_get length {
    list number [eval $mySearchCmd -length]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    set node [eval $mySearchCmd -index [expr {int($index)}]]
    if {$node ne ""} { 
      list object [::hv3::dom::wrapWidgetNode $myDom $node] 
    } else {
      list null
    }
  }

  # HTMLCollection.namedItem()
  #
  # TODO: This may fail if the $name argument contains spaces or 
  # something. This is not dangerous - there is no scope for injection
  # attacks.
  dom_call -string namedItem {THIS name} {
    set html     [lindex $mySearchCmd 0]
    set selector [lindex $mySearchCmd 2]

    # First search for a match on the id attribute.
    set sel [format {%s[id="%s"]} $selector $name]
    set node [$html search $sel -index 0]

    # Next try to find a node with a matching name attribute.
    if {$node eq ""} {
      set sel [format {%s[name="%s"]} $selector $name]
      set node [$html search $sel -index 0]
    }

    # Return a Node object if successful, otherwise null.
    if {$node ne ""} { 
      list object [::hv3::dom::wrapWidgetNode $myDom $node] 
    } else {
      list null
    }
  }

  # Handle an attempt to retrieve an unknown property.
  #
  dom_get * {

    # If $property looks like a number, treat it as an index into the list
    # of widget nodes. 
    #
    # Otherwise look for nodes with the "name" or "id" attribute set 
    # to the queried attribute name. If a single node is found, return
    # it directly. If more than one have matching names or ids, a NodeList
    # containing the matches is returned.
    #
    if {[string is double $property]} {
      set node [eval $mySearchCmd -index [expr {int($property)}]]
      if {$node ne ""} { 
        list object [::hv3::dom::wrapWidgetNode $myDom $node] 
      }
    } else {
      set name $property
      set html [lindex $mySearchCmd 0]
      set s    [lindex $mySearchCmd 2]
      set sel [format {%s[name="%s"],%s[id="%s"]} $s $name $s $name]
      set nNode [$html search $sel -length]
      if {$nNode > 0} {
        if {$nNode == 1} {
          list object [::hv3::dom::wrapWidgetNode $myDom [$html search $sel]]
        } else {
          list object [list \
            ::hv3::DOM::NodeListC $myDom [list $html search $sel]
          ]
        }
      }
    }
  }
}


#-------------------------------------------------------------------------
# DOM Type NodeListC
#
#     This object is used to store lists of child-nodes (property
#     Node.childNodes). There are three variations, depending on the
#     type of the Node that this object represents the children of:
#
#         * Document node (one child - the root (<HTML>) element)
#         * Element node (children based on html widget node-handle)
#         * Text or Attribute node (no children)
#
::hv3::dom2::stateless NodeListC {

  # The following option is set to a command to return the html-widget nodes
  # that comprise the contents of this list. i.e. for the value of
  # the "Document.childNodes" property, this option will be set to
  # [$hv3 node], where $hv3 is the name of an ::hv3::hv3 widget (that
  # delagates the [node] method to the html widget).
  #
  dom_parameter myNodelistcmd

  dom_call -string item {THIS index} {
    if {![string is double $index]} { return null }
    set idx [expr {int($index)}]
    NodeListC_item $myDom $myNodelistcmd $idx
  }

  dom_get length {
    list number [llength [eval $myNodelistcmd]]
  }

  # Unknown property request. If the property name looks like a number,
  # invoke the NodeList.item() method. Otherwise, return undefined ("").
  #
  dom_get * {
    if {[string is integer $property]} {
      NodeListC_item $myDom $myNodelistcmd $property
    }
  }
}

namespace eval ::hv3::DOM {
  proc NodeListC_item {dom nodelistcmd idx} {
    set children [eval $nodelistcmd]
    if {$idx < 0 || $idx >= [llength $children]} { return null }
    list object [::hv3::dom::wrapWidgetNode $dom [lindex $children $idx]]
  }
}

#-------------------------------------------------------------------------
# DOM Type NodeListS
#
#     This object is used to implement the NodeList collections
#     returned by getElementsByTagName() and kin. It is a wrapper
#     around [$html search].
#
::hv3::dom2::stateless NodeListS {

  # Name of the tkhtml widget to evaluate [$myHtml search] with.
  #
  # Command like ".html search $selector -root $rootnode"
  #
  dom_parameter mySearchCmd

  dom_call -string item {THIS index} {
    if {![string is double $index]} { return null }
    NodeListS_item $myDom $mySearchCmd [expr {int($index)}]
  }

  dom_get length {
    list number [eval $mySearchCmd -length] 
  }

  # Unknown property request. If the property name looks like a number,
  # invoke the NodeList.item() method. Otherwise, return undefined ("").
  #
  dom_get * {
    if {[string is integer $property]} {
      NodeListS_item $myDom $mySearchCmd $property
    }
  }
}

namespace eval ::hv3::DOM {
  proc NodeListS_item {dom searchcmd idx} {
    set N [eval $searchcmd -index $idx]
    if {$N eq ""} {
      list null
    } else {
      list object [::hv3::dom::wrapWidgetNode $dom $N]
    }
  }
}

::hv3::dom2::stateless FramesList {
  -- This class implements a container used for the 
  -- <code>window.frames</code> property in Hv3.
  XX

  dom_parameter myFrame

  -- Return the number of items in this container.
  dom_get length {
    list number [llength [$myFrame child_frames]]
  }

  dom_get * {
    set frames [$myFrame child_frames]
    if {[string is integer $property]} {
      set f [lindex $frames [expr {int($property)}]]
      if {$f eq ""} return ""
      return [list bridge [[$f hv3 dom] see]]
    }

    foreach f $frames {
      if {$property eq [$f cget -name]} {
        return [list bridge [[$f hv3 dom] see]]
      }
    }

    return ""
  }
}

::hv3::dom2::stateless HTMLOptionsCollection {
  dom_parameter mySelectNode

  # HTMLCollection.length
  #
  dom_get length {
    list number [llength [HTMLSelectElement_getOptions $mySelectNode]]
  }

  # HTMLCollection.item()
  #
  dom_call -string item {THIS index} {
    set cmd [list HTMLSelectElement_getOptions $mySelectNode]
    HTMLCollectionC_item $myDom $cmd $index
  }

  # HTMLCollection.namedItem()
  #
  dom_call -string namedItem {THIS name} {
    set cmd [list HTMLSelectElement_getOptions $mySelectNode]
    HTMLCollectionC_namedItem $myDom $cmd $name
  }

  # Handle an attempt to retrieve an unknown property.
  #
  dom_get * {
    set cmd [list HTMLSelectElement_getOptions $mySelectNode]
    HTMLCollectionC_getUnknownProperty $myDom $cmd $property
  }

  # The "selectedIndex" property of this collection is an alias for
  # the "selectedIndex" property of the <SELECT> node.
  dom_get selectedIndex {
    list number [[$mySelectNode replace] dom_selectionIndex]
  }
  dom_put -string selectedIndex value {
    [$mySelectNode replace] dom_setSelectionIndex $value
  }
}


