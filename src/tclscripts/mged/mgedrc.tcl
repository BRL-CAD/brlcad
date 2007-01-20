#                      M G E D R C . T C L
# BRL-CAD
#
# Copyright (c) 2004-2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
#
#                       M G E D R C . T C L
#
# Author -
#	Robert G. Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Description -
#	Procedure for dumping state to the user's .mgedrc file.
#

set MGEDRC_HEADER "############### MGEDRC_HEADER ###############"

proc update_mgedrc {} {
    global env
    global MGEDRC_HEADER

    if [info exists env(HOME)] {
	set dir $env(HOME)
    } else {
	set dir [pwd]
    }

    if [info exists env(TMPDIR)] {
	set tmpdir $env(TMPDIR)
    } else {
	set tmpdir $dir
    }

    set mgedrc [file join $dir .mgedrc]

    if [file exists $mgedrc] {
	# Update an existing .mgedrc

	set tmpmgedrc [file join $tmpdir .mgedrc_[pid]]

	# make a copy of the user's .mgedrc file
	if [catch {file copy -force $mgedrc $tmpmgedrc} msg] {
	    error $msg
	}

	if [catch {set fd [open $mgedrc w]} msg] {
	    error $msg
	}

	if [catch {set tfd [open $tmpmgedrc r]} msg] {
	    close $fd
	    file copy -force $tmpmgedrc $mgedrc
	    error $msg
	}

	set data [read $tfd]
	close $tfd

	# search for the MGEDRC_HEADER
	set index [string first $MGEDRC_HEADER $data]

	if {$index == -1} {
	    # keep everything
	    puts -nonewline $fd $data
	} elseif {$index > 0} {
	    incr index -1
	    # truncate old MGED generated settings
	    puts -nonewline $fd [string range $data 0 $index]
	}

	if [catch {dump_mged_state $fd} msg] {
	    close $fd
	    file copy -force $tmpmgedrc $mgedrc
	    file delete -force $tmpmgedrc
	    error $msg
	}

	close $fd
	file delete -force $tmpmgedrc
    } else {
	# Create a new .mgedrc
	if [catch {set fd [open $mgedrc w]} msg] {
	    error $msg
	}

	if [catch {dump_mged_state $fd} msg] {
	    close $fd
	    error $msg
	}

	close $fd
    }
}

