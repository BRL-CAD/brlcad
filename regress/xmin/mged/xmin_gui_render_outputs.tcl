#     X M I N _ G U I _ R E N D E R _ O U T P U T S . T C L
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
# Exercise MGED's Render View output dialogs through their live controls.

source $::env(MGED_GUI_TEST_LIBRARY)

namespace eval ::mged::xmin::render {
    variable raytrace_image_size 32
    variable raytrace_timeout_ms 10000
}

proc ::mged::xmin::render::read_file {path {translation auto}} {
    set channel [open $path r]
    fconfigure $channel -translation $translation
    set contents [read $channel]
    close $channel
    return $contents
}

proc ::mged::xmin::render::require_file {path description} {
    ::gui::test::require {
	[file exists $path] && [file size $path] > 0
    } "$description did not produce a nonempty file"
}

proc ::mged::xmin::render::wait_for_file_size {path minimum_size timeout_ms} {
    set deadline [expr {[clock milliseconds] + $timeout_ms}]
    while {[clock milliseconds] < $deadline} {
	update
	if {[file exists $path] && [file size $path] >= $minimum_size} {
	    return
	}
	after 25
    }

    set actual_size 0
    if {[file exists $path]} {
	set actual_size [file size $path]
    }
    ::mged::gui::test::fail \
	"raytrace output has $actual_size bytes, expected at least $minimum_size"
}

proc ::mged::xmin::render::exercise_rt_script {id top} {
    global rts_control

    set output [file join $::env(GUI_TEST_DIR) saved-view.sh]
    ::mged::gui::test::invoke $top {File {Render View} {RT Script...}}
    set dialog .$id.do_rtScript
    ::gui::test::require {
	[winfo exists $dialog] && [winfo ismapped $dialog] &&
	[wm title $dialog] eq "RT Script Tool"
    } "RT Script did not open its tool"

    set rts_control($id,file) $output
    set rts_control($id,args) {-A 0.25}
    $dialog.createB invoke
    ::mged::gui::test::settle
    require_file $output "RT Script"
    set contents [read_file $output]
    ::gui::test::require {
	[string first "-A 0.25" $contents] >= 0 &&
	[string first "xmin_render.r" $contents] >= 0
    } "RT Script omitted its requested options or displayed object"
    set initial_size [file size $output]

    ::mged::gui::test::with_dialog_answer .mged_dialog \
	"Append $output?" "" "" .mged_dialog.bot.button0 \
	[list $dialog.createB invoke]
    ::gui::test::require {[file size $output] > $initial_size} \
	"RT Script overwrite confirmation did not append another view"

    $dialog.dismissB invoke
    ::gui::test::require {![winfo exists $dialog]} \
	"RT Script tool did not dismiss"
}

proc ::mged::xmin::render::exercise_plot {id top} {
    global pl_control

    set output [file join $::env(GUI_TEST_DIR) rendered.plot3]
    ::mged::gui::test::invoke $top {File {Render View} {Plot...}}
    set dialog .$id.do_plot
    ::gui::test::require {
	[winfo exists $dialog] && [winfo ismapped $dialog]
    } "Plot did not open its tool"

    set pl_control($id,file) $output
    set pl_control($id,zclip) 1
    set pl_control($id,2d) 0
    set pl_control($id,float) 0
    $dialog.createB invoke
    ::mged::gui::test::settle
    require_file $output "Plot"
    set plot_text [exec $::env(PLOT3_ASC_BIN) $output]
    ::gui::test::require {
	[regexp -line {^L[ \t]} $plot_text] &&
	[llength [split $plot_text \n]] > 10
    } "Plot output could not be decoded into line records"

    $dialog.filterRB invoke
    ::gui::test::require {
	[$dialog.fileE cget -state] eq "disabled" &&
	[$dialog.filterE cget -state] eq "normal"
    } "Plot filter selection did not update entry states"
    $dialog.fileRB invoke
    ::gui::test::require {
	[$dialog.fileE cget -state] eq "normal" &&
	[$dialog.filterE cget -state] eq "disabled"
    } "Plot file selection did not restore entry states"

    $dialog.dismissB invoke
    ::gui::test::require {![winfo exists $dialog]} \
	"Plot tool did not dismiss"
}

