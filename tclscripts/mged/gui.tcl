proc gui {args} {
    eval gui_create $args
}

proc gui_create {args} {
    global mged_gui_create

    eval $mged_gui_create $args
}

proc gui_destroy {args} {
    global mged_gui_destroy

    eval $mged_gui_destroy $args
}