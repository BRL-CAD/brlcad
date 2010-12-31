#
# This file defines and implements a very primitive macro language for
# documents using the Tcl interpreter as it's parser. It is designed for
# use with "man-page" documents. Source documents can be formatted as
# either nroff (for the man command) or html (for browser and windows
# users).
#
# The following "macros" (really Tcl procs) are available:
#
#     TH
#     SQ
#     Section
#     Subsection
#     Option
#     Bulletlist
#     Subcommand
#     Code
#

###########################################################################
# Utility procedures
#
proc lshift {lvar} {
  upvar $lvar list
  set ret [lindex $list 0]
  set list [lrange $list 1 end]
  return $ret
}

proc strip_leading_tabs {text} {
  set ret ""
  foreach line [split $text "\n"] {
    append ret "[regsub {^\t} $line {}]\n"
  }
  return $ret
}

proc usage {} {
  puts stderr "Usage: $::argv0 \[-nroff|-html] <input-file>"
  exit -1
}
#
# End utility procedures:
###########################################################################

###########################################################################
# Generic document macros.
proc SQ {args} {
  return "\[$args\]"
}
# End generic document macros
###########################################################################

###########################################################################
# The following procedures interpret macros used in document files for
# nroff output.
#
namespace eval NroffMacros {

namespace export TH Section Option Bulletlist Code 
namespace export Subcommand Subsection process_text

proc process_text {input {nocommands 0}} {
  if {$nocommands} {
    set output [subst -novariables -nocommands $input]
  } else {
    set output [subst -novariables $input]
  }

  set output [
    regsub -all {_([^ \n]*)_} $output {\1}
  ]

  set output [string trim [strip_leading_tabs $output]]
  set output [regsub -all {[\n]*REMOVELINEBREAK\n} $output \n]

  return $output
}

proc TH {name section} {
  set date [clock format [clock seconds]]
  return "\n.TH \"$name\" \"$section\" \"$date\"\n"
}

proc Section {args} {
  set heading [string toupper $args]
  return ".SH \"$heading\""
}

proc Option {name description} {
  set dbname [string toupper [string range $name 0 0]][string range $name 1 end]
  return [string trim [subst {
	Command-Line Name:-$name
	.br
	Database Name:  $name
	.br
	Database Class: $dbname
	.br
	.RS
	.PP
	[process_text $description]
	.RE
  }]]
}

proc Bulletlist {args} {
  set nroff "REMOVELINEBREAK\n"
  append nroff ".RS\n"
  foreach a $args {
    append nroff ".IP * 2\n"
    append nroff "[string trim [strip_leading_tabs $a]]\n"
  }
  append nroff ".RE"

  return $nroff
}

proc Code {args} {
  set nroff "REMOVELINEBREAK\n"

  if {[llength $args] == 1} {set args [lindex $args 0]}

  append nroff ".RS\n"
  append nroff "\n.nf\n"
  append nroff [process_text $args 1]
  append nroff "\n.fi\n"
  append nroff ".RE"

  return $nroff
}

proc Subcommand {n {text {}}} {
  set nroff "REMOVELINEBREAK\n"

  if {$text == {}} {
    set text $n
    set n 1
  } else {
    set n [expr int(abs($n))]
  }

  set i 0
  foreach line [split [process_text $text] "\n"] {
    if {$i == $n} {
      append nroff "\n.RS\n"
    }
    if {$i < $n} {
      append nroff "\n$line\n.br"
    } else {
      append nroff "$line\n"
    }
    incr i
  }

  if {$i > $n} {
    append nroff ".RE"
  }

  return $nroff
}

proc Subsection {args} {
  set heading [string toupper $args]
  return ".SS \"$heading\""
}

}; # namespace eval NroffMacros...
#
# End nroff document macros.
###########################################################################

