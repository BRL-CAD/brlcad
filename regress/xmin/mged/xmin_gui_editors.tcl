#            X M I N _ G U I _ E D I T O R S . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
# Exercise MGED's Combination and Attribute editors through live widgets.

source $::env(MGED_XMIN_GUI_LIBRARY)

namespace eval ::mged::xmin::editors {}
proc ::mged::xmin::editors::confirm_combination_overwrite {button} {
    ::mged::xmin::test::with_dialog_answer .mged_dialog Warning! "" "" \
	.mged_dialog.bot.button0 [list $button invoke]
}


proc ::mged::xmin::editors::exercise_combination {id top} {
    global comb_control

    ::mged::xmin::test::invoke $top {Edit {Combination Editor}}
    set editor .$id.comb
    ::xmin::test::require {
	[winfo exists $editor] && [winfo ismapped $editor]
    } "Combination Editor did not open"

    ::mged::xmin::test::set_entry $editor.nameE xmin_group.c
    focus $editor.nameE
    event generate $editor.nameE <KeyPress> -keysym Return
    ::mged::xmin::test::settle
    set original_expression [string trim [$editor.combT get 1.0 end]]
    ::xmin::test::require {
	[string first "xmin_left.s" $original_expression] >= 0 &&
	[string first "xmin_right.s" $original_expression] >= 0
    } "Combination Editor did not load the selected combination tree"

    $editor.combT delete 1.0 end
    $editor.combT insert end "u xmin_left.s\n- xmin_right.s"
    set comb_control($id,color) {10 20 30}
    color_entry_update $editor color comb_control($id,color) \
	$comb_control($id,color)
    set comb_control($id,inherit) Yes
    $editor.shaderMB.m invoke plastic
    confirm_combination_overwrite $editor.applyB
    ::mged::xmin::test::settle

    set values [get_comb xmin_group.c]
    ::xmin::test::require {
	[lindex $values 1] eq "10 20 30" &&
	[lindex [lindex $values 2] 0] eq "plastic" &&
	[lindex $values 3] eq "Yes" &&
	[string first "- xmin_right.s" [lindex $values 4]] >= 0
    } "Combination Editor did not persist its Boolean, color, shader, or inheritance edits"

    $editor.combT delete 1.0 end
    $editor.combT insert end {u xmin_left.s}
    $editor.resetB invoke
    ::mged::xmin::test::settle
    ::xmin::test::require {
	[string first "- xmin_right.s" [$editor.combT get 1.0 end]] >= 0
    } "Combination Editor Reset did not reload database state"

    $editor.isRegionCB invoke
    set comb_control($id,id) 1201
    set comb_control($id,air) 0
    set comb_control($id,material) 7
    set comb_control($id,los) 80
    confirm_combination_overwrite $editor.applyB
    set values [get_comb xmin_group.c]
    ::xmin::test::require {
	[lindex $values 5] eq "Yes" &&
	[lindex $values 6] == 1201 &&
	[lindex $values 7] == 0 &&
	[lindex $values 8] == 7 &&
	[lindex $values 9] == 80
    } "Combination Editor did not persist region properties"

    confirm_combination_overwrite $editor.okB
    ::xmin::test::require {![winfo exists $editor]} \
	"Combination Editor did not close after OK"
}

proc ::mged::xmin::editors::load_attribute_object {editor object_name} {
    set object_entry $editor.fr_obj.obj_e
    ::mged::xmin::test::set_entry $object_entry $object_name
    focus $object_entry
    event generate $object_entry <KeyPress> -keysym Return
    ::mged::xmin::test::settle
}

proc ::mged::xmin::editors::select_attribute {listbox name} {
    set index [lsearch -exact [$listbox get 0 end] $name]
    if {$index < 0} {
	::mged::xmin::test::fail "Attribute Editor did not list '$name'"
    }
    $listbox selection clear 0 end
    $listbox selection set $index
    event generate $listbox <<ListboxSelect>>
    ::mged::xmin::test::settle
}

