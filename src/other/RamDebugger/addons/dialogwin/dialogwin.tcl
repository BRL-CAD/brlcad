
package require Tcl 8.5
package require Tk 8.5

# to auto load from GiD
catch {
    auto_load msgcat::mc
    msgcat::mc ""
}
package require msgcat
package require snit

namespace eval ::dialogwinmsgs {
    ::msgcat::mcload [file join [file dirname [info script]] msgs]

}

if { [info commands _] eq "" } {
    proc _ { args } {
	if { [regexp {(.*)\#C\#.*} [lindex $args 0] {} str] } {
	    set args [lreplace $args 0 0 $str]
	}
	return [uplevel 1 ::msgcat::mc $args]
    }
}

if { [info procs GetImage] == "" || ![info exists ::GIDDEFAULT] } {
    #using without GiD GetImage not exists, then return always dialogwinquestionhead
    image create photo dialogwinquestionhead -data {
	R0lGODlhKAAoAKUAAPHY8+y9+dmX1bxLt7EMprATqsIXtsE0tdaGz+bK2s94yt4UxeUNzNgK
	wcgEscQLsbEDocQjtOElzfEb2PMW1OQczMF5uuOr2dssyfw06vUk3M1ZvsScwMWrttFIwaw2
	qfY86tG4vvLW1uHV1NrExrSSpMw+uPhH79GYwftV+t8zyPXj5bRqqPxo/NxcyPos5cBYsN6j
	zuG42Nw+xMSivNx4xOxC3L8rr8xkvORM0MlrueBkyLxDtP///////////yH+FUNyZWF0ZWQg
	d2l0aCBUaGUgR0lNUAAh+QQBCgA/ACwAAAAAKAAoAAAG/sCfcEgsGo/IpHLJbDqf0Kj0CAhY
	AdOmYEDodguGAyKRNSoM6IWa0Wg4HhDIgFwORAwSyYRSoTAmDAtsDnEWWRcGGBkaExIbFhwd
	HRwKHoJuEB9SFxEgIIwbIQkJIiMiCSQhJSaDmVAJEScnjCgAIre4uAkdA36EA08mJymMCj+3
	tremuCMdGL4QKE0IICkZFCorK8srCSGiuQkse24ETSotJ3sux7c/CR4YGBbbuBwU+A0QhkoC
	Jy0v8JngtkLFHgoe2t0KwcfPgwJLPLRIMWHPBBgdQpDYgK8iB4UrYuDLB0GGkhktQByk0EiF
	HnwqaKzI9aMGvj4MHhhDksAG/sCOI4N6IDET1w8ZfW5SWAAjiYx/GlgmnephxExTK37EkDBy
	T4UFB5IIANGiItCREhIUdYfA4p+RFW4kiWGDGL4/XhEqFFEzKEu8GsIikfGCIsuVe3D8WPbj
	wsGkBzW8SIgEQIQUATu+pTBjGd8ciC1SeJFBcZJOLyoyYMmaAguNJFBM6NMHUMcXIAQo0YH7
	8J7NfDBwDWpxjwZPWZ0aWNS14h5Hlab+vZ2C3ZIDkn8fZKCiAwkSNFgk9U1Bw7ALTGQ8YGT2
	sG5lHUxMP1xthxMcDczirWBVl46OFhVmAxZOfJBfaxOEsNYKONhW0QuyoAfFDQ6s9tYGCWC1
	AiOHTkGYmxQr8FAhAyRSsEFGMczQkWQZqCDhFAgQ0EaJwFXEiAcElrGCDg840EYDbJC4QAMe
	xFDGERfocMAdBdywAQI5HinllFRWaeWVWB4RBAA7
    }
    proc GetImage { filename } {        
	return dialogwinquestionhead        
    }   
}

if { [info commands tkTabToWindow] == "" } {
    interp alias "" tkTabToWindow "" tk::TabToWindow
    #::tk::unsupported::ExposePrivateCommand tkTabToWindow
}
if { [info commands tkButtonInvoke] == "" } {
    interp alias "" tkButtonInvoke "" tk::ButtonInvoke
    #::tk::unsupported::ExposePrivateCommand tkButtonInvoke
}

package provide dialogwin 1.12

################################################################################
#  This software is copyrighted by Ramon Ribó (RAMSAN) ramsan@cimne.upc.es.
#  (http://gid.cimne.upc.es/ramsan) The following terms apply to all files
#  associated with the software unless explicitly disclaimed in individual files.

#  The authors hereby grant permission to use, copy, modify, distribute,
#  and license this software and its documentation for any purpose, provided
#  that existing copyright notices are retained in all copies and that this
#  notice is included verbatim in any distributions. No written agreement,
#  license, or royalty fee is required for any of the authorized uses.
#  Modifications to this software may be copyrighted by their authors
#  and need not follow the licensing terms described here, provided that
#  the new terms are clearly indicated on the first page of each file where
#  they apply.

#  IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
#  FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
#  ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
#  DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

#  THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
#  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
#  IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
#  NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
#  MODIFICATIONS.
################################################################################

if { [info commands CCGetRGB] eq "" } {
    proc CCGetRGB { w color} {
	set ret $color
	set n [ scan $color \#%2x%2x%2x r g b]
	if { $n != 3} {
	    set rgb [ winfo rgb $w $color]
	    set r [ expr int( [ lindex $rgb 0]/256.0)]
	    set g [ expr int( [ lindex $rgb 1]/256.0)]
	    set b [ expr int( [ lindex $rgb 2]/256.0)]
	    set ret [ format \#%02x%02x%02x $r $g $b]
	}
	return $ret
    }
}


if { [info commands CCColorActivo] eq "" } {
    proc CCColorActivo { color_usuario { factor 17} } {
	set ret ""
	set color_nuevo [ CCGetRGB . $color_usuario]
	set n [ scan $color_nuevo \#%2x%2x%2x r g b]
	if { $n == 3} {
	    set r [ expr $r + $factor]
	    if { $r > 255} { set r 255}
	    set g [ expr $g + $factor]
	    if { $g > 255} { set g 255}
	    set b [ expr $b + $factor]
	    if { $b > 255} { set b 255}
	    set ret [ format \#%2x%2x%2x $r $g $b]
	}
	return $ret
    }
}

namespace eval DialogWin {
    variable w
    variable action
    variable user
    variable oldGrab
    variable grabStatus
    variable grab
}

#current styles are:
#        ridgeframe
#        separator
#
proc DialogWin::Init { winparent title style { morebuttons "" } { OKname "" } { Cancelname "" } } {
    variable action
    variable w
    variable grab

    set grab 1
    if { [string match *nograb $style] } { set grab 0 }

    if { $winparent == "." } { set winparent "" }
    set w $winparent.__dialogwin

    catch { destroy $w }

#     set i 0
#     while { [winfo exists $w] } {
#         incr i
#         set w $winparent.__dialogwin$i
#     }
    toplevel $w
    if { $::tcl_platform(platform) == "windows" } {       
	wm attributes $w -toolwindow 1
    }
    wm title $w $title
    wm withdraw $w

    switch $style {
	ridgeframe {
	    frame $w.f -relief ridge -bd 2
	    frame $w.buts
	    grid $w.f -sticky ewns -padx 2 -pady 2
	    grid $w.buts -sticky ew
	    if { $::tcl_version >= 8.5 } { grid anchor $w.buts center }
	}
	separator - separator_nograb {
	    frame $w.f -bd 0
	    frame $w.sep -bd 2 -relief raised -height 2
	    frame $w.buts
	    grid $w.f -sticky ewns -padx 2 -pady 2
	    grid $w.sep -sticky ew
	    grid $w.buts -sticky ew
	    if { $::tcl_version >= 8.5 } { grid anchor $w.buts center }
	}
	default {
	    error "error: only accepted styles ridgeframe, separator_nograb and separator"
	}
    }
    $w.buts conf -bg [CCColorActivo [$w  cget -bg]]

    if { $OKname == "" } {
	set OKname [_ OK]
    }
    if { $Cancelname != "" } {
	set CancelName $Cancelname
    } elseif { $OKname == "-" } {
	set CancelName [_ Close]
    } else {
	set CancelName [_ Cancel]
    }

    set butwidth 7
    if { [string length $OKname] > $butwidth } { set butwidth [string length $OKname] }
    if { [string length $CancelName] > $butwidth } { set butwidth [string length $CancelName] }
    foreach i $morebuttons {
	if { [string length $i] > $butwidth } { set butwidth [string length $i] }
    }

    set usedletters [string tolower [string index $OKname 0]]
    if { [string tolower [string index $CancelName 0]] != $usedletters } {
	lappend usedletters [string tolower [string index $CancelName 0]]
	set underlinecancel 0
    } else {
	lappend usedletters [string tolower [string index $CancelName 1]]
	set underlinecancel 1
    }

    if { $OKname != "-" } {
	button $w.buts.ok -text $OKname -width $butwidth -und 0 -command \
		[namespace code "set action 1"]
    }

    set togrid ""
    if { $morebuttons != "" } {
	set iaction 2
	foreach i $morebuttons {
	    for { set ipos 0 } { $ipos < [string length $i] } { incr ipos } {
		set letter [string tolower [string index $i $ipos]]
		if { [regexp {[a-zA-Z]} $letter] && [lsearch $usedletters $letter] == -1 } {
		    break
		}
	    }
	    if { $ipos < [string length $i] } {
		button $w.buts.b$iaction -text $i -width $butwidth -und $ipos \
		        -command [namespace code "set action $iaction"]
		bind $w <Alt-$letter> \
		        "tkButtonInvoke $w.buts.b$iaction"
		bind $w.buts.b$iaction <Return> "tkButtonInvoke $w.buts.b$iaction"
		lappend usedletters [string tolower [string index $i $ipos]]
	    } else {
		button $w.buts.b$iaction -text $i -width $butwidth  \
		        -command [namespace code "set action $iaction"]
	    }
	    lappend togrid $w.buts.b$iaction
	    incr iaction
	}
    }
    if { $Cancelname != "-" } {
	button $w.buts.cancel -text $CancelName -width $butwidth -und $underlinecancel -command \
	[namespace code "set action 0"]
    }

    if { $OKname != "-" } {
	set togrid "$w.buts.ok $togrid"
    }
    if { $Cancelname != "-" } {
	set togrid "$togrid $w.buts.cancel"
    }
    eval grid $togrid -padx 2 -pady 4

    if { $OKname != "-" } {
	bind $w.buts.ok <Return> "tkButtonInvoke $w.buts.ok"
	catch {
	    bind $w <Alt-[string tolower [string index $OKname 0]]> "tkButtonInvoke $w.buts.ok"
	}
	focus $w.buts.ok
    } elseif { $Cancelname != "-" } {
	focus $w.buts.cancel
    }

    if { $Cancelname != "-" } {
	bind $w <Escape> "tkButtonInvoke $w.buts.cancel"
	bind $w.buts.cancel <Return> "tkButtonInvoke $w.buts.cancel"
	catch {
	    bind $w <Alt-[string tolower [string index $CancelName $underlinecancel]]> \
		"tkButtonInvoke $w.buts.cancel"
	}
	wm protocol $w WM_DELETE_WINDOW "tkButtonInvoke $w.buts.cancel"
    } else {
	bind $w <Escape> [namespace code "set action -1"]
	wm protocol $w WM_DELETE_WINDOW [namespace code "set action -1"]
    }

    grid columnconf $w 0 -weight 1
    grid rowconf $w 0 -weight 1

    return $w.f
}

proc DialogWin::InvokeOK { { visible 1 } } {
    variable w

    if { ![winfo exists $w.buts.ok] } { return }

    if { $visible } {
	tkButtonInvoke $w.buts.ok
    } else {
	$w.buts.ok invoke
    }
}

proc DialogWin::InvokeCancel { { visible 1 } } {
    variable w

    if { ![winfo exists $w.buts.cancel] } { return }

    if { $visible } {
	tkButtonInvoke $w.buts.cancel
    } else {
	$w.buts.cancel invoke
    }
}

proc DialogWin::FocusCancel {} {
    variable w
    focus $w.buts.cancel
}

proc DialogWin::InvokeButton { num { visible 1 } } {
    variable w

    if { ![winfo exists $w.buts] } { return }

    if { $num < 2 } {
	WarnWin "DialogWin::InvokeButton num>2"
	return
    }
    foreach i [winfo children $w.buts] {
	if { [regexp "\\m$num\\M" [$i cget -command]] } {
	    if { $visible } {
		tkButtonInvoke $i
	    } else {
		$i invoke
	    }
	    return
	}
    }
    WarnWin "DialogWin::InvokeButton num bad"
}

proc DialogWin::FocusButton { num } {
    variable w

    if { $num < 2 } {
	WarnWin "DialogWin::FocusButton num>2"
	return
    }
    foreach i [winfo children $w.buts] {
	if { [regexp "\\m$num\\M" [$i cget -command]] } {
	    focus $i
	    return
	}
    }
    WarnWin "DialogWin::FocusButton num bad"
}

proc DialogWin::CreateWindow { { geom "" } { minwidth "" } { minheight "" } } {
    CreateWindowNoWait $geom $minwidth $minheight
    return [WaitForWindow 0]
}

proc DialogWin::CreateWindowNoWait { { geom "" } { minwidth "" } { minheight "" } } {
    variable w
    variable grab
    variable oldGrab
    variable grabStatus

    set top [winfo toplevel [winfo parent $w]]

    wm withdraw $w
    update idletasks

    if { $geom != "" } {
	wm geom $w $geom
    } else {
	if { $minwidth != "" && [winfo reqwidth $w] < $minwidth } {
	    set width $minwidth
	} else { set width [winfo reqwidth $w] }
	if { $minheight != "" && [winfo reqheight $w] < $minheight } {
	    set height $minheight
	} else { set height [winfo reqheight $w] }

	if { [wm state $top] == "withdrawn" } {
	    set x [expr [winfo screenwidth $top]/2-$width/2]
	    set y [expr [winfo screenheight $top]/2-$height/2]
	} else {
	    set x [expr [winfo x $top]+[winfo width $top]/2-$width/2]
	    set y [expr [winfo y $top]+[winfo height $top]/2-$height/2]
	}
	if { $x < 0 } { set x 0 }
	if { $y < 0 } { set y 0 }

	wm geom $w ${width}x${height}+${x}+$y
    }
    wm deiconify $w
    update idletasks
    wm geom $w [wm geom $w]
    focus $w
    set oldGrab [grab current .]
    if {[string compare $oldGrab ""]} {
	set grabStatus [grab status $oldGrab]
	grab release $oldGrab
    }
    if { $grab } { grab $w }
}

proc DialogWin::WaitForWindow { { raise "" } } {
    variable action
    variable w

    if { $raise == "" } {
	# this is to avoid the 2 second problem in KDE 2
	if { $::tcl_platform(platform) == "windows" } {
	    set raise 1
	} else { set raise 0 }
    }
    if { $raise } {
	raise [winfo toplevel $w]
    }
    vwait [namespace which -variable action]
    return $action
}

proc DialogWin::DestroyWindow {} {
    variable w
    variable oldGrab
    variable grabStatus

    if {[string compare $oldGrab ""]} {
	if {[string compare $grabStatus "global"]} {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab $oldGrab }
	} else {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab -global $oldGrab }
	}
    }
    destroy $w
    set w ""
}

# NOTE: initial value of variables is not transferred
proc CopyNamespace { nfrom nto } {

    set comm "namespace eval $nto {\n"
    foreach i [info vars ${nfrom}::*] {
	append comm "variable [namespace tail $i]\n"
    }
    foreach i [info commands ${nfrom}::*] {
	set args ""
	foreach j [info args $i] {
	    if { [info default $i $j kk] } {
		lappend args [list $j $kk]
	    } else {
		lappend args $j
	    }
	}
	append comm "proc [namespace tail $i] { $args } {\n[info body $i]\n}\n"
    }

    append comm "}"
    eval $comm
}

namespace eval DialogWinTop {
    variable user
    variable nameprefix __
}

proc DialogWinTop::SetNamePrefix { prefix } {
    variable nameprefix $prefix
}

# command for OK is first; for cancel is last
proc DialogWinTop::Init { winparent title style commands { morebuttons "" } { OKname "" } \
    { Cancelname "" } } {
    variable nameprefix

    if { $winparent == "." } { set winparent "" }
    set w $winparent.${nameprefix}dialogwintop
    set i 0
    while { [winfo exists $w] } {
	incr i
	set w $winparent.${nameprefix}dialogwintop$i
    }
    toplevel $w
    if { $::tcl_platform(platform) == "windows" } {
	wm attributes $w -toolwindow 1
    }
    wm title $w $title

    switch $style {
	ridgeframe {
	    frame $w.f -relief ridge -bd 2
	    frame $w.buts
	    grid $w.f -sticky ewns -padx 2 -pady 2
	    grid $w.buts -sticky ew
	    if { $::tcl_version >= 8.5 } { grid anchor $w.buts center }
	}
	separator {
	    frame $w.f -bd 0
	    frame $w.sep -bd 2 -relief raised -height 2
	    frame $w.buts
	    grid $w.f -sticky ewns -padx 2 -pady 2
	    grid $w.sep -sticky ew
	    grid $w.buts -sticky ew
	    if { $::tcl_version >= 8.5 } { grid anchor $w.buts center }
	}
	default {
	    error "error: only accepted styles ridgeframe and separator"
	}
    }

    $w.buts conf -bg [CCColorActivo [$w  cget -bg]]

    if { $OKname == "" } {
	set OKname [_ OK]
    }
    if { $Cancelname != "" } {
	set CancelName $Cancelname
    } elseif { $OKname == "-" } {
	set CancelName [_ Close]
    } else {
	set CancelName [_ Cancel]
    }

    set butwidth 7
    if { [string length $OKname] > $butwidth } { set butwidth [string length $OKname] }
    if { [string length $CancelName] > $butwidth } { set butwidth [string length $CancelName] }
    foreach i $morebuttons {
	if { [string length $i] > $butwidth } { set butwidth [string length $i] }
    }

    set usedletters [list [string tolower [string index $OKname 0]]]
    if { [string tolower [string index $CancelName 0]] != $usedletters } {
	lappend usedletters [string tolower [string index $CancelName 0]]
	set underlinecancel 0
    } else {
	lappend usedletters [string tolower [string index $CancelName 1]]
	set underlinecancel 1
    }

    set icomm 0
    if { $OKname != "-" } {
	button $w.buts.ok -text $OKname -width $butwidth -und 0 -command \
	    "[lindex $commands 0] $w.f"
	incr icomm
    }
    set letterbindings ""
    set togrid ""
    if { $morebuttons != "" } {
	foreach i $morebuttons {
	    for { set ipos 0 } { $ipos < [string length $i] } { incr ipos } {
		set letter [string tolower [string index $i $ipos]]
		if { [regexp {[a-zA-Z]} $letter] && [lsearch $usedletters $letter] == -1 } {
		    break
		}
	    }
	    if { $ipos < [string length $i] } {
		button $w.buts.b$icomm -text $i -width $butwidth -und $ipos \
		        -command "[lindex $commands $icomm] $w.f"
		set letter [string tolower [string index $i $ipos]]
		bind $w <Alt-$letter> "tkButtonInvoke $w.buts.b$icomm"
		lappend letterbindings $letter "tkButtonInvoke $w.buts.b$icomm"
		bind $w.buts.b$icomm <Return> "tkButtonInvoke $w.buts.b$icomm"
		lappend usedletters [string tolower [string index $i $ipos]]
	    } else {
		button $w.buts.b$icomm -text $i -width $butwidth  \
		        -command "[lindex $commands $icomm] $w.f"
	    }
	    lappend togrid $w.buts.b$icomm
	    incr icomm
	}
    }
    if { $Cancelname != "-" } {
	button $w.buts.cancel -text $CancelName -width $butwidth -und $underlinecancel -command \
	    "[lindex $commands $icomm] $w.f"
    }
     if { $OKname != "-" } {
	set togrid "$w.buts.ok $togrid"
    }
    if { $Cancelname != "-" } {
	set togrid "$togrid $w.buts.cancel"
    }
    eval grid $togrid -padx 2 -pady 4


    if { $OKname != "-" } {
	bind $w.buts.ok <Return> "tkButtonInvoke $w.buts.ok"
	set letter [string tolower [string index $OKname 0]]
	catch {
	    bind $w <Alt-$letter> "tkButtonInvoke $w.buts.ok"
	    lappend letterbindings $letter "tkButtonInvoke $w.buts.ok"
	}
	focus $w.buts.ok
    } elseif { $Cancelname != "-" } {
	focus $w.buts.cancel
    }

    if { $Cancelname != "-" } {
	bind $w <Escape> "tkButtonInvoke $w.buts.cancel"
	bind $w.buts.cancel <Return> "tkButtonInvoke $w.buts.cancel"

	set letter [string tolower [string index $CancelName $underlinecancel]]
	catch {
	    bind $w <Alt-$letter> "tkButtonInvoke $w.buts.cancel"
	    lappend letterbindings $letter "tkButtonInvoke $w.buts.cancel"
	}
	wm protocol $w WM_DELETE_WINDOW "tkButtonInvoke $w.buts.cancel"
    } elseif { $OKname != "-" } {
	bind $w <Escape> "tkButtonInvoke $w.buts.ok"
	wm protocol $w WM_DELETE_WINDOW "tkButtonInvoke $w.buts.ok"
    } else {
	bind $w <Escape> "tkButtonInvoke $w.buts.b0"
	wm protocol $w WM_DELETE_WINDOW "tkButtonInvoke $w.buts.b0"
    }

     bind $w <Destroy> {
	 if { [winfo class %W] == "Toplevel" } {
	      DialogWinTop::DestroyWindow %W
	 }
    }

    foreach but [winfo children $w.buts] {
	foreach "letter command" $letterbindings {
	    bind $but <KeyPress-$letter> $command
	}
    }

    grid columnconf $w 0 -weight 1
    grid rowconf $w 0 -weight 1

    return $w.f
}

proc DialogWinTop::DestroyWindow { w } {
    variable oldGrab
    variable grabStatus
    variable taborder

    if { [info exists taborder($w)] } { unset taborder($w) }

    if {[string compare $oldGrab ""]} {
	if {[string compare $grabStatus "global"]} {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab $oldGrab }
	} else {
	    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab -global $oldGrab }
	}
    }
}

proc lreverse L {
    set res {}
    set i [llength $L]
    while {$i} {lappend res [lindex $L [incr i -1]]}
    set res
 }

proc DialogWinTop::TabOrderPrevNext { w what } {
    variable taborder

    set tabo $taborder([winfo toplevel $w])
    set ipos [lsearch $tabo $w]
    set tabo [concat [lrange $tabo [expr {$ipos+1}] end] [lrange $tabo 0 [expr {$ipos-1}]]]
    if { $what eq "prev" } { set tabo [lreverse $tabo] }

    foreach w $tabo {
	if { [tk::FocusOK $w] } {
	    tk::TabToWindow $w
	    return
	}
    }
    return
}

proc DialogWinTop::SetTabOrder { winlist } {
    variable taborder

    set top [winfo toplevel [lindex $winlist 0]]
    set taborder($top) $winlist

    foreach w $winlist {
	bind $w <Tab> "DialogWinTop::TabOrderPrevNext $w next; break"
	bind $w <<PrevWindow>> "DialogWinTop::TabOrderPrevNext $w prev; break"
    }
}

proc DialogWinTop::InvokeOK { f } {

    if { ![winfo exists $f] } { return }

    set w [winfo toplevel $f]
    tkButtonInvoke $w.buts.ok
}

proc DialogWinTop::InvokeCancel { f { visible 1 } } {
    variable w

    if { ![winfo exists $f] } { return }

    set w [winfo toplevel $f]
    if { $visible } {
	tkButtonInvoke $w.buts.cancel
    } else {
	$w.buts.cancel invoke
    }
}

proc DialogWinTop::InvokeButton { f num { visible 1 } } {

    if { ![winfo exists $f] } { return }

    set w [winfo toplevel $f]
    if { $num < 2 } {
	WarnWin "DialogWinTop::InvokeButton num>2"
	return
    }
    if { $visible } {
	tkButtonInvoke $w.buts.b[expr $num-1]
    } else {
	$w.buts.b[expr $num-1] invoke
    }
}

proc DialogWinTop::FocusButton { f num } {

    set w [winfo toplevel $f]
    if { $num < 2 } {
	WarnWin "DialogWinTop::FocusButton num>2"
	return
    }
    foreach i [winfo children $w.buts] {
	if { [regexp "\\m$num\\M" [$i cget -command]] } {
	    focus $i
	    return
	}
    }
    WarnWin "DialogWinTop::FocusButton num bad"
}

# what= 0 disable ; =1 enable
proc DialogWinTop::EnableDisableButton { f name what } {
    variable w

    set w [winfo toplevel $f]
    foreach i [winfo children $w.buts] {
	if { $name == [$i cget -text] } {
	    switch $what {
		1 { $i conf -state normal }
		0 { $i conf -state disabled }
	    }
	    return
	}
    }
    WarnWin "DialogWin::EnableDisableButton name bad"
}

proc DialogWinTop::CreateWindow { f { geom "" } { minwidth "" } { minheight "" } { grab 0 } } {
    variable oldGrab
    variable grabStatus


    set w [winfo parent $f]
    set top [winfo toplevel [winfo parent $w]]

    wm withdraw $w
    update idletasks

    if { $geom != "" } {
	wm geom $w $geom
    } else {
	if { $minwidth != "" && [winfo reqwidth $w] < $minwidth } {
	    set width $minwidth
	} else { set width [winfo reqwidth $w] }
	if { $minheight != "" && [winfo reqheight $w] < $minheight } {
	    set height $minheight
	} else { set height [winfo reqheight $w] }

	if { [wm state $top] == "withdrawn" } {
	    set x [expr [winfo screenwidth $top]/2-$width/2]
	    set y [expr [winfo screenheight $top]/2-$height/2]
	} else {
	    set x [expr [winfo x $top]+[winfo width $top]/2-$width/2]
	    set y [expr [winfo y $top]+[winfo height $top]/2-$height/2]
	}
	if { $x < 0 } { set x 0 }
	if { $y < 0 } { set y 0 }
	wm geom $w ${width}x${height}+${x}+$y
    }
    wm deiconify $w
    update idletasks
    #wm geom $w [wm geom $w]
    if {!$grab } {
	set oldGrab ""
	set grabStatus ""
    } else {
	set oldGrab [grab current $w]
	if {[string compare $oldGrab ""]} {
	    set grabStatus [grab status $oldGrab]
	    grab release $oldGrab
	}
	grab $w
    }
    focus $w
}

# proc DialogWin::messageBox { args } {
#     if { [info exists DialogWin2::w] } {
#         after 500 [list DialogWin::messageBox $args]
#         return
#     }
#     CopyNamespace ::DialogWin ::DialogWin2

#     array set opts [list -default "" -icon question-32.png -message "" -parent . -title "" \
#         -type ok]

#     for { set i 0 } { $i < [llength $args] } { incr i } {
#         set opt [lindex $args $i]
#         if { ![info exists opts($opt)] } {
#             error "unknown option '$opt' in DialogWin::messageBox"
#         }
#         incr i
#         set opts($opt) [lindex $args $i]
#     }
#     switch -- $opts(-type) {
#         abortretryignore {
#             set buts [list Abort Retry Ignore]
#         }
#         ok {
#             set buts [list OK]
#         }
#         okcancel {
#             set buts [list OK Cancel]
#         }
#         retrycancel {
#             set buts [list Retry Cancel]
#         }
#         yesno {
#             set buts [list Yes No]
#         }
#         yesnocancel {
#             set buts [list Yes No Cancel]
#         }
#         default {
#             error "unknown type: '$opts(-type)' in DialogWin::messageBox"
#         }
#     }
#     if { $opts(-default) == "" } {
#         set opts(-defaultpos) 0
#     } else {
#         set opts(-defaultpos) [lsearch -regexp $buts "(?iq)$opts(-default)"]
#         if { $opts(-defaultpos) == -1 } {
#             error "bad default option: '$opts(-default)' in DialogWin::messageBox"
#         }
#     }

#     set f [DialogWin2::Init $opts(-parent) $opts(-title) separator $buts - -]
#     set w [winfo toplevel $f]

#     label $f.l1 -image [GetImage question-32.png] -grid 0
#     label $f.msg -justify left -text $opts(-message) -wraplength 3i -grid "1 px5 py5"

#     supergrid::go $f

#     DialogWin2::FocusButton [expr $opts(-defaultpos)+2]

#     set action [DialogWin2::CreateWindow]
#     while 1 {
#         switch -- $action {
#             -1 {
#                 if { [lsearch $buts Cancel] != -1 } {
#                     catch {
#                         DialogWin2::DestroyWindow
#                         namespace delete ::DialogWin2
#                     }
#                     return cancel
#                 }
#                 if { [lsearch $buts OK] != -1 } {
#                     DialogWin2::DestroyWindow
#                     namespace delete ::DialogWin2
#                     return ok
#                 }
#             }
#             default {
#                 DialogWin2::DestroyWindow
#                 namespace delete ::DialogWin2
#                 return [string tolower [lindex $buts [expr $action-2]]]
#             }
#         }
#         set action [DialogWin2::WaitForWindow]
#     }
# }

# for compatibility
proc DialogWin::messageBox { args } {
    #return [eval DialogWinTop::messageBox $args]
    return [eval snit_messageBox $args]
}

proc DialogWinTop::messageBox { args } {
    return [eval snit_messageBox $args]
}

proc MessageWin { text title {image question-32.png} {parent .} } {
    if { $parent eq "." } { set parent "" }
    set w [dialogwin_snit $parent.%AUTO% -title $title -okname \
	       [_ "Ok"] -cancelname "-"]
    set f [$w giveframe]

    label $f.l1 -image [GetImage $image]
    label $f.msg -justify left -text $text -wraplength 3i

    grid $f.l1 $f.msg -sticky nw
    grid configure $f.msg -padx 5 -pady 5

    set action [$w createwindow]
    destroy $w
}

proc WarnWin { text {parent .} } {
    MessageWin $text [_ "Warning"] question-32.png $parent
}

proc WarnWin_hideerror { text errordata { parent .} } {
    if { $parent eq "." } { set parent "" }
    set w [dialogwin_snit $parent.%AUTO% -title [_ Warning] -okname [_ Ok] \
	       -cancelname -]
    set f [$w giveframe]

    label $f.l1 -image [GetImage question-32.png]
    label $f.msg -justify left -text $text -wraplength 3i

    grid $f.l1 $f.msg -sticky nw
    grid configure $f.msg -padx 5 -pady 5

    bind $w <2> [list error $text $errordata]

    set action [$w createwindow]
    destroy $w
}

proc snit_messageBox { args } {

    array set opts [list -default "" -icon question-32.png -message "" -parent . -title "" \
	    -type ok -do_not_ask_again 0 -do_not_ask_again_key ""]

    for { set i 0 } { $i < [llength $args] } { incr i } {
	set opt [lindex $args $i]
	if { ![info exists opts($opt)] } {
	    error [_ "unknown option '%s' in snit_messageBox" $opt]
	}
	incr i
	set opts($opt) [lindex $args $i]
    }

    if { $opts(-do_not_ask_again) } {
	if { $opts(-type) ni "ok okcancel yesno yesnocancel" } {
	    error "error. option -do_not_ask_again can only be used for types 'ok','okcancel','yesno' and 'yesnocancel'"
	}
	set d [dialogwin_snit give_typeuservar_value do_not_ask_again ""]
	if { $opts(-do_not_ask_again_key) eq "" } {
	    set opts(-do_not_ask_again_key) $opts(-message)
	}
	if { [dict exists $d $opts(-do_not_ask_again_key)] } {
	    return [dict get $d $opts(-do_not_ask_again_key)]
	}
    }
    switch -- $opts(-type) {
	abortretryignore {
	    set retbuts [list abort retry ignore]
	    set buts [list [_ Abort] [_ Retry] [_ Ignore]]
	}
	ok {
	    set retbuts [list ok]
	    set buts [list [_ OK]]
	}
	okcancel {
	    set retbuts [list ok cancel]
	    set buts [list [_ OK] [_ Cancel]]
	}
	retrycancel {
	    set retbuts [list retry cancel]
	    set buts [list [_ Retry] [_ Cancel]]
	}
	yesno {
	    set retbuts [list yes no]
	    set buts [list [_ Yes] [_ No]]
	}
	yesnocancel {
	    set retbuts [list yes no cancel]
	    set buts [list [_ Yes] [_ No] [_ Cancel]]
	}
	default {
	    error [_ "unknown type: '%s' in snit_messageBox" $opts(-type)]
	}
    }
    if { $opts(-default) == "" } {
	set opts(-defaultpos) 0
    } else {
	set opts(-defaultpos) [lsearch -regexp $retbuts "(?iq)$opts(-default)"]
	if { $opts(-defaultpos) == -1 } {
	    error [_ "bad default option: '%s' in snit_messageBox" $opts(-default)]
	}
    }

    if { $opts(-parent) eq "." } {
	set w .%AUTO%
    } else {
	set w $opts(-parent).%AUTO%
    }
    set w [dialogwin_snit $w -title $opts(-title) -morebuttons $buts \
	       -okname - -cancelname - -transient 1]
    set f [$w giveframe]

    ttk::label $f.l1 -image [GetImage $opts(-icon)]
    if { [winfo screenwidth .] < 300 } {
	set wraplength 1.5i
    } else {
	set wraplength 3i
    }
    ttk::label $f.msg -justify left -text $opts(-message) -wraplength $wraplength

    grid $f.l1 $f.msg -sticky nw
    grid configure $f.msg -padx 5 -pady 5

    if { $opts(-do_not_ask_again) } {
	ttk::checkbutton $f.cb1 -text [_ "Do not show again for this session"] -variable \
	    [$w give_uservar do_not_ask_again 0]
	grid $f.cb1 - -sticky w
    }

    $w focusbutton [expr $opts(-defaultpos)+2]
    set action [$w createwindow]

    if { $opts(-do_not_ask_again) && [$w give_uservar_value do_not_ask_again] } {
	set do_not_ask_again 1
    } else {
	set do_not_ask_again 0
    }
    destroy $w

    switch -- $action {
	-1 - 0 {
	    if { [lsearch -exact $buts [_ Cancel]] != -1 } {
		return cancel
	    }
	    return [lindex $retbuts end]
	}
	default {
	    set ipos [expr {$action-2}]
	    if { [lindex $retbuts $ipos] ne "cancel" && $do_not_ask_again } {
		set d [dialogwin_snit give_typeuservar_value do_not_ask_again ""]
		if { $opts(-do_not_ask_again_key) eq "" } {
		    set opts(-do_not_ask_again_key) $opts(-message)
		}
		dict set d $opts(-do_not_ask_again_key) [lindex $retbuts $ipos]
		dialogwin_snit set_typeuservar_value do_not_ask_again $d
	    }
	    return [lindex $retbuts $ipos]
	}
    }
}

proc tk_dialog_snit {w title text textsmall bitmap image default args} {
    if { $w == "" } {
	set parent "."
    } else { regsub {[.][^.]*$} $w {} parent }
    if {$parent == ""} {
	set parent "."
    }
    return [eval [list tk_dialog_snit1 $parent $title $text $textsmall $image $default] \
		$args]
}

proc tk_dialog_snit1 { parent title text textsmall image default args} {

    if { $parent eq "." } {
	set w .%AUTO%
    } else {
	set w $parent.%AUTO%
    }
    set w [dialogwin_snit $w -title $title -morebuttons $args \
	       -okname - -cancelname -]
    set f [$w giveframe]

    label $f.l1 -image $image
    label $f.msg -justify left -text $text -wraplength 3i

    grid $f.l1 $f.msg -sticky nw
    grid configure $f.msg -padx 5 -pady 5

    if { $textsmall ne "" } {
	set size [expr {[font actual [$f.msg cget -font] -size]-2}]
	label $f.ts -text $textsmall -font "-size $size"
	grid $f.ts - sticky nw
    }

    $w focusbutton [expr $default+2]
    set action [$w createwindow]

    destroy $w

    switch -- $action {
	-1 - 0 {
	    return -1
	}
	default {
	    return [expr {$action-2}]
	}
    }
}



#--------------------------------------------------------------------------------
#     dialogwin_snit
#--------------------------------------------------------------------------------

# NOTE: examples at the end

# style: ridgeframe or separator
# entrytype: entry or noneditable_entry or password
# -callback: calls function when user presses a button. Adds argument $w

snit::widget dialogwin_snit {
    option -title ""
    option -style separator
    option -grab 1
    option -transient 0
    option -callback ""
    option -morebuttons ""
    option -okname ""
    option -cancelname ""
    option -geometry ""
    option -minwidth ""
    option -minheight ""

    option -entrytype "" ;# entry,password,noneditable_entry
    option -entrytext ""
    option -entrylabel ""
    option -entrydefault ""
    option -entryvalues ""

    option -repeat_answer_check 0
    option -frame_grid_cmd ""
    option -toplevel_cmd ""
    option -show_frame_toplevel_toggle 1
    option -frame_toplevel toplevel

    hulltype toplevel

    delegate method * to hull
    delegate option * to hull

    variable action -1
    variable oldGrab ""
    variable grabStatus
    variable entryvalue
    variable repeat_my_answer 0
    variable uservar
    variable typeuservar
    variable traces ""
    variable destroy_handlers ""

    constructor args {
	#wm manage $win
	
	$self configurelist $args

	wm withdraw $win
	
	if {0&& [info commands ttk::button] eq "" } {
	    set button_cmd button
	    set label_cmd label
	    set entry_cmd entry
	    set combo_cmd ComboBox
	    set frame_cmd frame
	    set checkbutton_cmd checkbutton
	    set radiobutton_cmd radiobutton
	} else {
	    set button_cmd ttk::button
	    set label_cmd ttk::label
	    set entry_cmd ttk::entry
	    set combo_cmd ttk::combobox
	    set frame_cmd ttk::frame
	    set checkbutton_cmd ttk::checkbutton
	    set radiobutton_cmd ttk::radiobutton
	}
	set current_row -1
	if { $options(-show_frame_toplevel_toggle) && $options(-frame_grid_cmd) ne "" } {
	    frame $win.f0 -bg #880000 -bd 1 -relief solid -height 4 \
		-cursor hand2
	    grid $win.f0 -sticky ew -padx 2 -pady 2
	    incr current_row
	    bind $win.f0 <1> [mymethod toogle_frame_toplevel]
	}
	switch $options(-style) {
	    ridgeframe {
		$frame_cmd $win.f
		catch { $win.f configure -relief ridge -bd 2 }
		frame $win.buts
		grid $win.f -sticky ewns -padx 2 -pady 2
		incr current_row
		grid $win.buts -sticky ew
		catch { grid anchor $win.buts center }
	    }
	    separator - separator_right {
		$frame_cmd $win.f
		catch { $win.f configure -bd 0 }
		frame $win.sep -bd 2 -relief raised -height 2
		frame $win.buts
		grid $win.f -sticky ewns -padx 2 -pady 2
		incr current_row
		grid $win.sep -sticky ew
		grid $win.buts -sticky ew
		catch { grid anchor $win.buts center }
	    }
	    default {
		error "error: only accepted styles ridgeframe and separator"
	    }
	}
	$win.buts conf -bg [CCColorActivo [$win cget -bg]]
	
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure $win $current_row -weight 1

	if { $options(-okname) eq "" } { set options(-okname) [_ "Ok"] }
	if { $options(-cancelname) eq "" } {
	    set options(-cancelname) [_ "Cancel"]
	
	    if { $options(-okname) eq "-" && $options(-morebuttons) eq "" } {
		set options(-cancelname) [_ "Close"]
	    }
	}
	set butwidth 7
	if { [string length $options(-okname)] > $butwidth } {
	    set butwidth [string length $options(-okname)]
	}
	if { [string length $options(-cancelname)] > $butwidth } {
	    set butwidth [string length $options(-cancelname)]
	}
	foreach i $options(-morebuttons) {
	    if { [string length $i] > $butwidth } { set butwidth [string length $i] }
	}
	if { [catch { package present tile }] == 0 } {
	    set butwidth [expr {-1*$butwidth}]
	}
	set usedletters ""
	if { $options(-okname) != "-" } {
	    for { set ipos 0 } { $ipos < [string length $options(-okname)] } { incr ipos } {
		set letter [string tolower [string index $options(-okname) $ipos]]
		if { [regexp {[a-zA-Z]} $letter] && [lsearch $usedletters $letter] == -1 } {
		    break
		}
	    }
	    if { $ipos < [string length $options(-okname)] } {
		$button_cmd $win.buts.ok -text $options(-okname) -width $butwidth -und $ipos -command \
		        [mymethod _applyaction 1]
		bind $win <Alt-$letter> [mymethod _button_invoke $win.buts.ok]
		bind $win.buts.ok <Return> [mymethod _button_invoke $win.buts.ok]
		lappend usedletters $letter
		set widget($letter) $win.buts.ok
	    } else {
		$button_cmd $win.buts.ok -text $options(-okname) -width $butwidth -command \
		        [mymethod _applyaction 1]
	    }
	}
	if { $options(-cancelname) ne "-" } {
	    for { set ipos 0 } { $ipos < [string length $options(-cancelname)] } { incr ipos } {
		set letter [string tolower [string index $options(-cancelname) $ipos]]
		if { [regexp {[a-zA-Z]} $letter] && [lsearch $usedletters $letter] == -1 } {
		    break
		}
	    }
	    if { $ipos < [string length $options(-cancelname)] } {
		set underlinecancel $ipos
		lappend usedletters [string tolower [string index $options(-cancelname) $ipos]]
	    } else { set underlinecancel "" }
	}

	set togrid ""
	set iaction 2
	foreach i $options(-morebuttons) {
	    for { set ipos 0 } { $ipos < [string length $i] } { incr ipos } {
		set letter [string tolower [string index $i $ipos]]
		if { [regexp {[a-zA-Z]} $letter] && [lsearch $usedletters $letter] == -1 } {
		    break
		}
	    }
	    if { $ipos < [string length $i] } {
		$button_cmd $win.buts.b$iaction -text $i -width $butwidth -und $ipos \
		    -command [mymethod _applyaction $iaction]
		bind $win <Alt-$letter> [mymethod _button_invoke $win.buts.b$iaction]
		bind $win.buts.b$iaction <Return> [mymethod _button_invoke $win.buts.b$iaction]
		lappend usedletters $letter
		set widget($letter) $win.buts.b$iaction
	    } else {
		$button_cmd $win.buts.b$iaction -text $i -width $butwidth  \
		    -command [mymethod _applyaction $iaction]
	    }
	    lappend togrid $win.buts.b$iaction
	    incr iaction
	}
	if { $options(-cancelname) ne "-" } {
	    if { $underlinecancel != "" } {
		$button_cmd $win.buts.cancel -text $options(-cancelname) -width $butwidth \
		    -und $underlinecancel -command [mymethod _applyaction 0]
		set letter [string tolower [string index $options(-cancelname) $underlinecancel]]
		bind $win <Alt-$letter> [mymethod _button_invoke $win.buts.cancel]
		bind $win.buts.cancel <Return> [mymethod _button_invoke $win.buts.cancel]
		set widget($letter) $win.buts.cancel
	    } else {
		$button_cmd $win.buts.cancel -text $options(-cancelname) -width $butwidth \
		    -command [mymethod _applyaction 0]
	    }
	}

	if { $options(-okname) ne "-" } {
	    set togrid "$win.buts.ok $togrid"
	}
	if { $options(-cancelname) ne  "-" } {
	    set togrid "$togrid $win.buts.cancel"
	}
	
	if { $options(-repeat_answer_check) } {
	    checkbutton $win.buts.repeat -text [_ "Repeat my answer"] \
		-variable [myvar repeat_my_answer]
	    $win.buts.repeat configure -background [CCColorActivo [$win cget -bg]]
	}

	grid {*}$togrid -padx 2 -pady 4
	if { $options(-repeat_answer_check) } {
	    grid {*}$togrid -row 1
	    grid $win.buts.repeat {*}[lrepeat [expr {[llength $togrid]-1}] -] \
		-sticky w -padx 2 -pady 2 -row 0
	
#            grid $win.buts.repeat -row 0 -column [llength $togrid] -sticky w -padx 2
#             grid $win.buts.repeat {*}[lrepeat [expr {[llength $togrid]-1}] -] \
#                 -sticky w -padx 2 -pady 2
	}
	switch -- $options(-style) {
	    "separator_right" {
		grid configure {*}$togrid -sticky e
		grid columnconfigure $win.buts 0 -weight 1
	    }
	}
	if { $options(-okname) != "-" } {
	    focus $win.buts.ok
	} elseif { $options(-cancelname) != "-" } {
	    focus $win.buts.cancel
	}

	foreach i $togrid {
	    foreach letter $usedletters {
		bind $i <Key-$letter> [mymethod _button_invoke $widget($letter)]
	    }
	}
	if { $options(-cancelname) ne "-" } {
	    bind $win <Escape> [mymethod _button_invoke $win.buts.cancel]
	    bind $win.buts.cancel <Return> [mymethod _button_invoke $win.buts.cancel]
	    wm protocol $win WM_DELETE_WINDOW [mymethod _button_invoke $win.buts.cancel]
	} else {
	    bind $win <Escape> [mymethod _applyaction -1]
	    wm protocol $win WM_DELETE_WINDOW [mymethod _applyaction -1]
	}

	if { $options(-entrytext) ne "" } {
	    if { [winfo screenwidth .] < 300 } {
		set wraplength 100
	    } else {
		set wraplength 200
	    }
	    $label_cmd $win.f.l0 -text $options(-entrytext) -wraplength $wraplength \
		-justify left
	    grid $win.f.l0 - -sticky w -pady "2 5"
	}
	if { $options(-entrytype) ne "" } {
	    $label_cmd $win.f.l -text $options(-entrylabel)
	    switch $options(-entrytype) {
		entry {
		    if { $options(-entryvalues) eq "" } {
		        $entry_cmd $win.f.e -textvariable [varname entryvalue] -width 40
		    } else {
		        $combo_cmd $win.f.e -textvariable [varname entryvalue] -width 40 \
		            -values $options(-entryvalues)
		    }
		}
		password {
		    $entry_cmd $win.f.e -textvariable [varname entryvalue] -width 40 -show *
		}
		noneditable_entry {
		    if { [llength $options(-entryvalues)] <= 5 } {
		        set fr [$frame_cmd $win.f.e]
		        set idx 0
		        foreach i $options(-entryvalues) {
		            $radiobutton_cmd $fr.r$idx -text $i -variable [varname entryvalue] \
		                -value $i
		            grid $fr.r$idx -sticky w -padx 2 -pady 2
		            incr idx
		        }
		    } else {
		        $combo_cmd $win.f.e -textvariable [varname entryvalue] -width 40 \
		        -values $options(-entryvalues) -state readonly
		    }
		}
	    }
	    set entryvalue $options(-entrydefault)
	
	    grid $win.f.l $win.f.e -sticky w -padx 3 -pady 3
	    grid configure $win.f.e -sticky ew
	    grid columnconfigure $win.f 1 -weight 1

	    if { [winfo exists $win.f.l0] } {
		update idletasks
		$win.f.l0 configure -wraplength [expr {[winfo reqwidth $win.f]-5}]
	    }
	    tk::TabToWindow $win.f.e
	    bind $win <Return> [mymethod invokeok]
	}
    }
    destructor {
	set action -1

	if { $oldGrab ne "" }  {
	    if { [info exists grabStatus] && $grabStatus ne "global" } {
		if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab $oldGrab }
	    } else {
		if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } { grab -global $oldGrab }
	    }
	}
	foreach i $traces {
	    eval trace remove variable $i
	}
	foreach i $destroy_handlers {
	    uplevel #0 $i
	}
    }
    onconfigure -title {value} {
	set options(-title) $value
	wm title $win $options(-title)
    }
    method giveframe {} {
	return $win.f
    }
    method giveframe_background {} {
	set err [catch { $win.f cget -bg } bg]
	if { $err } {
	    set style [$win.f cget -style]
	    if { $style eq "" } { set style [winfo class $win.f] }
	    set err [catch { ttk::style lookup $style -background } bg]
	    if { $err } { set bg white }
	}
	return $bg
    }
    method invokeok { { visible 1 } } {
	if { ![winfo exists $win.buts.ok] } { return }
	
	if { $visible } {
	    $self _button_invoke $win.buts.ok
	} else {
	    $win.buts.ok invoke
	}
    }
    method invokecancel { { visible 1 } } {
	if { ![winfo exists $win.buts.cancel] } { return }
	
	if { $visible } {
	    $self _button_invoke $win.buts.cancel
	} else {
	    $win.buts.cancel invoke
	}
    }
    method invokebutton { num { visible 1 } } {
	if { ![winfo exists $win.buts] } { return }

	if { $num < 2 } {
	    error "error in dialogwin_snit invokebutton num>2"
	}
	foreach i [winfo children $win.buts] {
	    if { [regexp "\\m$num\\M" [$i cget -command]] } {
		if { $visible } {
		    $self _button_invoke $i
		} else {
		    $i invoke
		}
		return
	    }
	}
	error "error in dialogwin_snit invokebutton num bad"
    }
    method focusok {} {
	if { ![winfo exists $win.buts.ok] } { return }
	focus $win.buts.ok
    }
    method focuscancel {} {
	if { ![winfo exists $win.buts.cancel] } { return }
	focus $win.buts.cancel
    }
    method focusbutton { num } {
	
	if { $num < 2 } {
	    error "error in dialogwin_snit focusbutton num must be > 2"
	}
	foreach i [winfo children $win.buts] {
	    if { [regexp "\\m$num\\M" [$i cget -command]] } {
		focus $i
		return
	    }
	}
	error "error in dialogwin_snit focusbutton num bad"
    }
    method enableok {} {
	if { ![winfo exists $win.buts.ok] } { return }
	$win.buts.ok configure -state normal
    }
    method enablecancel {} {
	if { ![winfo exists $win.buts.cancel] } { return }
	$win.buts.cancel configure -state normal
    }
    method enablebutton { num } {
	
	if { $num < 2 } {
	    error "error in dialogwin_snit enablebutton num must be > 2"
	}
	foreach i [winfo children $win.buts] {
	    if { [regexp "\\m$num\\M" [$i cget -command]] } {
		$i configure -state normal
		return
	    }
	}
	error "error in dialogwin_snit enablebutton num bad"
    }
    method disableok {} {
	if { ![winfo exists $win.buts.ok] } { return }
	$win.buts.ok configure -state disabled
    }
    method disablecancel {} {
	if { ![winfo exists $win.buts.cancel] } { return }
	$win.buts.cancel configure -state disabled
    }
    method disablebutton { num } {
	
	if { $num < 2 } {
	    error "error in dialogwin_snit disablebutton num must be > 2"
	}
	foreach i [winfo children $win.buts] {
	    if { [regexp "\\m$num\\M" [$i cget -command]] } {
		$i configure -state disabled
		return
	    }
	}
	error "error in dialogwin_snit disablebutton num bad"
    }
    method _button_invoke { w } {
	if { [winfo class $w] eq "TButton" } {
	    $w instate !disabled {
		$w state pressed
		update idletasks
		after 100
		$w state !pressed
		update idletasks
		uplevel #0 [list $w invoke]
	    }
	} else {
	    if {[$w cget -state] ne "disabled"} {
		set oldRelief [$w cget -relief]
		set oldState [$w cget -state]
		$w configure -state active -relief sunken
		update idletasks
		after 100
		$w configure -state $oldState -relief $oldRelief
		uplevel #0 [list $w invoke]
	    }
	}
    }

    method changebuttonoptions { num args } {
	if { $num == 0 } {
	    eval [list $win.buts.cancel configure] $args
	} elseif { $num == 1 } {
	    eval [list $win.buts.ok configure] $args
	} else {
	    foreach i [winfo children $win.buts] {
		if { [regexp "\\m$num\\M" [$i cget -command]] } {
		    eval [list $i configure] $args
		    return
		}
	    }
	    error "error in dialogwin_snit changebuttonoptions num bad"
	}
    }
    method changebuttongridoptions { num args } {
	if { $num == 0 } {
	    eval [list grid configure $win.buts.cancel] $args
	} elseif { $num == 1 } {
	    eval [list grid configure $win.buts.ok] $args
	} else {
	    foreach i [winfo children $win.buts] {
		if { [regexp "\\m$num\\M" [$i cget -command]] } {
		    eval [list grid configure $i] $args
		    return
		}
	    }
	    error "error in dialogwin_snit changebuttongridoptions num bad"
	}
    }
    method showhidebutton { num what } {
	if { $num == 0 } {
	    set  b $win.buts.cancel
	} elseif { $num == 1 } {
	    set b $win.buts.ok
	} else {
	    foreach i [winfo children $win.buts] {
		if { [regexp "\\m$num\\M" [$i cget -command]] } {
		    set b $i
		    break
		}
	    }
	}
	switch $what {
	    show { grid $b }
	    hide {
		set i [grid info $b]
		if { $i eq "" } { return }
		grid columnconfigure $win.buts [dict get $i -column] \
		    -minsize [winfo width $b]
		grid remove $b
	    }
	}
    }
    method tooltip_button { num args } {
	
	package require tooltip
	if { $num == 0 } {
	    set  b $win.buts.cancel
	} elseif { $num == 1 } {
	    set b $win.buts.ok
	} else {
	    foreach i [winfo children $win.buts] {
		if { [regexp "\\m$num\\M" [$i cget -command]] } {
		    set b $i
		    break
		}
	    }
	}
	if { [llength $args] == 1 } {
	    tooltip::tooltip $b [lindex $args 0]
	} elseif { [llength $args] > 1 } {
	    error "error in tooltip_button arguments"
	}
	return [tooltip::tooltip $b]
    }
    method createwindow {} {
	$self createwindownowait
	return [$self waitforwindow 0]
    }
    method toogle_frame_toplevel {} {
	switch $options(-frame_toplevel) {
	    toplevel { set options(-frame_toplevel) frame }
	    frame { set options(-frame_toplevel) toplevel }
	}
	$self createwindow
    }
    method createwindownowait {} {
	if { $options(-frame_toplevel) eq "toplevel" } {
	    $self createwindownowait_as_toplevel
	} else {
	    $self createwindownowait_as_frame
	}
    }
    method createwindownowait_as_frame {} {
	$win configure -bd 1 -relief ridge
	update idletasks
	wm forget $win
	focus $win
	uplevel #0 $options(-frame_grid_cmd)
    }
    method createwindownowait_as_toplevel {} {
	
	set parent [winfo parent $win]
	set top [winfo toplevel $parent]

	if { $::tcl_platform(os) ne "Darwin" } {
	    wm manage $win
	}
	if { $::tcl_platform(os) eq "Windows CE" } {
	    bind $win <ConfigureRequest> { if { "%W" eq [winfo toplevel %W] } { etcl::autofit %W }}
	    bind $win <Expose> { if { "%W" eq [winfo toplevel %W] } { etcl::autofit %W }}
	}
	wm withdraw $win
	update idletasks

	lassign "" width height x y
	if { [catch { package present twapi }] == 0 } {
	    lassign [twapi::get_desktop_workarea] scr_x scr_y scr_w scr_h
	    set scr_w [expr {$scr_w-$scr_x}]
	    set scr_h [expr {$scr_h-$scr_y}]
	} else {
	    lassign [list 0 0 [winfo screenwidth $top] [winfo screenheight $top]] \
		scr_x scr_y scr_w scr_h
	}
	if { $options(-geometry) ne "" } {
	    if { ![regexp {(\d+)x(\d+)(?:\+([-\d]+)\+([-\d]+))?} \
		$options(-geometry) {} width height x y] } {
		regexp {^\+([-\d]+)\+([-\d]+)$} \
		    $options(-geometry) {} x y
	    }
	}
	if { $width eq "" || $height eq "" } {
	    if { $options(-minwidth) != "" && [winfo reqwidth $win] < $options(-minwidth) } {
		set width $minwidth
	    } else { set width [winfo reqwidth $win] }
	    if { $options(-minheight) != "" && [winfo reqheight $win] < $options(-minheight) } {
		set height $minheight
	    } else { set height [winfo reqheight $win] }

	    if { $width > $scr_w } { set width $scr_w }
	    if { $height > $scr_h } { set height $scr_h }
	}
	if { $x eq "" || $y eq "" } {
	    set big 0
	    if { $width > .8*$scr_w } { set big 1 }
	    if { $height > .8*$scr_h } { set big 1 }
	
	    if { $big || [wm state $top] == "withdrawn" } {
		set x [expr {$scr_x+$scr_w/2-$width/2}]
		set y [expr {$scr_y+$scr_h/2-$height/2}]
	    } else {
		set x [expr [winfo rootx $parent]+[winfo width $parent]/2-$width/2]
		set y [expr [winfo rooty $parent]+[winfo height $parent]/2-$height/2]
	    }
	    if { $x+$width > $scr_w+$scr_x } {
		set x [expr {$scr_x+$scr_w-$width}]
	    }
	    if { $y+$height > $scr_h+$scr_y } {
		set y [expr {$scr_y+$scr_h-$height}]
	    }
	    if { $x < 0 } { set x 0 }
	    if { $width > $scr_w } { set width $scr_w }

	    set err [catch { package present wce }]
	    if { !$err } {
		foreach "x0 y0 x1 y1" [wce sipinfo] break
		if { $y < $y0 } { set y $y0 }
		if { $height > $y1-$y0 } {
		    set height [expr {$y1-$y0}]
		}
	    }  else {
		if { $y < 0 } { set y 0 }
		if { $height > $scr_h } {
		    set height $scr_h
		}
	    }
	}

	wm geometry $win ${width}x${height}+${x}+$y
	
	if { $options(-transient) } {
	    if { [wm state $top] ne "withdrawn" } {
		wm transient $win $parent
	    }
	}
	update
	if { ![winfo exists $win] } {
	    return
	}
	wm deiconify $win
	update idletasks
	wm geometry $win [wm geometry $win]
	focus $win
	if { $options(-grab) } {
	    set oldGrab [grab current $win]
	    if { $oldGrab ne "" && [winfo exists $oldGrab] } {
		set grabStatus [grab status $oldGrab]
		grab release $oldGrab
	    }
	    catch { grab $win }
	} else {
	    set oldGrab ""
	}
	update
	if { ![winfo exists $win] } { return }
	set focus [focus -lastfor $win]
	if { $focus ne "" } { tk::TabToWindow $focus }
	if { $focus eq "" } { set focus $win }
	focus -force $focus
	if { $options(-toplevel_cmd) ne "" } {
	    uplevel #0 $options(-toplevel_cmd)
	}
    }
    method _applyaction { value } {
	set action $value
	if { $options(-callback) ne "" } {
	    uplevel #0 $options(-callback) $win
	}
    }
    method giveaction {} {
	return $action
    }
    method waitforwindow { { raise 0 } } {

	if { $raise == "" } {
	    # this is to avoid the 2 second problem in KDE 2
	    if { $::tcl_platform(platform) == "windows" } {
		set raise 1
	    } else { set raise 0 }
	}
	if { $raise } {
	    raise [winfo toplevel $win]
	}
	if { $options(-callback) ne "" } {
	    return
	}
	set action -2
	vwait [varname action]
	if { ![info exists action] } { return -1 }
	return $action
    }
    method withdrawwindow {} {
	if { $options(-grab) } {
	    grab release $win
	    if { $oldGrab ne "" }  {
		if { [info exists grabStatus] && $grabStatus ne "global" } {
		    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } {
		        grab $oldGrab
		    }
		} else {
		    if { [winfo exists $oldGrab] && [winfo ismapped $oldGrab] } {
		        grab -global $oldGrab
		    }
		}
	    }
	    set oldGrab ""
	}
	wm withdraw $win
    }
    method deiconifywindow {} {
	if { $options(-grab) } {
	    set oldGrab [grab current $win]
	    if { $oldGrab ne "" && [winfo exists $oldGrab] } {
		set grabStatus [grab status $oldGrab]
		grab release $oldGrab
	    }
	    catch { grab $win }
	} else {
	    set oldGrab ""
	}
	wm deiconify $win
    }
    method iswaiting {} {
	if { [info exists action] && $action == -2 } { return 1 }
	return 0
    }
    method giveentryvalue {} {
	return $entryvalue
    }
    method give_repeat_my_answer {} {
	return $repeat_my_answer
    }
    method exists_uservar { key } {
	return [info exists uservar($key)]
    }
    method give_uservar { args } {
	switch -- [llength $args] {
	    1 {
		#nothing
	    }
	    2 {
		set uservar([lindex $args 0]) [lindex $args 1]
	    }
	    default {
		error "error in give_uservar"
	    }
	}
	return [varname uservar([lindex $args 0])]
    }
    method set_uservar_value { key newvalue } {
	set uservar($key) $newvalue
    }
    method give_uservar_value { args } {
	set key [lindex $args 0]
	switch -- [llength $args] {
	    1 {
		return $uservar($key)
	    }
	    2 {
		if { [info exists uservar($key)] } {
		    return $uservar($key)
		} else {
		    return [lindex $args 1]
		}
	    }
	    default {
		error "error in give_uservar_value"
	    }
	}
    }
    method unset_uservar { key } {
	unset uservar($key)
    }
    typemethod exists_typeuservar { key } {
	return [info exists typeuservar($key)]
    }
    typemethod set_typeuservar_value { key newvalue } {
	set typeuservar($key) $newvalue
    }
    typemethod give_typeuservar_value { args } {
	set key [lindex $args 0]
	switch -- [llength $args] {
	    1 {
		return $typeuservar($key)
	    }
	    2 {
		if { [info exists typeuservar($key)] } {
		    return $typeuservar($key)
		} else {
		    return [lindex $args 1]
		}
	    }
	    default {
		error "error in give_typeuservar_value"
	    }
	}
    }
    typemethod unset_typeuservar { key} {
	unset typeuservar($key)
    }
    typemethod clear_do_not_ask_again { message } {
	if { ![info exists typeuservar(do_not_ask_again)] } { return }
	set d $typeuservar(do_not_ask_again)
	dict unset d $message
	set typeuservar(do_not_ask_again) $d
    }
    method add_trace_to_uservar { key cmd } {
	trace add variable [varname uservar($key)] write "$cmd;#"
	lappend traces [list [varname uservar($key)] write "$cmd;#"]
    }
    method add_traceN_to_uservar { key cmd } {
	append cmd " \[[list $self give_uservar_value $key]\]"
	trace add variable [varname uservar($key)] write "$cmd;#"
	lappend traces [list [varname uservar($key)] write "$cmd;#"]
    }
    method eval_uservar_traces { key } {
	set uservar($key) $uservar($key)
    }
    # dict contains values and active widgets for these values
    # example: if dict contains:
    #    value1 "w1 w2" value2 "w3 w4"
    # when value of key is changed to 'value1', widgets w1 w2 will be enabled
    # and widgets w3 w4 will be disabled
    # if widget is a number, it refers to a button
    method enable_disable_on_key { args } {
	set optional {
	    { -clear "" 0 }
	}
	set compulsory "key dict"
	parse_args $optional $compulsory $args

	if { $clear } {
	    $self remove_traces_to_uservar $key [mymethod _enable_disable_on_key_helper]*
	}
	$self add_trace_to_uservar $key [mymethod _enable_disable_on_key_helper \
		$key $dict]
	catch { $self _enable_disable_on_key_helper $key $dict }
    }
    method _enable_disable_on_key_helper { key dict args } {
	dict for "n v" $dict {
	    if { $n ne $uservar($key) } {
		foreach w $v {
		    if { [string is integer $w] } {
		        switch $w {
		            1 { $self disableok }
		            0 { $self disablecancel }
		            default { $self disablebutton $w }
		        }
		    } else {
		        set i_action disable
		        if { [regexp {^([-+])(.*)} $w {} sign w] } {
		            if { $sign eq "-" } {
		                set i_action enable
		            } else {
		                continue
		            }
		        }
		        $self _enable_disable_widget $w $i_action
		    }
		}
	    }
	}
	set n $uservar($key)
	if { [dict exists $dict $n] } {
	    set v [dict get $dict $n]
	} elseif { [dict exists $dict ""] } {
	    set v [dict get $dict ""]
	} else { set v "" }
	
	foreach w $v {
	    if { [string is integer $w] } {
		switch $w {
		    1 { $self enableok }
		    0 { $self enablecancel }
		    default { $self enablebutton $w }
		}
	    } else {
		set i_action enable
		if { [regexp {^([-+])(.*)} $w {} sign w] } {
		    if { $sign eq "-" } { set i_action disable }
		}
		$self _enable_disable_widget $w $i_action
	    }
	}
    }
    method _enable_disable_widget { w enable_disable } {
	switch [winfo class $w] {
	    Canvas {
		switch $enable_disable {
		    enable { $w itemconfigure all -fill black }
		    disable { $w itemconfigure all -fill grey }
		}
	    }
	    default {
		switch $enable_disable {
		    enable {
		        set err [catch { $w state !disabled }]
		        if { $err } { catch { $w configure -state normal } }
		    }
		    disable {
		        set err [catch { $w state disabled }]
		        if { $err } { catch { $w configure -state disabled } }
		    }
		}
	    }
	}
	foreach i [winfo children $w] {
	    $self _enable_disable_widget $i $enable_disable
	}
    }
    # dict contains values and "key2 newvalue" pairs for these values
    # example: if dict contains:
    #    value1 "key2 1" value2 "key2 0 key3 v" default "key4 1"
    # when value of key is changed to 'value1', the value of key "key2" is
    # changed to "1". when value of key is changed to 'value2', the value
    # of key "key2" is changed t0 "0" and the value of "key3" is changed to "v"
    # for any other value, key4 is changed to 1
    # there can be a "default" value that is applied if none of the other values apply
    # if a value for a variable is not given, it is just updated to raise traces
    method change_key_on_key { args } {
	set optional {
	    { -clear "" 0 }
	}
	set compulsory "key dict"
	parse_args $optional $compulsory $args
	
	if { $clear } {
	    $self remove_traces_to_uservar $key [mymethod _change_key_on_key_helper]*
	}
	$self add_trace_to_uservar $key [mymethod _change_key_on_key_helper \
		$key $dict]
	catch { $self _change_key_on_key_helper $key $dict }
    }
    method _change_key_on_key_helper { key dict args } {
	set n $uservar($key)
	if { [dict exists $dict $n] } {
	    set v [dict get $dict $n]
	    if { [llength $v]%2 == 1 } {
		lappend v $uservar([lindex $v end])
	    }
	    foreach "k v" $v {
		set uservar($k) $v
	    }
	} elseif { [dict exists $dict default] } {
	    set v [dict get $dict default]
	    if { [llength $v]%2 == 1 } {
		lappend v $uservar([lindex $v end])
	    }
	    foreach "k v" $v {
		set uservar($k) $v
	    }
	}
    }
    method remove_traces_to_uservar { key { cmd_pattern "" } } {
	foreach i $traces {
	    if { [lindex $i 0] eq [varname uservar($key)] } {
		if { $cmd_pattern eq "" || [string match $cmd_pattern [lindex $i 2]] } {
		    eval trace remove variable $i
		}
	    }
	}
    }
    method add_destroy_handler { cmd } {
	lappend destroy_handlers $cmd
    }
}


