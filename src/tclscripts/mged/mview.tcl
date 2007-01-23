#                       M V I E W . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
# Author - Bob Parker

check_externs "_mged_attach"

if ![info exists mged_default(bd)] {
    set mged_default(bd) 2
}

proc openmv { id w wc dpy dtype } {
    global win_to_id
    global mged_default
    global faceplate
    global orig_gui
    global perspective_mode

    frame $wc.ulF -relief sunken -borderwidth $mged_default(bd)
    frame $wc.urF -relief sunken -borderwidth $mged_default(bd)
    frame $wc.llF -relief sunken -borderwidth $mged_default(bd)
    frame $wc.lrF -relief sunken -borderwidth $mged_default(bd)

    attach -d $dpy -t 0 -n $w.ul $dtype
    dm set zclip $mged_default(zclip)
    if { $dtype == "ogl" } {
	dm set zbuffer $mged_default(zbuffer)
	dm set lighting $mged_default(lighting)
    }
    set faceplate $mged_default(faceplate)
    set orig_gui $mged_default(orig_gui)
    set perspective_mode $mged_default(perspective_mode)

    attach -d $dpy -t 0 -n $w.ur $dtype
    dm set zclip $mged_default(zclip)
    if { $dtype == "ogl" } {
	dm set zbuffer $mged_default(zbuffer)
	dm set lighting $mged_default(lighting)
    }
    set faceplate $mged_default(faceplate)
    set orig_gui $mged_default(orig_gui)
    set perspective_mode $mged_default(perspective_mode)

    attach -d $dpy -t 0 -n $w.ll $dtype
    dm set zclip $mged_default(zclip)
    if { $dtype == "ogl" } {
	dm set zbuffer $mged_default(zbuffer)
	dm set lighting $mged_default(lighting)
    }
    set faceplate $mged_default(faceplate)
    set orig_gui $mged_default(orig_gui)
    set perspective_mode $mged_default(perspective_mode)

    attach -d $dpy -t 0 -n $w.lr $dtype
    dm set zclip $mged_default(zclip)
    if { $dtype == "ogl" } {
	dm set zbuffer $mged_default(zbuffer)
	dm set lighting $mged_default(lighting)
    }
    set faceplate $mged_default(faceplate)
    set orig_gui $mged_default(orig_gui)
    set perspective_mode $mged_default(perspective_mode)

    set win_to_id($w.ul) $id
    set win_to_id($w.ur) $id
    set win_to_id($w.ll) $id
    set win_to_id($w.lr) $id

    grid $w.ul -in $wc.ulF -sticky "nsew" -row 0 -column 0
    grid $w.ur -in $wc.urF -sticky "nsew" -row 0 -column 0
    grid $w.ll -in $wc.llF -sticky "nsew" -row 0 -column 0
    grid $w.lr -in $wc.lrF -sticky "nsew" -row 0 -column 0

    grid rowconfigure $wc.ulF 0 -weight 1
    grid columnconfigure $wc.ulF 0 -weight 1
    grid rowconfigure $wc.urF 0 -weight 1
    grid columnconfigure $wc.urF 0 -weight 1
    grid rowconfigure $wc.llF 0 -weight 1
    grid columnconfigure $wc.llF 0 -weight 1
    grid rowconfigure $wc.lrF 0 -weight 1
    grid columnconfigure $wc.lrF 0 -weight 1
}

proc mview_build_menubar { id } {
    global mged_gui

    set w $mged_gui($id,top)

    if {$mged_gui($id,top) == $mged_gui($id,dmc)} {
	.$id.menubar clone $w.menubar menubar
	$w configure -menu $w.menubar

	menu_accelerator_bindings_for_clone $id $w $w.ul ul
	menu_accelerator_bindings_for_clone $id $w $w.ur ur
	menu_accelerator_bindings_for_clone $id $w $w.ll ll
	menu_accelerator_bindings_for_clone $id $w $w.lr lr
    } else {
	menu_accelerator_bindings $id $w.ul ul
	menu_accelerator_bindings $id $w.ur ur
	menu_accelerator_bindings $id $w.ll ll
	menu_accelerator_bindings $id $w.lr lr
    }
}