###########################################################################
# Html document macros.
#
namespace eval HtmlMacros {
namespace export TH Section Option Bulletlist Code Subcommand Subsection
namespace export process_text process_index

proc process_index {} {
  set state 1
  set html {<a name="index"></a><ul>}
  set i 0

  foreach s $::HtmlMacros::Index {
    foreach {level title} $s {}
    set I [incr i]
    switch $level {
      SECTION {
        if {$state == 2} {
          append html </ul>
          set state 1
        }
        append html "<li><a href=\"#section$I\">$title</a>"
      }
      SUBSECTION {
        if {$state == 1} {
          append html <ul>
          set state 2
        }
        append html "<li><a href=\"#section$I\">$title</a>"
      }
    }
  }

  if {$state == 2} {
    append html </ul>
  }
  append html </ul>
  return $html
}

proc process_text {input {nocommands 0} {noparagraphs 0}} {
  if {$nocommands} {
    set output [subst -novariables -nocommands $input]
  } else {
    set output [subst -novariables $input]
  }
  set output [string trim [strip_leading_tabs $output]]

  set output [
    regsub -all {_([^ \n]*)_} $output {<i>\1</i>}
  ]

  set blocknest 0
  set paraopen 0

  set out ""
  foreach o [split $output "\n"] {
    set isstart [expr [string match $o $::HtmlMacros::START]]
    set isend [expr [string match $o $::HtmlMacros::END]]

    if {$isstart} {
        if {$paraopen} {
           append out "</p>\n"
           set paraopen 0
        }
        incr blocknest
    } elseif {$isend} {
        incr blocknest -1
    } else {
      if {0 == $blocknest} {
        set isempty [regexp {^[:space:]*$} $o]
        if {$isempty && $paraopen} {
          append out "</p>\n"
          set paraopen 0
        } elseif {!$noparagraphs && !$isempty && !$paraopen} {
          append out "<p>\n"
          set paraopen 1
        }
      }
      append out "$o\n"
    }
  }

  return $out
}

set START STARTSTARTSTARTSTART
set END ENDENDENDENDEND

proc Block {code} {
  return "\n$::HtmlMacros::START\n$code\n$::HtmlMacros::END\n"
}

proc TH {name section} {
  set ::HtmlMacros::Title "$name ($section)"
  return ""
}

set N 0
proc Section {args} {
  set n [incr ::HtmlMacros::N]
  lappend ::HtmlMacros::Index [list SECTION $args]
  return [Block "<a name=\"section$n\"><h2>$args</h2></a>"]
}

proc Subsection {args} {
  set n [incr ::HtmlMacros::N]
  lappend ::HtmlMacros::Index [list SUBSECTION $args]
  return [Block "<a name=\"section$n\"><h3>$args</h3></a>"]
}

proc Code {args} {
  if {[llength $args] == 1} {set args [lindex $args 0]}
  set text [process_text $args 1 1] 
  return [Block "<div style=\"margin-left:8ex\"><pre>$text</pre></div>"]
}

proc Option {name description} {
  set dbname [string toupper [string range $name 0 0]][string range $name 1 end]
  return [Block [subst {
    <p>Command-Line Name: -$name<br>
       Database Name: $name<br>
       Database Class: $dbname
    </p>
    <div style="margin-left:8ex">

       [process_text $description]

    </div>
  }]]
}

proc Bulletlist {args} {
  set items {}
  foreach a $args {
    append items "<li>$a"
  }
  return [Block "<ul>$items</ul>"]
}

proc Subcommand {n {text {}}} {
  if {$text == {}} {
    set text $n
    set n 1
  } else {
    set n [expr int(abs($n))]
  }

  set i 0
  set description {}

  set lines [split [string trim $text] "\n"]

  append html [subst {
      <p>
      [join [lrange $lines 0 [expr $n - 1]] <br>]
      </p>
  }]

  append html {<div style="margin-left:8ex">}
  append html [process_text [join [lrange $lines $n end] "\n"]]
  append html {</div>}

  return [Block $html]
}

}
# End html document macros
###########################################################################

if {[llength $argv] != 2} usage
set mode [lindex $argv 0]
set file [lindex $argv 1]
if {$mode != "-html" && $mode != "-nroff"} usage

set fd [open $file]
set input [strip_leading_tabs [read $fd]]
close $fd

set ::TABS ""
catch {
  source [file join [file dirname $file] .. webpage common.tcl]
  if {[string match *hv3* $file]} {
    set ::TABS [getTabs 4]
  } else {
    set ::TABS [getTabs 2]
  }
} msg
# puts stderr "ERROR $msg"

switch -- $mode {
  -nroff {
    namespace import NroffMacros::*
    puts [process_text $input]
  }
  -html {
    namespace import HtmlMacros::*
    set input [regsub -all {>} $input {\&gt;}]
    set input [regsub -all {<} $input {\&lt;}]

    set text [process_text $input]
    puts [subst {
       <html>
       <head>
       <title>$::HtmlMacros::Title</title>
       <link rel="stylesheet" href="tkhtml_tcl_tk.css"</link>
       </head>
       <body>
       $::TABS
       <div id="body">
       <h1>$::HtmlMacros::Title</h1>

       <div class="sidebox"><div id="toc">
         <a name="TOC"><h3>Table Of Contents</h3></a>
         [process_index]
       </div></div>

       <div id="text">
         $text
       </div></div>
       </body>
       </html>
    }]
  }
}

