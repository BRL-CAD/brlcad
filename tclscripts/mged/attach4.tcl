if { [info exists tk_strictMotif] == 0 } {
    loadtk
}

set extern_commands "attach"
foreach cmd $extern_commands {
    if {[string compare [info command $cmd] $cmd] != 0} {
	puts stderr "Application fails to provide command '$cmd'"
	return
    }
}

# Open 4 display manager windows of type dtype
proc attach4 args {
    global multi_view
    global attach4_usage
    global faceplate
    global mged_top
    global mged_dmc
    global win_size

    if { [llength $args] < 3 } {
	return [help attach4]
    }

    set id [lindex $args 0]
    set screen [lindex $args 1]
    set dtype [lindex $args 2]

    set mged_top($id) .top$id
    set mged_dmc($id) $mged_top($id)

    toplevel $mged_top($id) -screen $screen -relief sunken -borderwidth 2
    set win_size($id) [expr [winfo screenheight $mged_top($id)] - 24]
    set mv_size [expr $win_size($id) / 2 - 4]
    openmv $mged_top($id) $id $dtype $mv_size

    if [catch {attach -t 0 -n $mged_top($id).$id -S $win_size($id) $dtype} result] {
	releasemv $id
	destroy $mged_top($id)
	return
    }

    set multi_view($id) 0
    pack $mged_top($id).$id -expand 1 -fill both
    setupmv $id
    set faceplate 1
    wm protocol $mged_top($id) WM_DELETE_WINDOW "release4 $id"
    wm title $mged_top($id) "$id's $dtype Display"
    wm resizable $mged_top($id) 0 0
}

proc release4 args {
    global multi_view
    global mged_top
    global release4_usage

    if { [llength $args] < 1 } {
	return [help release4]
    }

    set id [lindex $args 0]
    catch { releasemv $id }
    catch { release $mged_top($id).$id }
    catch { destroy $mged_top($id) }
    set multi_view($id) 0
    set id ""
}
