#!/bin/sh
#
# restart using tclsh
#\
TCLSH=`dirname $0`/tclsh.exe
#\
exec $TCLSH $0 $@

set usage "Usage: init.sh root_src_dir"

if {1} {
    # Use this section for calls from Visual Studio
    if {$argc != 0} {
	puts $usage
	return
    }

    set dir [file normalize ../../..]
    lappend auto_path [file join $dir src other tcl library]
} else {
    # Use this section when passing in the target directory
    if {$argc != 1} {
	puts $usage
	return
    }

    set dir [file normalize [lindex $argv 0]]
}

if {![file exists $dir]} {
    puts "$dir must exist and must be the root of the BRL-CAD source tree. "
    return
}


if {![file exists [file join $dir brlcadInstall]]} {
    puts "Creating [file join $dir brlcadInstall]"
    file mkdir [file join $dir brlcadInstall]
}

if {![file exists [file join $dir brlcadInstall bin]]} {
    puts "Creating [file join $dir brlcadInstall bin]"
    file mkdir [file join $dir brlcadInstall bin]
}

if {![file exists [file join $dir brlcadInstall lib]]} {
    puts "Creating [file join $dir brlcadInstall lib]"
    file mkdir [file join $dir brlcadInstall lib]
    puts "Copying [file join $dir src other blt library] [file join $dir brlcadInstall lib blt2.4]"
    file copy [file join $dir src other blt library] [file join $dir brlcadInstall lib blt2.4]
    puts "Copying [file join $dir src other tcl library] [file join $dir brlcadInstall lib tcl8.5]"
    file copy [file join $dir src other tcl library] [file join $dir brlcadInstall lib tcl8.5]
    puts "Copying [file join $dir src other tk library] [file join $dir brlcadInstall lib tk8.5]"
    file copy [file join $dir src other tk library] [file join $dir brlcadInstall lib tk8.5]
    puts "Copying [file join $dir src other incrTcl itcl library] [file join $dir brlcadInstall lib itcl3.4]"
    file copy [file join $dir src other incrTcl itcl library] [file join $dir brlcadInstall lib itcl3.4]
    puts "Copying [file join $dir src other incrTcl itk library] [file join $dir brlcadInstall lib itk3.4]"
    file copy [file join $dir src other incrTcl itk library] [file join $dir brlcadInstall lib itk3.4]

    puts "Creating [file join $dir brlcadInstall lib iwidgets4.0.2]"
    file mkdir [file join $dir brlcadInstall lib iwidgets4.0.2]
    puts "Copying [file join $dir src other iwidgets generic] [file join $dir brlcadInstall lib iwidgets4.0.2]"
    file copy [file join $dir src other iwidgets generic] [file join $dir brlcadInstall lib iwidgets4.0.2 scripts]

    set fd1 [open [file join $dir src other iwidgets iwidgets.tcl.in] r]
    set lines [read $fd1]
    close $fd1
    puts "Creating [file join $dir brlcadInstall lib iwidgets4.0.2 iwidgets.tcl]"
    set fd2 [open [file join $dir brlcadInstall lib iwidgets4.0.2 iwidgets.tcl] w]
    set lines [regsub -all {@ITCL_VERSION@} $lines "3.4"]
    set lines [regsub -all {@IWIDGETS_VERSION@} $lines "4.0.2"]
    puts $fd2 $lines
    close $fd2

    set fd1 [open [file join $dir src other iwidgets pkgIndex.tcl.in] r]
    set lines [read $fd1]
    close $fd1
    puts "Creating [file join $dir brlcadInstall lib iwidgets4.0.2 pkgIndex.tcl]"
    set fd2 [open [file join $dir brlcadInstall lib iwidgets4.0.2 pkgIndex.tcl] w]
    set lines [regsub -all {@IWIDGETS_VERSION@} $lines "4.0.2"]
    puts $fd2 $lines
    close $fd2
}

if {![file exists [file join $dir brlcadInstall doc]]} {
    puts "Copying [file join $dir doc] to [file join $dir brlcadInstall]"
    file copy [file join $dir doc] [file join $dir brlcadInstall]
}

if {![file exists [file join $dir brlcadInstall COPYING]]} {
    puts "Copying [file join $dir COPYING] to [file join $dir brlcadInstall]"
    file copy [file join $dir COPYING] [file join $dir brlcadInstall]
}

if {![file exists [file join $dir brlcadInstall tclscripts]]} {
    puts "Copying [file join $dir src tclscripts] to [file join $dir brlcadInstall]"
    file copy [file join $dir src tclscripts] [file join $dir brlcadInstall]
}

