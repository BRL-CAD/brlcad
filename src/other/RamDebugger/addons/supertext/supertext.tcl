# supertext.tcl v1.01
#
# Copyright (c) 1998 Bryan Oakley
# All Rights Reserved
#
# this code is freely distributable, but is provided as-is with
# no waranty expressed or implied.

# send comments to oakley@channelpoint.com

# What is this?
# 
# This is a replacement for (or superset of , or subclass of, ...) 
# the tk text widget. Its big feature is that it supports unlimited
# undo. It also has two poorly documented options: -preproc and 
# -postproc. 

# The entry point to this widget is supertext::text; it takes all of
# the same arguments as the standard text widget and exhibits all of
# the same behaviors.  The proc supertext::overrideTextCommand may be
# called to have the supertext widget be used whenever the command
# "text" is used (ie: it imports supertext::text as the command "text"). 
# Use at your own risk...

# To access the undo feature, use ".widget undo". It will undo the
# most recent insertion or deletion. On windows and the mac
# this command is bound to <Control-z>; on unix it is bound to
# <Control-_>

# if you are lucky, you might find documentation here:
# http://www1.clearlight.com/~oakley/tcl/supertext.html

#
#   RAMSAN: added option: -editable 0|1
#   RAMSAN: added option: -synctextwidget

# RAMSAN
#package provide supertext 1.01
package provide supertext 2.01

namespace eval supertext {

    variable undo
    variable undoIndex
    variable text "::text"
    variable preProc
    variable postProc
    # RAMSAN
    variable IsEditable
    variable undocallback
    variable syncvariable
    variable synctextwidget

    namespace export text
}

# this proc is probably attempting to be more clever than it should...
# When called, it will (*gasp*) rename the tk command "text" to "_text_", 
# then import our text command into the global scope. 
#
# Use at your own risk!

proc supertext::overrideTextCommand {} {
    variable text

    set text "::_text_"
    if { [info command $text] == "" } {
	rename ::text $text
	uplevel #0 namespace import supertext::text
    }
}

proc supertext::text {w args} {
    variable text
    variable undo
    variable undoIndex
    variable preProc
    variable postProc
    variable IsEditable
    variable undocallback
    variable syncvariable
    variable synctextwidget

    # this is what we will rename our widget proc to...
    set original __$w

    # do we have any of our custom options? If so, process them and 
    # strip them out before sending them to the real text command
    if {[set i [lsearch -exact $args "-preproc"]] >= 0} {
	set j [expr $i + 1]
	set preProc($original) [lindex $args $j]
	set args [lreplace $args $i $j]
    } else {
	set preProc($original) {}
    }

    if {[set i [lsearch -exact $args "-postproc"]] >= 0} {
	set j [expr $i + 1]
	set postProc($original) [lindex $args $j]
	set args [lreplace $args $i $j]
    } else {
	set postProc($original) {}
    }
    # RAMSAN
    if {[set i [lsearch -exact $args "-editable"]] >= 0} {
	set j [expr $i + 1]
	set IsEditable($original) [lindex $args $j]
	if { $IsEditable($original) != 0 && $IsEditable($original) != 1 } {
	    error "supertext::text option -editable must have argument 0 or 1"
	}
	set args [lreplace $args $i $j]
    } else {
	set IsEditable($original) 1
    }
    if {[set i [lsearch -exact $args "-undocallback"]] >= 0} {
	set j [expr $i + 1]
	set undocallback($original) [lindex $args $j]
	set args [lreplace $args $i $j]
    } else {
	set undocallback($original) {}
    }
    if {[set i [lsearch -glob $args "-syncvar*"]] >= 0} {
	set j [expr $i + 1]
	set syncvariable($original) [lindex $args $j]
	upvar \#0 [lindex $args $j] var
	if { ![info exists var] } { set var "" }
	uplevel \#0 [list trace var [lindex $args $j] w "[list supertext::fromvartotext $w] ;#"]
	set args [lreplace $args $i $j]
    } else {
	set syncvariable($original) {}
    }
    if {[set i [lsearch -glob $args "-synctext*"]] >= 0} {
	set j [expr $i + 1]
	set synctextwidget($original) [lindex $args $j]
	set args [lreplace $args $i $j]
    } else {
	set synctextwidget($original) {}
    }

    # let the text command create the widget...
    eval $text $w $args

    # now, rename the resultant widget proc so we can create our own
    rename ::$w $original

    # here's where we create our own widget proc.
    proc ::$w {command args} \
	"namespace eval supertext widgetproc $w $original \$command \$args"
    
    # set up platform-specific binding for undo; the only one I'm
    # really sure about is winders; the rest will stay the same for
    # now until someone has a better suggestion...
#     switch $::tcl_platform(platform) {
#         unix                 {event add <<Undo>> <Control-z>}
#         windows         {event add <<Undo>> <Control-z>}
#         macintosh         {event add <<Undo>> <Control-z>}
#     }
#     bind $w <<Undo>> "$w undo"

    set undo($original)        {}
    set undoIndex($original) -1
    set clones($original) {}

    return $w
}

