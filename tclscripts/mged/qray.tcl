#
#                       Q R A Y . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	Control Panel for MGED's Query Ray behavior.
#

proc init_qray_control { id } {
    global player_screen
    global mged_qray_control
    global mged_qray_effects
    global qray_control
    global mouse_behavior
    global use_air

    set top .$id.qray_control

    if [winfo exists $top] {
	raise $top
	set mged_qray_control($id) 1

	return
    }

    if ![info exists qray_control($id,oddcolor)] {
	if {$mouse_behavior == "q"} {
	    set qray_control($id,active) 1
	} else {
	    set qray_control($id,active) 0
	}

	set qray_control($id,use_air) $use_air
	set qray_control($id,cmd_echo) [qray echo]
	set qray_control($id,effects) [qray effects]
	set qray_control($id,basename) [qray basename]
	set qray_control($id,oddcolor) [qray oddcolor]
	set qray_control($id,evencolor) [qray evencolor]
	set qray_control($id,voidcolor) [qray voidcolor]
	set qray_control($id,overlapcolor) [qray overlapcolor]
    }

    set qray_control($id,padx) 4
    set qray_control($id,pady) 4

    toplevel $top -screen $player_screen($id)

    frame $top.gridF1 -relief groove -bd 2
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
    label $top.oddColorL -text "odd" -anchor w
    hoc_register_data $top.oddColorL "Odd Color"\
	    { { summary "The odd ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top oddColor qray_control($id,oddcolor)\
	    "color_entry_chooser $id $top oddColor \"Odd Color\"\
	    qray_control $id,oddcolor"\
	    12 $qray_control($id,oddcolor)
    grid $top.oddColorL -sticky "ew" -in $top.oddColorFF
    grid $top.oddColorF -sticky "ew" -in $top.oddColorFF
    grid columnconfigure $top.oddColorFF 0 -weight 1

    frame $top.evenColorFF
    label $top.evenColorL -text "even" -anchor w
    hoc_register_data $top.evenColorL "Even Color"\
	    { { summary "The even ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top evenColor qray_control($id,evencolor)\
	    "color_entry_chooser $id $top evenColor \"Even Color\"\
	    qray_control $id,evencolor"\
	    12 $qray_control($id,evencolor)
    grid $top.evenColorL -sticky "ew" -in $top.evenColorFF
    grid $top.evenColorF -sticky "ew" -in $top.evenColorFF
    grid columnconfigure $top.evenColorFF 0 -weight 1

    frame $top.voidColorFF
    label $top.voidColorL -text "void" -anchor w
    hoc_register_data $top.voidColorL "Void Color"\
	    { { summary "The void ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top voidColor qray_control($id,voidcolor)\
	    "color_entry_chooser $id $top voidColor \"Void Color\"\
	    qray_control $id,voidcolor"\
	    12 $qray_control($id,voidcolor)
    grid $top.voidColorL -sticky "ew" -in $top.voidColorFF
    grid $top.voidColorF -sticky "ew" -in $top.voidColorFF
    grid columnconfigure $top.voidColorFF 0 -weight 1

    frame $top.overlapColorFF
    label $top.overlapColorL -text "overlap" -anchor w
    hoc_register_data $top.overlapColorL "Overlap Color"\
	    { { summary "The overlap ray segments encountered along the
ray have their own color specification. This
allows the user to better visualize when the
ray enters/leaves an object." } }
    color_entry_build $top overlapColor qray_control($id,overlapcolor)\
	    "color_entry_chooser $id $top overlapColor \"Overlap Color\"\
	    qray_control $id,overlapcolor"\
	    12 $qray_control($id,overlapcolor)
    grid $top.overlapColorL -sticky "ew" -in $top.overlapColorFF
    grid $top.overlapColorF -sticky "ew" -in $top.overlapColorFF
    grid columnconfigure $top.overlapColorFF 0 -weight 1

    grid $top.colorL - -sticky "n" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid $top.oddColorFF $top.evenColorFF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid $top.voidColorFF $top.overlapColorFF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid columnconfigure $top.gridF1 0 -weight 1
    grid columnconfigure $top.gridF1 1 -weight 1
    grid $top.gridF1 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    frame $top.gridF2 -relief groove -bd 2
    frame $top.bnameF
    label $top.bnameL -text "Base Name" -anchor w
    hoc_register_data $top.bnameL "Base Name"\
	    { { summary "The base name is used to build the names
of fake solids that are created for the ray.
Specifically, there is one solid created for
each color used. Note that it is possible to
create a maximum of four fake solids as a
result of firing a query ray." } }
    entry $top.bnameE -relief sunken -bd 2 -textvar qray_control($id,basename)
    hoc_register_data $top.bnameE "Base Name"\
	    { { summary "Enter base name for query ray." } }
    grid $top.bnameL -sticky "ew" -in $top.bnameF
    grid $top.bnameE -sticky "ew" -in $top.bnameF
    grid columnconfigure $top.bnameF 0 -weight 1
    grid $top.bnameF -sticky "ew" -in $top.gridF2 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid columnconfigure $top.gridF2 0 -weight 1
    grid $top.gridF2 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

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
	    -offvalue 0 -onvalue 1 -variable qray_control($id,cmd_echo)
    hoc_register_data $top.cmd_echoCB "Echo Cmd"\
	    { { summary "Toggle echoing of the command." } }
    menubutton $top.effectsMB -textvariable qray_control($id,text_effects)\
	    -menu $top.effectsMB.m -indicatoron 1
    menu $top.effectsMB.m -title "Query Ray Effects" -tearoff 0
    $top.effectsMB.m add radiobutton -value t -variable qray_control($id,effects)\
	    -label "Text" -command "qray_effects $id"
    hoc_register_menu_data "Query Ray Effects" "Text" "Query Ray Effects - Text"\
	    { { summary "Produce only textual output. This consists
of information about the ray as specified
by the format strings." } }
    $top.effectsMB.m add radiobutton -value g -variable qray_control($id,effects)\
	    -label "Graphics" -command "qray_effects $id"
    hoc_register_menu_data "Query Ray Effects" "Graphics" "Query Ray Effects - Graphics"\
	    { { summary "Produce only graphical output. This consists
of the query rays being drawn through
the geometry." } }
    $top.effectsMB.m add radiobutton -value b -variable qray_control($id,effects)\
	    -label "Both" -command "qray_effects $id"
    hoc_register_menu_data "Query Ray Effects" "Both" "Query Ray Effects - Both"\
	    { { summary "Produce both textual and graphical output.
The textual output consists of information
about the ray as specified by the format
strings. Graphical output consists of the
query rays being drawn through the geometry." } }
    grid $top.effectsL x $top.cmd_echoCB x $top.effectsMB\
	    -sticky "ew" -in $top.gridF3 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid columnconfigure $top.gridF3 1 -weight 1
    grid columnconfigure $top.gridF3 3 -weight 1
    grid $top.gridF3 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    frame $top.gridF4
    checkbutton $top.activeCB -relief flat -text "Mouse Active"\
	    -offvalue 0 -onvalue 1 -variable qray_control($id,active)
    hoc_register_data $top.activeCB "Mouse Active"\
	    { { summary "Toggle query ray mode. When the mouse
behavior mode is \"query ray\" the mouse
can be used to fire query rays through
the geometry. This is done by pressing
the same mouse button that is used to
center the view in default mouse behavior
mode." } }
    checkbutton $top.use_airCB -relief flat -text "Use Air"\
	    -offvalue 0 -onvalue 1 -variable qray_control($id,use_air)
    hoc_register_data $top.use_airCB "Use Air"\
	    { { summary "Toggle whether or not to use air. By
default air is ignored." } }
    button $top.advB -relief raised -text "Advanced..."\
	    -command "init_qray_adv $id"
    hoc_register_data $top.advB "Advanced Query Ray Settings"\
	    { { summary "Pop up the advanced query ray settings
control panel." } }
    grid $top.activeCB $top.use_airCB x $top.advB -in $top.gridF4 -padx $qray_control($id,padx)
    grid $top.gridF4 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    frame $top.gridF5
    button $top.okB -relief raised -text "Ok"\
	    -command "qray_ok $id $top"
    hoc_register_data $top.okB "Ok"\
	    { { summary "Apply the query ray control panel settings
to MGED's internal state then close the
query ray control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "qray_apply $id"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the query ray control panel settings
to MGED's internal state." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "qray_reset $id"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Set the query ray control panel according
to MGED's internal state." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top; set mged_qray_control($id) 0 }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Close the query ray control panel." } }
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew" -in $top.gridF5
    grid columnconfigure $top.gridF5 2 -weight 1
    grid columnconfigure $top.gridF5 4 -weight 1
    grid $top.gridF5 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    grid columnconfigure $top 0 -weight 1

    qray_reset $id
    set qray_control($id,effects) [qray effects]
    qray_effects $id

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top; set mged_qray_control($id) 0 }"
    wm geometry $top +$x+$y
    wm title $top "Query Ray Control Panel ($id)"
}

proc qray_ok { id top } {
    global mged_qray_control

    qray_apply $id
    catch { destroy $top }

    set mged_qray_control($id) 0
}

proc qray_apply { id } {
    global mged_active_dm
    global mged_use_air
    global mged_mouse_behavior
    global mged_qray_effects
    global use_air
    global mouse_behavior
    global qray_control

    winset $mged_active_dm($id)

    if {$qray_control($id,active)} {
	set mouse_behavior q
	set mged_mouse_behavior($id) q
    } elseif {$mouse_behavior == "q"} {
	set mouse_behavior d
	set mged_mouse_behavior($id) d
    }

    set use_air $qray_control($id,use_air)
    set mged_use_air($id) $qray_control($id,use_air)

    qray echo $qray_control($id,cmd_echo)

    qray effects $qray_control($id,effects)
    set mged_qray_control($id,effects) $qray_control($id,effects)

    qray basename $qray_control($id,basename)

    eval qray oddcolor $qray_control($id,oddcolor)
    eval qray evencolor $qray_control($id,evencolor)
    eval qray voidcolor $qray_control($id,voidcolor)
    eval qray overlapcolor $qray_control($id,overlapcolor)
}

proc qray_reset { id } {
    global mged_active_dm
    global mged_use_air
    global qray_control
    global use_air
    global mouse_behavior

    winset $mged_active_dm($id)

    set top .$id.qray_control

    if {$mouse_behavior == "q"} {
	set qray_control($id,active) 1
    } else {
	set qray_control($id,active) 0
    }

    set qray_control($id,use_air) $use_air
    set qray_control($id,cmd_echo) [qray echo]
    set qray_control($id,effects) [qray effects]
    set qray_control($id,basename) [qray basename]

    set qray_control($id,oddcolor) [qray oddcolor]
    set_WidgetRGBColor $top.oddColorMB $qray_control($id,oddcolor)

    set qray_control($id,evencolor) [qray evencolor]
    set_WidgetRGBColor $top.evenColorMB $qray_control($id,evencolor)

    set qray_control($id,voidcolor) [qray voidcolor]
    set_WidgetRGBColor $top.voidColorMB $qray_control($id,voidcolor)

    set qray_control($id,overlapcolor) [qray overlapcolor]
    set_WidgetRGBColor $top.overlapColorMB $qray_control($id,overlapcolor)
}

proc qray_effects { id } {
    global mged_qray_effects
    global qray_control

    set top .$id.qray_control

    switch $qray_control($id,effects) {
	t {
	    set qray_control($id,text_effects) "Text"
	}
	g {
	    set qray_control($id,text_effects) "Graphics"
	}
	b {
	    set qray_control($id,text_effects) "both"
	}
    }
}

proc init_qray_adv { id } {
    global player_screen
    global qray_control

    set top .$id.qray_adv

    if [winfo exists $top] {
	raise $top

	return
    }

    toplevel $top -screen $player_screen($id)
    qray_reset_fmt $id

    frame $top.gridF1 -relief groove -bd 2
    label $top.fmtL -text "Query Ray Formats"
    hoc_register_data $top.fmtL "Query Ray Formats"\
	    { { summary "Ask pjt@arl.mil about the six different
format strings that can be set." } }
    grid $top.fmtL -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.rayF
    label $top.rayL  -text "Ray" -anchor w
    hoc_register_data $top.rayL "Ray Format String"\
	    { { summary "Ask pjt@arl.mil about the ray format string." }
              { see_also "nirt" } }
    entry $top.rayE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_ray)
    hoc_register_data $top.rayE "Ray Format String"\
	    { { summary "Enter the ray format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.rayL -sticky "ew" -in $top.rayF
    grid $top.rayE -sticky "ew" -in $top.rayF
    grid columnconfigure $top.rayF 0 -weight 1
    grid $top.rayF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.headF
    label $top.headL -text "Head" -anchor w
    hoc_register_data $top.headL "Head Format String"\
	    { { summary "Ask pjt@arl.mil about the head format string." }
              { see_also "nirt" } }
    entry $top.headE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_head)
    hoc_register_data $top.headE "Head Format String"\
	    { { summary "Enter the head format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.headL -sticky "ew" -in $top.headF
    grid $top.headE -sticky "ew" -in $top.headF
    grid columnconfigure $top.headF 0 -weight 1
    grid $top.headF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.partitionF
    label $top.partitionL -text "Partition" -anchor w
    hoc_register_data $top.partitionL "Partition Format String"\
	    { { summary "Ask pjt@arl.mil about the partition format string." }
              { see_also "nirt" } }
    entry $top.partitionE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_partition)
    hoc_register_data $top.partitionE "Partition Format String"\
	    { { summary "Enter the partition format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.partitionL -sticky "ew" -in $top.partitionF
    grid $top.partitionE -sticky "ew" -in $top.partitionF
    grid columnconfigure $top.partitionF 0 -weight 1
    grid $top.partitionF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.footF
    label $top.footL -text "Foot" -anchor w
    hoc_register_data $top.footL "Foot Format String"\
	    { { summary "Ask pjt@arl.mil about the foot format string." } 
              { see_also "nirt" } }
    entry $top.footE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_foot)
    hoc_register_data $top.footE "Foot Format String"\
	    { { summary "Enter the foot format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.footL -sticky "ew" -in $top.footF
    grid $top.footE -sticky "ew" -in $top.footF
    grid columnconfigure $top.footF 0 -weight 1
    grid $top.footF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.missF
    label $top.missL -text "Miss" -anchor w
    hoc_register_data $top.missL "Miss Format String"\
	    { { summary "Ask pjt@arl.mil about the miss format string." }
              { see_also "nirt" } }
    entry $top.missE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_miss)
    hoc_register_data $top.missE "Miss Format String"\
	    { { summary "Enter the miss format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.missL -sticky "ew" -in $top.missF
    grid $top.missE -sticky "ew" -in $top.missF
    grid columnconfigure $top.missF 0 -weight 1
    grid $top.missF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    frame $top.overlapF
    label $top.overlapL -text "Overlap" -anchor w
    hoc_register_data $top.overlapL "Overlap Format String"\
	    { { summary "Ask pjt@arl.mil about the overlay format string." }
              { see_also "nirt" } }
    entry $top.overlapE -relief sunken -bd 2 -width 80 -textvar qray_control($id,fmt_overlap)
    hoc_register_data $top.overlapE "Overlap Format String"\
	    { { summary "Enter the overlap format string. Note that the
middle mouse button can be used to scroll
the entry widget. Also, by default, the
entry widget supports some emacs style
bindings." }
              { see_also "nirt" } }
    grid $top.overlapL -sticky "ew" -in $top.overlapF
    grid $top.overlapE -sticky "ew" -in $top.overlapF
    grid columnconfigure $top.overlapF 0 -weight 1
    grid $top.overlapF -sticky "ew" -in $top.gridF1 -padx $qray_control($id,padx) -pady $qray_control($id,pady)
    grid columnconfigure $top.gridF1 0 -weight 1
    grid $top.gridF1 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    frame $top.gridF2
    button $top.okB -relief raised -text "Ok"\
	    -command "qray_ok_fmt $id $top"
    hoc_register_data $top.okB "Ok"\
	    { { summary "Apply the format string settings to
MGED's internal state then close the
advanced settings control panel." } }
    button $top.applyB -relief raised -text "Apply"\
	    -command "qray_apply_fmt $id"
    hoc_register_data $top.applyB "Apply"\
	    { { summary "Apply the format string settings to
MGED's internal state." } }
    button $top.resetB -relief raised -text "Reset"\
	    -command "qray_reset_fmt $id"
    hoc_register_data $top.resetB "Reset"\
	    { { summary "Set the format strings in the control
panel according to MGED's internal state." } }
    button $top.dismissB -relief raised -text "Dismiss"\
	    -command "catch { destroy $top }"
    hoc_register_data $top.dismissB "Dismiss"\
	    { { summary "Close the advanced settings control panel." } }
    grid $top.okB $top.applyB x $top.resetB x $top.dismissB -sticky "ew" -in $top.gridF2
    grid columnconfigure $top.gridF2 2 -weight 1
    grid columnconfigure $top.gridF2 4 -weight 1
    grid $top.gridF2 -sticky "ew" -padx $qray_control($id,padx) -pady $qray_control($id,pady)

    grid columnconfigure $top 0 -weight 1

    set pxy [winfo pointerxy $top]
    set x [lindex $pxy 0]
    set y [lindex $pxy 1]

    wm protocol $top WM_DELETE_WINDOW "catch { destroy $top }"
    wm geometry $top +$x+$y
    wm title $top "Query Ray Advanced Settings ($id)"
}

proc qray_ok_fmt { id top } {
    qray_apply_fmt $id
    catch { destroy $top }
}

proc qray_apply_fmt { id } {
    global mged_active_dm
    global qray_control

    winset $mged_active_dm($id)
    qray fmt r $qray_control($id,fmt_ray)
    qray fmt h $qray_control($id,fmt_head)
    qray fmt p $qray_control($id,fmt_partition)
    qray fmt f $qray_control($id,fmt_foot)
    qray fmt m $qray_control($id,fmt_miss)
    qray fmt o $qray_control($id,fmt_overlap)
}

proc qray_reset_fmt { id } {
    global mged_active_dm
    global qray_control

    winset $mged_active_dm($id)
    set qray_control($id,fmt_ray) [qray fmt r]
    set qray_control($id,fmt_head) [qray fmt h]
    set qray_control($id,fmt_partition) [qray fmt p]
    set qray_control($id,fmt_foot) [qray fmt f]
    set qray_control($id,fmt_miss) [qray fmt m]
    set qray_control($id,fmt_overlap) [qray fmt o]
}