if {![file exists [file join $dir brlcadInstall bin archer]] && [file exists [file join $dir src archer archer]]} {
    puts "Copying [file join $dir src archer archer] to [file join $dir brlcadInstall bin]"
    file copy [file join $dir src archer archer] [file join $dir brlcadInstall bin]
}

if {![file exists [file join $dir brlcadInstall bin archer.bat]] && [file exists [file join $dir src archer archer.bat]]} {
    puts "Copying [file join $dir src archer archer.bat] to [file join $dir brlcadInstall bin]"
    file copy [file join $dir src archer archer.bat] [file join $dir brlcadInstall bin]
}

if {![file exists [file join $dir brlcadInstall bin mged.bat]] && [file exists [file join $dir src mged mged.bat]]} {
    puts "Copying [file join $dir src mged mged.bat] to [file join $dir brlcadInstall bin]"
    file copy [file join $dir src mged mged.bat] [file join $dir brlcadInstall bin]
}

if {![file exists [file join $dir brlcadInstall plugins]]} {
    puts "Creating [file join $dir brlcadInstall plugins]"

    catch {
	file mkdir [file join $dir brlcadInstall plugins]
	file mkdir [file join $dir brlcadInstall plugins archer]
	file mkdir [file join $dir brlcadInstall plugins archer Utilities]
	file mkdir [file join $dir brlcadInstall plugins archer Wizards]

	file copy [file join $dir src archer plugins utility.tcl] [file join $dir brlcadInstall plugins archer]
	file copy [file join $dir src archer plugins wizards.tcl] [file join $dir brlcadInstall plugins archer]
    }
}

if {![file exists [file join $dir include conf COUNT]]} {
    if {![catch {open [file join $dir include conf COUNT] w} fd]} {
	puts "Creating [file join $dir include conf COUNT]"
	puts $fd "0"
	close $fd
    } else {
	puts "Unable to create [file join $dir include conf COUNT]\n$fd"
    }
}

if {![file exists [file join $dir include conf HOST]]} {
    if {![catch {open [file join $dir include conf HOST] w} fd]} {
	if {[info exists env(HOSTNAME)] && $env(HOSTNAME) != ""} {
	    set host $env(HOSTNAME)
	} else {
	    set host unknown
	}

	puts "Creating [file join $dir include conf HOST]"
	puts $fd \"$host\"
	close $fd
    } else {
	puts "Unable to create [file join $dir include conf DATE]\n$fd"
    }
}

if {![file exists [file join $dir include conf PATH]]} {
    if {![catch {open [file join $dir include conf PATH] w} fd]} {
	puts "Creating [file join $dir include conf PATH]"
	puts $fd \"brlcad\"
	close $fd
    } else {
	puts "Unable to create [file join $dir include conf PATH]\n$fd"
    }
}

if {![file exists [file join $dir include conf USER]]} {
    if {![catch {open [file join $dir include conf USER] w} fd]} {
	if {[info exists env(USER)] && $env(USER) != ""} {
	    set user $env(USER)
	} else {
	    set user unknown
	}

	puts "Creating [file join $dir include conf USER]"
	puts $fd \"$user\"
	close $fd
    } else {
	puts "Unable to create [file join $dir include conf USER]\n$fd"
    }
}

if {![file exists [file join $dir src other tk win wish.exe.manifest]] &&
    [file exists [file join $dir src other tk win wish.exe.manifest].in]} {
    set fd1 [open [file join $dir src other tk win wish.exe.manifest].in r]
    set lines [read $fd1]
    close $fd1
    puts "Creating [file join $dir src other tk win wish.exe.manifest]"
    set fd2 [open [file join $dir src other tk win wish.exe.manifest] w]
    set lines [regsub -all {@TK_WIN_VERSION@} $lines "8.5"]
    set lines [regsub -all {@MACHINE@} $lines "x86"]
    puts $fd2 $lines
    close $fd2
}

if {![file exists [file join $dir include conf DATE]]} {
    if {![catch {open [file join $dir include conf DATE] w} fd]} {
	puts "Creating [file join $dir include conf DATE]"
	puts $fd \"[clock format [clock seconds] -format "%B %d, %Y"]\"
	close $fd
    } else {
	puts "Unable to create [file join $dir include conf DATE]\n$fd"
    }
}

if {![file exists [file join $dir brlcadInstall share]]} {
    file mkdir [file join $dir brlcadInstall share]
    file mkdir [file join $dir brlcadInstall share brlcad]
    file mkdir [file join $dir brlcadInstall share brlcad 7.11.0]
    file copy [file join $dir doc html] [file join $dir brlcadInstall share brlcad 7.11.0]
}
