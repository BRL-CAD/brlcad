namespace eval hv3 { set {version($Id$)} 1 }

#--------------------------------------------------------------------------
# This file implements infrastructure used to create the [proc] definitions
# that implement the DOM objects. It adds the following command that
# may be used to declare DOM object classes, in the same way as 
# [::snit::type] is used to declare new Snit types:
#
#     ::hv3::dom2::stateless TYPE-NAME BASE-TYPE-LIST BODY
#

namespace eval ::hv3::dom::code {}
namespace eval ::hv3::DOM::docs {}

# This proc is used in place of a full [dom_object] for callable
# functions. i.e. the object returned by a Get on "document.write".
#
# Arguments:
#
#     $pSee     - Name of SEE interpreter command (returned by [::see::interp])
#     $isString - True to convert arguments to strings
#     $zScript  - Tcl script to invoke if this is a "Call"
#     $op       - SEE/Tcl op (e.g. "Call", "Get", "Put" etc.)
#     $args     - Args to SEE/Tcl op.
#
proc ::hv3::dom::TclCallableProc {pSee isString zScript op args} {
  switch -- $op {
    Get { return "" }

    Put { return "native" }

    Call {
      set THIS [lindex $args 0]
      if {$isString} {
        set A [list]
        foreach js_value [lrange $args 1 end] { 
          lappend A [$pSee tostring $js_value] 
        }
      } else {
        set A [lrange $args 1 end]
      }
      return [eval $zScript [list $THIS] $A]
    }

    Construct {
      error "Cannot call this as a constructor"
    }

    Finalize {
      # A no-op. There is no state data.
      return
    }

    Events { return }
    Scope  { return }
  }

  error "Unknown method: $op"
}

# This proc is used in place of a full [dom_object] for callable
# functions. i.e. the object returned by a Get on the "window.Image"
# property requested by the javascript "new Image()".
#
proc ::hv3::dom::TclConstructable {pSee zScript op args} {
  switch -- $op {
    Get { return "" }

    Put { return "native" }

    Construct {
      return [eval $zScript $args]
    }

    Call {
      error "Cannot call this object"
    }

    Finalize {
      # A no-op. There is no state data.
      return
    }

    Events { return }
  }

  error "Unknown method: $op"
}


#--------------------------------------------------------------------------
# Stateless DOM objects are defined using the following command:
#
#     ::hv3::dom2::stateless TYPE-NAME BASE-TYPE-LIST BODY
#
# Also:
#
#     ::hv3::dom2::compile TYPE-NAME
#     ::hv3::dom2::cleanup
#
# This generates code to implement the object using [proc]. The following are
# supported within the BODY block:
#
#     dom_parameter     NAME
#
#     dom_get           PROPERTY CODE
#     dom_put           ?-strings? PROPERTY ARG-NAME CODE
#
#     dom_call          ?-strings? PROPERTY ARG-LIST CODE
#     dom_construct     PROPERTY ARG-LIST CODE
#
#     dom_todo          PROPERTY
#     dom_call_todo     PROPERTY
#
#     dom_default_value CODE
#     dom_events        CODE
#     dom_scope         CODE
#
#
namespace eval ::hv3::dom2 {

  ::variable BaseArray
  ::variable TypeArray
  ::variable CurrentType
  ::variable DocBuffer
  ::variable Docs

  ::variable ClassList ""

  array set TypeArray ""
  set CurrentType ""

  proc evalcode {code} {
    #puts $code
    eval $code
  }

