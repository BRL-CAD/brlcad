#
#			A P P L Y . T C L
#
#	Procedures to apply commands to one or more display managers.
#
#	Author - Robert G. Parker
#

proc mged_apply { id cmd } {
    global mged_active_dm
    global mged_dm_loc
    global mged_apply_to

    if {$mged_apply_to($id) == 1} {
	mged_apply_local $id $cmd
    } elseif {$mged_apply_to($id) == 2} {
	mged_apply_using_list $id $cmd
    } elseif {$mged_apply_to($id) == 3} {
	mged_apply_all $cmd
    } else {
	winset $mged_active_dm($id)
#	catch [list uplevel #0 $cmd]
	catch { uplevel #0 $cmd }
    }
}

proc mged_apply_local { id cmd } {
    global mged_top
    global mged_active_dm

    winset $mged_top($id).ul
#    catch [list uplevel #0 $cmd]
    catch { uplevel #0 $cmd } msg

    winset $mged_top($id).ur
#    catch [list uplevel #0 $cmd]
    catch { uplevel #0 $cmd } msg

    winset $mged_top($id).ll
#    catch [list uplevel #0 $cmd]
    catch { uplevel #0 $cmd } msg

    winset $mged_top($id).lr
#    catch [list uplevel #0 $cmd] msg
    catch { uplevel #0 $cmd } msg

    winset $mged_active_dm($id)

    return $msg
}

proc mged_apply_using_list { id cmd } {
    global mged_apply_list

    foreach dm $mged_apply_list($id) {
	winset $dm
#	catch [list uplevel #0 $cmd] msg
	catch { uplevel #0 $cmd } msg
    }

    return $msg
}

proc mged_apply_all { cmd } {
    foreach dm [get_dm_list] {
	winset $dm
#	catch [list uplevel #0 $cmd] msg
	catch { uplevel #0 $cmd } msg
    }

    return $msg
}
