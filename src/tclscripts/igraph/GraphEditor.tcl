#             G R A P H E D I T O R . T C L
# BRL-CAD
#
# Copyright (c) 2012-2014 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
# Description -
#
# This is the GraphEditor object script. It generates a hierarchical view of
# geometry database objects.
#

package require Itcl
package require Itk
package require Iwidgets

package provide GraphEditor 1.0

class GraphEditor {
    inherit itk::Toplevel

    constructor {} {}
    destructor {}

    public {
	method clearDisplay {} {}
	method autosizeDisplay {} {}
	method zoomDisplay { { zoom "in" } } {}

	method renderPreview { {rtoptions "-P4 -R -B" } } {}

	method toggleAutosizing { { state "" } } {}
	method toggleAutorender { { state "" } } {}

	method toggleDebug {} {}
    }

    protected {
	# menu toggle options
	variable _autoRender

	# hooks to the hierarchy pop-up menus
	variable _itemMenu
	variable _bgMenu
    }

    private {
	variable _debug

	# track certain menu items that are dynamic
	variable _setcolorItemMenuIndex
	variable _autosizeItemMenuIndex
	variable _autosizeBgMenuIndex
	variable _autorenderItemMenuIndex
	variable _autorenderBgMenuIndex

	# port used for preview fbserv rendering
	variable _fbservPort
	variable _weStartedFbserv

	# save the mged framebuffer name
	variable _mgedFramebufferId

	method rgbToHex { { rgb "0 0 0" } } {}
	method checkAutoRender {} {}
    }
}


###########
# begin constructor/destructor
###########

body GraphEditor::constructor {} {
    # used to determine the mged port number
    global port
    global mged_players

    # set to 1/0 to turn call path debug messages on/off
    set _debug 0

    set _autoRender 0
    set _popup ""

    # pick some randomly high port for preview rendering and hope it, or one of
    # its neighbors is open.
    if [ catch { set port } _fbservPort ] {
	set _fbservPort 0
    }

    set _weStartedFbserv 0

    # set the window title
    $this configure -title "Graph Editor"

    itk_component add menubar {
	menu $itk_interior.menubar
    }

    menu $itk_interior.menubar.close -title "Close"
    $itk_interior.menubar.close add command -label "Close" -underline 0 -command ""

    # set up the adjustable sliding pane with a left and right side
    itk_component add pw_pane {
	panedwindow $itk_interior.pw_pane -orient vertical -height 512 -width 256
    }

    $itk_interior.pw_pane add left -margin 5
    $itk_interior.pw_pane add right -margin 5 -minimum 5
    # XXX keep the left side hidden away for now...
    $itk_interior.pw_pane hide right

    itk_component add cadtree {
	Hierarchy $itk_interior.cadtree \
	    -markforeground blue \
	    -markbackground red   \
	    -selectforeground black \
	    -selectbackground yellow \
	    -visibleitems 20x40 \
	    -alwaysquery 1
    }

    itk_component add cv {
	::iwidgets::scrolledcanvas $itk_interior.cv \
	    -textbackground gray \
	    -autoresize 0 \
	    -width 800 -height 400 \
	    -vscrollmode static \
	    -hscrollmode static
    } {
	keep -xscrollincrement
	keep -xscrollincrement
	keep -selectbackground -selectforeground
	keep -width -height
    }

    pack $itk_interior.cv -padx 0 -pady 0 -expand yes -fill both

    # save hooks to the cadtree pop-up menus for efficiency and convenience
    set _itemMenu [ $itk_interior.cadtree component itemMenu ]
    set _bgMenu [ $itk_interior.cadtree component bgMenu ]

    $_bgMenu add separator

    $_bgMenu add command \
	-label [ $this toggleAutosizing same ] \
	-command "[ code $this toggleAutosizing  ]"
    # save the index of this menu entry so we may modify its label later
    set _autosizeBgMenuIndex [ $_bgMenu index end ]

    $_bgMenu add command \
	-label [ $this toggleAutorender same ] \
	-command "[ code $this toggleAutorender  ]"
    # save the index of this menu entry so we may modify its label later
    set _autorenderBgMenuIndex [ $_bgMenu index end ]

    grid rowconfigure $itk_interior 0 -weight 1
    grid columnconfigure $itk_interior 0 -weight 1

    bind $_itemMenu <ButtonRelease> {::tk::MenuInvoke %W 1}
    bind $_bgMenu <ButtonRelease> {::tk::MenuInvoke %W 1}

    # get objects' names, types and positions within the graph
    set graphViewCommand "graph show"

    if { [ catch $graphViewCommand positions error] } {
	# check if there is an error saying that Adaptagrams isn't available
	if { [ string first "graph : ERROR This command is disabled due to the absence of Adaptagrams library" $::errorInfo ] != -1 } {
	    # get the name of the calling procedure
	    set calling_proc [lindex [split [info level [expr [info level] - 1]]] 0]

	    # Return an error message to the calling procedure
	    error "Error: $calling_proc : ERROR This command is disabled due to the absence of Adaptagrams library"
	} elseif { [ string first "invalid command name \"graph\"" $::errorInfo ] != -1 } {
	    # get the name of the calling procedure
	    set calling_proc [lindex [split [info level [expr [info level] - 1]]] 0]

	    #check if the name of the command has the "::" characters before it; if yes, erase them
	    if { [ string first "::" $calling_proc ] != -1 } {
		#string replace $calling_proc 0 1 "" calling_proc
		set calling_proc [string trimleft $calling_proc :]
	    }

	    # Return an error message to the calling procedure
	    error "Error: $calling_proc : ERROR This command is not available yet in Archer"
	} elseif { [ string first "A database is not open!" $::errorInfo ] != -1 } {
	    error "Error: A database is not open!"
	}
    }

    # set constant values for the following type of objects: primitive, combination and something else
    set primitive 1
    set combination 2

    set i 0
    set look_for_edges 0
    foreach element $positions {
	if { [string equal $element edge] } {
	    if { $look_for_edges == 1} {
		# construct the polyline that defines the connection between two nodes
		$itk_interior.cv create line $points \
		-width 0.05 \
		-arrow first
	    }
	    set points []
	    set look_for_edges 1
	    set i 0
	    continue
	}

	if { $look_for_edges == 1 } {
	    lappend points $element
	    incr i
	} else {
	    if { $i == 0 } {
		# set the name of the object
		set name $element
		incr i
	    } elseif { $i == 1 } {
		# set the color depending on the type of object: primitive / combination / something else
		if { [string equal $element $primitive] } {
		    set color red
		} elseif { [string equal $element $combination] } {
		    set color green
		} else {
		    set color yellow
		}
		incr i
	    } elseif { $i == 2 } {
		# set the x coordinate for the lower left corner
		set x1 $element
		incr i
	    } elseif { $i == 3 } {
		# set the y coordinate for the lower left corner
		set y1 $element
		incr i
	    } elseif { $i == 4 } {
		# set the x coordinate for the upper right corner
		set x2 $element
		incr i
	    } else {
		# set the y coordinate for the upper right corner
		set y2 $element
		set i 0

		# draw a rectangle for this object within the graph
		$itk_interior.cv create rectangle $x1 $y1 $x2 $y2 \
		-fill $color \
		-tags rectangle

		# write the object's name inside the rectangle
		$itk_interior.cv create text $x1 $y1 \
		-anchor nw \
		-text $name
	    }
	}
    }
}