proc ::mged::xmin::editors::set_attribute_value {text value} {
    focus $text
    $text delete 1.0 end
    $text insert end $value
    event generate $text <KeyRelease> -keysym a
    ::mged::xmin::test::settle
}

proc ::mged::xmin::editors::exercise_attributes {id top} {
    ::mged::xmin::test::invoke $top {Edit {Attribute Editor}}
    set editor .${id}_attr_edit
    ::xmin::test::require {
	[llength [info commands $editor]] == 1 &&
	[winfo exists $editor] && [winfo ismapped $editor]
    } "Attribute Editor did not open"

    load_attribute_object $editor xmin_attr.s
    set listbox [::mged::xmin::test::find_descendant $editor Listbox]
    set value_text [::mged::xmin::test::find_descendant $editor Text]
    select_attribute $listbox existing
    ::xmin::test::require {
	[string trim [$value_text get 1.0 end]] eq "original"
    } "Attribute Editor did not load the selected attribute value"

    set_attribute_value $value_text pending
    $editor.fr_attr.frc1.reset_sel invoke
    ::xmin::test::require {
	[string trim [$value_text get 1.0 end]] eq "original"
    } "Attribute Editor reset selected did not restore database state"

    set_attribute_value $value_text changed
    $editor.frc.apply invoke
    ::xmin::test::require {[attr get xmin_attr.s existing] eq "changed"} \
	"Attribute Editor Apply did not persist an edited value"

    $editor.fr_attr.fr_new.new invoke
    set name_entry $editor.fr_attr.fr_new.attr_e
    ::mged::xmin::test::set_entry $name_entry added
    focus $name_entry
    event generate $name_entry <KeyPress> -keysym Return
    ::mged::xmin::test::settle
    set_attribute_value $value_text {new value}
    $editor.frc.apply invoke
    ::xmin::test::require {[attr get xmin_attr.s added] eq "new value"} \
	"Attribute Editor did not create a new attribute"

    select_attribute $listbox added
    $editor.fr_attr.frc1.delete_selected invoke
    $editor.frc.apply invoke
    ::xmin::test::require {[catch {attr get xmin_attr.s added}]} \
	"Attribute Editor did not delete the selected attribute"

    select_attribute $listbox existing
    set_attribute_value $value_text discarded
    $editor.fr_attr.frc1.reset_all invoke
    select_attribute $listbox existing
    ::xmin::test::require {
	[string trim [$value_text get 1.0 end]] eq "changed"
    } "Attribute Editor reset all did not discard pending changes"

    set_attribute_value $value_text final
    $editor.frc.ok invoke
    ::xmin::test::require {
	![winfo exists $editor] && [attr get xmin_attr.s existing] eq "final"
    } "Attribute Editor OK did not persist and close"

    ::mged::xmin::test::invoke $top {Edit {Attribute Editor}}
    load_attribute_object $editor xmin_attr.s
    set listbox [::mged::xmin::test::find_descendant $editor Listbox]
    $editor do_new_obj
    set value_text [::mged::xmin::test::find_descendant $editor Text]
    select_attribute $listbox existing
    set_attribute_value $value_text unsaved
    $editor.frc.dismiss invoke
    ::xmin::test::require {
	![winfo exists $editor] && [attr get xmin_attr.s existing] eq "final"
    } "Attribute Editor Dismiss saved a pending edit or failed to close"
}

proc ::mged::xmin::editors::run {id top} {
    set database [file join $::env(XMIN_TEST_DIR) editors.g]
    cd $::env(XMIN_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED editor regression}
    make xmin_left.s sph
    make xmin_right.s sph
    g xmin_group.c xmin_left.s xmin_right.s
    make xmin_attr.s arb8
    attr set xmin_attr.s existing original

    ::xmin::test::exercise_sketch_segments Sketch_carc Sketch_bezier MGED ::mged::xmin::editors
    exercise_combination $id $top
    exercise_attributes $id $top
}

::mged::xmin::test::start ::mged::xmin::editors::run \
    {MGED Combination and Attribute editors} {MGED editor regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