proc menu_accelerator_bindings_for_clone { id parent w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-F> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-f> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#file %X %Y; break"
    bind $w <Alt-E> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-e> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#edit %X %Y; break"
    bind $w <Alt-C> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-c> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#create %X %Y; break"
    bind $w <Alt-V> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-v> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#view %X %Y; break"
    bind $w <Alt-R> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-r> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#viewring %X %Y; break"
    bind $w <Alt-S> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-s> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#settings %X %Y; break"
    bind $w <Alt-M> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-m> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#modes %X %Y; break"
    bind $w <Alt-I> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#misc %X %Y; break"
    bind $w <Alt-i> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#misc %X %Y; break"
    bind $w <Alt-T> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-t> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#tools %X %Y; break"
    bind $w <Alt-H> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
    bind $w <Alt-h> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup $parent.menubar.#$id#menubar#help %X %Y; break"
}

proc menu_accelerator_bindings { id w pos } {
    bind $w <Alt-ButtonPress-1> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-ButtonPress-2> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-F> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-f> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.file %X %Y; break"
    bind $w <Alt-E> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-e> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.edit %X %Y; break"
    bind $w <Alt-C> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-c> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.create %X %Y; break"
    bind $w <Alt-V> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-v> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.view %X %Y; break"
    bind $w <Alt-R> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-r> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.viewring %X %Y; break"
    bind $w <Alt-S> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-s> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.settings %X %Y; break"
    bind $w <Alt-M> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-m> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.modes %X %Y; break"
    bind $w <Alt-T> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-t> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.tools %X %Y; break"
    bind $w <Alt-H> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
    bind $w <Alt-h> "set mged_gui($id,dm_loc) $pos; set_active_dm $id;\
	    tk_popup .$id.menubar.help %X %Y; break"
}

proc packmv { id } {
    global mged_gui

    grid $mged_gui($id,dmc).ulF -sticky "nsew" -row 0 -column 0
    grid $mged_gui($id,dmc).urF -sticky "nsew" -row 0 -column 1
    grid $mged_gui($id,dmc).llF -sticky "nsew" -row 1 -column 0
    grid $mged_gui($id,dmc).lrF -sticky "nsew" -row 1 -column 1
}

proc unpackmv { id } {
    global mged_gui

    catch { eval grid forget [grid slaves $mged_gui($id,dmc)] }
}

proc releasemv { id } {
    global mged_gui

    catch  { release $mged_gui($id,top).ul }
    catch  { release $mged_gui($id,top).ur }
    catch  { release $mged_gui($id,top).ll }
    catch  { release $mged_gui($id,top).lr }
}

proc closemv { id } {
    global mged_gui

    releasemv $id
    catch { destroy $mged_gui($id,dmc) }
}

proc setupmv { id } {
    global mged_gui
    global mged_default
    global faceplate

    set_default_views $id
#    mged_apply_local $id "set faceplate $mged_default(faceplate)"

    grid columnconfigure $mged_gui($id,dmc) 0 -weight 1
    grid columnconfigure $mged_gui($id,dmc) 1 -weight 1
    grid rowconfigure $mged_gui($id,dmc) 0 -weight 1
    grid rowconfigure $mged_gui($id,dmc) 1 -weight 1
}

proc set_default_views { id } {
    global mged_gui

    winset $mged_gui($id,top).ul
    _mged_press reset
    catch {ae 0 90}

    winset $mged_gui($id,top).ur
    _mged_press reset
    catch {press 35,25}

    winset $mged_gui($id,top).ll
    _mged_press reset
    catch {press front}

    winset $mged_gui($id,top).lr
    _mged_press reset
    catch {press left}
}

proc setmv { id } {
    global mged_gui

    if { $mged_gui($id,multi_pane) } {
	# insure that the weight is not exaggerated
	grid columnconfigure $mged_gui($id,dmc) 0 -weight 1

	unpackmv $id
	packmv $id
    } else {
	# exaggerate the weight so that the single display manager window
	# will grow to completely fill the container window
	grid columnconfigure $mged_gui($id,dmc) 0 -weight 1000

	unpackmv $id
	grid $mged_gui($id,dmc).$mged_gui($id,dm_loc)\F -in $mged_gui($id,dmc) \
		-sticky "nsew" -row 0 -column 0
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