body GraphEditor::destructor {} {
    if { $_debug } {
	puts "destructor"
    }

    # destroy the framebuffer, if we opened it
    if { $_weStartedFbserv } {
	puts "cleaning up fbserv"
	set fbfree [file join [bu_brlcad_root "bin"] fbfree]
	if { [ catch { exec $fbfree -F $_fbservPort } error ] } {
	    puts $error
	    puts "Unable to properly clean up after our fbserv"
	}
    }
}

###########
# end constructor/destructor
###########


###########
# begin public methods
###########

# clearDisplay
#
# simply clears any displayed nodes from the graphics window
#
body GraphEditor::clearDisplay {} {
    if { $_debug } {
	puts "clearDisplay"
    }

    # verbosely describe what we are doing
    puts "Z"
    set retval [ Z ]
    $this checkAutoRender
    return $retval
}


# autosizeDisplay
#
# auto-fits the currently displayed objects to the current view size
#
body GraphEditor::autosizeDisplay {} {
    if { $_debug } {
	puts "autosizeDisplay"
    }

    # verbosely describe what we are doing
    puts "autoview"
    set retval [ autoview ]
    $this checkAutoRender
    return $retval
}


# zoomDisplay
#
# zooms the display in or out either in jumps via the keywords "in" and "out"
# or via a specified amount.
#
body GraphEditor::zoomDisplay { { zoom "in"} } {
    if { $_debug } {
	puts "zoomDisplay $zoom"
    }

    if { [ string compare $zoom "" ] == 0 } {
	set zoom "in"
    }

    if { [ string compare $zoom "in" ] == 0 } {
	puts "zoomin"
	return [ zoomin ]
    } elseif { [ string compare $zoom "out" ] == 0 } {
	puts "zoomout"
	set retval [ zoomout ]
	$this checkAutoRender
	return $retval
    }


    # XXX we do not check if the value is a valid number, we should
    puts "zoom $zoom"
    set retval [ zoom $zoom ]
    $this checkAutoRender
    return $retval
}


