proc _init_dm { func w } {
    $func $w
    catch { init_dm $w }
}

proc bind_dm { w } {
}

## - init_dm_obj
#
# Called to initialize display manager objects.
#
proc init_dm_obj { w } {
    if ![winfo exists $w] {
	return
    }

    bind $w <Configure> {catch {%W configure}}
}
# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