#--------------------------------------------------------------------------------
#   dialogwin_snit EXAMPLES
#--------------------------------------------------------------------------------

#     SIMPLE: only text and buttons

if 0 {
    dialogwin_snit $win._ask -title [_ "Action"] -okname [_ "New password"] \
	-morebuttons [list [_ "Uncrypt"]] -entrytext [_ "Choose action to perform"]:
    set action [$win._ask createwindow]
    destroy $win._ask
    if { $action <= 0 } {  return }
    if { $action == 2 } {
	$wordnoter_db enterpage $options(-page) - $data
	return
    }
}

#     MEDIUM: text and buttons and entry

if 0 {
    dialogwin_snit $win._ask -title [_ "Enter password"] -entrytype password \
	-entrylabel [_ "Password"]: -entrytext [_ "Enter password to encrypt"]:
    set action [$win._ask createwindow]
    while 1 {
	if { $action <= 0 } {
	    destroy $win._ask
	    return
	}
	set pass [string trim [$win._ask giveentryvalue]]
	if { [string length $pass] < 4 } {
	    $self warnwin [_ "Password must have at least 4 characters"]
	} else { break }
	set action [$win._ask waitforwindow]
    }
}

#     COMPLEX: user defined widgets

if 0 {
    set w [dialogwin_snit $win._ask -title [_ "Change page type"] -entrytext \
	    [_ "Choose a new page type for page '%s'" $page] \
	    -morebuttons [list [_ "Yes to all"] [_ "No to all"] [_ No]]]
#-grab 0 -callback [mymethod preferences_win_apply] -okname -
    set f [$w giveframe]
    if { $type eq "" } { set type Normal }
    label $f.l1 -text [_ "Current type: %s" $type]
    label $f.l2 -text [_ "New type:"]
    ComboBox $f.cb1 -textvariable [$w give_uservar newtype $type] -values \
	[list Normal Home] -editable 0
    tk::TabToWindow $f.cb1
    bind $f.cb1 <Return> [list $w invokeok]
    grid $f.l1 - -sticky w -pady 2
    grid $f.l2 $f.cb1 -sticky w -padx 2 -pady "2 4"
    grid configure $f.cb1 -sticky ew
    grid columnconfigure $f 1 -weight 1

    set action [$w createwindow]
    set newtype [$w give_uservar_value newtype]
    destroy $w
    if { $action <= 0 } {  return }
    if { $newtype eq "Normal" } { set newtype "" }

#     switch -- [$w giveaction] {
#         -1 - 0 { destroy $w }
#         1 - 2 {
#
#             if { [$w giveaction] == 1 } { destroy $w }
#         }
#     }
}