# renderPreview
#
# generates a small preview image of what geometry is presently displayed to a
# small temporary framebuffer
#
body GraphEditor::renderPreview { { rtoptions "-P4 -R -B" } } {

    # mged provides the port number it has available
    global port
    global mged_gui

    set size 128
    set device /dev/X
    set rgb "255 255 255"
    set rtrun ""
    set fbserv [file join [bu_brlcad_root "bin"] fbserv]
    set fbfree [file join [bu_brlcad_root "bin"] fbfree]
    set fbline [file join [bu_brlcad_root "bin"] fbline]
    set fbclear [file join [bu_brlcad_root "bin"] fbclear]

    # see if we can try to use the mged graphics window instead of firing up our own framebuffer
    set useMgedWindow 0
    if { [ set mged_gui($_mgedFramebufferId,fb) ] == 1 } {
	if { [ set mged_gui($_mgedFramebufferId,listen) ] == 1 } {
	    set useMgedWindow 1
	}
    }

    if { $_debug } {
	if { $useMgedWindow } {
	    puts "Using MGED Graphics Window for raytrace"
	} else {
	    puts "Attempting to use our own fbserv"
	}
    }

    if { $useMgedWindow } {

	# if we previously started up a framebuffer, shut it down
	if { $_weStartedFbserv } {
	    puts "cleaning up fbserv"
	    if { [ catch { exec $fbfree -F $_fbservPort } error ] } {
		puts $error
		puts "Unable to properly clean up after our fbserv"
	    }
	    set _weStartedFbserv 0
	}

	# if we got here, then we should be able to attach to a running framebuffer.
	scan $rgb {%d %d %d} r g b

	# make sure we are using the mged framebuffer port
	set _fbservPort [ set port ]

	# get the image dimensions of the mged graphics window
	set windowSize [ rt_set_fb_size $_mgedFramebufferId ]
	set windowWidth [ lindex [ split $windowSize x ] 0 ]
	set windowHeight [ lindex [ split $windowSize x ] 1 ]

	# scale the image to the proportional window dimensions so that the raytrace
	# "matches" the proportion size of the window.
	if { $windowWidth > $windowHeight } {
	    set scaledHeight [ expr round(double($size) / double($windowWidth) * double($windowHeight)) ]
	    set rtrun "rt [ split $rtoptions ] -F $_fbservPort -w$size -n$scaledHeight -C$r/$g/$b"
	} else {
	    set scaledWidth [ expr round(double($size) / double($windowHeight) * double($windowWidth)) ]
	    set rtrun "rt [ split $rtoptions ] -F $_fbservPort -w$scaledWidth -n$size -C$r/$g/$b"
	}

    } else {
	# cannot use mged graphics window for whatever reason, so make do.
	# try to fire up our own framebuffer.

	# see if there is an fbserv running
	if { [ catch { exec $fbclear -c -F $_fbservPort -s$size $rgb } error ] } {

	    if { $_debug } {
		puts "$error"
	    }

	    # failed, so try to start one
	    puts "exec $fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
	    exec $fbserv -S $size -p $_fbservPort -F $device > /dev/null &
	    exec sleep 1

	    # keep track of the fact that we started this fbserv, so we have to clean up
	    set _weStartedFbserv 1

	    # try again
	    if { [ catch { exec $fbclear -c -F $_fbservPort $rgb } error ] } {

		if { $_debug } {
		    puts "$error"
		}

		# try the next port
		incr _fbservPort

		# still failing, try to start one again
		puts "exec $fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
		exec $fbserv -S $size -p $_fbservPort -F $device > /dev/null &
		exec sleep 1

		# try again
		if { [ catch { exec $fbclear -c -F $_fbservPort $rgb } error ] } {

		    if { $_debug } {
			puts "$error"
		    }

		    incr _fbservPort

		    # still failing, try to start one again
		    puts "exec $fbserv -S $size -p $_fbservPort -F $device > /dev/null &"
		    exec $fbserv -S $size $_fbservPort $device > /dev/null &
		    exec sleep 1

		    # last try
		    if { [ catch { exec $fbclear -c -F $_fbservPort $rgb } error ] } {
			# strike three, give up!
			puts $error
			puts "Unable to attach to a framebuffer"

			# guess we did not really get to start it
			set _weStartedFbserv 0

			return
		    }
		}
		# end try three
	    }
	    # end try two
	}
	# end try one

	# write out some status lines for status-rendering cheesily via exec.
	# these hang for some reason if sent to the mged graphics window through
	# mged (though they work fine via command-line).
	exec $fbline -F $_fbservPort -r 255 -g 0 -b 0 [expr $size - 1] 0 [expr $size - 1] [expr $size - 1]
	exec $fbline -F $_fbservPort -r 0 -g 255 -b 0 [expr $size - 2] 0 [expr $size - 2] [expr $size - 1]
	exec $fbline -F $_fbservPort -r 0 -g 0 -b 255 [expr $size - 3] 0 [expr $size - 3] [expr $size - 1]

	# if we got here, then we were able to attach to a running framebuffer..
	scan $rgb {%d %d %d} r g b
	set rtrun "rt [ split $rtoptions ] -F $_fbservPort -s$size -C$r/$g/$b"
    }
    # end check for using mged graphics window or using fbserv

    puts "$rtrun"
    return [ eval $rtrun ]
}


