namespace eval hv3 { set {version($Id$)} 1 }

###########################################################################
# hv3_prop.tcl --
#
#     This file contains code to implement the Hv3 debugging interface.
#     Sometimes I call it the "tree browser". To open from Hv3, select
#     (Debug->Tree Browser...)
#

package require Tk

# The first argument, $HTML, must be the name of an html widget that
# exists somewhere in the application. If one does not already exist,
# invoking this proc instantiates an HtmlDebug object associated with
# the specified html widget. This in turn creates a top-level 
# debugging window exclusively associated with the specified widget.
# The HtmlDebug instance is deleted when the top-level window is 
# destroyed, either by hitting 'q' or 'Q' while the window has focus,
# or by closing the window using a window-manager interface.
#
# If the second argument is not "", then it is the name of a node 
# within the document currently loaded by the specified html widget.
# If this node exists, the debugging window presents information 
# related to it.
#
namespace eval ::HtmlDebug {
  proc browse {hv3 {node ""}} {
    set name [$hv3 html].debug
    if {![winfo exists $name]} {
      HtmlDebug $name $hv3
    }
    return [$name browseNode $node]
  }
}

snit::widget ::hv3::debug::tree {
  # The canvas widget on which the tree is drawn:
  #
  variable myCanvas ""

  # The Hv3 widget being inspected.
  variable myHtml ""

  # Internal tree state:
  #
  variable mySelected ""         ;# Currently selected node
  variable myExpanded -array ""  ;# Array. Entries are present for exp. nodes

  variable mySearchResults [list]
  
  constructor {HTML args} {
    set myHtml $HTML

    # The canvas widget.
    set myCanvas ${win}.canvas
    ::hv3::scrolled canvas $myCanvas -propagate 1

    # The two buttons (expand/collapse all).
    frame ${win}.buttons
    ::hv3::button ${win}.buttons.expand \
      -text "Expand All" -command [mymethod ExpandAll]
    ::hv3::button ${win}.buttons.collapse \
      -text "Collapse All" -command [mymethod CollapseAll]
    pack ${win}.buttons.expand -side left
    pack ${win}.buttons.collapse -side left

    # The search widget.
    frame ${win}.search
    ::hv3::label  ${win}.search.label -text "Search:"
    ::hv3::entry  ${win}.search.entry
    ::hv3::button ${win}.search.forward -text > -state disabled
    ::hv3::button ${win}.search.back    -text < -state disabled
    ::hv3::label  ${win}.search.label2 -text "?/0"

    bind ${win}.search.entry <Return> [mymethod Search]
    ${win}.search.forward configure -command [mymethod SearchNext +1]
    ${win}.search.back    configure -command [mymethod SearchNext -1]

    pack ${win}.search.label   -side left
    pack ${win}.search.entry   -side left
    pack ${win}.search.back    -side left
    pack ${win}.search.forward -side left
    pack ${win}.search.label2  -side left
  
    $myCanvas configure -background white -borderwidth 10 -width 5
    pack ${win}.search -fill x -expand 0
    pack $myCanvas -fill both -expand true
    pack ${win}.buttons -fill x -expand 0
  }

  method CollapseAll {} {
    array unset myExpanded
    $self redraw
  }
  method ExpandAll {} {
    array unset myExpanded
    $self ExpandChildren [$myHtml node]
    $self redraw
  }
  method ExpandChildren {N} {
    if {$N eq ""} return
    set myExpanded($N) 1
    foreach c [$N children] {
      $self ExpandChildren $c
    }
  }

  method SearchNext {dir} {
    set idx [lsearch $mySearchResults $mySelected]
    incr idx $dir
    set node [lindex $mySearchResults $idx]
    if {$node ne ""} {
      ::HtmlDebug::browse $myHtml $node
    }
  }

  method Search {} {
    set selector [${win}.search.entry get]
    set mySearchResults [list]

    # Allow two "special" selector syntaxes:
    #
    #     ::tkhtml::nodeNNN
    #     NNN
    #
    # where NNN is some integer. This is used to navigate to a node
    # given it's Tcl command name.
    #
    if {[string is integer $selector]} {
      set mySearchResults [list "::tkhtml::node${selector}"]
    } elseif {[regexp {::tkhtml::node([1234567890]+)} $selector X int]} {
      set mySearchResults [list "::tkhtml::node${int}"]
    } elseif {$selector ne ""} {
      set mySearchResults [$myHtml html search $selector]
    }

    if {[llength $mySearchResults] > 0} {

      # Expand the tree branches so that all of the found nodes are visible.
      #
      foreach s $mySearchResults {
        for {set N [$s parent]} {$N ne ""} {set N [$N parent]} {
          set myExpanded($N) 1
        }
      }

      $self selected [lindex $mySearchResults 0]
    }

    ::HtmlDebug::browse $myHtml $mySelected
  }

  method redraw {} {
    $myCanvas delete all

    # Make sure the parentage of the selected node is explanded.
    # This ensures that it is visible in the tree.
    #
    if {$mySelected ne ""} { set N [$mySelected parent] }
    while {$N ne ""} {
      set myExpanded($N) 1
      set N [$N parent]
    }

    $self drawSubTree [$myHtml node] 15 30
    set all [$myCanvas bbox all]
    $myCanvas configure -scrollregion $all

    # Scroll the canvas window the minimum vertical amount (may be zero) 
    # to make the selected node visible.
    #
    set bbox [$myCanvas bbox selected]
    if {$bbox ne ""} {
      set base [lindex $all 1]
      set ch [expr [lindex $all 3] - $base]
      set y1 [expr double([lindex $bbox 1]-$base)/double($ch)]
      set y2 [expr double([lindex $bbox 3]-$base)/double($ch)]
      set yview [$myCanvas yview]
      if {$y1 < [lindex $yview 0]} {
        $myCanvas yview moveto $y1
      }
      if {$y2 > [lindex $yview 1]} {
        $myCanvas yview moveto [expr $y2-([lindex $yview 1]-[lindex $yview 0])]
      }
    }

    # Set the text in the search-report label.
    #
    set idx [lsearch $mySearchResults $mySelected]
    incr idx
    if {$idx <= 0} { 
      ${win}.search.back configure -state disabled
      ${win}.search.forward configure -state disabled
      set idx ?
    } else {
      set state disabled
      if {$idx < [llength $mySearchResults]} {
        set state normal
      }
      ${win}.search.forward configure -state $state

      set state disabled
      if {$idx > 1} {
        set state normal
      }
      ${win}.search.back configure -state $state
    }
    set caption "$idx/[llength $mySearchResults]"
    ${win}.search.label2 configure -text $caption
  }

  method selected {args} {
    if {[llength $args] > 0} {
      catch { $mySelected override {} }
      set mySelected [lindex $args 0]

      set box [list outline-style solid outline-color blue outline-width 1px]
      catch { $mySelected override $box }
    } 
    set mySelected
  }

  method drawSubTree {node x y} {
    set IWIDTH [image width idir]
    set IHEIGHT [image width idir]

    set XINCR [expr $IWIDTH + 2]
    set YINCR [expr $IHEIGHT + 5]

    set tree [$myCanvas widget]

    set label [prop_nodeToLabel $node]
    if {$label == ""} {return 0}

    set leaf 1
    foreach child [$node children] {
        if {"" != [prop_nodeToLabel $child]} {
            set leaf 0
            break
        }
    }

    if {$leaf} {
      set iid [$tree create image $x $y -image ifile -anchor sw]
    } else {
      set iid [$tree create image $x $y -image idir -anchor sw]
    }

    set tid [$tree create text [expr $x+$XINCR] $y]
    $tree itemconfigure $tid -text $label -anchor sw
    set bbox [$tree bbox $tid]
    lset bbox 0 15

    if {$mySelected eq $node} {
        set rid [
            $tree create rectangle $bbox -fill skyblue -outline skyblue
        ]
        $tree lower $rid all
        $tree itemconfigure $rid -tags selected
    }
    if {[lsearch $mySearchResults $node] >= 0} {
        set rid [
            $tree create rectangle $bbox -fill wheat -outline wheat
        ]
        $tree lower $rid all
    }


    $tree bind $tid <1> [list HtmlDebug::browse $myHtml $node]
    $tree bind $iid <1> [mymethod toggleExpanded $node]
    
    set ret 1
    if {[info exists myExpanded($node)]} {
        set xnew [expr $x+$XINCR]
        foreach child [$node children] {
            set ynew [expr $y + $YINCR*$ret]
            set incrret [$self drawSubTree $child $xnew $ynew]
            incr ret $incrret
            if {$incrret > 0} {
                set y1 [expr $ynew - $IHEIGHT * 0.5]
                set x1 [expr $x + $XINCR * 0.5]
                set x2 [expr $x + $XINCR]
                $tree create line $x1 $y1 $x2 $y1 -fill black
            }
        }

        catch {
          $tree create line $x1 $y $x1 $y1 -fill black
        }
    }
    return $ret
  }

  method toggleExpanded {node} {
    if {[info exists myExpanded($node)]} {
      unset myExpanded($node)
    } else {
      set myExpanded($node) 1 
    }
    $self redraw
  }

  destructor {
    catch { $mySelected override {} }
  }

}

