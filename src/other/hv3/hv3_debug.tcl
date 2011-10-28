namespace eval hv3 { set {version($Id$)} 1 }

namespace eval ::hv3 {
  ::snit::widget console {

    # Entry field for typing commands (entry widget):
    variable myEntryField 

    # Output window for output of typed commands (text widget):
    variable myOutputWindow 

    # Viewer window for viewing HTML/CSS/Javascript code (text widget).
    variable myCodeViewer

    variable myLabel

    # Current value of language selector widget. Always one of:
    #
    #     "Tcl"
    #     "Javascript"
    #     "Search"
    #
    variable myLanguage

    # An array of tags to delete from widget $myCodeViewer when it is
    # next cleared.
    variable myCodeViewerLinks [list]

    # An array of tags to delete from widget $myOutputWindow when 
    # it is next cleared.
    variable myOutputWindowLinks [list]

    # Font size currently used in the two text widgets ($myOutputWindow and
    # $myCodeViewer).
    variable myFontSize 10

    # Command line history state.
    variable myHistory [list]
    variable myHistoryIdx 0

    variable myBreakpoints -array [list]

    variable myPage ""
    variable myPageId ""

    constructor {args} {
      set f [frame ${win}.f]

      set myEntryField [::hv3::entry ${win}.f.entry_field]
      set sel ${win}.f.select 
      set om [tk_optionMenu $sel [myvar myLanguage] Tcl Javascript Search]
      $sel configure -width 9

      $om configure -borderwidth 1 -activeborderwidth 1
      $om configure -borderwidth 1
      $sel configure -borderwidth 1
      ::hv3::UseHv3Font $om
      ::hv3::UseHv3Font $sel

      pack ${win}.f.select -side left
      pack $myEntryField -fill both -expand 1

      set pan [panedwindow ${win}.pan -orient vertical -bd 0]

      set myOutputWindow [::hv3::scrolled ::hv3::text ${win}.pan.output_window]
      set myCodeViewer   [::hv3::scrolled ::hv3::text ${win}.pan.code_viewer]

      $myOutputWindow configure -height 200 -width 600 -bg white
      $myCodeViewer   configure -height 200 -bg white

      $pan add $myCodeViewer
      $pan add $myOutputWindow

      set myCodeViewer [$myCodeViewer widget]

      set b [frame ${win}.b]
      ::hv3::button ${b}.viewindex         \
          -text "Application Index"        \
          -command [list $self Display index ""]
      set myLabel [::hv3::label ${b}.label -anchor w]
      pack ${b}.viewindex -side left

      ::hv3::button ${b}.reorient -text Resize -command [list $self Reorient]
      ::hv3::button ${b}.increasefont -text "+" -command [list $self font +2]
      ::hv3::button ${b}.decreasefont -text "-" -command [list $self font -2]
      pack ${b}.reorient -side right
      pack ${b}.increasefont -side right
      pack ${b}.decreasefont -side right

      pack ${b}.label -side left -fill x

      pack $b -side top -fill x
      pack $f -side bottom -fill x
      pack $pan -fill both -expand 1

      bind $myEntryField <Return> [list $self Evaluate]

      set boldfont [concat [$myOutputWindow cget -font] bold]

      $myOutputWindow tag configure error      -foreground red
      $myOutputWindow tag configure tcl        -background #DDDDDD
      $myOutputWindow tag configure javascript -background #DDDDFF
      $myOutputWindow tag configure search     -background #DDFFDD

      $myCodeViewer tag configure red -foreground red
      $myCodeViewer tag configure linenumber -background #BBBBBB
      $myCodeViewer tag configure wheat -background wheat
      $myCodeViewer tag configure english -wrap word
      $myCodeViewer tag configure breakpoint -background red

      bind $myEntryField <Control-j> [list set [myvar myLanguage] Javascript]
      bind $myEntryField <Control-t> [list set [myvar myLanguage] Tcl]
      bind $myEntryField <Control-s> [list set [myvar myLanguage] Search]

      bind $myEntryField <Up>   [list $self History -1]
      bind $myEntryField <Down> [list $self History +1]

      $myCodeViewer tag bind linenumber <1> [list $self ClickLineNumber %x %y]
      $myCodeViewer tag bind breakpoint <1> [list $self ClickLineNumber %x %y]

      $myOutputWindow configure -state disabled
      $myCodeViewer configure -state disabled
      focus $myEntryField

      $self font 0
      $self Display index ""
    }

    method History {iDir} {
      set iHistory [expr $myHistoryIdx + $iDir]
      if {$iHistory < 0 || $iHistory > [llength $myHistory]} return

      set myHistoryIdx $iHistory
      $myEntryField delete 0 end
      $myEntryField insert 0 [lindex $myHistory $myHistoryIdx 1]
      
      set lang [lindex $myHistory $myHistoryIdx 0]
      if {$lang ne ""} {set myLanguage $lang}
    }

    method font {incr} {
      incr myFontSize $incr
      $myOutputWindow configure -font [list monospace $myFontSize]
      $myCodeViewer configure   -font [list monospace $myFontSize]
      $myEntryField configure   -font [list monospace $myFontSize]
      if {($myFontSize + $incr) < 6} {
        ${win}.b.decreasefont configure -state disabled
      } else {
        ${win}.b.decreasefont configure -state normal
      }
    }

    method Reorient {} {
      set y [lindex [${win}.pan sash coord 0] 1]
      set h [winfo height ${win}.pan]
      if {$y > $h/2} {
        set y [expr {int($h*0.1)}]
      } else {
        set y [expr {int($h*0.9)}]
      }
      ${win}.pan sash place 0 0 $y
    }


    # This method is called to evaluate a command typed into $myEntryField.
    #
    method Evaluate {} {
      set cmd [$myEntryField get]
      if {$cmd eq ""} return

      lappend myHistory [list $myLanguage $cmd]
      set myHistoryIdx [expr [llength $myHistory]]
      $myEntryField delete 0 end

      $myOutputWindow configure -state normal
      switch -- $myLanguage {
        Tcl {
          set rc [catch [list \
            namespace eval ::hv3::console_commands $cmd
          ] result]
          set result [string map {"\n" "\n    "} $result]
          $myOutputWindow insert end "\$ $cmd\n" tcl
          if {$rc} {
            $myOutputWindow insert end "    $result\n" error
          } else {
            $myOutputWindow insert end "    $result\n"
          }
        }

        Javascript {
          set isEnabled [gui_current cget -enablejavascript]
          $myOutputWindow insert end "> $cmd\n" javascript
          if {!$isEnabled} {
            $myOutputWindow insert end "    Javascript is not enabled\n" error
          } else {
            set dom [[gui_current hv3] dom]
            set result [$dom javascript $cmd]
            set result [string map {"\n" "\n    "} $result]
            if {[lindex $result 0] eq "JS_ERROR"} {
              $myOutputWindow insert end "    $result\n" error
            } else {
              $myOutputWindow insert end "    $result\n"
            }
          }
        }

        Search {
          set ignore_case 0
          if {$cmd eq [string tolower $cmd]} {
            set ignore_case 1
          }
          $myOutputWindow insert end "/ $cmd\n" search

          # Search through all the javascript files for the string $cmd.
          set dom [[gui_current hv3] dom]
          set ii 0
          foreach logscript [$dom GetLog] {
            if {[$logscript cget -isevent]} continue

            set iLine 1
            foreach zLine [split [$logscript cget -script] "\n"] {
              set zLine [string trim $zLine]
              set zSearch $zLine
              if {$ignore_case} {set zSearch [string tolower $zSearch]}
              if {[set i [string first $cmd $zSearch]] >= 0} {
                set c [list $self DisplayJavascriptError 0 $logscript $iLine]

                set nLine [string length $zLine]

                set iStart [expr $i-20]
                set iEnd   [expr $i+20]
                if {$iEnd > $nLine} {
                  incr iStart [expr $nLine - $iEnd]
                  set iEnd $nLine
                }
                if {$iStart < 0} {
                  incr iEnd [expr 0 - $iStart]
                  set iStart 0
                }
                set match [string range $zLine $iStart $iEnd]

                if {$iStart > 0} {
                   $myOutputWindow insert end "..."
                } else {
                   $myOutputWindow insert end "   "
                }
                $myOutputWindow insert end [format %-40s $match]
                if {$iEnd < $nLine } {
                   $myOutputWindow insert end "...  "
                } else {
                   $myOutputWindow insert end "     "
                }

                set z "Line $iLine, [$logscript cget -heading]"
                $self OutputWindowLink $z $c
                $myOutputWindow insert end "\n"
              }
              incr iLine
            }
          }
        }
      }

      $myOutputWindow yview end
      $myOutputWindow configure -state disabled
    }

    # This proc is called to display a page in the "code viewer" pane 
    # (the top one). Parameter $page must be one of the following: 
    #
    #     html
    #     index
    #     css
    #     javascript
    #
    method Display {page pageid} {
      $myCodeViewer configure -state normal -cursor xterm

      $myCodeViewer delete 0.0 end
      foreach tag $myCodeViewerLinks {
        $myCodeViewer tag delete $tag
      }
      set myCodeViewerLinks [list]

      switch -- $page {
        html {
          set hv3 [$pageid hv3]
          $myCodeViewer insert end [$hv3 log get html]
          $myLabel configure -text "HTML Code: [$hv3 uri get]"
          $myCodeViewer configure -wrap word
        }

        index {
          $self DisplayIndex
          $myLabel configure -text ""
          $myCodeViewer configure -wrap none
        }

        css {
          eval $self DisplayCss $pageid
          $myLabel configure -text "CSS Code: [lindex $pageid 1]"
          $myCodeViewer configure -wrap word
        }

        javascript {
          eval $self DisplayJavascript $pageid
          $myCodeViewer configure -wrap word
        }

        default {error "Internal error - bad page \"$page\""}
      }
      $myCodeViewer configure -state disabled

      set myPage $page
      set myPageId $pageid
    }

    method CreateCodeViewerLink {text command} {
      set tag "link[expr rand()]"
      lappend myCodeViewerLinks $tag
      $myCodeViewer tag configure $tag -underline 1 -foreground darkblue
      $myCodeViewer tag bind $tag <1> $command
      $myCodeViewer tag bind $tag <Enter> "
          $myCodeViewer configure -cursor hand2
          $myCodeViewer tag configure $tag -background #d9d9d9
      "
      $myCodeViewer tag bind $tag <Leave> "
          $myCodeViewer configure -cursor xterm
          $myCodeViewer tag configure $tag -background white
      "
      $myCodeViewer insert end $text $tag
      return $tag
    }
    method OutputWindowLink {text command} {
      set tag "link[expr rand()]"
      lappend myOutputWindowLinks $tag
      set ow [$myOutputWindow widget]
      $ow tag configure $tag -underline 1 -foreground darkblue
      $ow tag bind $tag <1> $command
      $ow tag bind $tag <Enter> [list $ow configure -cursor hand2]
      $ow tag bind $tag <Leave> [list $ow configure -cursor xterm]
      $ow insert end $text $tag
      return $tag
    }

    proc getcss {frame id} {
      set hv3 [$frame hv3]
      foreach css [$hv3 log get css] {
        if {[lindex $css 0] eq $id} {return $css}
      }
      return ""
    }

    method DisplayJavascriptError {idx logscript iLine} {
      $self Display javascript [list $idx $logscript]
      $myCodeViewer yview -pickplace "$iLine.0"
      $myCodeViewer tag add wheat "$iLine.0" "$iLine.0 lineend"
    }
    method DisplayCssError {pageid iLine} {
      $self Display css $pageid
      $myCodeViewer yview -pickplace "$iLine.0"
    }

    proc getlogscript {name} {
      foreach {dom idx} [split $name .] {}
      foreach logscript [$dom GetLog] {
        if {[$logscript cget -name] eq $name} {return $logscript}
      }
      return ""
    }

    method Errors {page pageid} {
      $myOutputWindow configure -state normal
      switch -- $page {
        css {
          foreach {id f data errors} [eval getcss $pageid] break
          $myOutputWindow insert end "Errors from CSS: $f\n" error
          foreach {i n} $errors {
            # The offsets stored in the $errors array are 
            # byte-offsets. Transform these to character offsets 
            # before using them:
            set i [::tkhtml::charoffset $data $i]
            set n [::tkhtml::charoffset [string range $data $i end] $n]

            $myOutputWindow insert end "    "

            set nLine [llength [split [string range $data 0 $i] "\n"]]
            $self OutputWindowLink "Line $nLine (skipped $n characters)" [
              list $self DisplayCssError $pageid $nLine
            ]
            $myOutputWindow insert end "\n"
          }
        }
        javascript {
          foreach {idx dom logscript} $pageid break
          set f "$idx. [$logscript cget -heading]"
          $myOutputWindow insert end "Javascript errors: $f\n" error

          set r [$logscript cget -result]
          $myOutputWindow insert end "    "
          $myOutputWindow insert end "Error Message: \"[lindex $r 1]\"\n"
          if {[lindex $r 2] ne ""} {
              $myOutputWindow insert end "    "
              $myOutputWindow insert end "[lindex $r 2]\n"
          }

          foreach {zFile iLine zType zName} [lrange $r 3 end] {
            set target [getlogscript $zFile]
            if {$target ne ""} {
              $myOutputWindow insert end "    "
              set cmd [list $self DisplayJavascriptError $idx $target $iLine]
              $self OutputWindowLink "Line $iLine, [$target cget -heading]" $cmd
              if {$zType ne "" || $zName ne ""} {
                $myOutputWindow insert end "  ($zType $zName)"
              }
              $myOutputWindow insert end "\n"
            } 
          }

          $myOutputWindow insert end "\n"
        }
      }
      $myOutputWindow yview end
      $myOutputWindow configure -state disabled
    }

    method AddLineNumbers {{name ""}} {
      set line 1
      set nLine [lindex [split [$myCodeViewer index end] .] 0]
      for {set line 1} {$line < $nLine} {incr line} {
        set nDigit 7
        if {[info exists myBreakpoints($name,$line)]} {
          set nDigit 5
        }
        set num [format "% ${nDigit}d " $line]
        $myCodeViewer insert "${line}.0" $num linenumber

        if {$nDigit == 5} {
          $myCodeViewer insert "${line}.0" "  " breakpoint
        }
      }
    }

    method ClickLineNumber {x y} {
      if {$myPage ne "javascript"} return
      foreach {idx logscript} $myPageId break;
      set iLine [lindex [split [$myCodeViewer index @$x,$y] .] 0]

      set var myBreakpoints($logscript,$iLine)

      $myCodeViewer configure -state normal
      $myCodeViewer delete ${iLine}.0 ${iLine}.2
      if {[info exists $var]} {
        unset $var
        $myCodeViewer insert ${iLine}.0 "  " linenumber
      } else {
        set $var 1
        $myCodeViewer insert ${iLine}.0 "  " breakpoint
      }
      $myCodeViewer configure -state disabled
    }

    method DisplayCss {frame styleid} {
      set hv3 [$frame hv3]
      foreach css [$hv3 log get css] {
        foreach {id f data errors} $css break
        if {$id eq $styleid} {
          set iCurrent 0
          foreach {iStart nLen} $errors {
            # The offsets stored in the $errors array are 
            # byte-offsets. Transform these to character offsets 
            # before using them:
            set iStart [::tkhtml::charoffset $data $iStart]
            set nLen   [
                ::tkhtml::charoffset [string range $data $iStart end] $nLen
            ]
            $myCodeViewer insert end [
                string range $data $iCurrent [expr {$iStart-1}]
            ]
            $myCodeViewer insert end [
                string range $data $iStart [expr {$iStart+$nLen-1}]
            ] red
            set iCurrent [expr {$iStart + $nLen}]
          }
          $myCodeViewer insert end [string range $data $iCurrent end]
          $self AddLineNumbers
        }
      }
    }

    method DisplayJavascript {idx logscript} {
      set data [$logscript cget -script]
      $myCodeViewer insert end $data

      set heading [$logscript cget -heading]
      $myLabel configure -text "Javascript Code: [expr {$idx+1}] $heading"

      $self AddLineNumbers $logscript
    }

    method DisplayIndex {} {
      set top [gui_current top_frame]
      if {"" eq [[$top hv3] log get html]} {
        $myCodeViewer insert end [join {
            {Source logging was not enabled when this document was loaded.}
	    {To browse the document source code, select a different option}
            {from the "Debug->Application Source Logging" menu and reload}
            {the document.}
        }]
        return
      }
      $self DisplayResources $top 2
    }

    method DisplayResources {frame iIndent} {
      set hv3 [$frame hv3]

      set zIndent [string repeat " " $iIndent]
      set uri [$hv3 uri get]
      $myCodeViewer insert end "${zIndent}Frame: $uri\n"

      # Link for the HTML file.
      #
      $myCodeViewer insert end "${zIndent}  "
      set cmd1 [list $self Display html $frame]
      set cmd2 [list ::HtmlDebug::browse $hv3 [$hv3 node]]
      $self CreateCodeViewerLink "View Html" $cmd1
      $myCodeViewer insert end "         "
      $self CreateCodeViewerLink "Open Tree Browser..." $cmd2
      $myCodeViewer insert end "\n"

      # Links for each loaded CSS document.
      #
      foreach css [$hv3 log get css] {
        foreach {id filename data errors} $css break
        $myCodeViewer insert end "${zIndent}  "
        set cmd [list $self Display css [list $frame $id]]
        $self CreateCodeViewerLink "View CSS" $cmd
        $myCodeViewer insert end ":         $filename  "
        if {[llength $errors] > 0} {
            set nErr [expr {[llength $errors]/2}]
            set cmd [list $self Errors css [list $frame $id]]
            set t [$self CreateCodeViewerLink "($nErr parse errors)" $cmd]
            $myCodeViewer tag configure $t -foreground red
            $myCodeViewer insert end "  "
        }
        $myCodeViewer insert end "\n"
      }

      # Links for each javascript file.
      #
      set dom [$hv3 dom]
      set ii 0
      foreach logscript [$dom GetLog] {
        if {[$logscript cget -isevent]} continue
        incr ii
        set cmd [list $self Display javascript [list $ii $logscript]]
        set nLine [llength [split [$logscript cget -script] "\n"]]
        $myCodeViewer insert end "$zIndent  "
        $self CreateCodeViewerLink "View Javascript" $cmd
        $myCodeViewer insert end ":  [$logscript cget -heading]  "
        if {[$logscript cget -rc]} {
          set tag [$self CreateCodeViewerLink "(Failed)  " [
              list $self Errors javascript [list $ii $dom $logscript]
          ]]
          $myCodeViewer tag configure $tag -foreground red
        } else {
          $myCodeViewer insert end "(Ok)  "
        }
        $myCodeViewer insert end "($nLine lines)\n"
      }

      $myCodeViewer insert end "\n"
      foreach child [$frame child_frames] {
        $self DisplayResources $child [expr {$iIndent+4}]
      }
    }
  }

