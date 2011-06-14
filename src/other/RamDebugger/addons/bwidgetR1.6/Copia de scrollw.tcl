# -----------------------------------------------------------------------------
#  scrollw.tcl
#  This file is part of Unifix BWidget Toolkit
#  $Id$
# -----------------------------------------------------------------------------
#  Index of commands:
#     - ScrolledWindow::create
#     - ScrolledWindow::getframe
#     - ScrolledWindow::setwidget
#     - ScrolledWindow::configure
#     - ScrolledWindow::cget
#     - ScrolledWindow::_set_hframe
#     - ScrolledWindow::_set_vscroll
#     - ScrolledWindow::_setData
#     - ScrolledWindow::_setSBSize
#     - ScrolledWindow::_realize
# -----------------------------------------------------------------------------

namespace eval ScrolledWindow {
    Widget::declare ScrolledWindow {
        {-background  TkResource ""   0 button}
        {-scrollbar   Enum       both 0 {none both vertical horizontal}}
        {-auto        Enum       both 0 {none both vertical horizontal}}
        {-sides       Enum       se   0 {ne en nw wn se es sw ws}}
        {-size        Int        0    1 "%d >= 0"}
        {-ipad        Int        1    1 "%d >= 0"}
        {-managed     Boolean    1    1}
        {-relief      TkResource flat 0 frame}
        {-borderwidth TkResource 0    0 frame}
        {-bg          Synonym    -background}
        {-bd          Synonym    -borderwidth}
    }

    Widget::addmap ScrolledWindow "" ._grid.f {-relief {} -borderwidth {}}

