set pl_menu(front)		"ae 0 0"
set pl_menu(left)		"ae 90 0"
set pl_menu(rear)		"ae 180 0"
set pl_menu(right)		"ae 270 0"
set pl_menu(top)		"ae -90 90"
set pl_menu(bottom)		"ae -90 -90"
set pl_menu(35,25)		"ae 35 25"
set pl_menu(45,45)		"ae 45 45"

proc menu_init {} {
    global pl_menu

    toplevel .menu
    listbox .menu.l -bd 2 -exportselection false
    pack .menu.l -fill both -expand yes
    bind .menu.l <Button-1> "do_list_cmd %W %y"
    bind .menu.l <Button-2> "do_list_cmd %W %y"

    foreach item [array names pl_menu] {
	.menu.l insert end $item
    }

    .menu.l configure -height [array size pl_menu] -width 10
}

proc do_list_cmd { w y } {
    global pl_menu

    set curr_sel [$w curselection]
    if { $curr_sel != "" } {
	$w selection clear $curr_sel
    }

    set i [$w nearest $y]
    $w selection set $i
    eval $pl_menu([$w get $i])
}