  proc launch_console {} {
    if {![winfo exists .console]} {
      toplevel .console \
          -height [expr [winfo height .] - 100] \
          -width [expr [winfo width .] - 100]
      ::hv3::console .console.console
      pack .console.console -fill both -expand 1

      set w [winfo reqwidth .console]
      set h [winfo reqheight .console]
      scan [wm geometry [winfo parent .console]] "%dx%d+%d+%d" pw ph px py
      set geom "+[expr $px + $pw/2 - $w/2]+[expr $py + $ph/2 - $h/2]"
      wm geometry .console $geom
    }

    wm state .console normal
    raise .console
  }
}

# This [namespace eval] block adds the special commands designed for
# interactive use from the debugging console:
#
#     primitives
#     breakpoints
#     heapdebug
#
namespace eval ::hv3::console_commands {

  proc help {} {
    return "
      primitives
      breakpoints
      heapdebug
    "
  }

  proc primitives {} {
    set zRet ""
    set iIndent 0
    foreach primitive [hv3_html _primitives] {
      set t [lindex $primitive 0]
      if {$t eq "draw_origin_end"} {incr iIndent -4}
      append zRet [string repeat " " $iIndent] $primitive "\n"
      if {$t eq "draw_origin_start"} {incr iIndent 4}
      incr hist($t)
    }
  
    append zRet "\n"
    foreach {key value} [array get hist] {
      append zRet $key ":" $value "\n"
    }
  
    set zRet
  }