snit::widgetadaptor ::hv3::debug::report {
  option -reportcmd ""

  constructor {args} {
    installhull [::hv3::hv3 $win]
    $self configurelist $args
  }

  # This is called to load the report for node-handle $node (a Tkhtml node
  # handle belonging to hv3 mega-widget $hv3). Calling 
  #
  method report {hv3 node} {
    $hull configure -requestcmd [mymethod GetReport $hv3 $node]
    $hull goto "GetReport" -cachecontrol no-cache
  }
  method GetReport {hv3 node downloadHandle} {
    if {$options(-reportcmd) eq ""} {
      error "empty -reportcmd option"
    }
    $downloadHandle append [eval $options(-reportcmd) $hv3 $node]
    $downloadHandle finish
  }

  delegate method * to hull
  delegate option * to hull
}

proc ::hv3::debug::ReportStyle {} {
  return {
    <style>
      /* Table display parameters */
      table       { width: 100% }
      table,th,td { border: 1px solid; }
      td          { padding:0px 15px; }
      table       { margin: 20px; }

      /* Elements of class "code" are rendered in fixed font */
      .code       { font-family: fixed; }
    </style>
  }
}

snit::widget ::hv3::debug::FormReport {

  variable myReport ""

  variable myCurrentHv3  ""
  variable myCurrentNode ""

  component myHv3
  delegate option -fonttable to myHv3

  constructor {args} {

    # Button at the bottom of the frame to regenerate this report.
    # The point of regenerating it is that it will read new control
    # values from the forms modules.
    #
    ::hv3::button ${win}.button -text "Regenerate Report"
    ${win}.button configure -command [mymethod Regenerate]

    set myHv3 ${win}.hv3
    ::hv3::hv3 ${win}.hv3 
    ${win}.hv3 configure -requestcmd [mymethod GetReport]
    ${win}.hv3 configure -targetcmd  [mymethod ReturnSelf]

    pack ${win}.button -fill x -side bottom
    pack ${win}.hv3 -fill both -expand 1

    $self configurelist $args
  }

  method Regenerate {} {
    $self report $myCurrentHv3 $myCurrentNode
  }

  method report {hv3 node} {
    set myReport [::hv3::debug::ReportStyle]
    append myReport {<h1>Forms Engine Report</h1>}
    set msg [::hv3::formsreport $node]
    append myReport $msg
    ${win}.hv3 goto abc -cachecontrol no-cache

    set myCurrentHv3 $hv3
    set myCurrentNode $node
  }

  method GetReport {downloadHandle} {
    $downloadHandle append $myReport
    $downloadHandle finish
  }

  method goto {node args} {
    ::HtmlDebug::browse $myCurrentHv3 $node
  }

  method ReturnSelf {args} {
    return $self
  }
}

