#                        Q R A Y . T C L
# BRL-CAD
#
# Copyright (c) 2004-2026 United States Government as represented by
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
#
# Description -
#	Control Panel for MGED's Query Ray behavior.
#

check_externs "::brlcad::mged"

namespace eval ::brlcad::mged::qray {

namespace export init

variable color_fields {
    {oddcolor oddColor}
    {evencolor evenColor}
    {voidcolor voidColor}
    {overlapcolor overlapColor}
}
variable format_fields {
    {r fmt_ray}
    {h fmt_head}
    {p fmt_partition}
    {f fmt_foot}
    {m fmt_miss}
    {o fmt_overlap}
    {g fmt_gap}
}
variable panel_padding 4
variable qray_control

proc init { id } {
    global mged_gui
    variable color_fields
    variable panel_padding
    variable qray_control
    global mouse_behavior
    global use_air
    global ::tk::Priv

    if {[::brlcad::mged opendb] eq ""} {
	cad_dialog $::tk::Priv(cad_dialog) $mged_gui($id,screen) "No database." \
	    "No database has been opened!" info 0 OK
	return
    }

    set top .$id.qray_control

    if [winfo exists $top] {
	raise $top

	return
    }

    if {$mouse_behavior eq "q"} {
	set qray_control($id,active) 1
    } else {
	set qray_control($id,active) 0
    }

    set qray_control($id,use_air) $use_air
    set qray_control($id,cmd_echo) [::brlcad::mged qray echo]
    set qray_control($id,effects) [::brlcad::mged qray effects]
    set qray_control($id,basename) [::brlcad::mged qray basename]
    foreach color_field $color_fields {
	lassign $color_field setting widget_key
	set qray_control($id,$setting) [::brlcad::mged qray $setting]
    }

    toplevel $top -screen $mged_gui($id,screen)

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridFF1
    label $top.colorL -text "Query Ray Colors"
    hoc_register_data $top.colorL "Query Ray Colors"\
	{ { summary "A query ray is a ray that is fired from
a point in the view plane as specified by
the user with a mouse click. The user can
fire from precise points by activating grid
snapping. In general, the query ray is used
to hit objects in model space. The user can
then select among those that are hit for the
purposes of editing or for getting specific
information about the objects. By enabling
graphics effects the user can see the ray
drawn through the geometry with the color of
the ray segments changing as the ray enters/leaves
an object. More specifically, the \"odd\" objects
encountered can be colored one way, while the
\"even\" objects can be colored another. Where
no objects were hit (i.e. void), yet another
color can be used. Lastly, overlaps can also be
colored, distinguishing them from everything else." } }

    frame $top.oddColorFF
    label $top.oddColorL -text "odd" -anchor e
    hoc_register_data $top.oddColorL "Odd Color"\
	    { { summary "The odd ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top oddColor \
	::brlcad::mged::qray::qray_control($id,oddcolor) \
	[list color_entry_chooser $id $top oddColor "Odd Color" \
	    ::brlcad::mged::qray::qray_control $id,oddcolor] \
	12 $qray_control($id,oddcolor) not_rt

    frame $top.evenColorFF
    label $top.evenColorL -text "even" -anchor e
    hoc_register_data $top.evenColorL "Even Color"\
	    { { summary "The even ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top evenColor \
	::brlcad::mged::qray::qray_control($id,evencolor) \
	[list color_entry_chooser $id $top evenColor "Even Color" \
	    ::brlcad::mged::qray::qray_control $id,evencolor] \
	8 $qray_control($id,evencolor) not_rt

    frame $top.voidColorFF
    label $top.voidColorL -text "void" -anchor e
    hoc_register_data $top.voidColorL "Void Color"\
	    { { summary "The void ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top voidColor \
	::brlcad::mged::qray::qray_control($id,voidcolor) \
	[list color_entry_chooser $id $top voidColor "Void Color" \
	    ::brlcad::mged::qray::qray_control $id,voidcolor] \
	8 $qray_control($id,voidcolor) not_rt

    frame $top.overlapColorFF
    label $top.overlapColorL -text "overlap" -anchor e
    hoc_register_data $top.overlapColorL "Overlap Color"\
	    { { summary "The overlap ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top overlapColor \
	::brlcad::mged::qray::qray_control($id,overlapcolor) \
	[list color_entry_chooser $id $top overlapColor "Overlap Color" \
	    ::brlcad::mged::qray::qray_control $id,overlapcolor] \
	8 $qray_control($id,overlapcolor) not_rt

    grid $top.colorL - -sticky "ew" -in $top.gridFF1
    grid $top.oddColorL $top.oddColorF -sticky "nsew" -in $top.gridFF1
    grid $top.evenColorL $top.evenColorF -sticky "nsew" -in $top.gridFF1
    grid $top.voidColorL $top.voidColorF -sticky "nsew" -in $top.gridFF1
    grid $top.overlapColorL $top.overlapColorF -sticky "nsew" -in $top.gridFF1
    grid columnconfigure $top.gridFF1 1 -weight 1
    grid rowconfigure $top.gridFF1 1 -weight 1
    grid rowconfigure $top.gridFF1 2 -weight 1
    grid rowconfigure $top.gridFF1 3 -weight 1
    grid rowconfigure $top.gridFF1 4 -weight 1
    grid $top.gridFF1 -sticky "nsew" -in $top.gridF1 \
	    -padx $panel_padding -pady $panel_padding
    grid columnconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1

    frame $top.gridF2 -relief groove -bd 2
    frame $top.gridFF2
    set hoc_data { { summary "The base name is used to build the names
of fake solids that are created for the ray.
Specifically, there is one solid created for
each color used. Note that it is possible to
create a maximum of four fake solids as a
result of firing a query ray." } }
    label $top.bnameL -text "Base Name" -anchor e
    hoc_register_data $top.bnameL "Base Name" $hoc_data
    entry $top.bnameE -relief sunken -bd 2 \
	-textvariable ::brlcad::mged::qray::qray_control($id,basename)
    hoc_register_data $top.bnameE "Base Name" $hoc_data
    grid $top.bnameL $top.bnameE -sticky "nsew" -in $top.gridFF2
    grid columnconfigure $top.gridFF2 1 -weight 1
    grid rowconfigure $top.gridFF2 0 -weight 1
    grid $top.gridFF2 -sticky "nsew" -in $top.gridF2 \
	    -padx $panel_padding -pady $panel_padding
    grid columnconfigure $top.gridF2 0 -weight 1
    grid rowconfigure $top.gridF2 0 -weight 1

    frame $top.gridF3 -relief groove -bd 2
    label $top.effectsL -text "Effects" -anchor w
    hoc_register_data $top.effectsL "Query Ray Effects"\
	    { { summary "The query ray can produce textual
and graphical output. The textual output
consists of information about the ray
as specified by the format strings.
Graphical output consists of the query
rays being drawn through the geometry." } }
    checkbutton $top.cmd_echoCB -relief flat -text "Echo Cmd"\
	    -offvalue 0 -onvalue 1 \
	    -variable ::brlcad::mged::qray::qray_control($id,cmd_echo)
    hoc_register_data $top.cmd_echoCB "Echo Cmd"\
	    { { summary "Toggle echoing of the command used
to fire a query ray." }
	      { see_also "nirt" } }
    menubutton $top.effectsMB \
	    -textvariable ::brlcad::mged::qray::qray_control($id,text_effects)\
	    -menu $top.effectsMB.m -indicatoron 1
    hoc_register_data $top.effectsMB "Query Ray Effects"\
	    { { summary "Pops up a menu of query ray effects." } }
    menu $top.effectsMB.m -title "Query Ray Effects" -tearoff 0
    $top.effectsMB.m add radiobutton -value t \
	    -variable ::brlcad::mged::qray::qray_control($id,effects)\
	    -label "Text" \
	    -command [list ::brlcad::mged::qray::update_effects $id]
    hoc_register_menu_data "Query Ray Effects" "Text" "Query Ray Effects - Text"\
	    { { summary "Produce only textual output. This consists
of information about the ray as specified
by the format strings." } }
    $top.effectsMB.m add radiobutton -value g \
	    -variable ::brlcad::mged::qray::qray_control($id,effects)\
	    -label "Graphics" \
	    -command [list ::brlcad::mged::qray::update_effects $id]
    hoc_register_menu_data "Query Ray Effects" "Graphics" "Query Ray Effects - Graphics"\
	    { { summary "Produce only graphical output. This consists
of the query rays being drawn through
the geometry." } }
    $top.effectsMB.m add radiobutton -value b \
	    -variable ::brlcad::mged::qray::qray_control($id,effects)\
	    -label "Both" \
	    -command [list ::brlcad::mged::qray::update_effects $id]
    hoc_register_menu_data "Query Ray Effects" "Both" "Query Ray Effects - Both"\
	    { { summary "Produce both textual and graphical output.
The textual output consists of information
about the ray as specified by the format
strings. Graphical output consists of the
query rays being drawn through the geometry." } }
    grid $top.effectsL x $top.cmd_echoCB x $top.effectsMB\
	    -sticky "ew" -in $top.gridF3 \
	    -padx $panel_padding -pady $panel_padding
    grid columnconfigure $top.gridF3 1 -weight 1
    grid columnconfigure $top.gridF3 3 -weight 1

    frame $top.gridF4
    checkbutton $top.activeCB -relief flat -text "Mouse Active"\
	    -offvalue 0 -onvalue 1 \
	    -variable ::brlcad::mged::qray::qray_control($id,active)
    hoc_register_data $top.activeCB "Mouse Active"\
	    { { summary "Toggle query ray mode. When the mouse
behavior mode is \"query ray\" the mouse
can be used to fire query rays through
the geometry. This is done by pressing
the same mouse button that is used to
center the view in default mouse behavior
mode." } }
    checkbutton $top.use_airCB -relief flat -text "Use Air"\
	    -offvalue 0 -onvalue 1 \
	    -variable ::brlcad::mged::qray::qray_control($id,use_air)
    hoc_register_data $top.use_airCB "Use Air"\
	    { { summary "Toggle whether or not to use air. By
default air is ignored." } }
    button $top.advB -relief raised -text "Advanced..."\
	    -command [list ::brlcad::mged::qray::init_advanced $id]
    hoc_register_data $top.advB "Advanced Query Ray Settings"\
	    { { summary "Pop up the advanced query ray settings
control panel." } }
    grid $top.activeCB $top.use_airCB x $top.advB -in $top.gridF4 \
	-padx $panel_padding

    frame $top.gridF5
    button $top.okB -relief raised -text "OK"\
	    -command [list ::brlcad::mged::qray::ok $id $top]
    hoc_register_data $top.okB "OK"\
	    { { summary "Apply the query ray control panel settings
to MGED's internal state, then close the
query ray control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command [list ::brlcad::mged::qray::apply $id]
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the query ray control panel settings
to MGED's internal state." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command [list ::brlcad::mged::qray::reset $id]
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Set the query ray control panel according
to MGED's internal state." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command [list catch [list destroy $top]]
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Close the query ray control panel." } }
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew" -in $top.gridF5
    grid columnconfigure $top.gridF5 2 -weight 1
    grid columnconfigure $top.gridF5 4 -weight 1

    grid $top.gridF1 -sticky "nsew" \
	-padx $panel_padding -pady $panel_padding
    grid $top.gridF2 -sticky "nsew" \
	-padx $panel_padding -pady $panel_padding
    grid $top.gridF3 -sticky "ew" \
	-padx $panel_padding -pady $panel_padding
    grid $top.gridF4 -sticky "ew" \
	-padx $panel_padding -pady $panel_padding
    grid $top.gridF5 -sticky "ew" \
	-padx $panel_padding -pady $panel_padding
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 4
    grid rowconfigure $top 1 -weight 1

    reset $id
    set qray_control($id,effects) [::brlcad::mged qray effects]
    update_effects $id

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW [list catch [list destroy $top]]
    wm title $top "Query Ray Control Panel ($id)"
}

proc ok { id top } {
    apply $id
    catch { destroy $top }
}

proc apply { id } {
    global mged_gui
    global mouse_behavior
    variable color_fields
    variable qray_control

    if {$qray_control($id,active)} {
	set mged_gui($id,mouse_behavior) q
    } elseif {$mouse_behavior eq "q"} {
	set mged_gui($id,mouse_behavior) d
    }

    set mged_gui($id,use_air) $qray_control($id,use_air)

    set commands [list \
	[list set mouse_behavior $mged_gui($id,mouse_behavior)] \
	[list set use_air $qray_control($id,use_air)] \
	[list ::brlcad::mged qray echo $qray_control($id,cmd_echo)] \
	[list ::brlcad::mged qray effects $qray_control($id,effects)] \
	[list ::brlcad::mged qray basename $qray_control($id,basename)]]
    set top .$id.qray_control
    foreach color_field $color_fields {
	lassign $color_field setting widget_key
	set color [getRGB ${top}.${widget_key}MB \
	    $qray_control($id,$setting)]
	lappend commands [list ::brlcad::mged qray $setting {*}$color]
    }
    mged_apply $id [join $commands {; }]
}

proc reset { id } {
    global mged_gui
    variable color_fields
    variable qray_control
    global use_air
    global mouse_behavior

    set top .$id.qray_control

    if ![winfo exists $top] {
	return
    }

    winset $mged_gui($id,active_dm)

    if {$mouse_behavior eq "q"} {
	set qray_control($id,active) 1
    } else {
	set qray_control($id,active) 0
    }

    set qray_control($id,use_air) $use_air
    set qray_control($id,cmd_echo) [::brlcad::mged qray echo]
    set qray_control($id,effects) [::brlcad::mged qray effects]
    set qray_control($id,basename) [::brlcad::mged qray basename]

    update_effects $id

    foreach color_field $color_fields {
	lassign $color_field setting widget_key
	set qray_control($id,$setting) [::brlcad::mged qray $setting]
	set variable_name ::brlcad::mged::qray::qray_control($id,$setting)
	setWidgetRGBColor ${top}.${widget_key}MB $variable_name \
	    $qray_control($id,$setting)
    }
}

## - update_effects
#
# Update qray_control($id,text_effects)
#
proc update_effects { id } {
    variable qray_control

    switch $qray_control($id,effects) {
	t {
	    set qray_control($id,text_effects) "Text"
	}
	g {
	    set qray_control($id,text_effects) "Graphics"
	}
	b {
	    set qray_control($id,text_effects) "Both"
	}
    }
}

proc apply_effects {id} {
    global mged_gui

    mged_apply $id [list ::brlcad::mged qray effects \
	$mged_gui($id,qray_effects)]
}

proc init_advanced { id } {
    global mged_gui
    variable panel_padding
    variable qray_control

    set top .$id.qray_adv

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $mged_gui($id,screen)
    format_reset $id

    frame $top.gridF1 -relief groove -bd 2
    frame $top.gridFF1
    label $top.fmtL -text "Query Ray Formats"
    hoc_register_data $top.fmtL "Query Ray Formats"\
	{ { summary "Ask pjt@arl.army.mil about the six different
format strings that can be set." } }

    label $top.rayL  -text "Ray" -anchor e
    hoc_register_data $top.rayL "Ray Format String"\
	{ { summary "Enter/edit the ray format string." }
	    { see_also "nirt" } }
    entry $top.rayE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_ray)
    hoc_register_data $top.rayE "Ray Format String"\
	{ { summary "Enter/edit the ray format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.headL -text "Head" -anchor e
    hoc_register_data $top.headL "Head Format String"\
	{ { summary "Enter/edit the head format string." }
	    { see_also "nirt" } }
    entry $top.headE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_head)
    hoc_register_data $top.headE "Head Format String"\
	{ { summary "Enter/edit the head format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.partitionL -text "Partition" -anchor e
    hoc_register_data $top.partitionL "Partition Format String"\
	{ { summary "Enter/edit the partition format string." }
	    { see_also "nirt" } }
    entry $top.partitionE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_partition)
    hoc_register_data $top.partitionE "Partition Format String"\
	{ { summary "Enter/edit the partition format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.footL -text "Foot" -anchor e
    hoc_register_data $top.footL "Foot Format String"\
	{ { summary "Enter/edit the foot format string." }
	    { see_also "nirt" } }
    entry $top.footE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_foot)
    hoc_register_data $top.footE "Foot Format String"\
	{ { summary "Enter/edit the foot format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.missL -text "Miss" -anchor e
    hoc_register_data $top.missL "Miss Format String"\
	{ { summary "Enter/edit the miss format string." }
	    { see_also "nirt" } }
    entry $top.missE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_miss)
    hoc_register_data $top.missE "Miss Format String"\
	{ { summary "Enter/edit the miss format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.overlapL -text "Overlap" -anchor e
    hoc_register_data $top.overlapL "Overlap Format String"\
	{ { summary "Enter/edit the overlay format string." }
	    { see_also "nirt" } }
    entry $top.overlapE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_overlap)
    hoc_register_data $top.overlapE "Overlap Format String"\
	{ { summary "Enter/edit the overlap format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.gapL -text "Gap" -anchor e
    hoc_register_data $top.gapL "Gap Format String"\
	{ { summary "Enter/edit the gap format string." }
	    { see_also "nirt" } }
    entry $top.gapE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,fmt_gap)
    hoc_register_data $top.gapE "Gap Format String"\
	{ { summary "Enter/edit the gap format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    label $top.scriptL -text "Script" -anchor e
    hoc_register_data $top.scriptL "Script String"\
	{ { summary "Enter/edit the nirt script string." }
	    { see_also "nirt" } }
    entry $top.scriptE -relief sunken -bd 2 -width 80 \
	-textvariable ::brlcad::mged::qray::qray_control($id,script)
    hoc_register_data $top.scriptE "Script String"\
	{ { summary "Enter/edit the nirt script string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
	    { see_also "nirt" } }

    frame $top.gridF2
    button $top.okB -relief raised -text "OK"\
	-command [list ::brlcad::mged::qray::format_ok $id $top]
    hoc_register_data $top.okB "OK"\
	{ { summary "Apply the format string settings to
MGED's internal state, then close the
advanced settings control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	-command [list ::brlcad::mged::qray::format_apply $id]
    hoc_register_data $top.applyB "Apply"\
	{ { summary "Apply the format string settings to
MGED's internal state." } }
    button $top.resetB -relief raised -text "Reset"\
	-command [list ::brlcad::mged::qray::format_reset $id]
    hoc_register_data $top.resetB "Reset"\
	{ { summary "Set the format strings in the control
panel according to MGED's internal state." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	-command [list catch [list destroy $top]]
    hoc_register_data $top.dismissB "Dismiss"\
	{ { summary "Close the advanced settings control panel." } }

    grid $top.fmtL - -sticky "ew" -in $top.gridFF1
    grid $top.rayL $top.rayE -sticky "nsew" -in $top.gridFF1
    grid $top.headL $top.headE -sticky "nsew" -in $top.gridFF1
    grid $top.partitionL $top.partitionE -sticky "nsew" -in $top.gridFF1
    grid $top.footL $top.footE -sticky "nsew" -in $top.gridFF1
    grid $top.missL $top.missE -sticky "nsew" -in $top.gridFF1
    grid $top.overlapL $top.overlapE -sticky "nsew" -in $top.gridFF1
    grid $top.gapL $top.gapE -sticky "nsew" -in $top.gridFF1
    grid $top.scriptL $top.scriptE -sticky "nsew" -in $top.gridFF1
    grid columnconfigure $top.gridFF1 1 -weight 1
    grid rowconfigure $top.gridFF1 1 -weight 1
    grid rowconfigure $top.gridFF1 2 -weight 1
    grid rowconfigure $top.gridFF1 3 -weight 1
    grid rowconfigure $top.gridFF1 4 -weight 1
    grid rowconfigure $top.gridFF1 5 -weight 1
    grid rowconfigure $top.gridFF1 6 -weight 1
    grid rowconfigure $top.gridFF1 7 -weight 1
    grid $top.gridFF1 -sticky "nsew" -in $top.gridF1 \
	-padx $panel_padding -pady $panel_padding
    grid columnconfigure $top.gridF1 0 -weight 1
    grid rowconfigure $top.gridF1 0 -weight 1

    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1
    grid columnconfigure $top.gridF2 4 -weight 1

    grid $top.gridF1 -sticky "nsew" \
	-padx $panel_padding -pady $panel_padding
    grid $top.gridF2 -sticky "ew" \
	-padx $panel_padding -pady $panel_padding
    grid columnconfigure $top 0 -weight 1
    grid rowconfigure $top 0 -weight 1

    place_near_mouse $top
    wm protocol $top WM_DELETE_WINDOW [list catch [list destroy $top]]
    wm title $top "Query Ray Advanced Settings ($id)"
}

proc format_ok { id top } {
    format_apply $id
    catch { destroy $top }
}

proc format_apply { id } {
    global mged_gui
    variable format_fields
    variable qray_control

    winset $mged_gui($id,active_dm)
    foreach format_field $format_fields {
	lassign $format_field format_code setting
	::brlcad::mged qray fmt $format_code $qray_control($id,$setting)
    }
    ::brlcad::mged qray script $qray_control($id,script)
}

proc format_reset { id } {
    global mged_gui
    variable format_fields
    variable qray_control

    winset $mged_gui($id,active_dm)
    foreach format_field $format_fields {
	lassign $format_field format_code setting
	set qray_control($id,$setting) \
	    [::brlcad::mged qray fmt $format_code]
    }
    set qray_control($id,script) [::brlcad::mged qray script]
}

}

proc init_qray_control {id} {
    tailcall ::brlcad::mged::qray::init $id
}

package provide brlcad::mged::qray 1.0

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
