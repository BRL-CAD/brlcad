# from https://github.com/tcltk/tclapps/blob/master/apps/tkchat/mousewheel.tcl
# see tip 171 for origin
# added protections checking if Tk is present so we can run pkgIndex on this
# file with tclsh

package provide mousewheel 0.1

if {![catch {package present Tk}]} {

    bind Text <MouseWheel> {} 

    proc ::tk::MouseWheel {wFired X Y D {shifted 0}} {
    # Set event to check based on call
	set evt "<[expr {$shifted?{Shift-}:{}}]MouseWheel>"
	# do not double-fire in case the class already has a binding
	if {[bind [winfo class $wFired] $evt] ne ""} { return }
	# obtain the window the mouse is over
	set w [winfo containing $X $Y]
	# if we are outside the app, try and scroll the focus widget
	if {![winfo exists $w]} { catch {set w [focus]} }
	if {[winfo exists $w]} {
	    if {[bind $w $evt] ne ""} {
	    # Awkward ... this widget has a MouseWheel binding, but to
	    # trigger successfully in it, we must give it focus.
		catch {focus} old
		if {$w ne $old} { focus $w }
		event generate $w $evt -rootx $X -rooty $Y -delta $D
		if {$w ne $old} { focus $old }
		return
	    }
	    # aqua and x11/win32 have different delta handling
	    if {[tk windowingsystem] ne "aqua"} {
		set delta [expr {- ($D / 30)}]
	    } else {
		set delta [expr {- ($D)}]
	    }
	    # scrollbars have different call conventions
	    if {[string match "*Scrollbar" [winfo class $w]]} {
		catch {tk::ScrollByUnits $w \
	 [string index [$w cget -orient] 0] $delta}
	    } else {
		set cmd [list $w [expr {$shifted ? "xview" : "yview"}] \
		    scroll $delta units]
		    # Walking up to find the proper widget handles cases like
		    # embedded widgets in a canvas
		while {[catch $cmd] && [winfo toplevel $w] ne $w} {
		    set w [winfo parent $w]
		}
	    }
	}
    }

    bind all <MouseWheel> [list ::tk::MouseWheel %W %X %Y %D 0]
    bind all <Shift-MouseWheel> [list ::tk::MouseWheel %W %X %Y %D 1]
    if {[tk windowingsystem] eq "x11"} {
    # Support for mousewheels on Linux/Unix commonly comes through
    # mapping the wheel to the extended buttons.
	bind all <4> [list ::tk::MouseWheel %W %X %Y 120]
	bind all <5> [list ::tk::MouseWheel %W %X %Y -120]
    }

}

# mode: tcl
# tab-width: 8
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