snit::widget ::hv3::debug::LogReport {

  variable myReport ""

  option -title ""

  component myHv3
  delegate option -fonttable to myHv3

  constructor {htmldebug args} {
    ::hv3::button ${win}.button -text "Re-render Document With Logging"
    ${win}.button configure -command [list $htmldebug rerender]

    set myHv3 ${win}.hv3
    ::hv3::hv3 ${win}.hv3 
    ${win}.hv3 configure -requestcmd [mymethod GetReport]

    pack ${win}.button -fill x -side bottom
    pack ${win}.hv3 -fill both -expand 1

    $self configurelist $args
  }

  # This is called to load the report for node-handle $node (a Tkhtml node
  # handle belonging to hv3 mega-widget $hv3). Calling 
  #
  method report {log} {
    set Template {
      <html>
        [::hv3::debug::ReportStyle]
      <body>
        <h1>$options(-title)</h1>
        $Tbl
      </body> 
      </html>
    }
    set Tbl ""
    if {$log ne ""} {
      append Tbl {<table border=1>}
      foreach entry $log {
        set entry [regsub {[A-Za-z]+\(\)} $entry <b>&</b>]

        # Do some special processing if the string matches either:
        #
        #     {^nomatch (SELECTOR) from.*$}
        #     {^match (SELECTOR) from.*$}
        set pattern1 {^matches \((.*)\) from (.*)$}
        set pattern2 {^nomatch \((.*)\) from (.*)$}

        append Tbl "    <tr><td>"
        if {[regexp $pattern1 $entry DUMMY zSelector zFrom]} {
          set zFrom [::hv3::string::htmlize $zFrom]
          set zSelector [::hv3::string::htmlize $zSelector]
          append Tbl "<i style=color:green>match </i>"
          append Tbl "<span style=font-family:fixed>$zSelector</span> "
          append Tbl "<i style=color:green>from $zFrom</i>\n"
        } \
        elseif {[regexp $pattern2 $entry DUMMY zSelector zFrom]} {
          set zFrom [::hv3::string::htmlize $zFrom]
          set zSelector [::hv3::string::htmlize $zSelector]
          append Tbl "<i style=color:red>nomatch </i>"
          append Tbl "<span style=font-family:fixed>$zSelector</span> "
          append Tbl "<i style=color:red>from $zFrom</i>\n"
        } \
        else {
          append Tbl "$entry\n"
        }

      }
      append Tbl "</table>\n"
    } else {
      append Tbl {
        <I>No logging info available. Pressing the 
           "Re-render Document With Logging" button below might help.
        </I>
      }
    }

    set myReport [subst $Template]
    ${win}.hv3 goto abc -cachecontrol no-cache
  }

  method GetReport {downloadHandle} {
    $downloadHandle append $myReport
    $downloadHandle finish
  }
}