snit::widgetadaptor wizard_snit {
    option -image ""
    option -on_exit_callback ""

    delegate method * to hull
    delegate option * to hull

    # every element is composed of: title build_callback check_callback has_finish_button
    # is_labelframe is_hidden previous_page
    # check_callback can be void on all pages except the last
    variable dataList ""

    variable curr_callback
    variable frame

    constructor {args} {
	installhull using dialogwin_snit -callback [mymethod _callback] \
	    -morebuttons [list [_ Previous] [_ Next] [_ Finish] [_ Cancel]] \
	    -okname - -cancelname - -geometry 500x300 -transient 1

	$self configurelist $args
	
	set f [$win giveframe]
	
	ttk::label $f.l1
	if { $options(-image) ne "" } {
	    $f.l1 configure -image $options(-image)
	}
	set frame [ttk::labelframe $f.f1]
	
	grid $f.l1 $frame -sticky nsew
	grid configure $frame -padx 5 -pady 5
	grid columnconfigure $f 1 -weight 1
	grid rowconfigure $f 0 -weight 1
	
	set curr_callback 0
	
	$win changebuttongridoptions 5 -padx "20 0"
	$win createwindow
    }
    method create_page { args } {
	
	set optional {
	    { -check_callback callback "" }
	    { -has_finish_button boolean 0 }
	    { -is_labelframe boolean 1 }
	    { -is_hidden boolean 0 }
	    { -previous_page number|title "" }
	}
	set compulsory "title build_callback"
	parse_args $optional $compulsory $args
	
	if { $previous_page ne "" && ![string is integer -strict $previous_page] } {
	    set ipos [lsearch -exact -index 0 $dataList $previous_page]
	    if { $ipos == -1 } {
		error "error in create_page. previous_page not existant"
	    }
	    set previous_page [expr {$ipos+1}]
	}
	
	set elm [list $title $build_callback $check_callback $has_finish_button \
		$is_labelframe $is_hidden $previous_page]

	lappend dataList $elm
	return [llength $dataList]
    }
    method edit_page { num args } {
	
	incr num -1
	
	set optional {
	    { -title title "" }
	    { -build_callback callback "" }
	    { -check_callback callback "" }
	    { -has_finish_button boolean -1 }
	    { -is_labelframe boolean -1 }
	    { -is_hidden boolean -1 }
	    { -previous_page number|title "" }
	}
	set compulsory ""
	parse_args $optional $compulsory $args
	
	if { $previous_page ne "" && ![string is integer -strict $previous_page] } {
	    set ipos [lsearch -exact -index 0 $dataList $previous_page]
	    if { $ipos == -1 } {
		error "error in edit_page. previous_page not existant"
	    }
	    set previous_page [expr {$ipos+1}]
	}
	
	set idx 0
	foreach i $optional {
	    foreach "opt type def" $i break
	    set opt [string trimleft $opt -]
	    if { [set $opt] ne $def } {
		lset dataList $num $idx [set $opt]
	    }
	    incr idx
	}
    }
    method open_page { num } {
	set curr_callback [expr {$num-1}]
	$self _open_window ahead
    }
    method _open_window { direction } {

	while 1 {
	    set elm [lindex $dataList $curr_callback]
	    if { $elm eq "" } { return }
	    foreach [list title build_callback check_callback has_finish_button \
		    is_labelframe is_hidden previous_page] $elm break
	    if { !$is_hidden } { break }
	    switch $direction {
		ahead { incr curr_callback }
		behind { incr curr_callback -1 }
	    }
	}
	eval destroy [winfo children $frame]
	set i [grid info $frame]
	destroy $frame
	if { $is_labelframe } {
	    ttk::labelframe $frame -text $title
	} else {
	    ttk::frame $frame
	}
	eval grid $frame $i

	lappend build_callback $win $frame
	uplevel #0 $build_callback
	
	$win configure -title $title
	
	if { $curr_callback == 0 } {
	    $win disablebutton 2
	} else {
	    $win enablebutton 2
	}
	if { $curr_callback == [llength $dataList]-1 } {
	    $win disablebutton 3
	    $win showhidebutton 4 show
	} else {
	    $win enablebutton 3
	    if { $has_finish_button == 1 } {
		$win showhidebutton 4 show
	    } else {
		$win showhidebutton 4 hide
	    }
	}
	if { [wm state $win] ne "normal" } {
	    $self deiconifywindow
	}
    }
    method _callback { f } {
	switch -- [$win giveaction] {
	    -1 - 0 - 5 {
		return [$self withdraw]
	    }
	    2 {
		set previous_page [lindex $dataList $curr_callback 6]
		if { $previous_page ne "" } {
		    set curr_callback [expr {$previous_page-1}]
		} else {
		    incr curr_callback -1
		}
		$self _open_window behind
	    }
	    3 - 4 {
		set check_callback [lindex $dataList $curr_callback 2]
		set ret ""
		if { $check_callback ne "" } {
		    lappend check_callback $win $frame
		    if { [$win giveaction] == 4 } {
		        lappend check_callback finish
		    } else {
		        lappend check_callback next
		    }
		    set err [catch { uplevel #0 $check_callback } ret]
		    if { $err } {
		        if { $ret ne "" } {
		            snit_messageBox -parent $win -message $ret
		        }
		        return
		    }
		}
		if { $ret eq "finish" || [$win giveaction] == 4 } {
		    return [$self withdraw]
		} elseif { [string is integer -strict $ret] } {
		    set curr_callback [expr {$ret-1}]
		} elseif { $ret ne "" } {
		    set ipos [lsearch -exact -index 0 $dataList $ret]
		    if { $ipos == -1 } {
		        error "error in check_callback return value. page not existant"
		    }
		    set curr_callback $ipos
		} else {
		    incr curr_callback
		}
		$self _open_window ahead
	    }
	}
    }
    method withdraw {} {
	if { $options(-on_exit_callback) ne "" } {
	    uplevel #0 $options(-on_exit_callback) $win
	}
	$self withdrawwindow
	#destroy $win
	return ""
    }
}

################################################################################
#    parse args
#
# example:
#  proc myproc { args } {
#     set optional {
#         { -view_binding binding "" }
#         { -file file "" }
#         { -restart_file boolean 0 }
#         { -flag1 "" 0 }
#     }
#     set compulsory "levels"
#     parse_args $optional $compulsory $args
#
#     if { $view_binding ne "" } { puts hohoho }
#     if { $flag1 } { puts "activated flag" }
#  }
#
################################################################################


# ramsan: i perquè toqueu això?
if { [catch { package require compass_utils }] } {
    proc ::parse_args { args } {
	
	set optional {
	    { -raise_compulsory_error boolean 1 }
	    { -compulsory_min min_number "" }
	}
	set compulsory "optional compulsory arguments"
	
	set cmdname [lindex [info level [expr {[info level]-1}]] 0]
	
	if { [string match -* [lindex $args 0]] } {
	    parse_args $optional $compulsory $args
	} else {
	    set raise_compulsory_error 1
	    set compulsory_min ""
	    if { [llength $args] != [llength $compulsory] } {
		uplevel 1 [list error [_parse_args_string $cmdname $optional \
		            $compulsory $args]]
		return ""
	    }
	    foreach $compulsory $args break
	}
	
	foreach i $optional {
	    foreach "name namevalue default" $i break
	    set opts_value($name) $namevalue
	    if { [llength $i] > 2 } {
		set opts($name) $default
	    }
	}
	while { [string match -* [lindex $arguments 0]] } {
	    if { [lindex $arguments 0] eq "--" } {
		set arguments [lrange $arguments 1 end]
		break
	    }
	    foreach "name value" [lrange $arguments 0 1] break
	    if { [regexp {(.*)=(.*)} $name {} name value] } {
		set has_att_value 1
	    } else {
		set has_att_value 0
	    }
	    if { [info exists opts_value($name)] } {
		if { $has_att_value } {
		    set opts($name) $value
		    set arguments [lrange $arguments 1 end]
		} elseif { $opts_value($name) eq "" } {
		    set opts($name) 1
		    set arguments [lrange $arguments 1 end]
		} else {
		    set opts($name) $value
		    set arguments [lrange $arguments 2 end]
		}
	    } else {
		uplevel 1 [list error [_parse_args_string $cmdname $optional \
		            $compulsory $args]]
		return ""
	    }
	}
	if { $raise_compulsory_error } {
	    if { $compulsory_min ne "" } {
		if { [llength $arguments] < $compulsory_min || \
		    [llength $arguments] > [llength $compulsory] } {
		    uplevel 1 [list error [_parse_args_string $cmdname $optional $compulsory]]
		    return ""
		}
	    } elseif { [llength $arguments] != [llength $compulsory] } {
		uplevel 1 [list error [_parse_args_string $cmdname $optional \
		            $compulsory $args]]
		return ""
	    }
	
	}
	foreach name [array names opts] {
	    uplevel 1 [list set [string trimleft $name -] $opts($name)]
	}
	set inum 0
	foreach i $compulsory {
	    uplevel 1 [list set $i [lindex $arguments $inum]]
	    incr inum
	}
	return [lrange $arguments $inum end]
    }

    proc ::_parse_args_string { cmd optional compulsory arguments } {
	
	set str "error. usage: $cmd "
	foreach i $optional {
	    foreach "name namevalue default" $i break
	    append str "?$name $namevalue? "
	}
	append str $compulsory
	append str "\n\targs: $arguments"
	return $str
    }
}



