  proc gimages {} {
    images 1
  }
  proc images {{all_tabs 0}} {
    set nPix 0
    set nImg 0

    if {$all_tabs} {
      set data [list]
      foreach browser [.notebook tabs] {
        eval lappend data [[$browser hv3] html _images]
      }
    } else {
      set data [hv3 html _images]
    }

    set header [list URL "Tk image" pixmap width height alpha references]
    foreach record [concat [list $header] $data] {
      foreach $header $record break
      if {[string length $URL] > 15} {
        set URL "...[string range $URL [expr [string length $URL]-12] end]"
      }
      if {$URL eq ""} {set alpha ""}
      append ret [format "%-15s %7s %9s %6s  %6s  %8s  %11s\n" \
        $URL $pixmap ${Tk image} $width $height $alpha $references
      ]
      if {$width ne "width"} {
        if {$pixmap eq "PIX"} {
          incr nPix [expr $width * $height]
        } else {
          incr nImg [expr $width * $height]
        }
      }
    }
    append ret "\n"
    append ret "    total pixmap pixels: $nPix\n"
    append ret "    total image pixels: $nImg"
    set ret
  }

  proc heapdebug {} {
    pretty_print_heapdebug
  }

  proc breakpoints {} {
  }
  
  proc hv3 {args} {
    set hv3 [gui_current hv3]
    eval $hv3 $args
  }

