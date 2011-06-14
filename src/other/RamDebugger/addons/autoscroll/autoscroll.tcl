# autoscroll.tcl --
#
#       Package to create scroll bars that automatically appear when
#       a window is too small to display its content.
#
# Copyright (c) 2003    Kevin B Kenny <kennykb@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id$

package require Tk
package provide autoscroll 1.1

namespace eval ::autoscroll {
    namespace export autoscroll unautoscroll
    bind Autoscroll <Destroy> [namespace code [list destroyed %W]]
    bind Autoscroll <Map> [namespace code [list map %W]]
}

 #----------------------------------------------------------------------
 #
 # ::autoscroll::autoscroll --
 #
 #       Create a scroll bar that disappears when it is not needed, and
 #       reappears when it is.
 #
 # Parameters:
 #       w    -- Path name of the scroll bar, which should already exist
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       The widget command is renamed, so that the 'set' command can
 #       be intercepted and determine whether the widget should appear.
 #       In addition, the 'Autoscroll' bind tag is added to the widget,
 #       so that the <Destroy> event can be intercepted.
 #
 #----------------------------------------------------------------------

proc ::autoscroll::autoscroll { w } {
    if { [info commands ::autoscroll::renamed$w] != "" } { return $w }
    rename $w ::autoscroll::renamed$w
    interp alias {} ::$w {} ::autoscroll::widgetCommand $w
    bindtags $w [linsert [bindtags $w] 1 Autoscroll]
    eval [list ::$w set] [renamed$w get]
    return $w
}

 #----------------------------------------------------------------------
 #
 # ::autoscroll::unautoscroll --
 #
 #       Return a scrollbar to its normal static behavior by removing
 #       it from the control of this package.
 #
 # Parameters:
 #       w    -- Path name of the scroll bar, which must have previously
 #               had ::autoscroll::autoscroll called on it.
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       The widget command is renamed to its original name. The widget
 #       is mapped if it was not currently displayed. The widgets
 #       bindtags are returned to their original state. Internal memory
 #       is cleaned up.
 #
 #----------------------------------------------------------------------

proc ::autoscroll::unautoscroll { w } {
    if { [info commands ::autoscroll::renamed$w] != "" } {
        variable grid
        rename ::$w {}
        rename ::autoscroll::renamed$w ::$w
        if { [set i [lsearch -exact [bindtags $w] Autoscroll]] > -1 } {
            bindtags $w [lreplace [bindtags $w] $i $i]
        }
        if { [info exists grid($w)] } {
            eval [join $grid($w) \;]
            unset grid($w)
        }
    }
}

 #----------------------------------------------------------------------
 #
 # ::autoscroll::widgetCommand --
 #
 #       Widget command on an 'autoscroll' scrollbar
 #
 # Parameters:
 #       w       -- Path name of the scroll bar
 #       command -- Widget command being executed
 #       args    -- Arguments to the commane
 #
 # Results:
 #       Returns whatever the widget command returns
 #
 # Side effects:
 #       Has whatever side effects the widget command has.  In
 #       addition, the 'set' widget command is handled specially,
 #       by gridding/packing the scroll bar according to whether
 #       it is required.
 #
 #------------------------------------------------------------

proc ::autoscroll::widgetCommand { w command args } {
    variable grid
    if { $command == "set" } {
        foreach { min max } $args {}
        if { $min <= 0 && $max >= 1 } {
            switch -exact -- [winfo manager $w] {
                grid {
                    lappend grid($w) "[list grid $w] [grid info $w]"
                    grid forget $w
                }
                pack {
                    foreach x [pack slaves [winfo parent $w]] {
                        lappend grid($w) "[list pack $x] [pack info $x]"
                    }
                    pack forget $w
                }
            }
        } elseif { [info exists grid($w)] } {
            eval [join $grid($w) \;]
            unset grid($w)
        }
    }
    return [eval [list renamed$w $command] $args]
}


 #----------------------------------------------------------------------
 #
 # ::autoscroll::destroyed --
 #
 #       Callback executed when an automatic scroll bar is destroyed.
 #
 # Parameters:
 #       w -- Path name of the scroll bar
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Cleans up internal memory.
 #
 #----------------------------------------------------------------------

proc ::autoscroll::destroyed { w } {
    variable grid
    catch { unset grid($w) }
    rename ::$w {}
}


 #----------------------------------------------------------------------
 #
 # ::autoscroll::map --
 #
 #       Callback executed when an automatic scroll bar is mapped.
 #
 # Parameters:
 #       w -- Path name of the scroll bar.
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       Geometry of the scroll bar's top-level window is constrained.
 #
 # This procedure keeps the top-level window associated with an
 # automatic scroll bar from being resized automatically after the
 # scroll bar is mapped.  This effect avoids a potential endless loop
 # in the case where the resize of the top-level window resizes the
 # widget being scrolled, causing the scroll bar no longer to be needed.
 #
 #----------------------------------------------------------------------

proc ::autoscroll::map { w } {
    wm geometry [winfo toplevel $w] [wm geometry [winfo toplevel $w]]
}

 #----------------------------------------------------------------------
 #
 # ::autoscroll::wrap --
 #
 #       Arrange for all new scrollbars to be automatically autoscrolled
 #
 # Parameters:
 #       None.
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       ::scrollbar is overloaded to automatically autoscroll any new
 #          scrollbars.
 #
 #----------------------------------------------------------------------

proc ::autoscroll::wrap {} {
    if {[info commands ::autoscroll::_scrollbar] != ""} {return}
    rename ::scrollbar ::autoscroll::_scrollbar
    proc ::scrollbar {w args} {
        eval ::autoscroll::_scrollbar [list $w] $args
        ::autoscroll::autoscroll $w
        return $w
    }
}

 #----------------------------------------------------------------------
 #
 # ::autoscroll::unwrap --
 #
 #       Turns off automatic autoscrolling of new scrollbars. Does not
 #         effect existing scrollbars.
 #
 # Parameters:
 #       None.
 #
 # Results:
 #       None.
 #
 # Side effects:
 #       ::scrollbar is returned to its original state
 #
 #----------------------------------------------------------------------

proc ::autoscroll::unwrap {} {
    if {[info commands ::autoscroll::_scrollbar] == ""} {return}
    rename ::scrollbar {}
    rename ::autoscroll::_scrollbar ::scrollbar
}