# this is the command that we associate with a supertext widget. 
proc supertext::widgetproc {this w command args} {

    variable undo
    variable undoIndex
    variable preProc
    variable postProc
    # RAMSAN
    variable IsEditable
    variable undocallback
    variable syncvariable
    variable synctextwidget

    set NeedsUndoCallback 0

    # these will be the arguments to the pre and post procs
    set originalCommand $command
    set originalArgs $args

    # if the command is "undo", we need to morph it into the appropriate
    # command for undoing the last item on the stack
    if {$command == "undo"} {

	if {$undoIndex($w) == ""} {
	    # ie: last command was anything _but_ an undo...
	    set undoIndex($w) [expr [llength $undo($w)] -1]
	}

	# if the index is pointing to a valid list element, 
	# lets undo it...
	if {$undoIndex($w) < 0} {
	    # nothing to undo...
	    if { [info exists undocallback($w)] && $undocallback($w) != "" } {
		if {[catch "$undocallback($w)" error]} {
		    return -code error -errorinfo $::errorInfo \
		       "error during processing of -undocallback: $error"
		}
	    } else { bell }
	} else {
	    
	    # data is a list comprised of a command token
	    # (i=insert, d=delete) and parameters related 
	    # to that token
	    set data [lindex $undo($w) $undoIndex($w)]

	    if {[lindex $data 0] == "d"} {
		set command "delete"
	    } else {
		set command "insert"
	    }
	    set args [lrange $data 1 end]

	    # adjust the index
	    incr undoIndex($w) -1
	    
	    if { $undoIndex($w) < 0 } { set NeedsUndoCallback 1 }
	}
    }
    
    
    # is there a pre-proc? If so, run it. If there is a problem,
    # die. This is potentially bad, because once there is a problem
    # in a preproc the user must fix the preproc -- there is no
    # way to unconfigure the preproc. Oh well. The other choice
    # is to ignore errors, but then how will the caller know if
    # the proc fails?
    if {[info exists preProc($w)] && $preProc($w) != ""} {
	if {[catch "$preProc($w) $command $args" error]} {
	    return -code error  -errorinfo $::errorInfo \
	       "error during processing of -preproc: $error"
	}
    }

    # now, process the command (either the original one, or the morphed
    # undo command
    switch -glob -- $command {
	original {
	    # RAMSAN
	    set result supertext::$w
	}
	clearundo {
	    set undoIndex($w) ""
	    set undo($w) ""
	    set result {}
	}
	cget {
	    # RAMSAN
	    set option [lindex $args 0]
	    if {$option == "-preproc"} {
		set result $preProc($w)
		
	    } elseif {$option == "-postproc"} {
		set result $postProc($w)
		
	    } elseif {$option == "-editable"} {
		# RAMSAN
		set result $IsEditable($w)
	    } elseif {$option == "-undocallback"} {
		# RAMSAN
		set result $undocallback($w)
		
	    } elseif { [string match -syncvar* $option]} {
		# RAMSAN
		set result $syncvariable($w)
	    } elseif { [string match -synctext* $option]} {
		# RAMSAN
		set result $synctextwidget($w)
	    } else {
		if {[catch "$w $command $args" result]} {
		    regsub $w $result $this result
		    return -code error -errorinfo $::errorInfo \
		       $result
		}
	    }
	}
	conf* {
	    # we have to deal with configure specially, since the
	    # user could try to configure the -preproc or -postproc
	    # options...
	    
	    if {[llength $args] == 0} {
		# first, the case where they just type "configure"; lets 
		# get it out of the way
		set list [$w configure]
		lappend list [list -preproc preproc Preproc {} $preProc($w)]
		lappend list [list -postproc postproc Postproc {} $postProc($w)]
		# RAMSAN
		lappend list [list -editable editable Editable {} $IsEditable($w)]
		lappend list [list -undocallback undocallback Undocallback {} $undocallback($w)]
		lappend list [list -syncvariable syncvariable Syncvariable {} $syncvariable($w)]
		lappend list [list -synctextwidget synctextwidget Synctextwidget {} $synctextwidget($w)]
		set result $list
	    } elseif {[llength $args] == 1} {
		# this means they are wanting specific configuration 
		# information
		set option [lindex $args 0]
		if {$option == "-preproc"} {
		    set result [list -preproc preproc Preproc {} $preProc($w)]

		} elseif {$option == "-postproc"} {
		    set result [list -postproc postproc Postproc {} $postProc($w)]
		    
		} elseif {$option == "-editable"} {
		    # RAMSAN
		    set result [list -editable editable Editable {} $IsEditable($w)]
		} elseif {$option == "-undocallback"} {
		    # RAMSAN
		    set result [list -undocallback undocallback Undocallback {} $undocallback($w)]
		} elseif { [string match -syncvar* $option] } {
		    # RAMSAN
		    set result [list -syncvariable syncvariable Syncvariable {} $syncvariable($w)]
		} elseif { [string match -syncvar* $option] } {
		    # RAMSAN
		    set result [list -synctextwidget synctextwidget Synctextwidget {} $synctextwidget($w)]
		} else {
		    if {[catch "$w $command $args" result]} {
		        regsub $w $result $this result
		        return -code error -errorinfo $::errorInfo $result
		    }
		}

	    } else {
		# ok, the user is actually configuring something... 
		# we'll deal with our special options first
		if {[set i [lsearch -exact $args "-preproc"]] >= 0} {
		    set j [expr $i + 1]
		    set preProc($w) [lindex $args $j]
		    set args [lreplace $args $i $j]
		    set result {}
		}

		if {[set i [lsearch -exact $args "-postproc"]] >= 0} {
		    set j [expr $i + 1]
		    set postProc($w) [lindex $args $j]
		    set args [lreplace $args $i $j]
		    set result {}
		}
		# RAMSAN
		if {[set i [lsearch -exact $args "-editable"]] >= 0} {
		    set j [expr $i + 1]
		    set IsEditable($w) [lindex $args $j]
		    if { $IsEditable($w) != 0 && $IsEditable($w) != 1 } {
		        error "supertext::text option -editable must have argument 0 or 1"
		    }
		    set args [lreplace $args $i $j]
		    set result {}
		}
		if {[set i [lsearch -exact $args "-undocallback"]] >= 0} {
		    set j [expr $i + 1]
		    set undocallback($w) [lindex $args $j]
		    set args [lreplace $args $i $j]
		    set result {}
		}
		if {[set i [lsearch -glob $args "-syncvar*"]] >= 0} {
		    set j [expr $i + 1]
		    set syncvariable($w) [lindex $args $j]
		    upvar \#0 [lindex $args $j] var
		    if { ![info exists var] } { set var "" }
		    uplevel \#0 [list trace var [lindex $args $j] w \
		            "[list supertext::fromvartotext $this] ;#"]
		    set args [lreplace $args $i $j]
		    set result {}
		}
		if {[set i [lsearch -glob $args "-synctext*"]] >= 0} {
		    set j [expr $i + 1]
		    set synctextwidget($w) [lindex $args $j]
		    set args [lreplace $args $i $j]
		    set result {}
		}
		# now, process any remaining args
		if {[llength $args] > 0} {
		    if {[catch "$w $command $args" result]} {
		        regsub $w $result $this result
		        return -code error -errorinfo $::errorInfo $result
		    }
		}
	    }
	}

	undo {
	    # if an undo command makes it to here, that means there 
	    # wasn't anything to undo; this effectively becomes a
	    # no-op
	    set result {}
	}

	ins* {
	    # RAMSAN
	    if { !$IsEditable($w) } { return }

	    if {[catch {set index  [text_index $w [lindex $args 0]]}]} {
		set index [lindex $args 0]
	    }

	    # since the insert command can have an arbitrary number
	    # of strings and possibly tags, we need to ferret that out
	    # now... what a pain!
	    set myargs [lrange $args 1 end]
	    set length 0
	    while {[llength $myargs] > 0} {
		incr length [string length [lindex $myargs 0]]
		if {[llength $myargs] > 1} {
		    # we have a tag...
		    set myargs [lrange $myargs 2 end]
		} else {
		    set myargs [lrange $myargs 1 end]
		}
	    }

	    # now, let the real widget command do the dirty work
	    # of inserting the text. If we fail, do some munging 
	    # of the error message so the right widget name appears...
	    if {[catch "$w $command $args" result]} {
		regsub $w $result $this result
		return -code error -errorinfo $::errorInfo $result
	    }

	    if { $synctextwidget($w) != "" } {
		set state [$synctextwidget($w) cget -state]
		$synctextwidget($w) configure -state normal
		eval $synctextwidget($w) $command $index [lrange $args 1 end]
		$synctextwidget($w) configure -state $state
	    }

	    # we need this for the undo stack; index2 couldn't be
	    # computed until after we inserted the data...
	    set index2 [text_index $w "$index + $length chars"]

	    if {$originalCommand == "undo"} {
		# let's do a "see" so what we just did is visible;
		# also, we'll move the insertion cursor to the end
		# of what we just did...
		$w see $index2
		$w mark set insert $index2
		
	    } else {
		# since the original command wasn't undo, we need
		# to reset the undoIndex. This means that the next
		# time an undo is called for we'll start at the 
		# end of the stack
		set undoIndex($w) ""
	    }

	    # add a delete command on the undo stack.
	    set w_old ""
	    foreach "w_old idx1_old idx2_old" [lindex $undo($w) end] break
	    if { $w_old == "d" && ![regexp {\s} [$w get $index $index2]] && \
		$idx2_old == $index } {
		set undo($w) [lreplace $undo($w) end end "d $idx1_old $index2"]
	    } else {
		lappend undo($w) "d $index $index2"
	    }
	}

	del* {
	    # RAMSAN
	    if { !$IsEditable($w) } { return }

	    # this converts the insertion index into an absolute address
	    set index [text_index $w [lindex $args 0]]

	    # lets get the data we are about to delete; we'll need
	    # it to be able to undo it (obviously. Duh.)
	    set data [eval $w get $args]

	    # add an insert on the undo stack
	    set w_old ""
	    foreach "w_old idx_old data_old" [lindex $undo($w) end] break
	    if { $w_old == "i" && ![regexp {\s} $data] && \
		$idx_old == [text_index $w $index+[string length $data]c] } {
		set undo($w) [lreplace $undo($w) end end [list "i" $index $data$data_old]]
	    } else {
		lappend undo($w) [list "i" $index $data]
	    }

	    if {$originalCommand == "undo"} {
		# let's do a "see" so what we just did is visible;
		# also, we'll move the insertion cursor to a suitable
		# spot
		$w see $index
		$w mark set insert $index

	    } else {
		# since the original command wasn't undo, we need
		# to reset the undoIndex. This means that the next
		# time an undo is called for we'll start at the 
		# end of the stack
		set undoIndex($w) ""
	    }

	    # let the real widget command do the actual deletion. If
	    # we fail, do some munging of the error message so the right
	    # widget name appears...

	    if { $synctextwidget($w) != "" } {
		set rargs ""
		foreach i $args { lappend rargs [$w index $i] }
	    }

	    if {[catch "$w $command $args" result]} {
		regsub $w $result $this result
		return -code error -errorinfo $::errorInfo $result
	    }
	    if { $synctextwidget($w) != "" } {
		set state [$synctextwidget($w) cget -state]
		$synctextwidget($w) configure -state normal
		eval $synctextwidget($w) $command $rargs
		$synctextwidget($w) configure -state $state
	    }
	}
	
	default {
	    # if the command wasn't one of the special commands above,
	    # just pass it on to the real widget command as-is. If
	    # we fail, do some munging of the error message so the right
	    # widget name appears...
	    if {[catch "$w $command $args" result]} {
		regsub $w $result $this result
		return -code error -errorinfo $::errorInfo $result
	    }
	}
    }

    # is there a post-proc? If so, run it. 
    if {[info exists postProc($w)] && $postProc($w) != ""} {
	if {[catch "$postProc($w) $command $args" error]} {
	    return -code error -errorinfo $::errorInfo "error during processing of -postproc: $error"
	}
    }

    if { [regexp {^(ins|del)} $command] && $syncvariable($w) != "" } { fromtexttovar $this }

    if { $NeedsUndoCallback && [info exists undocallback($w)] && $undocallback($w) != "" } {
	if {[catch "$undocallback($w)" error]} {
	    return -code error -errorinfo $::errorInfo \
	    "error during processing of -undocallback: $error"
	}
    }

    return $result
}

# this returns a normalized index (ie: line.column), with special
# handling for the index "end"; to undo something we pretty much
# _have_ to have a precise row and column number.
proc supertext::text_index {w i} {
    if {$i == "end"} {
	set index [$w index "end-1c"]
    } else {
	set index [$w index $i]
    }

    return $index
}


proc supertext::fromvartotext { w } {
    variable syncvariable

    set original __$w
    if { ![winfo exists $w] } { return }
    upvar \#0 $syncvariable($original) var

    set data [$w get 1.0 end-1c]

    if { $var != $data } {
	set varname $syncvariable($original)
	set syncvariable($original) ""

	$w del 1.0 end
	$w ins end $var

	set syncvariable($original) $varname
    }

}

proc supertext::fromtexttovar { w } {
    variable syncvariable

    set original __$w
    upvar \#0 $syncvariable($original) var

    set var [$w get 1.0 end-1c]
}