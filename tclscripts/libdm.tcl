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