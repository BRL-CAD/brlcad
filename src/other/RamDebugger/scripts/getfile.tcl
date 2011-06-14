

# what: open or save
proc RamDebugger::GetFile { what types title }  {
    variable text
    variable options
    variable text_secondary
    variable mainframe
    variable getFileToolbar
    variable getFile_done
    variable getfileentry
    variable getfilestring
    variable getFile_dirs
    variable currentfile
    variable options

    #if { [info exists getFile_done] } { return }

    if { ![info exists options(getFile_askpassword)] } {
	set options(getFile_askpassword) 1
    }

    if { [info exists text_secondary] && [focus -lastfor $text] eq \
	$text_secondary } {
	set active_text $text_secondary
    } else { set active_text $text }

    set w [winfo toplevel $active_text]
    
    if { [info exists ::RamDebugger::getFileToolbar] } {
	$mainframe showtoolbar 2 1
	set f [$mainframe gettoolbar 2]
	eval destroy [winfo children $f]
    } else {
	$mainframe showtoolbar 2 1
	set f [$mainframe gettoolbar 2]
	#set f [$mainframe addtoolbar]
    }
    
    if { ![info exists getFile_dirs] } {
	lappend getFile_dirs ""
	if { $::tcl_platform(platform) == "windows" } {
	    package require registry
	    foreach winfolder {Personal Desktop} {
		set key {HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders}
		set err [catch { registry get $key $winfolder } ret]
		if { $err } { continue }
		lappend getFile_dirs [file normalize $ret]/
	    }
	} else {
	    lappend getFile_dirs [file normalize ~]/
	}
    }

    ttk::label $f.l1 -text [_ "File:"]
    ttk::combobox $f.e1 -textvariable ::RamDebugger::getfilestring \
	-values $getFile_dirs
    set getfileentry $f.e1

    ttk::button $f.b1 -image fileopen16 -style Toolbutton \
	-command [list RamDebugger::GetFile_Browser $w $what $types $title]
    tooltip::tooltip $f.b1 [_ "Select file"]
    
    grid $f.l1 $f.e1 $f.b1 -sticky w -padx 2
    grid $f.e1 -sticky ew
    grid columnconfigure $f 1 -weight 1

    bind $f.e1 <Tab> "[list RamDebugger::GetFile_Complete $f.e1] ; break"
    bind $f <Destroy> "[list unset ::RamDebugger::getFileToolbar]"

    bind $f.e1 <<ComboboxSelected>> [list set RamDebugger::getFile_done 1]
    bind $f.e1 <$::control-o> "[list $f.b1 invoke] ; break"
    bind $f.e1 <Return> "[list set RamDebugger::getFile_done 1] ; break"
    bind $f.e1 <Escape> [list set RamDebugger::getFile_done 0]
    bind $f.e1 <Alt-BackSpace> "[list RamDebugger::GetFile_del_backwards] ; break"
    bind $f.e1 <$::control-BackSpace> "[list RamDebugger::GetFile_del_backwards] ; break"
    bind $f.e1 <Home> [list RamDebugger::GetFile_Home home]
    bind $f.e1 <Shift-Home> [list RamDebugger::GetFile_Home desktop]
    bind $f <1> {
	if { %Y < [winfo rooty %W] || %Y > \
	    [expr {[winfo rooty %W]+[winfo height %W]}] } {
	    set RamDebugger::getFile_done 0
	    break
	}
	if { %X < [winfo rootx %W] } {
	    RamDebugger::GetFile_contextual [winfo parent %W] "" %W %X %Y
	}
    }
    bind $f.l1 <1> [list RamDebugger::GetFile_contextual $f $f.e1 $f.b1 %X %Y]
    bind $f.l1 <<Contextual>> [list RamDebugger::GetFile_contextual $f $f.e1 $f.b1 %X %Y]

    bind $f.e1 <<Contextual>> [list RamDebugger::GetFile_contextual $f $f.e1 $f.b1 %X %Y]
    #bind $f.e1.e <<Contextual>> [list RamDebugger::GetFile_contextual $f $f.e1 $f.b1 %X %Y]

    bind $f.e1 <Control-Tab> "[list set RamDebugger::getFile_done 0] ; [bind $text <Control-KeyPress-Tab>]"

    tooltip::tooltip $f.e1 "\
	Enter the filename path. Remote paths are also allowed. Examples:\n\
	\tplink://user@host/filename"


    if { [file isdirectory [file dirname $currentfile]] && \
	[string index $currentfile 0] ne "*" } {
	set getfilestring [file normalize [file dirname $currentfile]]/
    } else {
	if { ![info exists options(defaultdir)] } { set options(defaultdir) [pwd] }
	set getfilestring $options(defaultdir)/
    }
    update idletasks
    grab $f
    focus $f.e1

    $f.e1 xview moveto 1
    $f.e1 icursor end

    set getFileToolbar 1

    while 1 {
	set getFile_done 0
	vwait RamDebugger::getFile_done
	if { !$getFile_done } { break }
	switch $what {
	    save {
		if { ![file isdirectory $getfilestring] && \
		    [file isdirectory [file dirname $getfilestring]] } {

		    if { [file exists $getfilestring] } {
		        set ret [DialogWin::messageBox -default ok -icon question -message \
		                [_ "Are you sure to overwrite file '%s'?" $getfilestring] \
		                -title [_ "Warning"] -type okcancel]
		        if { $ret == "cancel" } { continue }
		    }
		    break
		}
	    }
	    open {
		GetFile_Complete $f.e1
		if { [file isfile $getfilestring] } { break }
	    }
	}
	focus $f.e1
	$f.e1 xview moveto 1
	$f.e1 icursor end
    }
    GetFile_close $f
    if { $getFile_done } {
	unset getFile_done

	set dir [file normalize [file dirname $getfilestring]]/
	if { [set ipos [lsearch -exact $getFile_dirs $dir]] != -1 } {
	    set getFile_dirs [lreplace $getFile_dirs $ipos $ipos]
	}
	set getFile_dirs [linsert $getFile_dirs 0 $dir]

	return $getfilestring
    } else {
	unset getFile_done
	return ""
    }
}

