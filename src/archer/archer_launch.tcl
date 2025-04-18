#                A R C H E R _ I N I T . T C L
# BRL-CAD
#
# Copyright (c) 2002-2025 United States Government as represented by
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
# Archer Initialization script
#
# Author(s):
#    Bob Parker
#    Doug Howard
#
# Description:
#    Main for Archer.


# If we're not launching with argv0 populated (i.e. we're
# launching via the mged -o option ) try info nameofexecutable
if {! [info exists argv0] } {
   set argv0 [info nameofexecutable]
}

# If we're running from a build directory but an installed BRL-CAD
# already exists in the configured install path, all tcl scripts
# will be pulled from the installed location and not the local
# build location.  This is an inevitable consequence of how
# BRL-CAD's path lookup has to work (there are security implication to
# NOT using the installed files if they are present) but it also
# makes for a subtle and vexing problem if a developer is trying
# to tweak tcl scripts in the source tree and forgets that
# there is already an installed copy of that script.  It will look like
# changes being made to the script are having no effect, because the
# installed file will be loaded instead of the edited version.
# Warn if it looks like this situation is occurring by checking
# the root path against the argv0 path.
set check_root_dir [file normalize [bu_dir bin]]
set check_bin_dir [file dirname [file normalize $argv0]]

# Because there are conditions where we don't know our argv0 full path
# reliably, make sure the "normalized" argv0 executable actually exists at that
# path before we start complaining.
if {[file exists [file normalize $argv0]]} {
    set dir_same [string compare $check_root_dir $check_bin_dir]
    if {!$dir_same == 0} {
	puts "WARNING - bu_dir's bin value is set to [file dirname $check_root_dir], but binary being run is located in [file dirname $check_bin_dir].  This probably means you are running [file tail $argv0] from a non-install directory with BRL-CAD already present in [file dirname $check_root_dir] - be aware that .tcl files from [file dirname $check_root_dir] will be loaded INSTEAD OF local files. Tcl script changes made to source files for testing purposes will not be loaded, even though [file tail $argv0] will most likely 'work'.  To test local changes, either clear [file dirname $check_root_dir], specify a different install prefix (i.e. a directory *without* BRL-CAD installed) while building, or manually set the BRLCAD_ROOT environment variable."
    }
}

# Itk's default class doesn't keep the menu, but Archer needs it - redefine itk:Toplevel
set itk_file [file join [bu_dir data] "tclscripts" archer itk_redefines.tcl]
if {[file exists [file normalize $itk_file]]} {
    source $itk_file
}

# Set ttk theme
if {[tk windowingsystem] eq "aqua"} {
   ::ttk::style theme use aqua
} else {
   ::ttk::style theme use clam
}

# normalize dir
if {[info exists argv0]} {
    set dir [file normalize [file join [file dir $argv0] ..]]
} else {
    set dir [file normalize [pwd]]
}

if {$tcl_platform(platform) == "windows"} {
    lappend auto_path ${dir}/lib/Tkhtml3.0
    lappend auto_path ${dir}/lib/Tktable2.10
}

# load archer guts
if { [catch {package require Archer 1.0} _initialized] } {
    puts "$_initialized"
    puts ""
    puts "ERROR: Unable to load Archer"
    exit 1
}

# Initialize bgerror
initBgerror


# PROCEDURE: showMainWindow
#
# Show the main window.
#
# Arguments:
#       None
#
# Results:
#       No return.
#
proc showMainWindow {} {
    if {$::ArcherCore::inheritFromToplevel} {
	set initx 0
	set inity 22
	wm deiconify $::ArcherCore::application
	wm geometry $::ArcherCore::application +$initx+$inity
	raise $::ArcherCore::application
	focus -force $::ArcherCore::application
    } else {
	wm deiconify .
	raise .
	focus -force .
    }
}

# PROCEDURE: createSplashScreen
#
# Create the proper splash screen based on gui mode.
#
# Arguments:
#       None
#
# Results:
#       No return.
#
proc createSplashScreen {} {
    global env

    if { ![info exists ::ArcherCore::splash] } {
	return
    }

    set useImage 1

    if {$useImage} {
	set imgfile [file join [bu_dir data] tclscripts archer images aboutArcher.png]
	set image [image create photo -file $imgfile]
	set ::ArcherCore::splash [Splash .splash -image $image]
    } else {
	set ::ArcherCore::splash [Splash .splash -message "Loading Archer ... please wait ..."]
    }

    update idletasks
    $::ArcherCore::splash configure -background $::ArcherCore::SystemWindow
    $::ArcherCore::splash center
    update idletasks
    $::ArcherCore::splash activate
    after 1500 destroySplashScreen; # 1.5 more sec
    update
}

