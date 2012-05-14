#!/bin/sh
#                    R T W I Z A R D . T C L
# BRL-CAD
#
# Copyright (c) 2006-2012 United States Government as represented by
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
# Start-up script for both the command line and graphical modes of
# rtwizard
#

# The trailing backslash forces tcl to skip the next line \
STARTUP_HOME=`dirname $0`/..
#\
export STARTUP_HOME
# restart using btclsh \
TCLSH="btclsh"
#\
for clsh in btclsh btclsh_d ; do
# see if we're installed \
    if test -f ${STARTUP_HOME}/bin/$clsh ; then
#\
	TCLSH="${STARTUP_HOME}/bin/$clsh"
#\
	break;
#\
    fi
# see if we're not installed yet \
    if test -f ${STARTUP_HOME}/btclsh/$clsh ; then
#\
	TCLSH="${STARTUP_HOME}/btclsh/$clsh"
#\
	break;
#\
    fi
#\
done
#\
exec "$TCLSH" "$0" "$@"

#
# Begin Tcl here!
#

# We might as well populate options directly in the namespace
# where they will eventually be used.  Define that namespace
# up front.
namespace eval RtWizard {}

# Set up the options we support with rtwizard.  We will use
# an associative array named wizard_state in the RtWizard
# namespace to hold the key information - have getopt place the
# results of its parsing directly in that array
package require GetOpt
getopt::init {
        # GUI controls
        {gui 			""  	{::use_gui}}
        {no-gui 		""  	{::disable_gui}}
	# Input/output files and framebuffers
        {g-file   		i  	{::have_gfile ::RtWizard::wizard_state(dbFile)}}
        {output   		o  	{::output ::RtWizard::wizard_state(output_filename)}}
        {framebuffer_type 	F  	{::framebuffer_type ::RtWizard::wizard_state(framebuffer_type)}}
        {width 			w	{::have_width ::RtWizard::wizard_state(width)}}
        {height 		n 	{::have_nlines ::RtWizard::wizard_state(nlines)}}
	# Objects to raytrace
        {color-objects		c	{::have_full_color_objs ::RtWizard::wizard_state(color_objlist) ...}}
        {ghost-objects		g	{::have_ghost_objs ::RtWizard::wizard_state(ghost_objlist) ...}}
        {edge-objects		e	{::have_edge_objs ::RtWizard::wizard_state(edge_objlist) ...}}
	# Settings
        {background-color 	C 	{::have_bg_color ::RtWizard::wizard_state(bg_color)}}
        {ghosting-intensity 	G	{::have_ghosting_intensity ::RtWizard::wizard_state(ghosting_intensity)}}
        {edge-color		""	{::have_line_color ::RtWizard::wizard_state(e_color)}}
        {non-edge-color 	"" 	{::have_non_line_color ::RtWizard::wizard_state(ne_color)}}
        {occlusion 		O 	{::have_occlusion_mode ::RtWizard::wizard_state(occmode)}}
	# Image type
        {type 			t 	{::have_picture_type ::RtWizard::wizard_state(picture_type)}}
	# View
        {azimuth 		a  	{::have_azimuth ::RtWizard::wizard_state(init_azimuth)}}
        {elevation 		l 	{::have_elevation ::RtWizard::wizard_state(init_elevation)}}
        {twist 			"" 	{::have_twist ::RtWizard::wizard_state(init_twist)}}
        {zoom 			z 	{::have_zoom ::RtWizard::wizard_state(zoom)}}
}

# Perform the actual option parsing
if {[info exists argv]} {
  set argv2 [getopt::getopt $argv]
}
if {[info exists argc]} {
  set argc2 $argc
} else {
  set argc2 0
}

# If we have both gui and no-gui specified, use gui
if {[info exists ::use_gui] && [info exists ::disable_gui]} {
   puts "Warning - both -gui and -no-gui supplied - enabling gui"
   unset ::disable_gui
}

# There are three common possibilities for inputs specified without an option flag - the
# Geometry Database, the output filename and one or more full color components (i.e. the
# standard rt paradigm.)  It isn't possible to fully generalize handling of unspecified
# options, but there are a few cases we can support for convenience.

