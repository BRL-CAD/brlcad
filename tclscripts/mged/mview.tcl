set extern_commands "attach"
foreach cmd $extern_commands {
    if {[string compare [info command $cmd] $cmd] != 0} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}

proc openmv { w id dtype S } {
#puts "openmv $w $id $dtype $S"
    frame $w.u
    frame $w.l
    frame $w.u.l -relief sunken -borderwidth 2
    frame $w.u.r -relief sunken -borderwidth 2
    frame $w.l.l -relief sunken -borderwidth 2
    frame $w.l.r -relief sunken -borderwidth 2

    attach -t 0 -S $S -n $w.u.l.ul$id $dtype
    attach -t 0 -S $S -n $w.u.r.ur$id $dtype
    attach -t 0 -S $S -n $w.l.l.ll$id $dtype
#    attach -t 0 -W $S -N $S -n $w.l.r.lr$id $dtype

#    pack $w.u.l.ul$id $w.u.r.ur$id $w.l.l.ll$id $w.l.r.lr$id -expand 1 -fill both
    pack $w.u.l.ul$id $w.u.r.ur$id $w.l.l.ll$id -expand 1 -fill both
    pack $w.u.l $w.u.r -side left -anchor w -expand 1 -fill both
    pack $w.l.l $w.l.r -side left -anchor w -expand 1 -fill both
#puts "openmv: leave"
}


proc packmv { id } {
    global mged_dmc

    pack $mged_dmc($id).u $mged_dmc($id).l -expand 1 -fill both
}


proc unpackmv { id } {
    global mged_dmc

    pack forget $mged_dmc($id).u $mged_dmc($id).l
}


proc releasemv { id } {
    global mged_dmc

    catch { release $mged_dmc($id).u.l.ul$id }
    catch { release $mged_dmc($id).u.r.ur$id }
    catch { release $mged_dmc($id).l.l.ll$id }
#    catch { release $mged_dmc($id).l.r.lr$id }
}


proc closemv { id } {
    global mged_dmc

    releasemv $id
    catch { destroy $mged_dmc($id) }
}


proc setupmv { id } {
    global mged_top
    global mged_dmc
    global faceplate

    winset $mged_dmc($id).u.l.ul$id
    press 35,25
    set faceplate 0

    winset $mged_dmc($id).u.r.ur$id
    press right
    set faceplate 0

    winset $mged_dmc($id).l.l.ll$id
    press front
    set faceplate 0

    winset $mged_top($id).$id
    set faceplate 0

    bind $mged_top($id).$id m "togglemv $id"
    bind $mged_dmc($id).u.l.ul$id m "togglemv $id"
    bind $mged_dmc($id).u.r.ur$id m "togglemv $id"
    bind $mged_dmc($id).l.l.ll$id m "togglemv $id"
}


proc setmv { id } {
    global mged_top
    global mged_dmc
    global win_size
    global multi_view

    winset $mged_top($id).$id
    if $multi_view($id) {
	set mv_size [expr $win_size($id) / 2 - 4]
	pack forget $mged_top($id).$id
	dm size $mv_size $mv_size
	pack $mged_top($id).$id -in $mged_dmc($id).l.r
	packmv $id
    } else {
	unpackmv $id
	dm size $win_size($id) $win_size($id)
	pack $mged_top($id).$id -in $mged_dmc($id)
    }
}


proc togglemv { id } {
    global multi_view

    if $multi_view($id) {
	set multi_view($id) 0
    } else {
	set multi_view($id) 1
    }

    setmv $id
}