# toggleAutosizing
#
# makes it so that the view is either auto-sizing or not.
# XXX if the menu entries get more complex than what is already below (8 references),
# it should really be reorganized.
#
body GraphEditor::toggleAutosizing { { state "" } } {
    if { $_debug } {
	puts "toggleAutosizing $state"
    }

    global autosize

    # handle different states
    if { [ string compare $state "same" ] == 0 } {
	# this one is used by the constructor to set the label to the current state
	# we return the current state as the label
	if [info exists autosize] {
	    if { [ set autosize ] == 0 } {
		return "Turn Autosizing On"
	    }
	}
	return "Turn Autosizing Off"

    } elseif { [ string compare $state "off" ] == 0 } {
	# specifically turning the thing off
	$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing On"
	puts "set autosize 0"
	return [ set autosize 0 ]
    } elseif { [ string compare $state "on" ] == 0 } {
	$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing Off"
	puts "set autosize 1"
	return [ set autosize 1 ]
    }

    if { [ set autosize ] == 0 } {
	$_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing Off"
	puts "set autosize 1"
	return [ set autosize 1 ]
    }

    $_bgMenu entryconfigure $_autosizeBgMenuIndex -label "Turn Autosizing On"
    puts "set autosize 0"
    return [ set autosize 0 ]
}


# toggleAutorender
#
# makes it so that the view is either auto-sizing or not.
# XXX if the menu entries get more complex than what is already below (8 references),
# it should really be reorganized.
#
body GraphEditor::toggleAutorender { { state "" } } {
    if { $_debug } {
	puts "toggleAutorender $state"
    }

    # handle different states
    if { [ string compare $state "same" ] == 0 } {
	# this one is used by the constructor to set the label to the current state
	# we return the current state as the label
	if { [ set _autoRender ] == 0 } {
	    return "Turn Auto-Render On"
	}
	return "Turn Auto-Render Off"

    } elseif { [ string compare $state "off" ] == 0 } {
	# specifically turning the thing off
	$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render On"
	puts "set _autoRender 0"
	set retval [ set _autoRender 0 ]
	$this checkAutoRender
	return $retval

    } elseif { [ string compare $state "on" ] == 0 } {
	$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render Off"
	puts "set _autoRender 1"
	set retval [ set _autoRender 1 ]
	$this checkAutoRender
	return $retval
    }

    # we are not handling a specific request, so really toggle whatever the real
    # value is.

    if { [ set _autoRender ] == 0 } {
	$_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render Off"
	puts "set _autoRender 1"
	set retval [ set _autoRender 1 ]
	$this checkAutoRender
	return $retval
    }

    $_bgMenu entryconfigure $_autorenderBgMenuIndex -label "Turn Auto-Render On"
    puts "set _autoRender 0"
    set retval [ set _autoRender 0 ]
    $this checkAutoRender
    return $retval
}

# toggleDebug
#
# turns debugging on/off
#
body GraphEditor::toggleDebug { } {
    if { $_debug } {
	set _debug 0
    } else {
	set _debug 1
    }
}

##########
# end public methods
##########


##########
# begin private methods
##########

# rgbToHex
#
# takes an rgb triplet string as input and returns an html-style
# color entity that tcl understands.
#
body GraphEditor::rgbToHex { { rgb "0 0 0" } } {
    if { $_debug } {
	puts "rgbToHex $rgb"
    }

    if { [ string compare $rgb "" ] == 0 } {
	set rgb "0 0 0"
    }

    scan $rgb {%d %d %d} r g b

    # clamp the values between 0 and 255
    if { [ expr $r > 255 ] } {
	set r 255
    } elseif { [ expr $r < 0 ] } {
	set r 0
    }
    if { [ expr $g > 255 ] } {
	set g 255
    } elseif { [ expr $g < 0 ] } {
	set g 0
    }
    if { [ expr $b > 255 ] } {
	set b 255
    } elseif { [ expr $b < 0 ] } {
	set b 0
    }

    if [ catch { format {#%02x%02x%02x} $r $g $b } color ] {
	puts $color
	return "#000000"
    }

    return $color
}


# checkAutoRender
#
# simply checks if a render needs to occur automatically
#
body GraphEditor::checkAutoRender {} {
    if { $_debug } {
	puts "checkAutoRender"
    }

    if { $_autoRender == 1 } {
	$this renderPreview
    }

    return
}

##########
# end private methods
##########

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
