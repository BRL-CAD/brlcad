# Copyright (c) 2012-2014 United States Government as represented by
# the U.S. Army Research Laboratory.
# Copyright (c) 2008, Federico Ferri
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Federico Ferri nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL FEDERICO FERRI BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Apr 19 2012 - Enhancing original getopt functionality to support options
# taking a variable number of arguments - CY

# Example:
#
# getopt::init {
#         {verbose  v  {::verbose}}
#         {output   o  {::output ::output_filename}}
#         {objects  l  {::have_objs ::objlist ...}}
# }
# set argv2 [getopt::getopt $argv]

package provide GetOpt 1.0

namespace eval getopt {
	# list of option vars (keys are long option names)
	variable optlist

	# map short option names to long option names
	variable stl_map
}

proc getopt::init {optdata} {
	variable optlist
	variable stl_map
	array set optlist {}
	array set stl_map {}
	foreach item $optdata {
		foreach {longname shortname varlist} $item {
			set optlist($longname) $varlist
			set stl_map($shortname) $longname
		}
	}
}

proc getopt::expandOptNames {argv} {
	variable optlist
	variable stl_map
	set argv2 {}
	set argc [llength $argv]
	for {set i 0} {$i < $argc} {} {
		set argv_i [lindex $argv $i]
		incr i

		if [isShortOpt $argv_i] {
			set argv_i_opts [split [regsub {^-} $argv_i {}] {}]
			foreach shortOpt $argv_i_opts {
				if [info exists stl_map($shortOpt)] {
					set longOpt $stl_map($shortOpt)
					lappend argv2 --$longOpt
					set var2 [lindex $optlist($longOpt) 2]
					if {![regexp {^\.\.\.$} $var2]} {
					set n_required_opt_args [expr {-1+[llength $optlist($longOpt)]}]
					while {$n_required_opt_args > 0} {
						incr n_required_opt_args -1
						if {$i >= $argc} {
							puts "error: not enough arguments for option -$shortOpt"
							exit 3
						}
						lappend argv2 [lindex $argv $i]
						incr i
					}
					} else {
					  set curr_argv [lindex $argv $i]
					  while {![isShortOpt $curr_argv] && ![isLongOpt $curr_argv] && $i < $argc} {
						lappend argv2 [lindex $argv $i]
						incr i
						set curr_argv [lindex $argv $i]
					  }
					}
				} else {
					puts "error: unknown option: -$shortOpt"
					exit 2
				}
			}
			continue
		}

		lappend argv2 $argv_i
	}
	return $argv2
}

proc getopt::isShortOpt {o} {
	set is_short 0
	if {[regexp {^-[a-zA-Z0-9]+} $o] && ![regexp {^-[0-9]+} $o]} {set is_short 1}
	return $is_short
}

proc getopt::isLongOpt {o} {
	return [regexp {^--[a-zA-Z0-9][a-zA-Z0-9]*} $o]
}

proc getopt::getopt {argv} {
	variable optlist
	set argv [expandOptNames $argv]
	set argc [llength $argv]

	set residualArgs {}

	for {set i 0} {$i < $argc} {} {
		set argv_i [lindex $argv $i]
		incr i

		if [isLongOpt $argv_i] {
			set optName [regsub {^--} $argv_i {}]
			if [info exists optlist($optName)] {
				set varlist $optlist($optName)
				uplevel [list set [lindex $optlist($optName) 0] 1]
				set var2 [lindex $optlist($optName) 2]
				if {![regexp {^\.\.\.$} $var2]} {
				set n_required_opt_args [expr {-1+[llength $varlist]}]
				set j 1
				while {$n_required_opt_args > 0} {
					incr n_required_opt_args -1
					if {$i >= $argc} {
						puts "error: not enough arguments for option --$optName"
						exit 5
					}
					uplevel [list set [lindex $varlist $j] [lindex $argv $i]]
					incr j
					incr i
				}
				} else {
				    set curr_argv [lindex $argv $i]
				    set curr_container [lindex $optlist($optName) 1]
				    while {![isShortOpt $curr_argv] && ![isLongOpt $curr_argv] && $i < $argc} {
					 lappend $curr_container [lindex $argv $i]
					 incr i
					 set curr_argv [lindex $argv $i]
				    }
				}
			} else {
				puts "error: unknown option: --$optName"
				exit 4
			}
			continue
		}

		lappend residualArgs $argv_i
	}
	return $residualArgs
}