# PROCEDURE: destroySplashScreen
#
# Makes sure the gui is ready to go and then
# destroys the splash screen.
#
# Arguments:
#       None
#
# Results:
#       No return.
#
proc destroySplashScreen {} {
    # check to see if main gui is ready
    if {$::ArcherCore::showWindow == 0} {
        after 1000 destroySplashScreen; # 1 more sec
        return
    }

    if {$::ArcherCore::splash == ""} {
	return
    }
    $::ArcherCore::splash deactivate
    itcl::delete object $::ArcherCore::splash
    set ::ArcherCore::splash ""
    showMainWindow
}

proc exitArcher {archer} {
    $archer askToSave

    if {$::ArcherCore::inheritFromToplevel} {
	wm withdraw $archer
    } else {
	wm withdraw .
    }

    if {$archer != ""} {
	::itcl::delete object $archer
    }

    exit
}

# *************** Main Function ***************

# PROCEDURE: main
#
# Main procedure for AJEM/MUVES GUI.  Sets up and handles initialization.
#
# Arguments:
#       None
#
# Results:
#       No return.
#
proc main {} {
    global env
    global tcl_platform
    global argv
    global brlcad_version

    if {$::ArcherCore::inheritFromToplevel} {
	set ::ArcherCore::application [Archer .\#auto]
	wm title $::ArcherCore::application "Archer $brlcad_version"
	if {$tcl_platform(os) == "Windows NT"} {
	    wm iconbitmap $::ArcherCore::application -default \
		[file join [bu_dir data] icons archer.ico]
	}
	set size [wm maxsize $::ArcherCore::application]
	set w [lindex $size 0]
	set h [lindex $size 1]
	if {1400 < $w} {
	    set w 1400
	}
	if {1100 < $h} {
	    set h 1100
	}
        set width [expr {$w - 8}]
        set height [expr {$h - 40}]
	$::ArcherCore::application configure -geometry "$width\x$height+0+0"
	wm protocol $::ArcherCore::application WM_DELETE_WINDOW {exitArcher $::ArcherCore::application}
    } else {
	wm title . "Archer $brlcad_version"
	if {$tcl_platform(os) == "Windows NT"} {
	    wm iconbitmap . -default \
		[file join [bu_dir data] html manuals archer archer.ico]
	}
	set ::ArcherCore::application [Archer .\#auto]
	set size [wm maxsize .]
	set w [lindex $size 0]
	set h [lindex $size 1]
	if {1600 < $w} {
	    set w 1600
	}
	if {1200 < $h} {
	    set h 1200
	}
        set width [expr {$w - 8}]
        set height [expr {$h - 40}]
	pack $::ArcherCore::application -expand yes -fill both
	$::ArcherCore::application configure -geometry "$width\x$height+0+0"
	wm protocol . WM_DELETE_WINDOW [list exitArcher $::ArcherCore::application]
    }

    if {[info exists argv] && $argv != ""} {
	package require cadwidgets::GeometryIO
	set target [cadwidgets::geom_load [lindex $argv 0] 1]
	$::ArcherCore::application Load $target
    }

    $::ArcherCore::application configure -quitcmd [list exitArcher $::ArcherCore::application]

    update
}

# TCL script execution start HERE!
wm withdraw .

# start splash screen
createSplashScreen
if { [info exists argc] && $::argc eq 1 } {
   set argv1 [lindex $argv 0]
   set argv1_ext [file extension $argv1]
   if {[string compare -nocase $argv1_ext ".g"] != 0} {
       $::ArcherCore::splash deactivate
   }
}


# do main procedure
if { [catch {main} _runnable] } {
	puts "$_runnable"
	puts ""
	puts "Unexpected error encountered while running Archer."
	puts "Aborting."
	exit 1
}

set ::ArcherCore::showWindow 1

# If we don't have bwish in the background, use vwait
if { [info exists ::no_bwish] } {
    vwait forever
}

# Local Variables:
# mode: tcl
# tab-width: 8
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8