# Format and return the "Events" report.
#
proc ::hv3::debug::EventsReport {hv3 node} {
  set Template {
    <html>
      [::hv3::debug::ReportStyle]
    <body>
      <h1>DOM Event Listeners Report</h1>
      $Tbl
    </body> 
    </html>
  }

  set dom [$hv3 dom]

  set Tbl {<table border=1 width=100%>}
  append Tbl {<tr><th>Event<th>Listener-Type<th>Javascript</tr>}
  if {$dom ne ""} {
    foreach evt [$dom eventdump $node] {
      foreach {e l j} $evt {}
      set fj [::hv3::string::htmlize [::see::format $j]]
      append Tbl "<tr><td>$e<td>$l<td><pre>$fj</pre>" 
    }
  }
  append Tbl { </table> }

  subst $Template
}

# Format and return the "Tkhtml" report.
#
proc ::hv3::debug::TkhtmlReport {hv3 node} {

  # Template for the node report. The $CONTENT variable is replaced by
  # some content generated by the Tcl code below.
  set Template {
    <html>
      <head>
        [::hv3::debug::ReportStyle]
      </head>
      <body>
        $CONTENT
      </body>
    </html>
  }

  set doc {}

    if {[$node tag] == ""} {
        append doc "<h1>Text</h1>"
        append doc "<p>Tcl command: <span class=\"code\">\[$node\]</span>"
        set text [string map {< &lt; > &gt;} [$node text]]
        set tokens [string map {< &lt; > &gt;} [$node text -tokens]]
        append doc [subst {
            <h1>Text</h1>
            <p>$text
            <h1>Tokens</h1>
            <p>$tokens
        }]
    } else {
        set property_rows ""
        foreach {p v} [prop_compress [$node prop]] {
            append property_rows "<tr><td>$p<td>$v"
        }

        set before_tbl ""
        catch {
            set rows ""
            foreach {p v} [prop_compress [$node prop -before]] {
                append rows "<tr><td>$p<td>$v"
            }
            set before_tbl "
              <table class=computed>
                <tr><th colspan=2>:before Element Properties
                $rows
              </table>
            "
        }
        set after_tbl ""
        catch {
            set rows ""
            foreach {p v} [prop_compress [$node prop -after]] {
                append rows "<tr><td>$p<td>$v"
            }
            set after_tbl "
              <table class=computed>
                <tr><th colspan=2>:after Element Properties
                $rows
              </table>
            "
        }

        set attribute_rows ""
        foreach {p v} [$node attr] {
          append attribute_rows "<tr><td>$p<td>$v"
        }
        if {$attribute_rows eq ""} {
          set attribute_rows {
            <tr><td colspan=2><i>Node has no attributes defined</i>
          }
        } 
        
        append doc [subst {
            <h1>&lt;[$node tag]&gt;</h1>
            <p>Tcl command: <span class="code">\[$node\]</span>
            <p>Replacement: <span class="code">\[[$node replace]\]</span>

            <table>
                <tr><th colspan=2>Attributes
                $attribute_rows
            </table>

            <p>Note: Fields 'margin', 'padding' and sometimes 'position' 
               contain either one or four length values. If there is only
	       one value, then this is the value for the associated top,
               right, bottom and left lengths. If there are four values, they
               are respectively the top, right, bottom, and left lengths.
            </p>

            <table>
                <tr><th colspan=2>Computed Property Values
                $property_rows
            </table>

            $after_tbl
            $before_tbl
        }]
    }

    if {[$node tag] ne ""} {
        append doc {<table><tr><th>CSS Dynamic Conditions}
        foreach condition [$node dynamic conditions] {
            set c [string map {< &lt; > &gt;} $condition]
            append doc "<tr><td>$c"
        }
        append doc {</table>}
    }

    if {[info exists myLayoutEngineLog($node)]} {
        append doc {<table class=layout_engine><tr><th>Layout Engine}
        foreach entry $myLayoutEngineLog($node) {
            set entry [regsub {[A-Za-z]+\(\)} $entry <b>&</b>]
            append doc "    <tr><td>$entry\n"
        }
        append doc "</table>\n"
    }
    if {[info exists myStyleEngineLog($node)]} {
        append doc {<table class=style_engine><tr><th>Style Engine}
        foreach entry $myStyleEngineLog($node) {
            if {[string match matches* $entry]} {
                append doc "    <tr><td><b>$entry<b>\n"
            } else {
                append doc "    <tr><td>$entry\n"
            }
        }
        append doc "</table>\n"
    }

    set CONTENT $doc
    set doc [subst $Template]
    return $doc
}