    proc ::ScrolledWindow {path args} {
        return [eval ScrolledWindow::create $path $args]
    }
    proc use {} {}
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::create
# -----------------------------------------------------------------------------
proc ScrolledWindow::create { path args } {
    upvar \#0 ScrolledWindow::$path data

    Widget::init ScrolledWindow $path $args

    set bg     [Widget::cget $path -background]
    set sbsize [Widget::cget $path -size]
    set ipad   [Widget::cget $path -ipad]
    set sw     [frame $path \
                    -relief flat -borderwidth 0 -background $bg \
                    -highlightthickness 0 -takefocus 0]
    set grid   [frame $path._grid \
                    -relief flat -borderwidth 0 -background $bg \
                    -highlightthickness 0 -takefocus 0]
    set fv     [frame $grid.vframe \
                    -relief flat -borderwidth 0 -background $bg \
                    -highlightthickness 0 -takefocus 0]
    set fh     [frame $grid.hframe \
                    -relief flat -borderwidth 0 -background $bg \
                    -highlightthickness 0 -takefocus 0]
    eval frame $grid.f -background $bg -highlightthickness 0 \
        [Widget::subcget $path ._grid.f]

    scrollbar $grid.hscroll \
        -highlightthickness 0 -takefocus 0 \
        -orient  horiz	\
        -relief  sunken	\
        -bg      $bg
    scrollbar $grid.vscroll \
        -highlightthickness 0 -takefocus 0 \
        -orient  vert  	\
        -relief  sunken	\
        -bg      $bg

    set data(realized) 0

    _setData $path \
        [Widget::cget $path -scrollbar] \
        [Widget::cget $path -auto] \
        [Widget::cget $path -sides]

    if {[Widget::cget $path -managed]} {
        set data(hsb,packed) $data(hsb,present)
        set data(vsb,packed) $data(vsb,present)
    } else {
        set data(hsb,packed) 0
        set data(vsb,packed) 0
    }
    if {$sbsize} {
        $grid.vscroll configure -width $sbsize
        $grid.hscroll configure -width $sbsize
    } else {
        set sbsize [$grid.vscroll cget -width]
    }
    set size [expr {$sbsize+$ipad}]

    $grid.vframe configure -width  $size
    $grid.hframe configure -height $size
    set vplaceopt [list -in $grid.vframe -x [expr {(1-$data(vsb,west))*$ipad}] -y 0 -width [expr {-$ipad}]]
    set hplaceopt [list -in $grid.hframe -x 0 -y [expr {(1-$data(hsb,north))*$ipad}] -height [expr {-$ipad}]]
    pack propagate $grid.vframe 0
    pack propagate $grid.hframe 0
    pack $grid.vscroll -in $grid.vframe
    pack $grid.hscroll -in $grid.hframe

    bind $grid.hscroll <Configure> \
        "ScrolledWindow::_setSBSize $grid.hscroll $size -relwidth 1.0 -relheight 1.0 $hplaceopt"
    bind $grid.vscroll <Configure> \
        "ScrolledWindow::_setSBSize $grid.vscroll $size -relwidth 1.0 -relheight 1.0 $vplaceopt"

    grid $grid.hframe \
        -column     [expr {$data(vsb,west)*$data(vsb,packed)}] \
        -row        [expr {1-$data(hsb,north)}]  \
        -columnspan [expr {2-$data(vsb,packed)}] \
        -sticky we
    grid $grid.vframe \
        -column  [expr {1-$data(vsb,west)}] \
        -row     [expr {$data(hsb,north)*$data(hsb,packed)}] \
        -rowspan [expr {2-$data(hsb,packed)}] \
        -sticky ns

    grid $grid.f \
        -column     [expr {$data(vsb,west)*$data(vsb,packed)}]  \
        -row        [expr {$data(hsb,north)*$data(hsb,packed)}] \
        -columnspan [expr {2-$data(vsb,packed)}] \
        -rowspan    [expr {2-$data(hsb,packed)}] \
        -sticky     nwse

    grid columnconfigure $grid $data(vsb,west)  -weight 1
    grid rowconfigure    $grid $data(hsb,north) -weight 1
    pack $grid -fill both -expand yes

    bind $grid <Configure> "ScrolledWindow::_realize $path"
    bind $grid <Destroy>   "ScrolledWindow::_destroy $path"
    raise $grid.f
    rename $path ::$path:cmd
    proc ::$path { cmd args } "return \[eval ScrolledWindow::\$cmd $path \$args\]"

    return $path
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::getframe
# -----------------------------------------------------------------------------
proc ScrolledWindow::getframe { path } {
    return $path
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::setwidget
# -----------------------------------------------------------------------------
proc ScrolledWindow::setwidget { path widget } {
    upvar \#0 ScrolledWindow::$path data

    set grid   $path._grid

    # RAMSAN change
    foreach i [pack slaves $grid.f] {
	pack forget $i
    }

    pack $widget -in $grid.f -fill both -expand yes

    $grid.hscroll configure -command "$widget xview"
    $grid.vscroll configure -command "$widget yview"
    $widget configure \
        -xscrollcommand "ScrolledWindow::_set_hscroll $path" \
        -yscrollcommand "ScrolledWindow::_set_vscroll $path"
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::configure
# -----------------------------------------------------------------------------
proc ScrolledWindow::configure { path args } {
    upvar \#0 ScrolledWindow::$path data

    set grid $path._grid
    set res [Widget::configure $path $args]
    if { [Widget::hasChanged $path -background bg] } {
        $path configure -background $bg
        $grid configure -background $bg
        $grid.f configure -background $bg
        catch {$grid.hscroll configure -background $bg}
        catch {$grid.vscroll configure -background $bg}
    }

    if {[Widget::hasChanged $path -scrollbar scrollbar] |
        [Widget::hasChanged $path -auto      auto]     |
        [Widget::hasChanged $path -sides     sides]} {
        _setData $path $scrollbar $auto $sides
        set hscroll [$grid.hscroll get]
        set vmin    [lindex $hscroll 0]
        set vmax    [lindex $hscroll 1]
        set data(hsb,packed) [expr {$data(hsb,present) &&
                                    (!$data(hsb,auto) || ($vmin != 0 || $vmax != 1))}]
        set vscroll [$grid.vscroll get]
        set vmin    [lindex $vscroll 0]
        set vmax    [lindex $vscroll 1]
        set data(vsb,packed) [expr {$data(vsb,present) &&
                                    (!$data(vsb,auto) || ($vmin != 0 || $vmax != 1))}]

        set ipad [Widget::cget $path -ipad]
        place configure $grid.vscroll \
            -x [expr {(1-$data(vsb,west))*$ipad}]
        place configure $grid.hscroll \
            -y [expr {(1-$data(hsb,north))*$ipad}]

        grid configure $grid.hframe \
            -column     [expr {$data(vsb,west)*$data(vsb,packed)}] \
            -row        [expr {1-$data(hsb,north)}]  \
            -columnspan [expr {2-$data(vsb,packed)}]
        grid configure $grid.vframe \
            -column  [expr {1-$data(vsb,west)}] \
            -row     [expr {$data(hsb,north)*$data(hsb,packed)}] \
            -rowspan [expr {2-$data(hsb,packed)}]
        grid configure $grid.f \
            -column     [expr {$data(vsb,west)*$data(vsb,packed)}] \
            -row        [expr {$data(hsb,north)*$data(hsb,packed)}] \
            -columnspan [expr {2-$data(vsb,packed)}] \
            -rowspan    [expr {2-$data(hsb,packed)}]
        grid columnconfigure $grid $data(vsb,west)             -weight 1
        grid columnconfigure $grid [expr {1-$data(vsb,west)}]  -weight 0
        grid rowconfigure    $grid $data(hsb,north)            -weight 1
        grid rowconfigure    $grid [expr {1-$data(hsb,north)}] -weight 0
    }
    return $res
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::cget
# -----------------------------------------------------------------------------
proc ScrolledWindow::cget { path option } {
    return [Widget::cget $path $option]
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::_destroy
# -----------------------------------------------------------------------------
proc ScrolledWindow::_destroy { path } {
    upvar \#0 ScrolledWindow::$path data

    unset data
    Widget::destroy $path
    rename $path {}
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::_set_hscroll
# -----------------------------------------------------------------------------
proc ScrolledWindow::_set_hscroll { path vmin vmax } {
    upvar \#0 ScrolledWindow::$path data

   # JORGE CHANGE
#      rename ::ScrolledWindow::_set_hscroll __save_hscroll__
#      proc ::ScrolledWindow::_set_hscroll { args } {
#  	return
#      }
    if {$data(realized) && $data(hsb,present)} {
        set grid $path._grid
        if {$data(hsb,auto)} {

            if {$data(hsb,packed) && $vmin == 0 && $vmax == 1} {
		# RAMSAN change
		if { [info exists ScrolledWindow::recent_change($path)] && \
		    $ScrolledWindow::recent_change($path) } {
#  		    rename ::ScrolledWindow::_set_hscroll ""
#  		    rename __save_hscroll__ ::ScrolledWindow::_set_hscroll
		    return
		}
                set data(hsb,packed) 0
                grid configure $grid.f $grid.vframe -row 0 -rowspan 2
            } elseif {!$data(hsb,packed) && ($vmin != 0 || $vmax != 1)} {
                set data(hsb,packed) 1
                grid configure $grid.f $grid.vframe -row $data(hsb,north) -rowspan 1
            }
        }
	# RAMSAN change
	#update idletask
	set ::ScrolledWindow::recent_change($path) 1
	after 200 set ::ScrolledWindow::recent_change($path) 0

        $grid.hscroll set $vmin $vmax
    }
    # JORGE CHANGE
#      rename ::ScrolledWindow::_set_hscroll ""
#      rename __save_hscroll__ ::ScrolledWindow::_set_hscroll
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::_set_vscroll
# -----------------------------------------------------------------------------
proc ScrolledWindow::_set_vscroll { path vmin vmax } {
    upvar \#0 ScrolledWindow::$path data

    # RAMSAN change
    if { $vmin == 0 && $vmax == 0 } { return }

    # JORGE CHANGE
#      rename ::ScrolledWindow::_set_vscroll __save_vscroll__
#      proc ::ScrolledWindow::_set_vscroll { args } {
#  	return
#      }
    if {$data(realized) && $data(vsb,present)} {
        set grid $path._grid
        if {$data(vsb,auto)} {
            if {$data(vsb,packed) && $vmin == 0 && $vmax == 1} {
                set data(vsb,packed) 0
                grid configure $grid.f $grid.hframe -column 0 -columnspan 2
            } elseif {!$data(vsb,packed) && ($vmin != 0 || $vmax != 1) } {
                set data(vsb,packed) 1
                grid configure $grid.f $grid.hframe -column $data(vsb,west) -columnspan 1
            }
        }
	# RAMSAN change
	#update idletask
        $grid.vscroll set $vmin $vmax
    }
    # JORGE CHANGE
#      rename ::ScrolledWindow::_set_vscroll ""
#      rename __save_vscroll__ ::ScrolledWindow::_set_vscroll
}


proc ScrolledWindow::_setData {path scrollbar auto sides} {
    upvar \#0 ScrolledWindow::$path data

    set sb    [lsearch {none horizontal vertical both} $scrollbar]
    set auto  [lsearch {none horizontal vertical both} $auto]
    set north [string match *n* $sides]
    set west  [string match *w* $sides]

    set data(hsb,present)  [expr {($sb & 1) != 0}]
    set data(hsb,auto)     [expr {($auto & 1) != 0}]
    set data(hsb,north)    $north

    set data(vsb,present)  [expr {($sb & 2) != 0}]
    set data(vsb,auto)     [expr {($auto & 2) != 0}]
    set data(vsb,west)     $west
}


proc ScrolledWindow::_setSBSize {sb size args} {
    $sb configure -width $size
    eval place $sb $args
}


# -----------------------------------------------------------------------------
#  Command ScrolledWindow::_realize
# -----------------------------------------------------------------------------
proc ScrolledWindow::_realize { path } {
    upvar \#0 ScrolledWindow::$path data

    set grid $path._grid
    bind $grid <Configure> {}
    set data(realized) 1
    place $grid -anchor nw -x 0 -y 0 -relwidth 1.0 -relheight 1.0
}