proc dump_mged_state {fd} {
    global MGEDRC_HEADER
    global glob_compat_mode
    global mged_default
    global solid_data
    global mged_players
    global mged_gui

    set id [lindex $mged_players 0]

    puts $fd $MGEDRC_HEADER
    puts $fd "# You can modify the values below. However, if you want"
    puts $fd "# to add new lines, add them above the MGEDRC_HEADER."
    puts $fd "# Note - it's not a good idea to set the same variables"
    puts $fd "# above the MGEDRC_HEADER that are set below (i.e. the last"
    puts $fd "# value set wins)."
    puts $fd ""
    puts $fd "# Activate/deactivate globbing against database objects"
    puts $fd "set glob_compat_mode $glob_compat_mode"
    puts $fd ""
    puts $fd "# Used by the opendb command to determine what version"
    puts $fd "# of the database to use when creating a new database."
    puts $fd "set mged_default(db_version) $mged_default(db_version)"
    puts $fd ""
    puts $fd "# Used by the opendb command to determine whether or"
    puts $fd "# not to warn the user when reading an old style database."
    puts $fd "# 0 - no warn,   1 - warn"
    puts $fd "set mged_default(db_warn) $mged_default(db_warn)"
    puts $fd ""
    puts $fd "# Used by the opendb command to determine whether or"
    puts $fd "# not to automatically upgrade the database."
    puts $fd "# 0 - no upgrade,   1 - upgrade"
    puts $fd "set mged_default(db_upgrade) $mged_default(db_upgrade)"
    puts $fd ""
    puts $fd "# Used by the arrow keys to control how fast to rotate"
    puts $fd "set mged_default(rot_factor) $mged_default(rot_factor)"
    puts $fd ""
    puts $fd "# Used by the arrow keys to control how fast to translate"
    puts $fd "set mged_default(tran_factor) $mged_default(tran_factor)"
    puts $fd ""
    puts $fd "# Determines the maximum number of lines of"
    puts $fd "# output displayed in the command window"
    puts $fd "set mged_default(max_text_lines) $mged_default(max_text_lines)"
    puts $fd ""
    puts $fd "# Determines the border width in the geometry window"
    puts $fd "set mged_default(bd) $mged_default(bd)"
    puts $fd ""
    puts $fd "# X display string used to determine the screen"
    puts $fd "# on which to display the GUI's windows"
    puts $fd "set mged_default(display) $mged_default(display)"
    puts $fd ""
    puts $fd "# Configures the GUI. Note - this is only"
    puts $fd "# useful if the GUI combines the geometry and text windows"
    puts $fd "# in the same toplevel window (i.e. mged_default(comb) is 1"
    puts $fd "# or the \"gui\" command is issued with the -c option."
    puts $fd "# Valid values are:"
    puts $fd "# b - show both geometry and command window"
    puts $fd "# c - show only the command window"
    puts $fd "# g - show only the geometry window"
    puts $fd "set mged_default(config) $mged_default(config)"
    puts $fd ""
    puts $fd "# The maximum number of milliseconds between button clicks"
    puts $fd "# in order to be interpreted as a double-click"
    puts $fd "set mged_default(doubleClickTol) $mged_default(doubleClickTol)"
    puts $fd ""
    puts $fd "# Determines the height of the command window in"
    puts $fd "# number of lines. Note - this is only useful"
    puts $fd "# if the GUI combines the geometry and text windows"
    puts $fd "# in the same toplevel window (i.e. mged_default(comb) is 1"
    puts $fd "# or the \"gui\" command is issued with the -c option."
    puts $fd "set mged_default(num_lines) $mged_gui($id,num_lines)"
    puts $fd ""
    puts $fd "# Determines where to look for MGED's html documentation"
    puts $fd "set mged_default(html_dir) $mged_default(html_dir)"
    puts $fd ""
#    puts $fd "# Determines whether or not to use tearoff menus"
#    puts $fd "set mged_default(tearoff_menus) $mged_default(tearoff_menus)"
#    puts $fd ""
    puts $fd "# Combines geometry and command windows"
    puts $fd "set mged_default(comb) $mged_gui($id,comb)"
    puts $fd ""
    puts $fd "# Activate/deactivate display lists. Note - display lists"
    puts $fd "# increase the interactivity with the geometry, especially if"
    puts $fd "# displaying remotely. However, if the geometry is huge"
    puts $fd "# w.r.t. the amount of RAM on the machine used to display"
    puts $fd "# the geometry, it may cause the machine to thrash."
    puts $fd "set mged_default(dlist) $mged_gui($id,dlist)"
    puts $fd ""
    puts $fd "# Display manager type (X or possibly ogl)"
    puts $fd "set mged_default(dm_type) $mged_gui($id,dtype)"
    puts $fd ""
    puts $fd "# Sets the type of command line editing (emacs or vi)"
    puts $fd "set mged_default(edit_style) $mged_gui($id,edit_style)"
    puts $fd ""
    puts $fd "# Position/size of command window"
    puts $fd "set mged_default(geom) [winfo geometry .$id]"
    puts $fd ""
    puts $fd "# Position/size of geometry window or both if combined"
    if { $mged_gui($id,comb) } {
	puts $fd "set mged_default(geom) [winfo geometry .$id]"
    } else {
	puts $fd "set mged_default(ggeom) [winfo geometry $mged_gui($id,dmc)]"
    }
    puts $fd ""
    puts $fd "# Activate/deactivate zclipping, F2"
    puts $fd "set mged_default(zclip) $mged_gui($id,zclip)"
    puts $fd ""
    puts $fd "# zbuffer"
    if { $mged_gui($id,dtype) == "ogl" } {
	puts $fd "set mged_default(zbuffer) $mged_gui($id,zbuffer)"
    } else {
	puts $fd "set mged_default(zbuffer) $mged_default(zbuffer)"
    }
    puts $fd ""
    puts $fd "# lighting"
    if { $mged_gui($id,dtype) == "ogl" } {
	puts $fd "set mged_default(lighting) $mged_gui($id,lighting)"
    } else {
	puts $fd "set mged_default(lighting) $mged_default(lighting)"
    }
    puts $fd ""
    puts $fd "# Activate/deactivate perspective mode, F3"
    puts $fd "set mged_default(perspective_mode) $mged_gui($id,perspective_mode)"
    puts $fd ""
    puts $fd "# Activate/deactivate old mged faceplate, F7"
    puts $fd "set mged_default(faceplate) $mged_gui($id,faceplate)"
    puts $fd ""
    puts $fd "# Activate/deactivate old mged faceplate GUI, F8"
    puts $fd "set mged_default(orig_gui) $mged_gui($id,orig_gui)"
    puts $fd ""
    puts $fd "# Specifies the active pane"
    puts $fd "set mged_default(pane) [lindex [split $mged_gui($id,active_dm) .] 2]"
    puts $fd ""
    puts $fd "# Activate/deactivate muli-pane mode (i.e. four geometry windows or one)"
    puts $fd "set mged_default(multi_pane) $mged_gui($id,multi_pane)"
    puts $fd ""
    puts $fd "# Specifies the web browser"
    puts $fd "set mged_default(web_browser) $mged_default(web_browser)"
    puts $fd ""
    puts $fd "# Used by the font GUI to demonstrate overstrike"
    puts $fd "set mged_default(overstrike_font) [list $mged_default(overstrike_font)]"
    puts $fd ""
    puts $fd "# Used by the font GUI to demonstrate underline"
    puts $fd "set mged_default(underline_font) [list $mged_default(underline_font)]"
    puts $fd ""
    puts $fd "# Used by the font GUI to demonstrate italic"
    puts $fd "set mged_default(italic_font) [list $mged_default(italic_font)]"
    puts $fd ""
    puts $fd "# Used by the font GUI to demonstrate bold"
    puts $fd "set mged_default(bold_font) [list $mged_default(bold_font)]"
    puts $fd ""
    puts $fd "# Font to use everywhere "
    puts $fd "set mged_default(all_font) [list $mged_default(all_font)]"
    puts $fd ""
    puts $fd "# Font to use in button widgets"
    puts $fd "set mged_default(button_font) [list $mged_default(button_font)]"
    puts $fd ""
    puts $fd "# Font to use in entry widgets"
    puts $fd "set mged_default(entry_font) [list $mged_default(entry_font)]"
    puts $fd ""
    puts $fd "# Font to use in label widgets"
    puts $fd "set mged_default(label_font) [list $mged_default(label_font)]"
    puts $fd ""
    puts $fd "# Font to use in list widgets"
    puts $fd "set mged_default(list_font) [list $mged_default(list_font)]"
    puts $fd ""
    puts $fd "# Font to use in menu widgets"
    puts $fd "set mged_default(menu_font) [list $mged_default(menu_font)]"
    puts $fd ""
    puts $fd "# Font to use in menubutton widgets"
    puts $fd "set mged_default(menubutton_font) [list $mged_default(menubutton_font)]"
    puts $fd ""
    puts $fd "# Font to use for text widgets"
    puts $fd "set mged_default(text_font) [list $mged_default(text_font)]"
    puts $fd ""
    puts $fd "###################### Rubber Band Settings ######################"
    puts $fd "# 0 - do not draw rubberband"
    puts $fd "# 1 - draw rubberband"
    puts $fd "rset r draw [rset r draw]"
    puts $fd ""
    puts $fd "# rubberband line width"
    puts $fd "rset r linewidth [rset r linewidth]"
    puts $fd ""
    puts $fd "# s - solid lines"
    puts $fd "# d - dashed lines"
    puts $fd "rset r linestyle [rset r linestyle]"
    puts $fd ""
    puts $fd "###################### Grid Settings ######################"
    puts $fd "# 0 - do not draw grid"
    puts $fd "# 1 - draw grid"
    puts $fd "rset g draw [rset g draw]"
    puts $fd ""
    puts $fd "# 0 - snap to grid off"
    puts $fd "# 1 - snap to grid on"
    puts $fd "rset g snap [rset g snap]"
    puts $fd ""
    puts $fd "# anchor grid to the specified point"
    puts $fd "rset g anchor [rset g anchor]"
    puts $fd ""
    puts $fd "# horizontal distance between ticks"
    puts $fd "rset g rh [rset g rh]"
    puts $fd ""
    puts $fd "# vertical distance between ticks"
    puts $fd "rset g rv [rset g rv]"
    puts $fd ""
    puts $fd "# horizontal ticks per major (vertical) line"
    puts $fd "rset g mrh [rset g mrh]"
    puts $fd ""
    puts $fd "# vertical ticks per major (horizontal) line"
    puts $fd "rset g mrv [rset g mrv]"
    puts $fd ""
    puts $fd "###################### Axes Settings ######################"
    puts $fd "# The size of all axes are specifed in"
    puts $fd "# relative screen dimensions, where the"
    puts $fd "# width of the screen is 4096."
    puts $fd ""
    puts $fd "# 0 - do not draw model axes"
    puts $fd "# 1 - draw model axes"
    puts $fd "rset ax model_draw [rset ax model_draw]"
    puts $fd ""
    puts $fd "# See above explanation of axes size"
    puts $fd "rset ax model_size [rset ax model_size]"
    puts $fd ""
    puts $fd "# line width of model axes"
    puts $fd "rset ax model_linewidth [rset ax model_linewidth]"
    puts $fd ""
    puts $fd "# position in model coordinates"
    puts $fd "rset ax model_pos [rset ax model_pos]"
    puts $fd ""
    puts $fd "# 0 - do not draw view axes, 1 - draw view axes"
    puts $fd "rset ax view_draw [rset ax view_draw]"
    puts $fd ""
    puts $fd "# See above explanation of axes size"
    puts $fd "rset ax view_size [rset ax view_size]"
    puts $fd ""
    puts $fd "# line width of view axes"
    puts $fd "rset ax view_linewidth [rset ax view_linewidth]"
    puts $fd ""
    puts $fd "# position in relative screen dimensions, where the"
    puts $fd "# screen center is (0 0) and the range is +-2048."
    puts $fd "rset ax view_pos [rset ax view_pos]"
    puts $fd ""
    puts $fd "# The edit axes are a pair of axes. The first set of"
    puts $fd "# axes is drawn with no edits applied. The second"
    puts $fd "# set is drawn with the current edits applied."
    puts $fd ""
    puts $fd "# 0 - do not draw edit axes"
    puts $fd "# 1 - draw edit axes"
    puts $fd "rset ax edit_draw [rset ax edit_draw]"
    puts $fd ""
    puts $fd "# Specify the size of the first set of edit axes"
    puts $fd "# See above explanation of axes size"
    puts $fd "rset ax edit_size1 [rset ax edit_size1]"
    puts $fd ""
    puts $fd "# Specify the size of the second set of edit axes"
    puts $fd "# See above explanation of axes size"
    puts $fd "rset ax edit_size2 [rset ax edit_size2]"
    puts $fd ""
    puts $fd "# line width of the first set of edit axes"
    puts $fd "rset ax edit_linewidth1 [rset ax edit_linewidth1]"
    puts $fd ""
    puts $fd "# line width of the second set of edit axes"
    puts $fd "rset ax edit_linewidth2 [rset ax edit_linewidth2]"
    puts $fd ""
    puts $fd "###################### Color Scheme Settings ######################"
    puts $fd "# The MGED GUI has four panes (geometry windows) and typically only"
    puts $fd "# one of these is considered to be the active pane (i.e. the pane that"
    puts $fd "# is affected by interacting with the GUI). The other three panes are"
    puts $fd "# considered to be inactive. MGED colors the active panes one way and"
    puts $fd "# the inactive panes another. Each attribute of the pane has three"
    puts $fd "# variables for specifying its color. The first variable specifies the"
    puts $fd "# current color (i.e. bg specifies the pane's current background color)."
    puts $fd "# The second variable specifies the active color and the variable name"
    puts $fd "# always ends with a trailing _a (i.e. bg_a specifies the pane's active"
    puts $fd "# background color). The third variable specifies the inactive color and"
    puts $fd "# the variable name always ends with a trailing _ia (i.e. bg_ia specifies"
    puts $fd "# the pane's inactive background color)."
    puts $fd ""
    puts $fd "# Pane Background Colors"
    puts $fd "rset cs bg [rset cs bg]"
    puts $fd "rset cs bg_a [rset cs bg_a]"
    puts $fd "rset cs bg_ia [rset cs bg_ia]"
    puts $fd ""
    puts $fd "# Angle Distance Cursor Colors"
    puts $fd "rset cs adc_line [rset cs adc_line]"
    puts $fd "rset cs adc_line_a [rset cs adc_line_a]"
    puts $fd "rset cs adc_line_ia [rset cs adc_line_ia]"
    puts $fd "rset cs adc_tick [rset cs adc_tick]"
    puts $fd "rset cs adc_tick_a [rset cs adc_tick_a]"
    puts $fd "rset cs adc_tick_ia [rset cs adc_tick_ia]"
    puts $fd ""
    puts $fd "# Default Geometry Colors"
    puts $fd "rset cs geo_def [rset cs geo_def]"
    puts $fd "rset cs geo_def_a [rset cs geo_def_a]"
    puts $fd "rset cs geo_def_ia [rset cs geo_def_ia]"
    puts $fd ""
    puts $fd "# Geometry Highlight Colors"
    puts $fd "rset cs geo_hl [rset cs geo_hl]"
    puts $fd "rset cs geo_hl_a [rset cs geo_hl_a]"
    puts $fd "rset cs geo_hl_ia [rset cs geo_hl_ia]"
    puts $fd ""
    puts $fd "# Geometry Label Colors"
    puts $fd "rset cs geo_label [rset cs geo_label]"
    puts $fd "rset cs geo_label_a [rset cs geo_label_a]"
    puts $fd "rset cs geo_label_ia [rset cs geo_label_ia]"
    puts $fd ""
    puts $fd "# Model Axes Colors"
    puts $fd "rset cs model_axes [rset cs model_axes]"
    puts $fd "rset cs model_axes_a [rset cs model_axes_a]"
    puts $fd "rset cs model_axes_ia [rset cs model_axes_ia]"
    puts $fd "rset cs model_axes_label [rset cs model_axes_label]"
    puts $fd "rset cs model_axes_label_a [rset cs model_axes_label_a]"
    puts $fd "rset cs model_axes_label_ia [rset cs model_axes_label_ia]"
    puts $fd ""
    puts $fd "# View Axes Colors"
    puts $fd "rset cs view_axes [rset cs view_axes]"
    puts $fd "rset cs view_axes_a [rset cs view_axes_a]"
    puts $fd "rset cs view_axes_ia [rset cs view_axes_ia]"
    puts $fd "rset cs view_axes_label [rset cs view_axes_label]"
    puts $fd "rset cs view_axes_label_a [rset cs view_axes_label_a]"
    puts $fd "rset cs view_axes_label_ia [rset cs view_axes_label_ia]"
    puts $fd ""
    puts $fd "# Edit Axes Colors"
    puts $fd "rset cs edit_axes1 [rset cs edit_axes1]"
    puts $fd "rset cs edit_axes1_a [rset cs edit_axes1_a]"
    puts $fd "rset cs edit_axes1_ia [rset cs edit_axes1_ia]"
    puts $fd "rset cs edit_axes2 [rset cs edit_axes2]"
    puts $fd "rset cs edit_axes2_a [rset cs edit_axes2_a]"
    puts $fd "rset cs edit_axes2_ia [rset cs edit_axes2_ia]"
    puts $fd "rset cs edit_axes_label1 [rset cs edit_axes_label1]"
    puts $fd "rset cs edit_axes_label1_a [rset cs edit_axes_label1_a]"
    puts $fd "rset cs edit_axes_label1_ia [rset cs edit_axes_label1_ia]"
    puts $fd "rset cs edit_axes_label2 [rset cs edit_axes_label2]"
    puts $fd "rset cs edit_axes_label2_a [rset cs edit_axes_label2_a]"
    puts $fd "rset cs edit_axes_label2_ia [rset cs edit_axes_label2_ia]"
    puts $fd ""
    puts $fd "# Rubberband Colors"
    puts $fd "rset cs rubber_band [rset cs rubber_band]"
    puts $fd "rset cs rubber_band_a [rset cs rubber_band_a]"
    puts $fd "rset cs rubber_band_ia [rset cs rubber_band_ia]"
    puts $fd ""
    puts $fd "# Grid Colors"
    puts $fd "rset cs grid [rset cs grid]"
    puts $fd "rset cs grid_a [rset cs grid_a]"
    puts $fd "rset cs grid_ia [rset cs grid_ia]"
    puts $fd ""
    puts $fd "# Predictor Colors"
    puts $fd "rset cs predictor [rset cs predictor]"
    puts $fd "rset cs predictor_a [rset cs predictor_a]"
    puts $fd "rset cs predictor_ia [rset cs predictor_ia]"
    puts $fd ""
    puts $fd "# Faceplate Menu Colors"
    puts $fd "rset cs menu_line [rset cs menu_line]"
    puts $fd "rset cs menu_line_a [rset cs menu_line_a]"
    puts $fd "rset cs menu_line_ia [rset cs menu_line_ia]"
    puts $fd "rset cs menu_text1 [rset cs menu_text1]"
    puts $fd "rset cs menu_text1_a [rset cs menu_text1_a]"
    puts $fd "rset cs menu_text1_ia [rset cs menu_text1_ia]"
    puts $fd "rset cs menu_text2 [rset cs menu_text2]"
    puts $fd "rset cs menu_text2_a [rset cs menu_text2_a]"
    puts $fd "rset cs menu_text2_ia [rset cs menu_text2_ia]"
    puts $fd "rset cs menu_title [rset cs menu_title]"
    puts $fd "rset cs menu_title_a [rset cs menu_title_a]"
    puts $fd "rset cs menu_title_ia [rset cs menu_title_ia]"
    puts $fd "rset cs menu_arrow [rset cs menu_arrow]"
    puts $fd "rset cs menu_arrow_a [rset cs menu_arrow_a]"
    puts $fd "rset cs menu_arrow_ia [rset cs menu_arrow_ia]"
    puts $fd ""
    puts $fd "# Faceplate Slider Colors"
    puts $fd "rset cs slider_line [rset cs slider_line]"
    puts $fd "rset cs slider_line_a [rset cs slider_line_a]"
    puts $fd "rset cs slider_line_ia [rset cs slider_line_ia]"
    puts $fd "rset cs slider_text1 [rset cs slider_text1]"
    puts $fd "rset cs slider_text1_a [rset cs slider_text1_a]"
    puts $fd "rset cs slider_text1_ia [rset cs slider_text1_ia]"
    puts $fd "rset cs slider_text2 [rset cs slider_text2]"
    puts $fd "rset cs slider_text2_a [rset cs slider_text2_a]"
    puts $fd "rset cs slider_text2_ia [rset cs slider_text2_ia]"
    puts $fd ""
    puts $fd "# Faceplate Line Colors"
    puts $fd "rset cs other_line [rset cs other_line]"
    puts $fd "rset cs other_line_a [rset cs other_line_a]"
    puts $fd "rset cs other_line_ia [rset cs other_line_ia]"
    puts $fd ""
    puts $fd "# Faceplate Status Colors"
    puts $fd "rset cs status_text1 [rset cs status_text1]"
    puts $fd "rset cs status_text1_a [rset cs status_text1_a]"
    puts $fd "rset cs status_text1_ia [rset cs status_text1_ia]"
    puts $fd "rset cs status_text2 [rset cs status_text2]"
    puts $fd "rset cs status_text2_a [rset cs status_text2_a]"
    puts $fd "rset cs status_text2_ia [rset cs status_text2_ia]"
    puts $fd ""
    puts $fd "# Faceplate State Colors"
    puts $fd "rset cs state_text1 [rset cs state_text1]"
    puts $fd "rset cs state_text1_a [rset cs state_text1_a]"
    puts $fd "rset cs state_text1_ia [rset cs state_text1_ia]"
    puts $fd "rset cs state_text2 [rset cs state_text2]"
    puts $fd "rset cs state_text2_a [rset cs state_text2_a]"
    puts $fd "rset cs state_text2_ia [rset cs state_text2_ia]"
    puts $fd ""
    puts $fd "# Faceplate Edit Information Colors"
    puts $fd "rset cs edit_info [rset cs edit_info]"
    puts $fd "rset cs edit_info_a [rset cs edit_info_a]"
    puts $fd "rset cs edit_info_ia [rset cs edit_info_ia]"
    puts $fd ""
    puts $fd "# Faceplate Center Dot Colors"
    puts $fd "rset cs center_dot [rset cs center_dot]"
    puts $fd "rset cs center_dot_a [rset cs center_dot_a]"
    puts $fd "rset cs center_dot_ia [rset cs center_dot_ia]"
    puts $fd ""
    puts $fd "###################### Other Variable Settings ######################"
    puts $fd "# 0 - display framebuffer only in the rectangular area"
    puts $fd "# 1 - display framebuffer in the entire geometry window"
    puts $fd "rset var fb_all [rset var fb_all]"
    puts $fd ""
    puts $fd "# 0 - underlay (place framebuffer below everything)"
    puts $fd "# 1 - interlay (place framebuffer above the wireframe and below the faceplate)"
    puts $fd "# 2 - overlay  (place framebuffer above everything)"
    puts $fd "rset var fb_overlay [rset var fb_overlay]"
    puts $fd ""
    puts $fd "# Constraint coordinate system"
    puts $fd "# m - model"
    puts $fd "# v - view"
    puts $fd "rset var coords [rset var coords]"
    puts $fd ""
    puts $fd "# Center about which to rotate"
    puts $fd "# m - model origin"
    puts $fd "# v - view center"
    puts $fd "# e - eye point"
    puts $fd "rset var rotate_about [rset var rotate_about]"
    puts $fd ""
    puts $fd "# v - apply mouse events to the view"
    puts $fd "# a - apply mouse events to the angle distance cursor"
    puts $fd "#     if active, otherwise apply to the view"
    puts $fd "# e - apply mouse events to the current edit"
    puts $fd "rset var transform [rset var transform]"
    puts $fd ""
    puts $fd "# set the perspective angle"
    puts $fd "rset var perspective [rset var perspective]"
    puts $fd ""
    puts $fd "###################### Query Ray Settings ######################"
    puts $fd "# Set the basename of the fake solids generated by nirt"
    puts $fd "qray basename [qray basename]"
    puts $fd ""
    puts $fd "# Specifies the kind of output generated by nirt."
    puts $fd "# g - graphics"
    puts $fd "# t - text"
    puts $fd "# b - both graphics and text"
    puts $fd "qray effects [qray effects]"
    puts $fd ""
    puts $fd "# 0 - command echo off"
    puts $fd "# 1 - command echo on"
    puts $fd "qray echo [qray echo]"
    puts $fd ""
    puts $fd "# Color of odd ray segments"
    puts $fd "qray oddcolor [qray oddcolor]"
    puts $fd ""
    puts $fd "# Color of even ray segments"
    puts $fd "qray evencolor [qray evencolor]"
    puts $fd ""
    puts $fd "# Color of ray segments that go"
    puts $fd "# through areas void of geometry"
    puts $fd "qray voidcolor [qray voidcolor]"
    puts $fd ""
    puts $fd "# Color of ray segments that go"
    puts $fd "# through areas of overlapping geometry"
    puts $fd "qray overlapcolor [qray overlapcolor]"
    puts $fd ""
    puts $fd "# Ray format string."
    puts $fd "qray fmt r [list [qray fmt r]]"
    puts $fd ""
    puts $fd "# Head format string."
    puts $fd "qray fmt h [list [qray fmt h]]"
    puts $fd ""
    puts $fd "# Partition format string."
    puts $fd "qray fmt p [list [qray fmt p]]"
    puts $fd ""
    puts $fd "# Foot format string."
    puts $fd "qray fmt f [list [qray fmt f]]"
    puts $fd ""
    puts $fd "# Miss format string."
    puts $fd "qray fmt m [list [qray fmt m]]"
    puts $fd ""
    puts $fd "# Overlap format string."
    puts $fd "qray fmt o [list [qray fmt o]]"
    puts $fd ""
    puts $fd "###################### Primitive Editor Variables ######################"
    puts $fd "# When using the Primitive Editor the following"
    puts $fd "# variables determine the default geometry"
    puts $fd "# attributes for specific solids."
    puts $fd "set solid_data(attr,arb8) [list $solid_data(attr,arb8)]"
    puts $fd "set solid_data(attr,sph) [list $solid_data(attr,sph)]"
    puts $fd "set solid_data(attr,ell) [list $solid_data(attr,ell)]"
    puts $fd "set solid_data(attr,tor) [list $solid_data(attr,tor)]"
    puts $fd "set solid_data(attr,tgc) [list $solid_data(attr,tgc)]"
    puts $fd "set solid_data(attr,rec) [list $solid_data(attr,rec)]"
    puts $fd "set solid_data(attr,half) [list $solid_data(attr,half)]"
    puts $fd "set solid_data(attr,rpc) [list $solid_data(attr,rpc)]"
    puts $fd "set solid_data(attr,rhc) [list $solid_data(attr,rhc)]"
    puts $fd "set solid_data(attr,epa) [list $solid_data(attr,epa)]"
    puts $fd "set solid_data(attr,ehy) [list $solid_data(attr,ehy)]"
    puts $fd "set solid_data(attr,eto) [list $solid_data(attr,eto)]"
    puts $fd "set solid_data(attr,part) [list $solid_data(attr,part)]"
    puts $fd "set solid_data(attr,dsp) [list $solid_data(attr,dsp)]"
    puts $fd "set solid_data(attr,hf) [list $solid_data(attr,hf)]"
    puts $fd "set solid_data(attr,ebm) [list $solid_data(attr,ebm)]"
    puts $fd "set solid_data(attr,vol) [list $solid_data(attr,vol)]"
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