proc RamDebugger::GetFile_Browser { w what types title} {
    variable getfilestring
    variable options
    variable getFile_done

    if { [file isdirectory $getfilestring] } {
	set dir $getfilestring
    } elseif { [file isdirectory [file dirname $getfilestring]] } {
	set dir [file dirname $getfilestring]
    } else {
	if { ![info exists options(defaultdir)] } { set options(defaultdir) [pwd] }
	set dir $options(defaultdir)
    }
    switch $what {
	open {
	    set file [tk_getOpenFile -filetypes $types -initialdir $dir -parent $w \
		    -title $title]
	}
	save {
	    set file [tk_getSaveFile -filetypes $types -initialdir $dir -parent $w \
		    -title $title]
	}
    }
    if { $file ne "" } {
	set getfilestring $file
	set getFile_done 1
    }
}

proc RamDebugger::GetFile_contextual { f combo button x y } {

    catch { destroy $f.menu }
    set menu [menu $f.menu]
    $menu add command -label "Open browser" -accelerator Ctrl+o \
	-command [list $button invoke]
    $menu add command -label "Complete" -accelerator Tab \
	-command [list RamDebugger::GetFile_Complete $combo]
    $menu add command -label "Accept" -accelerator Enter \
	-command [list set RamDebugger::getFile_done 1]
    $menu add command -label "Back delete" -accelerator Alt-Backspace \
	-command [list RamDebugger::GetFile_del_backwards]
    $menu add separator
    $menu add checkbutton -label "Ask password" -variable \
	RamDebugger::options(getFile_askpassword)
    $menu add checkbutton -label "Use browser preference" -variable \
	RamDebugger::options(openfile_browser) -command \
	[list RamDebugger::GetFile_close $f]

    tk_popup $menu $x $y
}

proc RamDebugger::GetFile_Home { where } {
    variable getfilestring
    variable getfileentry
    
    switch $where {
	home {
	    set err [catch { set getfilestring [file normalize $::env(HOME)]/ }]
	}
	desktop {
	    set err [catch {
		    package require registry
		    set key {HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders}
		    set getfilestring [file normalize [registry get $key Desktop]]/
		}]
	}
    }
    $getfileentry icursor end
    if { !$err } { return -code break }
}

proc RamDebugger::GetFile_del_backwards {} {
    variable getfilestring

    set getfilestring [string trimright $getfilestring /]
    regexp {(.*/)[^/]*} $getfilestring {} getfilestring
}

proc  RamDebugger::_getfile_change_protocol { w } {

    switch [$w give_uservar_value protocol] {
	plink - ssh { set port 22 }
	rsh { set port 514 }
	rlogin { set port 513 }
	ftp { set port 21 }
	telnet { set port 23 }
	default { set port "" }
    }
    $w set_uservar_value port $port
}