  proc stateless {type_name args} {
    set compiler2::parameter dummy
    set compiler2::default_value error
    set compiler2::finalize ""
    set compiler2::events ""
    set compiler2::scope ""
    array unset compiler2::get_array
    array unset compiler2::put_array
    array unset compiler2::call_array

    set mappings [list]
    foreach v [info vars ::hv3::dom::code::*] {
      set name [string range $v [expr [string last : $v]+1] end]
      lappend mappings %${name}% [set $v]
    }
    set body ""
    foreach a $args {
      append body "\n"
      append body [string map $mappings $a]
    }

    # Compile the documentation for this object. This step is optional.
    if {$::hv3::dom::CREATE_DOM_DOCS} {
      doccompiler::clean
      namespace eval doccompiler $body
      unset -nocomplain doccompiler::get_array(*)
      set documentation [doccompiler::make $type_name]
      evalcode [list \
         proc ::hv3::DOM::docs::${type_name} {} [list return $documentation]
      ]
    } else {
      namespace eval compiler2 $body
  
      if {[info exists compiler2::get_array(*)]} {
        set zDefaultGet $compiler2::get_array(*)
        unset compiler2::get_array(*)
      }

      set DYNAMICGET ""
      if {[info exists compiler2::get_array(**)]} {
        set zDynamicGet $compiler2::get_array(**)
        unset compiler2::get_array(**)
        set DYNAMICGET "
            set property \[lindex \$args 0\]
            set result \[eval {$zDynamicGet} \]
            if {\$result ne {}} {return \$result}
            unset result property
        "
      }

      set SETSTATEARRAY ""
      if {$compiler2::parameter eq "myStateArray"} {
        set SETSTATEARRAY {upvar $myStateArray state}
      }
  
      set GET [array get compiler2::get_array]
      set LIST [array names compiler2::get_array]
  
      foreach {zProp val} [array get compiler2::call_array] {
        foreach {isString call_args zCode} $val {}

        set zCode "
          $SETSTATEARRAY
          $zCode
        "
  
        set procname ::hv3::DOM::${type_name}.${zProp}
        set arglist [concat myDom $compiler2::parameter $call_args]
        set proccode [list proc $procname $arglist $zCode]
        evalcode $proccode
  
        if {$isString < 0} {
          set calltype TclConstructable
          set isString ""
        } else {
          set calltype TclCallableProc
        }
  
        lappend GET $zProp [string map [list \
          %CALLTYPE% $calltype               \
          %ISSTRING% $isString               \
          %PROCNAME% $procname               \
          %PARAM% $compiler2::parameter    \
        ] {
          list cache transient [list \
            ::hv3::dom::%CALLTYPE%   \
              [$myDom see] %ISSTRING% [list %PROCNAME% $myDom $%PARAM%]
          ]
        }]
      }
      if {[info exists zDefaultGet]} {
        lappend GET default [join \
          [list {set property [lindex $args 0]} $zDefaultGet] "\n"
        ]
      }
  
      set PUT [list]
      foreach {zProp val} [array get compiler2::put_array] {
        foreach {isString zArg zCode} $val {}
  
        if {$isString} {
          set Template {
            set %ARG% [[$myDom see] tostring [lindex $args 1]]
            %CODE%
          }
        } else {
          set Template {
            set %ARG% [lindex $args 1]
            %CODE%
          }
        }
        lappend PUT $zProp [string map       \
            [list %ARG% $zArg %CODE% $zCode] \
            $Template
        ]
      }
      lappend PUT default {return "native"}
  
      set hasproperty [string map [list   \
        %FUNC%  ::hv3::DOM::$type_name    \
        %PARAM% $compiler2::parameter     \
      ] {
        expr {[llength [%FUNC% $myDom $%PARAM% Get [lindex $args 0]]]>0}
      }]
  
      set arglist [list myDom $compiler2::parameter Method args]
      set proccode [list \
        proc ::hv3::DOM::$type_name $arglist [string map [list \
          %DYNAMICGET% $DYNAMICGET \
          %GET% $GET %PUT% $PUT \
          %DEFAULTVALUE% $compiler2::default_value \
          %FINALIZE% $compiler2::finalize   \
          %HASPROPERTY% $hasproperty        \
          %LIST%          $LIST             \
          %SETSTATEARRAY% $SETSTATEARRAY    \
          %EVENTS% $compiler2::events       \
          %SCOPE% $compiler2::scope       \
        ] {
          %SETSTATEARRAY%
          switch -exact -- $Method {
            Get {
              %DYNAMICGET%
              switch -exact -- [lindex $args 0] {
                %GET%
              }
            }
            Put {
              switch -exact -- [lindex $args 0] { %PUT% }
            }
            HasProperty {
              %HASPROPERTY%
            }
            DefaultValue {
              %DEFAULTVALUE%
            }
            Finalize {
              %FINALIZE%
            }
  
            Events {
              %EVENTS%
            }
  
            Scope {
              %SCOPE%
            }
  
            List { list %LIST% }
          }
        }
      ]]
  
      evalcode $proccode

      if {![info exists zDefaultGet] && ![info exists zDynamicGet]} {
        set property_list [concat \
            [array names compiler2::get_array] \
            [array names compiler2::call_array] \
        ]
        evalcode [list ::see::class ::hv3::DOM::$type_name $property_list]
      } 
    }
  }

