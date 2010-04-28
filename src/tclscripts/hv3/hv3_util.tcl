catch {namespace eval hv3 { set {version($Id$)} 1 }}


namespace eval hv3 {

  proc ReturnWithArgs {retval args} {
    return $retval
  }

  proc scrollbar {args} {
    set w [eval [linsert $args 0 ttk::scrollbar]]
    return $w
  }

  # scrolledwidget
  #
  #     Widget to add automatic scrollbars to a widget supporting the
  #     [xview], [yview], -xscrollcommand and -yscrollcommand interface (e.g.
  #     html, canvas or text).
  #
  namespace eval scrolledwidget {
  
    proc new {me widget args} {
      upvar #0 $me O
      set w $O(win)

      set O(-propagate) 0 
      set O(-scrollbarpolicy) auto
      set O(-takefocus) 0

      set O(myTakeControlCb) ""

      # Create the three widgets - one user widget and two scrollbars.
      set O(myWidget) [eval [linsert $widget 1 ${w}.widget]]
      set O(myVsb) [::hv3::scrollbar ${w}.vsb -orient vertical -takefocus 0] 
      set O(myHsb) [::hv3::scrollbar ${w}.hsb -orient horizontal -takefocus 0]

      set wid $O(myWidget)
      bind $w <KeyPress-Up>     [list $me scrollme $wid yview scroll -1 units]
      bind $w <KeyPress-Down>   [list $me scrollme $wid yview scroll  1 units]
      bind $w <KeyPress-Return> [list $me scrollme $wid yview scroll  1 units]
      bind $w <KeyPress-Right>  [list $me scrollme $wid xview scroll  1 units]
      bind $w <KeyPress-Left>   [list $me scrollme $wid xview scroll -1 units]
      bind $w <KeyPress-Next>   [list $me scrollme $wid yview scroll  1 pages]
      bind $w <KeyPress-space>  [list $me scrollme $wid yview scroll  1 pages]
      bind $w <KeyPress-Prior>  [list $me scrollme $wid yview scroll -1 pages]
  
      $O(myVsb) configure -cursor "top_left_arrow"
      $O(myHsb) configure -cursor "top_left_arrow"
  
      grid configure $O(myWidget) -column 0 -row 1 -sticky nsew
      grid columnconfigure $w 0 -weight 1
      grid rowconfigure    $w 1 -weight 1
      grid propagate       $w $O(-propagate)
  
      # First, set the values of -width and -height to the defaults for 
      # the scrolled widget class. Then configure this widget with the
      # arguments provided.
      $me configure -width  [$O(myWidget) cget -width] 
      $me configure -height [$O(myWidget) cget -height]
      eval $me configure $args
  
      # Wire up the scrollbars using the standard Tk idiom.
      $O(myWidget) configure -yscrollcommand [list $me scrollcallback $O(myVsb)]
      $O(myWidget) configure -xscrollcommand [list $me scrollcallback $O(myHsb)]
      $O(myVsb) configure -command [list $me scrollme $O(myWidget) yview]
      $O(myHsb) configure -command [list $me scrollme $O(myWidget) xview]
  
      # Propagate events from the scrolled widget to this one.
      bindtags $O(myWidget) [concat [bindtags $O(myWidget)] $O(win)]
    }

    proc destroy {me} {
      uplevel #0 [list unset $me]
      rename $me ""
    }
  
    proc configure-propagate {me} {
      upvar #0 $me O
      grid propagate $O(win) $O(-propagate)
    }
  
    proc take_control {me callback} {
      upvar #0 $me O
      if {$O(myTakeControlCb) ne ""} {
        uplevel #0 $O(myTakeControlCb)
      }
      set O(myTakeControlCb) $callback
    }
  
    proc scrollme {me args} {
      upvar #0 $me O
      if {$O(myTakeControlCb) ne ""} {
        uplevel #0 $O(myTakeControlCb)
        set O(myTakeControlCb) ""
      }
      eval $args
    }
  