proc RamDebugger::GetFile_CheckMounts { combo } {
    variable getfilestring
    variable options

    if { [regexp {^(/?(?:\w+)://[^/:]+):/(.*)$} $getfilestring {} pre post] } {
	set getfilestring $pre/$post
    } elseif { [regexp {^(/[^/:]+):(\d*)/(?!/)(.*)$} $getfilestring {} pre port post] } {
	switch -- $port {
	    "" { set getfilestring plink:/$pre/$post }
	    22 { set getfilestring plink:/$pre:$port/$post }
	    513 { set getfilestring rlogin:/$pre:$port/$post }
	    514 { set getfilestring rsh:/$pre:$port/$post }
	    23 { set getfilestring telnet:/$pre:$port/$post }
	    21 { set getfilestring ftp:/$pre:$port/$post }
	}
    }

    set rex {^(/?(\w+)://(?:([^@]+)@)?([^/:]+)(?::(\d+))?)/(.*)}
    if { [regexp $rex $getfilestring {} prefix protocol username host port dir] && \
	[lindex [file system $getfilestring] 0] eq "native" && \
	[lindex [file system /$getfilestring] 0] eq "native" } {

	if { $options(getFile_askpassword) || $username eq "" || \
	    [regexp {^ftp|telnet|rlogin$} $protocol] } {
	    set w [dialogwin_snit $combo._ask -title "Enter user name and password"]
	    set f [$w giveframe]
	    
	    label $f.l01 -text "Protocol:"
	    ComboBox $f.cb0 -textvariable [$w give_uservar protocol $protocol] \
		-values [list plink ssh ftp rsh rlogin telnet] -width 5
	    label $f.l02 -text "Port:"
	    entry $f.sp1 -textvariable [$w give_uservar port $port] -width 4
	    $w add_trace_to_uservar protocol \
		[list RamDebugger::_getfile_change_protocol $w]

	    label $f.l1 -text "User name:"
	    entry $f.e1 -textvariable [$w give_uservar username $username] \
		-width 40
	    label $f.l2 -text "Password:"
	    entry $f.e2 -textvariable [$w give_uservar password ""] -show *
	    
	    grid $f.l01 $f.cb0 $f.l02 $f.sp1 -sticky nw -pady 2 -padx 2
	    grid $f.l1 $f.e1 - - -sticky nw -pady 2 -padx 2
	    grid $f.l2 $f.e2 - - -sticky nw -pady 2 -padx 2
	    grid configure $f.e1 $f.e2 $f.cb0 -sticky new
	    grid columnconfigure $f 1 -weight 1
	    
	    bind $w <Return> [list $w invokeok]
	    if { $username eq "" } {
		tkTabToWindow $f.e1
	    } else {
		tkTabToWindow $f.e2
	    }
	    
	    set action [$w createwindow]
	    foreach i [list username password protocol port] {
		set $i [$w give_uservar_value $i]
	    }
	    destroy $w
	    if { $action <= 0 } { return }
	} else {
	    set password ""
	}
	if { $port eq "" } {
	    set prefix $protocol://$username@$host
	} else {
	    set prefix $protocol://$username@$host:$port
	}
	set getfilestring $prefix/$dir
	switch $protocol {
	    plink - ssh - telnet - rlogin - rsh {
		if { $username ne "" } {
		    ::vfs::template::fish::Mount -volume . -user $username \
		        -password $password $prefix
		} else {
		    ::vfs::template::fish::Mount -volume . \
		        -password $password $prefix
		}
	    }
	    ftp {
		set prefix /$prefix
		package require vfs::ftp
		if { $port eq "" } { set port 21 }
		vfs::ftp::Mount $username:$password@$host:$port \
		    $prefix
	    }
	}
    }
}

proc RamDebugger::GetFile_Complete { combo } {
    variable getfilestring
    variable getFile_dirs

    if { [regexp {^.+/.*[^:]/([/~].*)} $getfilestring {} rest] } {
	set getfilestring $rest
    }

    GetFile_CheckMounts $combo

    if { $getfilestring eq "" } {
	$combo configure -values $getFile_dirs    
	return
    }
    if { [regexp {^ftp://} $getfilestring] } {
	set file /$getfilestring
    } else {
	set file $getfilestring
    }

    if { [string match */ $file] && [file isdirectory $file] } {
	set getfilestring [string trimright $getfilestring /]
	set dir [string trimright $file /]
	set file ""
    } elseif { [file isdirectory [file dirname $file]] } {
	set dir [file dirname $file]
	set file [file tail $file]
    } else {
	$combo configure -values $getFile_dirs
	bell
	return
    }
    set files [glob -nocomplain -dir $dir $file*]
    if { ![llength $files] } {
	bell
    } elseif { [llength $files] == 1 } {
	set getfilestring [file normalize [lindex $files 0]]
	if { [file isdirectory $getfilestring] } {
	    append getfilestring /
	}
	$combo configure -values [concat [list $getfilestring ""] $getFile_dirs]
    } else {
	if { $file eq "" } { append getfilestring / }
	set f1 [file tail [lindex $files 0]]
	for { set i [string length $file] } { $i < [string length $f1] } { incr i } {
	    set isequal 1
	    foreach j [lrange $files 1 end] {
		set f2 [file tail $j]
		if { [string index $f1 $i] ne [string index $f2 $i] } {
		    set isequal 0
		    break
		}
	    }
	    if { $isequal } {
		append getfilestring [string index $f1 $i]
	    } else { break }
	}
	set values ""
	foreach i $files {
	    if { [file isdirectory $i] } {
		lappend values [file normalize $i]/
	    } else { lappend values [file normalize $i] }
	}
	eval lappend values $getFile_dirs
	$combo configure -values $values
    }
    $combo xview moveto 1
    $combo icursor end
    $combo selection clear
}

proc RamDebugger::GetFile_close { f } {
    variable getFile_done
    variable mainframe
    variable text
    variable text_secondary

    #if { ![info exists getFile_done] } { return }

    grab release $f
    eval destroy [winfo children $f]
    $mainframe showtoolbar 2 0

    if { [info exists text_secondary] && [focus -lastfor $text] eq \
	$text_secondary } {
	focus $text_secondary
    } else {focus $text }
}






