  namespace eval compiler2 {

    variable parameter
    variable default_value
    variable finalize
    variable events
    variable scope

    variable get_array
    variable put_array
    variable call_array

    proc dom_parameter {zParam} {
      variable parameter
      set parameter $zParam
    }

    proc dom_default_value {zDefault} {
      variable default_value
      set default_value $zDefault
    }

    proc dom_finalize {zScript} {
      variable finalize
      set finalize $zScript
    }

    proc check_for_is_string {isStringVar argsVar} {
      upvar $isStringVar isString
      upvar $argsVar args

      set isString 0
      if {[lindex $args 0] eq "-string"} {
        set isString 1
        set args [lrange $args 1 end]
      }
    }

    # dom_call ?-string? PROPERTY ARG-LIST CODE
    #
    proc dom_call {args} {
      variable call_array
      check_for_is_string isString args
      if {[llength $args] != 3} {
        set shouldbe "\"dom_call ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_call - should be: $shouldbe" 
      }
      foreach {zMethod zArgs zCode} $args {}
      set call_array($zMethod) [list $isString $zArgs $zCode]
    }

    proc dom_call_todo {zProc} {}
    proc dom_todo {zAttr} {}

    # dom_construct PROPERTY ARG-LIST CODE
    #
    proc dom_construct {args} {
      variable call_array
      if {[llength $args] != 3} {
        set shouldbe "\"dom_construct ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_construct - should be: $shouldbe" 
      }
      foreach {zMethod zArgs zCode} $args {}
      set call_array($zMethod) [list -1 $zArgs $zCode]
    }

    # dom_get PROPERTY CODE
    #
    proc dom_get {zProperty zScript} {
      variable get_array
      set get_array($zProperty) $zScript
    }

    # dom_put ?-string? PROPERTY ARG-NAME CODE
    #
    proc dom_put {args} {
      variable put_array
      check_for_is_string isString args
      if {[llength $args] != 3} {
        set shouldbe "\"dom_put ?-string? PROPERTY ARG-NAME CODE\""
        error "Invalid arguments to dom_put - should be: $shouldbe" 
      }
      foreach {zProperty zArg zCode} $args {}
      set put_array($zProperty) [list $isString $zArg $zCode]
    }

    proc dom_events {zCode} {
      variable events
      set events $zCode
    }
    proc dom_scope {zCode} {
      variable scope
      set scope $zCode
    }

    proc -- {args} {}
    proc XX {args} {}
    proc Ref {args} {}

    proc Inherit {superclass code} {
      eval $code
    }
  }


  namespace eval doccompiler {

    variable get_array
    variable put_array
    variable call_array

    variable superclass

    variable current_xx ""
    variable xx_array

    proc dom_parameter     {args} {}
    proc dom_default_value {args} {}
    proc dom_finalize      {args} {}
    proc dom_call_todo    {zProc} {}
    proc dom_todo         {zAttr} {}
    proc dom_construct    {args} {}
    proc dom_events    {args} {}
    proc dom_scope {args} {}

    proc Inherit {super code} {
      variable superclass
      set superclass $super
    }

