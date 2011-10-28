namespace eval hv3 { set {version($Id$)} 1 }

#-------------------------------------------------------------------------
# DOM Level 2 Style.
#
# This file contains the Hv3 implementation of the DOM Level 2 Style
# specification.
#
#     ElementCSSInlineStyle        (mixed into Element)
#     CSSStyleDeclaration          (mixed into ElementCSSInlineStyle)
#     CSS2Properties               (mixed into CSSStyleDeclaration)
#
# http://www.w3.org/TR/2000/REC-DOM-Level-2-Style-20001113/css.html
#
#

# Set up an array of known "simple" properties. This is used during
# DOM compilation and at runtime.
#
set ::hv3::DOM::CSS2Properties_simple(width) width
set ::hv3::DOM::CSS2Properties_simple(height) height
set ::hv3::DOM::CSS2Properties_simple(display) display
set ::hv3::DOM::CSS2Properties_simple(position) position
set ::hv3::DOM::CSS2Properties_simple(top) top
set ::hv3::DOM::CSS2Properties_simple(left) left
set ::hv3::DOM::CSS2Properties_simple(bottom) bottom
set ::hv3::DOM::CSS2Properties_simple(right) right
set ::hv3::DOM::CSS2Properties_simple(z-index) zIndex
set ::hv3::DOM::CSS2Properties_simple(cursor) cursor
set ::hv3::DOM::CSS2Properties_simple(float) cssFloat 
set ::hv3::DOM::CSS2Properties_simple(font-size) fontSize 
set ::hv3::DOM::CSS2Properties_simple(clear) clear
set ::hv3::DOM::CSS2Properties_simple(border-top-width)    borderTopWidth
set ::hv3::DOM::CSS2Properties_simple(border-right-width)  borderRightWidth
set ::hv3::DOM::CSS2Properties_simple(border-left-width)   borderLeftWidth
set ::hv3::DOM::CSS2Properties_simple(border-bottom-width) borderBottomWidth
set ::hv3::DOM::CSS2Properties_simple(margin-top)          marginTop
set ::hv3::DOM::CSS2Properties_simple(margin-right)        marginRight
set ::hv3::DOM::CSS2Properties_simple(margin-left)         marginLeft
set ::hv3::DOM::CSS2Properties_simple(margin-bottom)       marginBottom
set ::hv3::DOM::CSS2Properties_simple(visibility)          visibility

set ::hv3::DOM::CSS2Properties_simple(background-color) backgroundColor
set ::hv3::DOM::CSS2Properties_simple(background-image) backgroundImage


set ::hv3::dom::code::ELEMENTCSSINLINESTYLE {
  -- A reference to the [Ref CSSStyleDeclaration] object used to access 
  -- the HTML \"style\" attribute of this document element.
  --
  dom_get style {
    list object [list ::hv3::DOM::CSSStyleDeclaration $myDom $myNode]
  }
}

set ::hv3::dom::code::CSS2PROPERTIES {

  dom_parameter myNode

  foreach {k v} [array get ::hv3::DOM::CSS2Properties_simple] {
    if {$v eq ""} { set v $k }
    dom_get $v "
      CSSStyleDeclaration.getStyleProperty \$myNode $k
    "
    dom_put -string $v value "
      CSSStyleDeclaration.setStyleProperty \$myNode $k \$value
    "
  }
  unset -nocomplain k
  unset -nocomplain v

  dom_put -string border value {
    set style [$myNode attribute -default {} style]
    if {$style ne ""} {append style ";"}
    append style "border: $value"
    $myNode attribute style $style
  }

  dom_put -string background value {
    array set current [$myNode prop -inline]
    unset -nocomplain current(background-color)
    unset -nocomplain current(background-image)
    unset -nocomplain current(background-repeat)
    unset -nocomplain current(background-attachment)
    unset -nocomplain current(background-position)
    unset -nocomplain current(background-position-y)
    unset -nocomplain current(background-position-x)

    set style "background:$value;"
    foreach prop [array names current] {
      append style "$prop:$current($prop);"
    }
    $myNode attribute style $style
  }
}

# In a complete implementation of the DOM Level 2 style for an HTML 
# browser, the CSSStyleDeclaration interface is used for two purposes:
#
#     * As the ElementCSSInlineStyle.style property object. This 
#       represents the contents of an HTML "style" attribute.
#
#     * As part of the DOM representation of a parsed stylesheet 
#       document. Hv3 does not implement this function.
#
::hv3::dom2::stateless CSSStyleDeclaration {
  %CSS2PROPERTIES%

  # cssText attribute - access the text of the style declaration. 
  # TODO: Setting this to a value that does not parse is supposed to
  # throw a SYNTAX_ERROR exception.
  #
  dom_get cssText { list string [$myNode attribute -default "" style] }
  dom_put -string cssText val { 
    $myNode attribute style $val
  }

  dom_call_todo getPropertyValue
  dom_call_todo getPropertyCSSValue
  dom_call_todo removeProperty
  dom_call_todo getPropertyPriority

  dom_call -string setProperty {THIS propertyName value priority} {
    if {[info exists ::hv3::DOM::CSS2Properties_simple($propertyName)]} {
      CSSStyleDeclaration_setStyleProperty $myNode $propertyName $value
      return
    }
    error "DOMException SYNTAX_ERROR {unknown property $propertyName}"
  }
  
  # Interface to iterate through property names:
  #
  #     readonly unsigned long length;
  #     DOMString              item(in unsigned long index);
  #
  dom_get length {
    list number [expr {[llength [$myNode prop -inline]]/2}]
  }
  dom_call -string item {THIS index} {
    set idx [expr {2*int([lindex $index 1])}]
    list string [lindex [$myNode prop -inline] $idx]
  }

  # Read-only parentRule property. Always null in hv3.
  #
  dom_get parentRule { list null }
}

namespace eval ::hv3::DOM {
  proc CSSStyleDeclaration.getStyleProperty {node css_property} {
    set val [$node property -inline $css_property]
    list string $val
  }

  proc CSSStyleDeclaration.setStyleProperty {node css_property value} {
    array set current [$node prop -inline]

    if {$value ne ""} {
      set current($css_property) $value
    } else {
      unset -nocomplain current($css_property)
    }

    set style ""
    foreach prop [array names current] {
      append style "$prop:$current($prop);"
    }

    $node attribute style $style
  }
}