# See if any of the residual arguments after getopt identify a .g file that exists
if {[info exists argv2]} {
   set possible_incorrect_g_name 0
   set residualArgs {}
   foreach item $argv2 {
     if {[file extension $item] == ".g"} {
	 if {![info exists ::RtWizard::wizard_state(dbFile)]} {
	    if {[file exists $item]} {
	      set ::RtWizard::wizard_state(dbFile) $item
	    } else {
	      set possible_incorrect_g_name $item
	    }
	 }
     } else {
       lappend residualArgs $item
     }
   }
   if {![info exists ::RtWizard::wizard_state(dbFile)] && $possible_incorrect_g_name} {
      puts "Error: $possible_incorrect_g_name appears to specify a .g file, but file is not found."
      if {[info exists argv]} {exit}
   }
   set argv2 $residualArgs
}

# If it looks like we have a .pix or .png filename, use it for output
if {[info exists argv2]} {
   set residualArgs {}
   foreach item $argv2 {
     if {[file extension $item] == ".pix" || [file extension $item] == ".png"} {
	 if {![info exists ::RtWizard::wizard_state(output_filename)]} {
	    set ::RtWizard::wizard_state(output_filename) $item
	 } else {
	    puts "Note - $item potentially specifies an output file, but $::RtWizard::wizard_state(output_filename) is already set as the output file."
            lappend residualArgs $item
	 }
     } else {
       lappend residualArgs $item
     }
   }
   set argv2 $residualArgs
}

# If we still have something left, assume full color objects are being specified.  May be an incorrect
# assumption, but after the parsing already done they're either object names or garbage and we may as
# well fail after trying them.

if {[info exists argv2]} {
    if {![info exists ::RtWizard::wizard_state(color_objlist)]} {
       set ::RtWizard::wizard_state(color_objlist) {}
    }
    foreach item $argv2 {
       lappend ::RtWizard::wizard_state(color_objlist) $item
    }
}