#--------------------------------------------------------------------------
# class HtmlDebug --
#
#     This class encapsulates code used for debugging the Html widget. 
#     It manages a gui which can be used to investigate the structure and
#     Tkhtml's handling of the currently loaded Html document.
#
snit::widget HtmlDebug {
  hulltype toplevel

  # Class internals
  typevariable Template

  variable mySelected ""         ;# Currently selected node
  variable myExpanded -array ""  ;# Array. Entries are present for exp. nodes
  variable myHtml                ;# Name of html widget being debugged

  variable mySearch ""

    # Debugging window widgets. The top 3 are managed by panedwindow
    # widgets.
    #
    #   $myTreeCanvas                  ;# [::hv3::scrolled canvas] mega-widget
    #   $myReports                     ;# Tab set of reports.

  variable myStyleEngineLog
  variable myLayoutEngineLog
  variable mySearchResults ""

  variable myTree               ;# The tree widget
  variable myHtmlReport         ;# The hv3 widget with the report
  variable myEventsReport       ;# The DOM events report for the node.
  variable myLayoutReport       ;# The layout engine report.
  variable myStyleReport        ;# The style engine report.
  variable myFormsReport        ;# The forms engine report.

  variable myReports            ;# The ::hv3::notebook widget.

  constructor {HTML} {
    set myHtml $HTML
    set fonttable [$HTML cget -fonttable]
  
    # Top level window bindings.
    bind $win <Escape>          [list destroy $win]
  
    set myTree       $win.hpan.vpan.tree

    set myHtmlReport   $win.hpan.vpan.reports.tkhtml
    set myEventsReport $win.hpan.vpan.reports.events
    set myLayoutReport $win.hpan.vpan.reports.layout
    set myStyleReport  $win.hpan.vpan.reports.style
    set myFormsReport  $win.hpan.vpan.reports.forms
    set myReports      $win.hpan.vpan.reports

    panedwindow $win.hpan -orient vertical
    # ::hv3::debug::search $mySearch $HTML
    panedwindow $win.hpan.vpan -orient horizontal

    ::hv3::debug::tree $myTree $HTML

    # The tabs in the right-hand pane:
    ::hv3::notebook $myReports -width 600
    ::hv3::debug::report $myEventsReport \
        -reportcmd ::hv3::debug::EventsReport -fonttable $fonttable
    ::hv3::debug::report $myHtmlReport \
        -reportcmd ::hv3::debug::TkhtmlReport -fonttable $fonttable
    ::hv3::debug::LogReport $myLayoutReport $self \
        -title "Layout Engine Log" -fonttable $fonttable
    ::hv3::debug::LogReport $myStyleReport $self \
        -title "Style Engine Log" -fonttable $fonttable
    ::hv3::debug::FormReport $myFormsReport -fonttable $fonttable

    $myReports add $myHtmlReport    "Tkhtml"
    $myReports add $myLayoutReport  "Layout Engine"
    $myReports add $myStyleReport   "Style Engine"
    $myReports add $myEventsReport  "DOM Event Listeners"
    $myReports add $myFormsReport   "HTML Forms"
  
    $win.hpan add $win.hpan.vpan
    $win.hpan.vpan add $myTree
    $win.hpan.vpan add $myReports 
    catch {
      $win.hpan.vpan paneconfigure $myTree       -stretch always
      $win.hpan.vpan paneconfigure $myReports    -stretch always
    }
  
    pack ${win}.hpan -expand true -fill both
    ${win}.hpan configure -width 800 -height 600
  
    bind ${win}.hpan <Destroy> [list destroy $win]
  }

  method browseNode {node} {
    wm state $win normal
    raise $win

    if {$node != "" && [info commands $node] != ""} {
      $myTree selected $node
    } else {
      $myTree selected [$myHtml node]
      set mySearchResults {}
    }

    set selected [$myTree selected]
    if {$selected ne ""} {
      $myEventsReport report $myHtml $selected
      $myHtmlReport   report $myHtml $selected
      $myFormsReport  report $myHtml $selected
      if {[info exists myLayoutEngineLog($selected)]} {
        $myLayoutReport report $myLayoutEngineLog($selected)
      } else {
        $myLayoutReport report ""
      }
      if {[info exists myStyleEngineLog($selected)]} {
        $myStyleReport report $myStyleEngineLog($selected)
      } else {
        $myStyleReport report ""
      }
    }
    $myTree redraw
  }

  # These are not actually part of the public interface, but they must
  # be declared public because they are invoked by widget callback scripts
  # and so on. They are really private methods.

  # HtmlDebug::rerender
  #
  #     This method is called to force the attached html widget to rerun the
  #     style and layout engines.
  #
  method rerender {} {
    set html [$myHtml html]
    set logcmd [$html cget -logcmd]
    $html configure -logcmd [mymethod Logcmd]
    unset -nocomplain myLayoutEngineLog
    unset -nocomplain myStyleEngineLog
    set selected [$myTree selected]
    $myTree selected ""
    $html _relayout
    $html _force
    after idle [list ::HtmlDebug::browse $myHtml $selected]
    $html configure -logcmd $logcmd
  }

  # Logcmd
  #
  #     This method is registered as the -logcmd callback with the widget
  #     when it is running the "Rerender with logging" function.
  #
  method Logcmd {subject message} {
    set arrayvar ""
    switch -- $subject {
      STYLEENGINE {
        set arrayvar myStyleEngineLog
      }
      LAYOUTENGINE {
        set arrayvar myLayoutEngineLog
      }
    }
    if {$arrayvar != ""} {
      if {$message == "START"} {
        #unset -nocomplain $arrayvar
        unset -nocomplain $arrayvar
      } else {
        set idx [string first " " $message]
        set node [string range $message 0 [expr $idx-1]]
        set msg  [string range $message [expr $idx+1] end]
        lappend ${arrayvar}($node) $msg
      }
    }
  }
}
#--------------------------------------------------------------------------

