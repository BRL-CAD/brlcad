#==============================================================================
#
# Help facility for pl-dm
#
#==============================================================================

set help_data(?)	{{} {summary of available commands}}
set help_data(ae)	{{[-i] azim elev [twist]} {set view using azim, elev and twist angles}}
set help_data(clear)	{{} {clear screen}}
set help_data(closepl)	{{plot file(s)} {close one or more plot files}}
set help_data(dm)	{{set var [val]} {Do display-manager specific command}}
set help_data(draw)	{{object(s)} {draw object(s)}}
set help_data(erase)	{{object(s)} {erase object(s)}}
set help_data(exit)	{{} {quit}}
set help_data(help)	{{[commands]} {give usage message for given commands}}
set help_data(ls)	{{} {list objects}}
set help_data(openpl)	{{plotfile(s)} {open one or more plot files}}
set help_data(q)	{{} {quit}}
set help_data(reset)    {{reset} {reset view}}
set help_data(sv)	{{x y [z]} {Move view center to (x, y, z)}}
set help_data(t)	{{} {list object(s)}}
set help_data(vrot)	{{xdeg ydeg zdeg} {rotate viewpoint}}
set help_data(zoom)	{{scale_factor} {zoom view in or out}}

proc help {args} {
	global help_data

        if {[llength $args] > 0} {
                return [help_comm help_data $args]
        } else {
                return [help_comm help_data]
        }
}

proc ? {} {
        global help_data

        return [?_comm help_data 20 4]
}

proc apropos key {
        global help_data

        return [apropos_comm help_data $key]
}