  proc console {args} {
    eval .console.console $args
  }
}


proc pretty_print_heapdebug {} {
  set data [lsort -index 2 -integer [::tkhtml::heapdebug]]
  set ret ""
  set nTotalS 0
  set nTotalB 0
  foreach type $data {
    foreach {zStruct nStruct nByte} $type {}
    incr nTotalB $nByte
    incr nTotalS $nStruct
    set avg [expr {$nByte/$nStruct}]
    append ret [
      format "% -30s % 10d % 10d %12s %10d\n" $zStruct $nStruct $nByte "($avg/alloc)" $nTotalB]
  }
  append ret [format "% -30s % 10d % 10d\n" "Totals" $nTotalS $nTotalB]
  set ret
}

proc pretty_print_vars {} {
  set ret ""
  set nTotal 0
  set nNamespace 0
  foreach e [lsort -integer -index 1 [get_vars]] {
    incr nTotal [lindex $e 1]
    incr nNamespace 1
    incr nArray [lindex $e 2]
    append ret [format "% -50s %d (%d arrays)\n" [lindex $e 0] [lindex $e 1] [lindex $e 2]]
  }
  append ret [format "% -50s %d\n" TOTAL($nNamespace) $nTotal]
  set ret
}
proc pretty_print_cmds {} {
  set ret ""
  set nTotal 0
  set nNamespace 0
  count_commands :: ::pretty_print_cmds_report
  foreach e [lsort -integer -index 1 $::pretty_print_cmds_report] {
    incr nTotal [lindex $e 1]
    incr nNamespace 1
    append ret [format "% -50s %d\n" [lindex $e 0] [lindex $e 1]]
  }
  unset ::pretty_print_cmds_report
  append ret [format "% -50s %d\n" TOTAL($nNamespace) $nTotal]
  set ret
}