#--------------------------------------------------------------------------
# Document template for the debugging window report.
#
#--------------------------------------------------------------------------

proc tclInputHandler {node} {
  set widget [$node attr -default "" widget]
  if {$widget != ""} {
    $node replace $widget -configurecmd [list tclInputConfigure $widget]
  }
  set tcl [$node attr -default "" tcl]
  if {$tcl != ""} {
    eval $tcl
  }
}
proc tclInputConfigure {widget args} {
  if {[catch {set font [$widget cget -font]}]} {return 0}
  set descent [font metrics $font -descent]
  set ascent  [font metrics $font -ascent]
  set drop [expr ([winfo reqheight $widget] + $descent - $ascent) / 2]
  return $drop
}


# ::hv3::use_tclprotocol
#
# Configure the -requestcmd option of the hv3 widget to interpret
# URI's as tcl scripts.
#
proc ::hv3::use_tclprotocol {hv3} {
  $hv3 configure -requestcmd ::hv3::tclprotocol
}
proc ::hv3::tclprotocol {handle} {
  set uri [$handle cget -uri]
  set cmd [string range [$handle cget -uri] 7 end]
  $handle append [eval $cmd]
  $handle finish
}

proc prop_nodeToLabel {node} {
    if {[$node tag] == ""} {
        return [string range [string trim [$node text]] 0 20]
    }
    set d "<[$node tag]"
    foreach {a v} [$node attr] {
        set v [string map [list "\n" " "] $v]
        append d " $a=\"$v\""
    }
    append d ">"
}