proc ::mged::xmin::render::exercise_postscript {id top} {
    global ps_control

    set output [file join $::env(GUI_TEST_DIR) rendered.ps]
    ::mged::gui::test::invoke $top {File {Render View} {PostScript...}}
    set dialog .$id.do_ps
    ::gui::test::require {
	[winfo exists $dialog] && [winfo ismapped $dialog]
    } "PostScript did not open its tool"

    set ps_control($id,file) $output
    set ps_control($id,title) {Xmin Render Output}
    set ps_control($id,creator) {MGED regression}
    set ps_control($id,size) 3.5
    set ps_control($id,linewidth) 2
    set ps_control($id,zclip) 1
    $dialog.fontMB.fontM.helveticaM invoke Bold
    ::gui::test::require {$ps_control($id,font) eq "Helvetica-Bold"} \
	"PostScript font menu did not update the selected font"

    $dialog.createB invoke
    ::mged::gui::test::settle
    require_file $output "PostScript"
    set contents [read_file $output]
    ::gui::test::require {
	[string first "%!PS-Adobe" $contents] == 0 &&
	[string first "Xmin Render Output" $contents] >= 0 &&
	[string first "MGED regression" $contents] >= 0 &&
	[string first "Helvetica-Bold" $contents] >= 0
    } "PostScript output omitted its header or configured metadata"

    $dialog.dismissB invoke
    ::gui::test::require {![winfo exists $dialog]} \
	"PostScript tool did not dismiss"
}

proc ::mged::xmin::render::exercise_raytrace {id top} {
    global rt_control
    variable raytrace_image_size
    variable raytrace_timeout_ms

    set output [file join $::env(GUI_TEST_DIR) rendered.pix]
    ::mged::gui::test::invoke $top {File Raytrace}
    set dialog .$id.rt
    ::gui::test::require {
	[winfo exists $dialog] && [winfo ismapped $dialog]
    } "File > Raytrace did not open the Raytrace Control Panel"

    file delete -force $output
    rt_force_cook_dest $id {}
    set rt_control($id,size) $raytrace_image_size
    set rt_control($id,nproc) 1
    set rt_control($id,hsample) 0
    set rt_control($id,jitter) 0
    set rt_control($id,lmodel) 0
    set rt_control($id,opencl) 0
    set rt_control($id,other) [list -o $output]
    set rt_control($id,omode) one
    set rt_control($id,olist) {xmin_render.r}
    $dialog.raytraceB invoke
    wait_for_file_size $output \
	[expr {$raytrace_image_size * $raytrace_image_size * 3}] \
	$raytrace_timeout_ms

    $dialog.dismissB invoke
    ::gui::test::require {![winfo exists $dialog]} \
	"Raytrace Control Panel did not dismiss"
}

proc ::mged::xmin::render::run {id top} {
    set database [file join $::env(GUI_TEST_DIR) render.g]
    cd $::env(GUI_TEST_DIR)
    file delete -force $database
    opendb $database y
    title {Xmin MGED render-output regression}
    make xmin_render.s sph
    r xmin_render.r u xmin_render.s
    attr set xmin_render.r region yes
    draw xmin_render.r
    autoview
    ae 35 25

    exercise_rt_script $id $top
    exercise_plot $id $top
    exercise_postscript $id $top
    exercise_raytrace $id $top
}

::mged::gui::test::start ::mged::xmin::render::run \
    {MGED Render View output dialogs} {MGED render-output regression}

# Local Variables:
# mode: Tcl
# tab-width: 8
# indent-tabs-mode: t
# End:
