#
#			A P P L Y . T C L
#
#	Procedures to apply commands to one or more display managers.
#
#	Author - Robert G. Parker
#

proc mged_apply { id cmd } {
    global mged_gui

    if {$mged_gui($id,apply_to) == 1} {
	mged_apply_local $id $cmd
    } elseif {$mged_gui($id,apply_to) == 2} {
	mged_apply_using_list $id $cmd
    } elseif {$mged_gui($id,apply_to) == 3} {
	mged_apply_all $cmd
    } else {
	winset $mged_gui($id,active_dm)
	catch { uplevel #0 $cmd }
    }
}

proc mged_apply_local { id cmd } {
    global mged_gui

    winset $mged_gui($id,top).ul
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).ur
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).ll
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,top).lr
    catch { uplevel #0 $cmd } msg

    winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_using_list { id cmd } {
    global mged_gui

    set msg ""
    foreach dm $mged_gui($id,apply_list) {
	winset $dm
	catch { uplevel #0 $cmd } msg
    }

    winset $mged_gui($id,active_dm)

    return $msg
}

proc mged_apply_all { cmd } {
    global mged_gui

    set msg ""
    foreach dm [get_dm_list] {
	winset $dm
	catch { uplevel #0 $cmd } msg
    }

    winset $mged_gui($id,active_dm)

    return $msg
}