    proc scrollcallback {me scrollbar first last} {
      upvar #0 $me O

      $scrollbar set $first $last
      set ismapped   [expr [winfo ismapped $scrollbar] ? 1 : 0]
  
      if {$O(-scrollbarpolicy) eq "auto"} {
        set isrequired [expr ($first == 0.0 && $last == 1.0) ? 0 : 1]
      } else {
        set isrequired $O(-scrollbarpolicy)
      }
  
      if {$isrequired && !$ismapped} {
        switch [$scrollbar cget -orient] {
          vertical   {grid configure $scrollbar  -column 1 -row 1 -sticky ns}
          horizontal {grid configure $scrollbar  -column 0 -row 2 -sticky ew}
        }
      } elseif {$ismapped && !$isrequired} {
        grid forget $scrollbar
      }
    }

    proc configure-scrollbarpolicy {me} {
      upvar #0 $me O
      eval $me scrollcallback $O(myHsb) [$O(myWidget) xview]
      eval $me scrollcallback $O(myVsb) [$O(myWidget) yview]
    }
  
    proc widget {me} {
      upvar #0 $me O
      return $O(myWidget)
    }

    proc unknown {method me args} {
      # puts "UNKNOWN: $me $method $args"
      upvar #0 $me O
      uplevel 3 [list eval $O(myWidget) $method $args]
    }
    namespace unknown unknown

    set DelegateOption(-width) hull
    set DelegateOption(-height) hull
    set DelegateOption(-cursor) hull
    set DelegateOption(*) myWidget
  }

  # Wrapper around the ::hv3::scrolledwidget constructor. 
  #
  # Example usage to create a 400x400 canvas widget named ".c" with 
  # automatic scrollbars:
  #
  #     ::hv3::scrolled canvas .c -width 400 -height 400
  #
  proc scrolled {widget name args} {
    return [eval [concat ::hv3::scrolledwidget $name $widget $args]]
  }

  proc Expand {template args} {
    return [string map $args $template]
  }
}

namespace eval ::hv3::string {

