package require snit

namespace eval ::hv3::dom {
  # This array is a map between the DOM name of a CSS property
  # and the CSS name.
  array set CSS_PROPERTY_MAP [list \
    display display                \
    height  height                 \
    width   width                  \
  ]
}

#-----------------------------------------------------------------------
# ::hv3::dom::InlineStyle 
#
#     This Snit type implements a javascript element.style object, used to
#     provide access to the "style" attribute of an HTML element.
# 
set InlineStyleDefn {
  js_init {dom node} { 
    set myNode $node
  }
}
foreach p [array names ::hv3::dom::CSS_PROPERTY_MAP] {
  append InlineStyleDefn [subst -nocommands {
    js_get $p   { [set self] GetStyleProp $p }
    js_put $p v { [set self] PutStyleProp $p [set v] }
  }]
}
append InlineStyleDefn {

  variable myNode

  method GetStyleProp {prop} {
      list string [$myNode prop -inline $hv3::dom::CSS_PROPERTY_MAP($prop)]
  }

  method PutStyleProp {property js_value} {
    set value [[$self see] tostring $js_value]

    array set current [$myNode prop -inline]

    if {$value ne ""} {
      set current($::hv3::dom::CSS_PROPERTY_MAP($property)) $value
    } else {
      unset -nocomplain current($::hv3::dom::CSS_PROPERTY_MAP($property))
    }

    set style ""
    foreach prop [array names current] {
      append style "$prop : $current($prop); "
    }

    $myNode attribute style $style
  }

  js_finish {}
}
::snit::type ::hv3::dom::InlineStyle $InlineStyleDefn
unset InlineStyleDefn
#
# End of DOM class InlineStyle.
#-----------------------------------------------------------------------


namespace eval ::hv3::dom {
  
  # List of DOM Level 0 events.
  #
  set DOM0Events_EventList [list                                         \
    onclick ondblclick onmousedown onmouseup onmouseover                 \
    onmousemove onmouseout onkeypress onkeydown onkeyup onfocus onblur   \
    onsubmit onreset onselect onchange                                   \
  ]

  # Variable DOM0Events_ElementCode contains code to insert into the
  # ::hv3::dom::HTMLElement type for DOM Level 0 event support.
  #
  set DOM0Events_ElementCode {
    variable myEventFunctionsCompiled 0

    # This method loops through all the DOM Level 0 event attributes of
    # html widget node $myNode (onclick, onfocus etc.). For each defined 
    # attribute, compile the value of the attribute into the body of
    # a javascript function object. Set the event property of the 
    # parent object to the compiled function object.
    #
    method CompileEventFunctions {} {
      if {$myEventFunctionsCompiled} return
      set see [$self see]
      foreach event $::hv3::dom::DOM0Events_EventList {
        set body [$myNode attr -default "" $event]
        if {$body ne ""} {
          set ref [$see function $body]
          $myJavascriptParent Put $event [list object $ref]
          eval $see $ref Finalize
        }
      }
      set myEventFunctionsCompiled 1
    }
  }

  foreach event $::hv3::dom::DOM0Events_EventList {
    append DOM0Events_ElementCode [subst -nocommands {
      js_get $event {
        [set self] CompileEventFunctions
        [set myJavascriptParent] Get $event
      }
      js_put $event value {
        [set self] CompileEventFunctions
        [set myJavascriptParent] Put $event [set value]
      }
    }]
  }
}


