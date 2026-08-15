#                     G U I _ B I N D I N G S _ T E S T . T C L
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by the U.S.
# Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# Exercise set_text_key_bindings without requiring a display.  The command
# stubs model the Tk queries performed while bindings are installed; the bind
# scripts themselves are deliberately not evaluated.

if {$argc != 1} {
    puts stderr "Usage: gui_bindings_test.tcl text.tcl"
    exit 1
}

source [lindex $argv 0]

set installed_bindings {}

proc bindtags {w args} {
    if {![llength $args]} {
	return [list $w Text all]
    }
    return [lindex $args 0]
}

proc bind {tag sequence args} {
    if {[llength $args]} {
	lappend ::installed_bindings [list $tag $sequence [lindex $args 0]]
    }
    return ""
}

proc winfo {operation args} {
    switch -- $operation {
	rgb {return {0 0 0}}
	exists {return 1}
	default {return 1}
    }
}

proc .test.t {operation args} {
    if {$operation == "cget"} {
	switch -- [lindex $args 0] {
	    -foreground {return white}
	    -background {return black}
	    -font {return TkFixedFont}
	    -width {return 80}
	}
    }
    return ""
}

set mged_gui(test,edit_style) emacs
if {[catch {set_text_key_bindings test} result options]} {
    puts stderr [dict get $options -errorinfo]
    exit 1
}

set configure_script ""
foreach binding $installed_bindings {
    if {[lindex $binding 0] == ".test.t" &&
	[lindex $binding 1] == "<Configure>"} {
	set configure_script [lindex $binding 2]
    }
}
if {$configure_script != "+mged_completion_display_schedule_repaint %W"} {
    puts stderr "incorrect Configure binding: $configure_script"
    exit 1
}

exit 0

# Local Variables:
# tab-width: 8
# mode: Tcl
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