  # A generic tokeniser procedure for strings. This proc splits the
  # input string $input into a list of tokens, where each token is either:
  #
  #     * A continuous set of alpha-numeric characters, or
  #     * A quoted string (quoted by " or '), or
  #     * Any single character.
  #
  # White-space characters are not returned in the list of tokens.
  #
  proc tokenise {input} {
    set tokens [list]
    set zIn [string trim $input]
  
    while {[string length $zIn] > 0} {
  
      if {[ regexp {^([[:alnum:]_.-]+)(.*)$} $zIn -> zToken zIn ]} {
        # Contiguous alpha-numeric characters
        lappend tokens $zToken
  
      } elseif {[ regexp {^(["'])} $zIn -> zQuote]} {      #;'"
        # Quoted string
  
        set nEsc 0
        for {set nToken 1} {$nToken < [string length $zIn]} {incr nToken} {
          set c [string range $zIn $nToken $nToken]
          if {$c eq $zQuote && 0 == ($nEsc%2)} break
          set nEsc [expr {($c eq "\\") ? $nEsc+1 : 0}]
        }
        set zToken [string range $zIn 0 $nToken]
        set zIn [string range $zIn [expr {$nToken+1}] end]
  
        lappend tokens $zToken
  
      } else {
        lappend tokens [string range $zIn 0 0]
        set zIn [string range $zIn 1 end]
      }
  
      set zIn [string trimleft $zIn]
    }
  
    return $tokens
  }

  # Dequote $input, if it appears to be a quoted string (starts with 
  # a single or double quote character).
  #
  proc dequote {input} {
    set zIn $input
    set zQuote [string range $zIn 0 0]
    if {$zQuote eq "\"" || $zQuote eq "\'"} {
      set zIn [string range $zIn 1 end]
      if {[string range $zIn end end] eq $zQuote} {
        set zIn [string range $zIn 0 end-1]
      }
      set zIn [regsub {\\(.)} $zIn {\1}]
    }
    return $zIn
  }


  # A procedure to parse an HTTP content-type (media type). See section
  # 3.7 of the http 1.1 specification.
  #
  # A list of exactly three elements is returned. These are the type,
  # subtype and charset as specified in the parsed content-type. Any or
  # all of the fields may be empty strings, if they are not present in
  # the input or a parse error occurs.
  #
  proc parseContentType {contenttype} {
    set tokens [::hv3::string::tokenise $contenttype]

    set type [lindex $tokens 0]
    set subtype [lindex $tokens 2]

    set enc ""
    foreach idx [lsearch -regexp -all $tokens (?i)charset] {
      if {[lindex $tokens [expr {$idx+1}]] eq "="} {
        set enc [::hv3::string::dequote [lindex $tokens [expr {$idx+2}]]]
        break
      }
    }

    return [list $type $subtype $enc]
  }

  proc htmlize {zIn} {
    string map [list "<" "&lt;" ">" "&gt;" "&" "&amp;" "\"" "&quote;"] $zIn
  }

}


proc ::hv3::char {text idx} {
  return [string range $text $idx $idx]
}

proc ::hv3::next_word {text idx idx_out} {

  while {[char $text $idx] eq " "} { incr idx }

  set idx2 $idx
  set c [char $text $idx2] 

  if {$c eq "\""} {
    # Quoted identifier
    incr idx2
    set c [char $text $idx2] 
    while {$c ne "\"" && $c ne ""} {
      incr idx2
      set c [char $text $idx2] 
    }
    incr idx2
    set word [string range $text [expr $idx+1] [expr $idx2 - 2]]
  } else {
    # Unquoted identifier
    while {$c ne ">" && $c ne " " && $c ne ""} {
      incr idx2
      set c [char $text $idx2] 
    }
    set word [string range $text $idx [expr $idx2 - 1]]
  }

  uplevel [list set $idx_out $idx2]
  return $word
}

proc ::hv3::sniff_doctype {text pIsXhtml} {
  upvar $pIsXhtml isXHTML
  # <!DOCTYPE TopElement Availability "IDENTIFIER" "URL">

  set QuirksmodeIdentifiers [list \
    "-//w3c//dtd html 4.01 transitional//en" \
    "-//w3c//dtd html 4.01 frameset//en"     \
    "-//w3c//dtd html 4.0 transitional//en" \
    "-//w3c//dtd html 4.0 frameset//en" \
    "-//softquad software//dtd hotmetal pro 6.0::19990601::extensions to html 4.0//en" \
    "-//softquad//dtd hotmetal pro 4.0::19971010::extensions to html 4.0//en" \
    "-//ietf//dtd html//en//3.0" \
    "-//w3o//dtd w3 html 3.0//en//" \
    "-//w3o//dtd w3 html 3.0//en" \
    "-//w3c//dtd html 3 1995-03-24//en" \
    "-//ietf//dtd html 3.0//en" \
    "-//ietf//dtd html 3.0//en//" \
    "-//ietf//dtd html 3//en" \
    "-//ietf//dtd html level 3//en" \
    "-//ietf//dtd html level 3//en//3.0" \
    "-//ietf//dtd html 3.2//en" \
    "-//as//dtd html 3.0 aswedit + extensions//en" \
    "-//advasoft ltd//dtd html 3.0 aswedit + extensions//en" \
    "-//ietf//dtd html strict//en//3.0" \
    "-//w3o//dtd w3 html strict 3.0//en//" \
    "-//ietf//dtd html strict level 3//en" \
    "-//ietf//dtd html strict level 3//en//3.0" \
    "html" \
    "-//ietf//dtd html//en" \
    "-//ietf//dtd html//en//2.0" \
    "-//ietf//dtd html 2.0//en" \
    "-//ietf//dtd html level 2//en" \
    "-//ietf//dtd html level 2//en//2.0" \
    "-//ietf//dtd html 2.0 level 2//en" \
    "-//ietf//dtd html level 1//en" \
    "-//ietf//dtd html level 1//en//2.0" \
    "-//ietf//dtd html 2.0 level 1//en" \
    "-//ietf//dtd html level 0//en" \
    "-//ietf//dtd html level 0//en//2.0" \
    "-//ietf//dtd html strict//en" \
    "-//ietf//dtd html strict//en//2.0" \
    "-//ietf//dtd html strict level 2//en" \
    "-//ietf//dtd html strict level 2//en//2.0" \
    "-//ietf//dtd html 2.0 strict//en" \
    "-//ietf//dtd html 2.0 strict level 2//en" \
    "-//ietf//dtd html strict level 1//en" \
    "-//ietf//dtd html strict level 1//en//2.0" \
    "-//ietf//dtd html 2.0 strict level 1//en" \
    "-//ietf//dtd html strict level 0//en" \
    "-//ietf//dtd html strict level 0//en//2.0" \
    "-//webtechs//dtd mozilla html//en" \
    "-//webtechs//dtd mozilla html 2.0//en" \
    "-//netscape comm. corp.//dtd html//en" \
    "-//netscape comm. corp.//dtd html//en" \
    "-//netscape comm. corp.//dtd strict html//en" \
    "-//microsoft//dtd internet explorer 2.0 html//en" \
    "-//microsoft//dtd internet explorer 2.0 html strict//en" \
    "-//microsoft//dtd internet explorer 2.0 tables//en" \
    "-//microsoft//dtd internet explorer 3.0 html//en" \
    "-//microsoft//dtd internet explorer 3.0 html strict//en" \
    "-//microsoft//dtd internet explorer 3.0 tables//en" \
    "-//sun microsystems corp.//dtd hotjava html//en" \
    "-//sun microsystems corp.//dtd hotjava strict html//en" \
    "-//ietf//dtd html 2.1e//en" \
    "-//o'reilly and associates//dtd html extended 1.0//en" \
    "-//o'reilly and associates//dtd html extended relaxed 1.0//en" \
    "-//o'reilly and associates//dtd html 2.0//en" \
    "-//sq//dtd html 2.0 hotmetal + extensions//en" \
    "-//spyglass//dtd html 2.0 extended//en" \
    "+//silmaril//dtd html pro v0r11 19970101//en" \
    "-//w3c//dtd html experimental 19960712//en" \
    "-//w3c//dtd html 3.2//en" \
    "-//w3c//dtd html 3.2 final//en" \
    "-//w3c//dtd html 3.2 draft//en" \
    "-//w3c//dtd html experimental 970421//en" \
    "-//w3c//dtd html 3.2s draft//en" \
    "-//w3c//dtd w3 html//en" \
    "-//metrius//dtd metrius presentational//en" \
  ]

  set isXHTML 0
  set idx [string first <!DOCTYPE $text]
  if {$idx < 0} { return "quirks" }

  # Try to parse the TopElement bit. No quotes allowed.
  incr idx [string length "<!DOCTYPE "]
  while {[string range $text $idx $idx] eq " "} { incr idx }

  set TopElement   [string tolower [next_word $text $idx idx]]
  set Availability [string tolower [next_word $text $idx idx]]
  set Identifier   [string tolower [next_word $text $idx idx]]
  set Url          [next_word $text $idx idx]

#  foreach ii [list TopElement Availability Identifier Url] {
#    puts "$ii -> [set $ii]"
#  }

  # Figure out if this should be handled as XHTML
  #
  if {[string first xhtml $Identifier] >= 0} {
    set isXHTML 1
  }
  if {$Availability eq "public"} {
    set s [expr [string length $Url] > 0]
    if {
         $Identifier eq "-//w3c//dtd xhtml 1.0 transitional//en" ||
         $Identifier eq "-//w3c//dtd xhtml 1.0 frameset//en" ||
         ($s && $Identifier eq "-//w3c//dtd html 4.01 transitional//en") ||
         ($s && $Identifier eq "-//w3c//dtd html 4.01 frameset//en")
    } {
      return "almost standards"
    }
    if {[lsearch $QuirksmodeIdentifiers $Identifier] >= 0} {
      return "quirks"
    }
  }

  return "standards"
}


proc ::hv3::configure_doctype_mode {html text pIsXhtml} {
  upvar $pIsXhtml isXHTML
  set mode [sniff_doctype $text isXHTML]

  switch -- $mode {
    "quirks"           { set defstyle [::tkhtml::htmlstyle -quirks] }
    "almost standards" { set defstyle [::tkhtml::htmlstyle] }
    "standards"        { set defstyle [::tkhtml::htmlstyle]
    }
  }

  $html configure -defaultstyle $defstyle -mode $mode

  return $mode
}

namespace eval ::hv3 {

  variable Counter 1

  proc handle_destroy {me obj win} {
    if {$obj eq $win} {
      upvar #0 $me O
      set cmd $O(cmd)
      $me destroy
      rename $cmd ""
    }
  }
  proc handle_rename {me oldname newname op} {
    upvar #0 $me O
    set O(cmd) $newname
  }

  proc construct_object {ns obj arglist} {

    set PROC proc
    if {[info commands real_proc] ne ""} {
      set PROC real_proc
    } 

    set isWidget [expr {[string range $obj 0 0] eq "."}]

    # The name of the array to use for this object.
    set arrayname $obj
    if {$arrayname eq "%AUTO%" || $isWidget} {
      set arrayname ${ns}::inst[incr ${ns}::_OBJ_COUNTER]
    }

    # Create the object command.
    set body "namespace eval $ns \$m $arrayname \$args"
    namespace eval :: [list $PROC $arrayname {m args} $body]

    # If the first character of the new command name is ".", then
    # this is a new widget. Populate the state array with the following
    # special variables:
    #
    #   O(win)        Window path.
    #   O(hull)       Window command.
    #
    if {[string range $obj 0 0] eq "."} {
      variable HullType
      variable Counter
      upvar #0 $arrayname O

      set O(hull) ${obj}_win[incr Counter]
      set O(win) $obj
      eval $HullType($ns) $O(win)
      namespace eval :: rename $O(win) $O(hull)

      bind $obj <Destroy> +[list ::hv3::handle_destroy $arrayname $obj %W]

      namespace eval :: [list $PROC $O(win) {m args} $body]
      set O(cmd) $O(win)
      trace add command $O(win) rename [list ::hv3::handle_rename $arrayname]
    }

    # Call the object constructor.
    namespace eval $ns new $arrayname $arglist
    return [expr {$isWidget ? $obj : $arrayname}]
  }

  proc make_constructor {ns {hulltype frame}} {
    variable HullType

    if {[info commands ${ns}::destroy] eq ""} {
      error "Object class has no destructor: $ns"
    }
    set HullType($ns) $hulltype

    # Create the constructor
    #
    proc $ns {obj args} "::hv3::construct_object $ns \$obj \$args"

    # Create the [cget] method.
    #
    namespace eval $ns "
      proc cget {me option} {
        upvar \$me O
        if {!\[info exists O(\$option)\]} {
          variable DelegateOption
          if {\[info exists DelegateOption(\$option)\]} {
            return \[
              eval \$O(\$DelegateOption(\$option)) [list cget \$option]
            \]
            return
          } elseif {\[info exists DelegateOption(*)\]} {
            return \[eval \$O(\$DelegateOption(*)) [list cget \$option ]\]
          }
          error \"unknown option: \$option\"
        }
        return \$O(\$option)
      }
    "
    # Create the [configure] method.
    #
    set cc ""
    foreach cmd [info commands ${ns}::configure*] {
      set key [string range $cmd [string length ${ns}::configure] end]
      append cc "if {\$option eq {$key}} {configure$key \$me}\n"
    }
    namespace eval $ns "
      proc configure {me args} {
        upvar \$me O
        foreach {option value} \$args {
          if {!\[info exists O(\$option)\]} {
            variable DelegateOption
            if {\[info exists DelegateOption(\$option)\]} {
              eval \$O(\$DelegateOption(\$option)) [list configure \$option \$value]
            } elseif {\[info exists DelegateOption(*)\]} {
              eval \$O(\$DelegateOption(*)) [list configure \$option \$value]
            } else {
              error \"unknown option: \$option\"
            }
          } elseif {\$O(\$option) != \$value} {
            set O(\$option) \$value
            $cc
          }
        }
      }
    "
  }
}

::hv3::make_constructor ::hv3::scrolledwidget