    proc dom_get  {zProperty args} {
      variable get_array
      variable docbuffer

      set get_array($zProperty) $docbuffer
      set docbuffer ""
    }
    proc dom_put  {args} {
      variable put_array
      if {[lindex $args 0] eq "-string"} {
        set put_array([lindex $args 1]) 1
      } else {
        set put_array([lindex $args 0]) 1
      }
    }

    # dom_call -string method args ...
    proc dom_call {args} {
      variable call_array
      variable docbuffer

      if {[lindex $args 0] eq "-string"} {
        set args [lrange $args 1 end]
      }
      set method  [lindex $args 0]
      set arglist [lindex $args 1]

      set call_array($method) [list $docbuffer [lrange $arglist 1 end]]
      set docbuffer ""
    }

    proc -- {args} {
      variable docbuffer
      if {[llength $args] == 0} {
        append docbuffer <p>
      } else {
        if {$docbuffer eq ""} {append docbuffer <p>}
        append docbuffer [join $args " "]
        append docbuffer "\n"
      }
      return ""
    }
    proc XX {args} {
      variable current_xx
      variable xx_array
      variable docbuffer

      if {[llength $args] != 0} {
        set current_xx [join $args " "]
      }
      set xx_array($current_xx) $docbuffer
      set docbuffer ""
    }
    proc Ref {ref {text ""}} {
      if {$text eq ""} {set text $ref}
      subst {<A href="${ref}">${text}</A>}
    }

    proc clean {} {
      variable get_array
      variable put_array
      variable call_array
      variable docbuffer
      variable superclass
      variable current_xx
      variable xx_array

      set superclass ""
      set docbuffer ""
      set current_xx ""
      array unset get_array
      array unset put_array
      array unset call_array
      array unset xx_array
      array set xx_array [list "" "<I>TODO: Class documentation</I>"]
    }

    proc make {classname} {
      variable get_array
      variable put_array
      variable call_array
      variable xx_array
      variable superclass

      set properties "<TR><TD colspan=3><H2>Properties</H2>"
      set iStripe 0
      foreach {zProp} [lsort [array names get_array]] {
        set docs $get_array($zProp)
        set readwrite ""
        if {[info exists put_array($zProp)]} {
          set readwrite "<I>r/w</I>"
        }
        append properties "<TR class=stripe${iStripe}>
          <TD class=spacer> 
          <TD class=\"property\"><B>$zProp</B>
          <TD>$readwrite
          <TD width=100%>$docs
        "
        set iStripe [expr {($iStripe+1)%2}]
      }

      set methods "<TR><TD colspan=3><H2>Methods</H2>"
      set iStripe 0
      foreach {zProp} [lsort [array names call_array]] {
        set data $call_array($zProp)
        foreach {docs arglist} $data {break}
        set zArglist [join $arglist ", "]
        append methods "<TR class=stripe${iStripe}>
          <TD class=spacer> 
          <TD class=\"method\" colspan=2><B>${zProp}</B>(${zArglist})
          <TD width=100%>$docs
        "
        set iStripe [expr {($iStripe+1)%2}]
      }

      set super ""
      if {$superclass ne ""} {
        set super [string map [list %SUPER% $superclass] {
          <P class=superclass>
            This object type inherits from <A href="%SUPER%">%SUPER%</A>.
            In addition to the properties and methods shown below, it has
            all the properties and methods of the %SUPER% object.
          </P>
        }]
      }

      set Docs [string map [list       \
          %CLASSNAME%  $classname      \
          %OVERVIEW%   $xx_array()     \
          %PROPERTIES% $properties     \
          %METHODS%    $methods        \
          %SUPERCLASS% $super          \
      ] {
        <LINK rel="stylesheet" href="home://dom/style.css">
        <TITLE>Class %CLASSNAME%</TITLE>
        <H1>DOM Class %CLASSNAME%</H1>
        <DIV class=overview> %OVERVIEW% </DIV>
        %SUPERCLASS%
        <TABLE>
          %PROPERTIES%
          %METHODS%
        </TABLE>
      }]

      return $Docs
    }
  }
}