proc prop_compress {props} {
    array set p $props

    set p(padding) $p(padding-top)
    if {
            $p(padding-left) != $p(padding) ||
            $p(padding-right) != $p(padding) ||
            $p(padding-bottom) != $p(padding)
    } {
        lappend p(padding) $p(padding-right) $p(padding-bottom) $p(padding-left)
    }
    unset p(padding-left)
    unset p(padding-right)
    unset p(padding-bottom)
    unset p(padding-top)

    set p(margin) $p(margin-top)
    if {
            $p(margin-left) != $p(margin) ||
            $p(margin-right) != $p(margin) ||
            $p(margin-bottom) != $p(margin)
    } {
        lappend p(margin) $p(margin-right) $p(margin-bottom) $p(margin-left)
    }
    unset p(margin-left)
    unset p(margin-right)
    unset p(margin-bottom)
    unset p(margin-top)

    foreach edge {top right bottom left} { 
        if {
            $p(border-$edge-width) != "inherit" &&
            $p(border-$edge-style) != "inherit" &&
            $p(border-$edge-color) != "inherit"
        } {
            set p(border-$edge) [list \
                $p(border-$edge-width) \
                $p(border-$edge-style) \
                $p(border-$edge-color) \
            ]
            unset p(border-$edge-width) 
            unset p(border-$edge-style) 
            unset p(border-$edge-color) 
        }
    }

    if {
        [info exists p(border-top)] &&
        [info exists p(border-bottom)] &&
        [info exists p(border-right)] &&
        [info exists p(border-left)] &&
        $p(border-top) == $p(border-right) &&
        $p(border-right) == $p(border-bottom) &&
        $p(border-bottom) == $p(border-left)
    } {
        set p(border) $p(border-top)
        unset p(border-top)
        unset p(border-left)
        unset p(border-right)
        unset p(border-bottom)
    }

    if {$p(position) ne "static"} {
        lappend p(position) $p(top)
        set v $p(top)
        if {$v ne $p(right) || $v eq $p(bottom) || $v eq $p(left)} {
            lappend p(position) $p(right)
            lappend p(position) $p(bottom)
            lappend p(position) $p(left)
        }
    }
    unset p(top) p(right) p(bottom) p(left)

    set ret [list               \
        {<b>Handling} ""        \
        display  $p(display)    \
        float    $p(float)      \
        clear    $p(clear)      \
        position $p(position)   \
        {<b>Dimensions} ""      \
        width $p(width)         \
        height $p(height)       \
        margin $p(margin)       \
        padding $p(padding)     \
        {<b>Other} ""           \
    ]

    foreach {a b} $ret { 
      unset -nocomplain p($a)
    }

    set keys [lsort [array names p]]
    foreach r $keys {
      lappend ret $r $p($r)
    }
    return $ret
}

