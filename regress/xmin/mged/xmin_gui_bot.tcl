#                X M I N _ G U I _ B O T . T C L
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
# Exercise MGED's BoT editor through its live controls.

source $::env(MGED_XMIN_GUI_LIBRARY)

namespace eval ::mged::xmin::bot {}

proc ::mged::xmin::bot::find_combobox {root first_value} {
    foreach widget [::xmin::test::descendants $root] {
	if {[winfo class $widget] eq "TCombobox" &&
	    [lindex [$widget cget -values] 0] eq $first_value} {
	    return $widget
	}
    }
    ::mged::xmin::test::fail \
	"$root has no combobox beginning with '$first_value'"
}

proc ::mged::xmin::bot::confirm_action {editor button_text} {
    set button [::mged::xmin::test::find_widget_by_text $editor $button_text]
    $button invoke
    ::mged::xmin::test::settle

    set confirmation $editor.confirmDialog
    ::mged::xmin::test::require_mapped $confirmation \
	"$button_text confirmation"
    [::mged::xmin::test::find_widget_by_text $confirmation Yes] invoke
    ::mged::xmin::test::settle
}

proc ::mged::xmin::bot::require_counts {bot vertices faces description} {
    ::xmin::test::require {
	[llength [get $bot V]] == $vertices &&
	[llength [get $bot F]] == $faces
    } "$description does not have $vertices vertices and $faces faces"
}

proc ::mged::xmin::bot::exercise_editor {id top} {
    put xmin_bot.s bot mode surface orient no flags {} V {
	{0 0 0} {100 0 0} {0 100 0} {0 0 0} {999 999 999}
    } F {{0 1 2} {3 1 2}}

    ::mged::xmin::test::with_dialog_answer .$id.botname "BoT to Edit" \
	.$id.botname.mid.ent xmin_bot.s .$id.botname.bot.button0 \
	[list ::mged::xmin::test::invoke $top {Tools {BoT Edit Tool}}]

    set editor .botedit
    ::mged::xmin::test::require_mapped $editor "BoT Edit Tool"
    require_counts xmin_bot.s.edit 5 2 "initial working BoT"

    set mode [find_combobox $editor Surface]
    set orientation [find_combobox $editor Unoriented]
    ::xmin::test::require {
	[$mode current] == 0 && [$orientation current] == 0
    } "BoT Editor did not display the initial mode and orientation"

    $mode current 1
    event generate $mode <<ComboboxSelected>>
    $orientation current 1
    event generate $orientation <<ComboboxSelected>>
    ::xmin::test::require {
	[bot get type xmin_bot.s.edit] eq "solid" &&
	[bot get orientation xmin_bot.s.edit] eq "ccw"
    } "BoT Editor did not apply mode and orientation changes"

    [::mged::xmin::test::find_widget_by_text $editor {Remove Selected}] invoke
    ::mged::xmin::test::settle
    require_counts xmin_bot.s.edit 3 1 "simplified working BoT"

    confirm_action $editor {Start Over}
    require_counts xmin_bot.s.edit 5 2 "reverted working BoT"
    ::xmin::test::require {
	[bot get type xmin_bot.s.edit] eq "surface" &&
	[bot get orientation xmin_bot.s.edit] eq "none" &&
	[$mode current] == 0 && [$orientation current] == 0
    } "Start Over did not refresh the BoT properties"

    $mode current 1
    event generate $mode <<ComboboxSelected>>
    $orientation current 1
    event generate $orientation <<ComboboxSelected>>
    foreach option {
	{Remove Unused Vertices} {Remove Duplicate Vertices}
	{Remove Duplicate Faces}
    } {
	[::mged::xmin::test::find_widget_by_text $editor $option] invoke
    }
    [::mged::xmin::test::find_widget_by_text $editor {Remove Selected}] invoke
    ::mged::xmin::test::settle
    confirm_action $editor Accept

    ::xmin::test::require {
	![winfo exists $editor] && ![exists xmin_bot.s.edit] &&
	[bot get type xmin_bot.s] eq "solid" &&
	[bot get orientation xmin_bot.s] eq "ccw"
    } "BoT Editor Accept did not persist properties or close cleanly"
    require_counts xmin_bot.s 3 1 "accepted BoT"
}

proc ::mged::xmin::bot::run {id top} {
    set database [file join $::env(XMIN_TEST_DIR) bot.g]
    cd $::env(XMIN_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED BoT editor regression}

    exercise_editor $id $top
}

::mged::xmin::test::start ::mged::xmin::bot::run \
    {MGED BoT editor} {MGED BoT editor regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
