
###########################################################################
# hv3_nav.tcl --
#
#     The public interface to this file is the commands:
#
#         nav_init HTML
#         nav_add HTML URL
# 

#--------------------------------------------------------------------------
# Global variables section
set ::html_nav_doclist [list]
set ::html_nav_where -1
 
#--------------------------------------------------------------------------

proc nav_init {HTML} {
    .m add cascade -label {Navigation} -menu [menu .m.nav]
    .m.nav add command -label {Forward} -command [list navForward $HTML]
    .m.nav add command -label {Back}    -command [list navBack $HTML]
    .m.nav add separator

    set ::html_nav_doclist [list]
    navEnableDisable
}

proc nav_add {HTML url} {
    if {$url == [lindex $::html_nav_doclist $::html_nav_where]} return

    if {$url !=  [lindex $::html_nav_doclist [expr $::html_nav_where + 1]]} {
        set ::html_nav_doclist [lrange $::html_nav_doclist 0 $::html_nav_where]
        lappend ::html_nav_doclist $url
    }
    incr ::html_nav_where
    navEnableDisable
}

proc navForward {HTML} {
    navGoto [expr $::html_nav_where + 1]
}

proc navBack {HTML} {
    navGoto [expr $::html_nav_where - 1]
}

proc navGoto {where} {
    set ::html_nav_where $where
    gui_goto [lindex $::html_nav_doclist $::html_nav_where]
    navEnableDisable
}

proc navEnableDisable {} {
    if {$::html_nav_where < 1} {
        .m.nav entryconfigure Back -state disabled
    } else {
        .m.nav entryconfigure Back -state normal
    }
    if {$::html_nav_where == ([llength $::html_nav_doclist] - 1)} {
        .m.nav entryconfigure Forward -state disabled
    } else {
        .m.nav entryconfigure Forward -state normal
    }

    if {[.m.nav index end] > 3} {
        .m.nav delete 4 end
    }
    for {set ii 0} {$ii < [llength $::html_nav_doclist]} {incr ii} {
        set doc [lindex $::html_nav_doclist $ii]
        .m.nav add command -label $doc -command [list navGoto $ii]
    }

    if {[.m.nav index end] > 3} {
        set idx [expr $::html_nav_where + 4] 
        .m.nav entryconfigure $idx -background white -state disabled
    }
}