image create photo idir -data {
    R0lGODdhEAAQAPIAAAAAAHh4eLi4uPj4APj4+P///wAAAAAAACwAAAAAEAAQAAADPVi63P4w
    LkKCtTTnUsXwQqBtAfh910UU4ugGAEucpgnLNY3Gop7folwNOBOeiEYQ0acDpp6pGAFArVqt
    hQQAO///
}
image create photo ifile -data {
    R0lGODdhEAAQAPIAAAAAAHh4eLi4uPj4+P///wAAAAAAAAAAACwAAAAAEAAQAAADPkixzPOD
    yADrWE8qC8WN0+BZAmBq1GMOqwigXFXCrGk/cxjjr27fLtout6n9eMIYMTXsFZsogXRKJf6u
    P0kCADv/
}
image create photo iopendir -data {
    R0lGODlhEAANAKIAANnZ2YSEhP///8bGxv//AAAAAP///////yH5BAEAAAAALAAAAAAQAA0A
    AANgCIqhiqDLgaIaCLoagkNDIxi6AIFCQ0M4KKpRgCFDQzg0NIQThaHLSxgVKLochRMVMkhD
    Q4M0VBFYEDKEQ0NDOFFRgCE0NEhDQ4MVBRAoNDSEQ0NRWAAYuqyFBQBYurwJADs=
}

image create photo itrash -data {
    R0lGODdhEAAQALMAAP///wAAANzc3KCgoEBAAABAQMPDw4CAgFhYWP//////////////////
    /////////ywAAAAAEAAQAAAEkxBIGYIIc84QgoCDlBBCAACAAAAExpgTjBkBABCgGAYAYAxC
    KAAAAShHiDHGOCdAIGUYY5yD0EEBAinDAEOccxAKEEgZBhjinINQgEDKMMAQ5xyEAgRShgGG
    OOcgFCCQMgwwxDkHoQCBlGGAIc45CAUIpAwDDHHOQShAIGUQYIhzDkIBAjlDEGKME8KcEwpS
    QiBnBAA7
}
image create photo iopentrash -data {
    R0lGODdhEAAQALMAAP///wAAAP//wADAwMPDw8DAAKCgoP+oWICAAFhYWICAgNzc3P//////
    /////////ywAAAAAEAAQAAAElRCAEGQIUkoJAoAihDFCCAEAAAIEQhBCSClBBAAgAEEUE8I5
    QQgBAwAABGQCISkUKAIEUgaTglIqpRQgkDKYtYhRSqUAgZTBALOUSSoFCKQMBpilTFIpQCBl
    MMAsZZJKAQIpgwFmKZNUChBIGQwwS5mkUoBAymCAWcoklQIEUoYFzFImqRQgkDOEtYxJIcw5
    oSAlBHJGADs=
}
image create photo iworld -data {
    R0lGODlhEAAQALMAANnZ2f///yCZEBBs0uz41vH558ns+tLusUWi6Ze75lWzS4rSbHLHV9Lt
    w6zapwpexSH5BAEAAAAALAAAAAAQABAAAASHEMhJqwUhhHBngEGIMUKAQE4QhCClmCECBFIG
    UQg5CKUjYAAAgDDKOQotBo8IAIAwTDsLIagWcyIAEIZBDkGEFlNOBADCMAkiiZRSTgQAgmhw
    MYTQGMKNAACAQbSl1HgvjQAABCAId5QYL6URIJAyCOFcSmOEOQEoBDHGkBDISUMIgVJKqYQR
    ADs=
}
image create photo iworldfile -data {
    R0lGODlhEAAQAMQAANnZ2f///7HM2I20w4SpvnaXtNvm7OXt9ezx9oS68FdwkcPh/l60VzKf
    LBx0xySN2vL2+r/f7a/ZnTaUkVJhe67X7uHyvqPYb0eO05PH91W0NXjEXHK5rgBLrDxDVHCb
    2yH5BAEAAAAALAAAAAAQABAAAAW3YCCO5AgAQCAMA0EQxFAURREAQDAYB3KAiGEcx5EoARAM
    x7IITOM8ApIkShAMUAQyjQRFycQYEBUEBFRNkHU9k8QMEBUEBAhhmXVNj7ZtDUQFAQE5Fcc9
    z6ZtDUQFYEBAToQ9z9M0W3NQQVAgjbRNz9N1jHOAXhAUSNVYWtN1n5MdXhAUSMVoG4NhTgJm
    hhcEBbRUXDM5WJZliBcEigEh4CGOBiJ4QVAoikJRFAV6okiBgTiSpRgCADs=
}



