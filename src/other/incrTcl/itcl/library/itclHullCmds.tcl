#
# itclHullCmds.tcl
# ----------------------------------------------------------------------
# Invoked automatically upon startup to customize the interpreter
# for [incr Tcl] when one of setupcomponent or createhull is called.
# ----------------------------------------------------------------------
#   AUTHOR:  Arnulf P. Wiedemann
#
#      RCS:  $Id$
# ----------------------------------------------------------------------
#            Copyright (c) 2008  Arnulf P. Wiedemann
# ======================================================================
# See the file "license.terms" for information on usage and
# redistribution of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tk 8.6

namespace eval ::itcl::internal::commands {

# ======================= widgetDeleted ===========================

proc widgetDeleted {oldName newName op} {
    # The widget is beeing deleted, so we have to delete the object
    # which had the widget as itcl_hull too!
    # We have to get the real name from for example
    # ::itcl::internal::widgets::hull1.lw
    # we need only .lw here

#puts stderr "widgetDeleted!$oldName!$newName!$op!"
    set cmdName [namespace tail $oldName]
    set flds [split $cmdName {.}]
    set cmdName .[join [lrange $flds 1 end] {.}]
#puts stderr "DELWIDGET![namespace current]!$cmdName![::info command $cmdName]!"
    rename $cmdName {}
}

}

namespace eval ::itcl::builtin {

# ======================= createhull ===========================

proc createhull {widget_type path args} {
    variable hullCount
    upvar this this
    upvar win win


#puts stderr "createhull!$widget_type!$path!$args!$this![::info command $this]!"
#puts stderr "ns1![uplevel 1 namespace current]!"
#puts stderr "ns2![uplevel 2 namespace current]!"
#puts stderr "ns3![uplevel 3 namespace current]!"
#puts stderr "level-1![::info level -1]!"
#puts stderr "level-2![::info level -2]!"
    set my_this [namespace tail $this]
    set tmp $my_this
#puts stderr "II![::info command $this]![::info command $tmp]!"
#puts stderr "rename1!rename $my_this ${tmp}_!"
    rename ::$my_this ${tmp}_
    set options [list]
    foreach {option_name value} $args {
        switch -glob -- $option_name {
	-class {
	      lappend options $option_name [namespace tail $value]
	  }
        -* {
            lappend options $option_name $value
          }
        default {
	    return -code error "bad option name\"$option_name\" options must start with a \"-\""
          }
        }
    }
    set my_win [namespace tail $path]
#puts stderr "my_win!$my_win!"
    set cmd [list $widget_type $my_win]
    if {[llength $options] > 0} {
        lappend cmd {*}$options
    }
    set widget [uplevel 1 $cmd]
    trace add command $widget delete ::itcl::internal::commands::widgetDeleted
    set opts [uplevel 1 info delegated options]
    foreach entry $opts {
        foreach {optName compName} $entry break
	if {$compName eq "itcl_hull"} {
	    set optInfos [uplevel 1 info delegated option $optName]
	    set realOptName [lindex $optInfos 4]
	    # strip off the "-" at the beginning
	    set myOptName [string range $realOptName 1 end]
            set my_opt_val [option get $my_win $myOptName *]
            if {$my_opt_val ne ""} {
                $my_win configure -$myOptName $my_opt_val
            }
	}
    }
    set idx 1
    while {1} {
        set widgetName ::itcl::internal::widgets::hull${idx}$my_win
	if {[string length [::info command $widgetName]] == 0} {
	    break
	}
        incr idx
    }
#puts stderr "rename2!rename $tmp $widgetName!"
    set dorename 0
    rename $tmp $widgetName
#puts stderr "rename3!rename ${tmp}_ $tmp![::info command ${tmp}_]!"
    rename ${tmp}_ ::$tmp
    set exists [uplevel 1 ::info exists itcl_hull]
    if {!$exists} {
	# that does not yet work, beacause of problems with resolving 
        ::itcl::addcomponent $my_this itcl_hull
    }
    upvar itcl_hull itcl_hull
    ::itcl::setcomponent $my_this itcl_hull $widgetName
#puts stderr "IC![::info command $my_win]!"
    set win $my_win
    set exists [uplevel 1 ::info exists itcl_interior]
    if {!$exists} {
	# that does not yet work, beacause of problems with resolving 
        ::itcl::addcomponent $this itcl_interior
    }
    upvar itcl_interior itcl_interior
    set itcl_interior $my_win
    return $my_win
}

# ======================= setupcomponent ===========================

proc setupcomponent {comp using widget_type path args} {
    upvar this this

#puts stderr "setupcomponent!$comp!$widget_type!$path!$args!$this!"
#puts stderr "ns1![uplevel 1 namespace current]!"
#puts stderr "ns2![uplevel 2 namespace current]!"
#puts stderr "ns3![uplevel 3 namespace current]!"
    set options [list]
    foreach {option_name value} $args {
        switch -glob -- $option_name {
        -* {
            lappend options $option_name $value
          }
        default {
	    return -code error "bad option name\"$option_name\" options must start with a \"-\""
          }
        }
    }
    set cmd [list $widget_type $path]
    if {[llength $options] > 0} {
        lappend cmd {*}$options
    }
#puts stderr "cmd0![::info command $widget_type]!$path![::info command $path]!"
#puts stderr "cmd1!$cmd!"
#    set my_comp [uplevel 3 $cmd]
    set my_comp [uplevel #0 $cmd]
#puts stderr 111![::info command $path]!
    ::itcl::setcomponent $this $comp $my_comp
    set opts [uplevel 1 info delegated options]
    foreach entry $opts {
        foreach {optName compName} $entry break
	if {$compName eq $my_comp} {
	    set optInfos [uplevel 1 info delegated option $optName]
	    set realOptName [lindex $optInfos 4]
	    # strip off the "-" at the beginning
	    set myOptName [string range $realOptName 1 end]
            set my_opt_val [option get $my_win $myOptName *]
            if {$my_opt_val ne ""} {
                $my_comp configure -$myOptName $my_opt_val
            }
	}
    }
#puts stderr END!$path![::info command $path]!
}

# ======================= setupcomponent ===========================

proc initoptions {args} {
    upvar win win

    if {[llength $args]} {
        set argsDict [dict create {*}$args]
    } else {
        set argsDict [dict create]
    }
    set myOptions [uplevel 1 info option]
#    set myOptions [uplevel 1 info options]
    set my_class [uplevel 1 info class]
    set myDelegatedOptions [uplevel 1 info delegated options]
    set opt_lst [list configure]
    foreach opt $myOptions {
       set my_win $win
       if {![catch {
           set resource [uplevel 1 info option $opt -resource]
           set class [uplevel 1 info option $opt -class]
       } msg]} {
           set val [uplevel #0 ::option get $my_win $resource $class]
           if {![::dict exists $argsDict $opt]} {
	       if {[string length $val] > 0} {
	           uplevel 1 set itcl_options($opt) $val
	       }
	   }
           set val [uplevel 1 set itcl_options($opt)]
	   # FIXME temporary catch as we get all options instead of the
	   # ones for the class only
           catch {uplevel 1 configure $opt $val}
#	   lappend opt_lst $opt $val
       }
    }
#    uplevel 1 $opt_lst
}

}