# If we have an explicit picture type, check whether we satisfy the minimum
# data input for that type.
if {[info exists ::have_picture_type] && ![info exists ::use_gui]} {
  switch $::RtWizard::wizard_state(picture_type) {
    A   -
    1	{
          if {![info exists ::RtWizard::wizard_state(color_objlist)]} {
             if ([info exists ::disable_gui]) {
               puts "Error - picture type $RtWizard::wizard_state(picture_type) specified, but no full color objects listed"
               puts "Please specify full color objects using the -c option\n"
               exit
             } else {
               set ::use_gui 1
             }
          }
        }
    B   -
    2	{
          if {![info exists ::RtWizard::wizard_state(edge_objlist)]} {
             if ([info exists ::disable_gui]) {
               puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no edge objects listed"
               puts "Please specify edge objects using the -e option\n"
               exit
             } else {
               set ::use_gui 1
             }
          }
        }
    C   -
    D   -
    3	-
    4	{
          if {![info exists ::RtWizard::wizard_state(color_objlist)] || ![info exists ::RtWizard::wizard_state(edge_objlist)]} {
            if (![info exists ::disable_gui]) {
               set ::use_gui 1
            } else {
              if {![info exists ::RtWizard::wizard_state(edge_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no edge objects listed"
                puts "Please specify edge objects using the -e option\n"
              }
              if {![info exists ::RtWizard::wizard_state(color_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no color objects listed"
                puts "Please specify full color objects using the -c option\n"
              }
             exit
            }
          }
        }
    E   -
    5	{
          if {![info exists ::RtWizard::wizard_state(color_objlist)] || ![info exists ::RtWizard::wizard_state(ghost_objlist)]} {
            if (![info exists ::disable_gui]) {
               set ::use_gui 1
            } else {
              if {![info exists ::RtWizard::wizard_state(ghost_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no ghost objects listed"
                puts "Please specify ghost objects using the -g option\n"
              }
              if {![info exists ::RtWizard::wizard_state(color_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no color objects listed"
                puts "Please specify full color objects using the -c option\n"
              }
             exit
            }
          }
        }
    F   -
    6	{
          if {![info exists ::RtWizard::wizard_state(color_objlist)] || ![info exists ::RtWizard::wizard_state(edge_objlist)] || ![info exists ::RtWizard::wizard_state(ghost_objlist)]} {
            if (![info exists ::disable_gui]) {
               set ::use_gui 1
            } else {
              if {![info exists ::RtWizard::wizard_state(ghost_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no ghost objects listed"
                puts "Please specify ghost objects using the -g option\n"
              }
              if {![info exists ::RtWizard::wizard_state(color_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no color objects listed"
                puts "Please specify full color objects using the -c option\n"
              }
              if {![info exists ::RtWizard::wizard_state(edge_objlist)]} {
                puts "Error - picture type $::RtWizard::wizard_state(picture_type) specified, but no edge objects listed"
                puts "Please specify edge objects using the -e option\n"
              }
             exit
            }
          }
        }
    default {puts "Error - unknown picture type $::RtWizard::wizard_state(picture_type)\n"; exit}
  }
}

# OK, we've collected all the info we can from the inputs.  Make sure all the key
# variables are initialized to sane defaults.  The viewsize and eye_pt defaults are determined from
# the drawing of the objects into the display manager.
# Initial orientation
if {![info exists ::RtWizard::wizard_state(init_azimuth)]} { set ::RtWizard::wizard_state(init_azimuth) 35 }
if {![info exists ::RtWizard::wizard_state(init_elevation)]} { set ::RtWizard::wizard_state(init_elevation) 25 }
if {![info exists ::RtWizard::wizard_state(init_twist)]} { set ::RtWizard::wizard_state(init_twist) 0 }
# Initial zoom
if {![info exists ::RtWizard::wizard_state(zoom)]} { set ::RtWizard::wizard_state(zoom) 1 }
# Background color
if {![info exists ::RtWizard::wizard_state(bg_color)]} { set ::RtWizard::wizard_state(bg_color) {255 255 255} }
# Edge lines color
if {![info exists ::RtWizard::wizard_state(e_color)]} { set ::RtWizard::wizard_state(e_color) {0 0 0} }
# Edge not-lines color
if {![info exists ::RtWizard::wizard_state(ne_color)]} { set ::RtWizard::wizard_state(ne_color) {0 0 0} }
# Occlusion mode
if {![info exists ::RtWizard::wizard_state(occmode)]} { set ::RtWizard::wizard_state(occmode) 1 }
# Ghost intensity
if {![info exists ::RtWizard::wizard_state(ghosting_intensity)]} { set ::RtWizard::wizard_state(ghosting_intensity) 12 }
# Pix width
if {![info exists ::RtWizard::wizard_state(width)]} { set ::RtWizard::wizard_state(width) 512 }
# Pix height (number of scan lines) 
if {![info exists ::RtWizard::wizard_state(nlines)]} { set ::RtWizard::wizard_state(nlines) 512 }


# If we haven't been told what .g file to use, we're going to have to go graphical
if {![info exists ::RtWizard::wizard_state(dbFile)]} {
   if {![info exists ::disable_gui]} {
    set ::use_gui 1
   } else {
    puts "Error - please specify Geometry Database (.g) file."
    if {[info exists argv]} {exit}
   }
}

# We can set a lot of defaults, but not the objects to draw - if we don't have *something* specified,
# we have to go graphical.
if {![info exists ::RtWizard::wizard_state(color_objlist)] && ![info exists ::RtWizard::wizard_state(edge_objlist)] && ![info exists ::RtWizard::wizard_state(ghost_objlist)]} {
   if {![info exists ::disable_gui]} {
    set ::use_gui 1
   } else {
    puts "Error - please specify one or more objects for at least one of color, ghost, or edge rendering modes."
    if {[info exists argv]} {exit}
   }
}

# Now that we have made our gui determination based on the variables, initialize any lists that aren't
# initialized for the objects
# Color objects
if {![info exists ::RtWizard::wizard_state(color_objlist)]} { set ::RtWizard::wizard_state(color_objlist) {} }
# Edge objects
if {![info exists ::RtWizard::wizard_state(edge_objlist)]} { set ::RtWizard::wizard_state(edge_objlist) {} }
# Ghost objects
if {![info exists ::RtWizard::wizard_state(ghost_objlist)]} { set ::RtWizard::wizard_state(ghost_objlist) {} }

# If we're launching without enough arguments to fully specify an rtwizard 
# run, a gui run has been specifically requested, or we've got arguments 
# that aren't understood, go graphical
if {[info exists ::use_gui]} {
   # Have to do these loads until we get "package require tclcad" and "package require dm" 
   # working - bwish loads them for us by default, but since rtwizard may be either
   # graphical or command line we need to start with btclsh
   load [file join [bu_brlcad_root "lib"] libtclcad[info sharedlibextension]]
   load [file join [bu_brlcad_root "lib"] libdm[info sharedlibextension]]
   # Now, load the actual Raytrace Wizard GUI
   package require RaytraceWizard
   if {[info exists argv]} {exit}
} else {

   if {![info exists ::RtWizard::wizard_state(output_filename)] && ![info exists ::RtWizard::wizard_state(framebuffer_type)]} {
     set ::RtWizard::wizard_state(output_filename) rtwizard.pix
     if {![file exists $::RtWizard::wizard_state(output_filename)]} {
        puts "Warning - no output file or framebuffer specified - using file rtwizard.pix for output."
     }
   }
   if {[info exists ::RtWizard::wizard_state(output_filename)]} {
     if {[file exists $::RtWizard::wizard_state(output_filename)]} {
        puts "Error - cannot create output file, $::RtWizard::wizard_state(output_filename) alread exists."
        if {[info exists argv]} {exit}
     }
   }

   package require cadwidgets::RtImage
   set db [go_open db db $::RtWizard::wizard_state(dbFile)]
   db new_view v1 nu

   # Get an in-memory framebuffer to hold the intermediate image stages
   if {![info exists ::RtWizard::wizard_state(framebuffer_type)]} {
      set ::RtWizard::wizard_state(framebuffer_type) /dev/mem
   }
   set fbserv_port 12
   catch {exec [file join [bu_brlcad_root bin] fbserv] -w $::RtWizard::wizard_state(width) -n $::RtWizard::wizard_state(nlines) $fbserv_port $::RtWizard::wizard_state(framebuffer_type) &} pid
   set fbserv_pid $pid
   if {[llength $::RtWizard::wizard_state(color_objlist)]} {
      foreach item $::RtWizard::wizard_state(color_objlist) {
	db draw $item 
      }
   } 
   if {[llength $::RtWizard::wizard_state(edge_objlist)]} {
      foreach item $::RtWizard::wizard_state(edge_objlist) {
	db draw $item 
      }
   } 
   if {[llength $::RtWizard::wizard_state(ghost_objlist)]} {
      foreach item $::RtWizard::wizard_state(ghost_objlist) {
	db draw $item 
      }
   } 
   db autoview v1
   db aet v1 $::RtWizard::wizard_state(init_azimuth) $::RtWizard::wizard_state(init_elevation) $::RtWizard::wizard_state(init_twist)
   db zoom v1 $::RtWizard::wizard_state(zoom)
   set view_info [regsub -all ";" [db get_eyemodel v1] ""]
   set vdata [split $view_info "\n"]
   set viewsize [lindex [lindex $vdata 0] 1]
   set orientation [lrange [lindex $vdata 1] 1 end]
   set eye_pt [lrange [lindex $vdata 2] 1 end]
   ::cadwidgets::rtimage $::RtWizard::wizard_state(dbFile) $fbserv_port \
			$::RtWizard::wizard_state(width) $::RtWizard::wizard_state(nlines) \
			$viewsize $orientation $eye_pt \
			$::RtWizard::wizard_state(bg_color) $::RtWizard::wizard_state(e_color) $::RtWizard::wizard_state(ne_color)\
			$::RtWizard::wizard_state(occmode) $::RtWizard::wizard_state(ghosting_intensity) \
			$::RtWizard::wizard_state(color_objlist) \
			$::RtWizard::wizard_state(ghost_objlist) \
			$::RtWizard::wizard_state(edge_objlist)
		
   if {[info exists ::RtWizard::wizard_state(output_filename)]} {
      if {[file extension $::RtWizard::wizard_state(output_filename)] == ".pix"} {
         exec [file join [bu_brlcad_root bin] fb-pix] -w $::RtWizard::wizard_state(width) -n $::RtWizard::wizard_state(nlines) -F $fbserv_port $::RtWizard::wizard_state(output_filename) > junk
      }
      if {[file extension $::RtWizard::wizard_state(output_filename)] == ".png"} {
         exec [file join [bu_brlcad_root bin] fb-png] -w $::RtWizard::wizard_state(width) -n $::RtWizard::wizard_state(nlines) -F $fbserv_port $::RtWizard::wizard_state(output_filename) > junk
      }
   }

   if {$::RtWizard::wizard_state(framebuffer_type) == "/dev/mem"} {
       if {$tcl_platform(platform) == "windows"} {
	   set kill_cmd [auto_execok taskkill]
       } else {
	   set kill_cmd [auto_execok kill]
       }

       if {$kill_cmd != ""} {
	   exec $kill_cmd $fbserv_pid
       }
   }
}

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