proc tree_to_report {tree indent} {
  set i [string repeat " " $indent]
  set f [lindex $tree 0]

  set name [$f cget -name]
  set uri  [[$f hv3] uri get]

  append ret [format "% -40s %s\n" $i\"$name\" $uri]
  foreach child [lindex $tree 1] {
    append ret [tree_to_report $child [expr {$indent+4}]] 
  }
  set ret
}
proc pretty_print_frames {} {
  tree_to_report [lindex [gui_current frames_tree] 0] 0
}

proc get_vars {{ns ::}} {
  set nVar 0
  set nArray 0
  set ret [list]
  set vlist [info vars ${ns}::*]
  foreach var $vlist {
    if {[array exists $var]} {
      incr nVar [llength [array names $var]]
      incr nArray 1
    } else {
      incr nVar 1
    }
  }
  lappend ret [list $ns $nVar $nArray]
  foreach child [namespace children $ns] {
    eval lappend ret [get_vars $child]
  }
  set ret
}
proc count_vars {{ns ::} {print 0}} {
  set nVar 0
  foreach entry [get_vars $ns] {
    incr nVar [lindex $entry 1]
  }
  set nVar
}
proc count_commands {{ns ::} {reportvar ""}} {
  set nCmd [llength [info commands ${ns}::*]]
  if {$reportvar ne ""} {
    uplevel #0 [list lappend $reportvar [list $ns $nCmd]]
  }
  foreach child [namespace children $ns] {
    incr nCmd [count_commands $child $reportvar]
  }
  set nCmd
}
proc count_namespaces {{ns ::}} {
  set nNs 1
  foreach child [namespace children $ns] {
    incr nNs [count_namespaces $child]
  }
  set nNs
}

