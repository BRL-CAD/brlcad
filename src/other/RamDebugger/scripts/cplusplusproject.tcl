
namespace eval cproject {
    variable project ""
    variable group All
    variable groupbefore ""
    variable groups All
    variable links
    variable scripttabs
    variable debugreleasebefore ""
    variable debugrelease debug
    variable files ""

    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataE
    variable dataE

    variable compilationstatus
}

################################################################################
#   project creation/edition
################################################################################

proc cproject::Init { w } {
    variable project

    trace var ::cproject::group w "cproject::SetGroupActive;#"
    trace var ::cproject::debugrelease w "cproject::SetDebugReleaseActive;#"

    if { [info exists RamDebugger::options(recentprojects)] && \
	    [llength $RamDebugger::options(recentprojects)] > 0 } {
	set project [lindex $RamDebugger::options(recentprojects) 0]
	set err [catch { OpenProject $w 0 1 } errstring]
	if { $err } {
	    set project ""
	    set RamDebugger::options(recentprojects) [lreplace \
		    $RamDebugger::options(recentprojects) 0 0]
	}
    }
}

proc cproject::synctoUI {} {
    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataE
    variable dataE
    variable thisdataS
    variable dataS
    variable group
    variable debugrelease

    foreach i [array names dataC $group,$debugrelease,*] {
	regexp {^[^,]+,[^,]+,(.*)} $i {} prop
	set thisdataC($prop) $dataC($i)
    }
    foreach i [array names dataM $debugrelease,*] {
	regexp {^[^,]+,([^,]+)} $i {} prop
	set thisdataM($prop) $dataM($i)
    }
    foreach i [array names dataL $debugrelease,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	set thisdataL($prop) $dataL($i)
    }
    foreach i [array names dataS $debugrelease,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	set thisdataS($prop) $dataS($i)
    }
    foreach i [array names dataE $debugrelease,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	set thisdataE($prop) $dataE($i)
    }
}

proc cproject::syncfromUI {} {
    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataS
    variable dataS
    variable thisdataE
    variable dataE
    variable groupbefore
    variable debugreleasebefore

    if { $debugreleasebefore == "" } { return }

    foreach i [array names dataC $groupbefore,$debugreleasebefore,*] {
	regexp {^[^,]+,[^,]+,(.*)} $i {} prop

	if { $groupbefore == "All" || $debugreleasebefore == "both" } {
	    TransferDataToLowerGroups $groupbefore $debugreleasebefore $prop $dataC($i) \
		    $thisdataC($prop) dataC
	}
	set dataC($i) $thisdataC($prop)
    }
    foreach i [array names dataM $debugreleasebefore,*] {
	regexp {^[^,]+,([^,]+)} $i {} prop

	if { $debugreleasebefore == "both" } {
	    TransferDataToLowerGroups "" $debugreleasebefore $prop $dataM($i) \
		    $thisdataM($prop) dataM
	}
	set dataM($i) $thisdataM($prop)
    }
    foreach i [array names dataL $debugreleasebefore,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	if { $debugreleasebefore == "both" } {
	    TransferDataToLowerGroups "" $debugreleasebefore $prop $dataL($i) $thisdataL($prop) \
		dataL
	}
	set dataL($i) $thisdataL($prop)
    }
    foreach i [array names dataS $debugreleasebefore,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	if { ![info exists thisdataS($prop)] } { continue }
	if { $debugreleasebefore == "both" } {
	    TransferDataToLowerGroups "" $debugreleasebefore $prop $dataS($i) $thisdataS($prop) \
		dataS
	}
	set dataS($i) $thisdataS($prop)
    }
    foreach i [array names dataE $debugreleasebefore,*] {
	regexp {^[^,]+,(.*)} $i {} prop
	if { $debugreleasebefore == "both" } {
	    TransferDataToLowerGroups "" $debugreleasebefore $prop $dataE($i) $thisdataE($prop) \
		dataE
	}
	set dataE($i) $thisdataE($prop)
    }
}

proc cproject::TransferDataToLowerGroups { gr dr prop olddata newdata dataname } {
    variable dataC
    variable dataM
    variable dataL
    variable dataS
    variable dataE

    upvar 0 $dataname data

    set toadd ""
    set todel ""

    if { ![string match *dirs $prop] } {
	set olddataL [regexp -inline -all {[^\s;]+} $olddata]
	set newdataL [regexp -inline -all {[^\s;]+} $newdata]
	set searchcmd lsearch
    } else {
	set olddataL $olddata
	set newdataL $newdata
	set searchcmd RamDebugger::lsearchfile
    }

    foreach i $newdataL {
	if { [$searchcmd $olddataL $i] == -1 } { lappend toadd $i }
    }
    foreach i $olddataL {
	if { [$searchcmd $newdataL $i] == -1 } { lappend todel $i }
    }
    foreach i [array names data *,$prop] {
	switch $dataname {
	    dataC { regexp {^([^,]+),([^,]+),(.*)} $i {} gr_in dr_in prop_in }
	    dataL - dataS - dataE - dataM {
		set gr_in ""
		regexp {^([^,]+),(.*)} $i {} dr_in prop_in
	    }
	}
	if { $gr_in == $gr && $dr_in == $dr } { continue }

	if { $gr == "All" && $dr == "both" } {
	    # nothing
	} elseif { $gr == "All" } {
	    if { $dr != $dr_in } { continue }
	} elseif { $dr == "both" } {
	    if { $gr != $gr_in } { continue }
	} else { continue }

	if { $dataname == "dataS" } {
	    regsub -all {(?n)^\s*\#.*$} $data($i) "" res
	    if { [string trim $res] == "" } {
		set data($i) $newdata
	    }
	    continue
	}

	if { ![string match *dirs $prop] } {
	    set dataLocal [regexp -inline -all {[^\s;]+} $data($i)]
	} else {
	    set dataLocal $data($i)
	}
	foreach j $toadd {
	    if { [$searchcmd $dataLocal $j] == -1 } {
		if { ![string match *dirs $prop] } {
		    append data($i) " $j"
		} else {
		    lappend data($i) $j
		}
	    }
	}
	foreach j $todel {
	    if { [$searchcmd $dataLocal $j] != -1 } {
		if { ![string match *dirs $prop] } {
		    set ipos [$searchcmd $dataLocal $j]
		    set data($i) [join [lreplace $dataLocal $ipos $ipos]]
		} else {
		    set ipos [$searchcmd $dataLocal $j]
		    set data($i) [lreplace $data($i) $ipos $ipos]
		}
	    }
	}
	if { ![string match *dirs $prop] } {
	    set data($i) [string trim $data($i)]
	}
    }
}

proc cproject::SaveProjectC { w } {
    variable project
    variable group
    variable groups
    variable links
    variable scripttabs
    variable debugrelease
    variable files
    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataS
    variable dataS
    variable thisdataE
    variable dataE

    if { $project == "" } { return }

    syncfromUI

    set err [catch { open $project w } fout]
    if { $err } {
	WarnWin "Could not open file '$project' to save ($fout)" $w
	return
    }
    foreach i [list groups group links scripttabs debugrelease files thisdataC dataC \
	    thisdataM dataM thisdataL \
	    dataL thisdataS dataS thisdataE dataE ] {
	if { [array exists $i] } {
	    puts $fout [list array set $i [array get $i]]
	} else {
	    puts $fout [list set $i [set $i]]
	}
    }
    close $fout

    if { [RamDebugger::lsearchfile $RamDebugger::options(recentprojects) $project] != -1 } {
	set ipos [RamDebugger::lsearchfile $RamDebugger::options(recentprojects) $project]
	set RamDebugger::options(recentprojects) [lreplace $RamDebugger::options(recentprojects) \
	    $ipos $ipos]
    }
    set RamDebugger::options(recentprojects) [linsert $RamDebugger::options(recentprojects) \
	    0 $project]
}


# proc cproject::UpdateComboValues { combo varname } {
#     if { ![winfo exists $combo] } { return }
#     $combo configure -values [set $varname]
# }

proc cproject::NewProject { w } {
    variable project
    variable groupbefore
    variable group
    variable groups
    variable links
    variable scripttabs
    variable debugrelease
    variable files
    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataE
    variable dataE


    set types {
	{{Project files}      {.prj}   }
	{{All Files}        *          }
    }
    set dir $RamDebugger::options(defaultdir)

    set file [tk_getSaveFile -filetypes $types -initialdir $dir -parent $w \
	-title "New project" -defaultextension .prj]
    if { $file == "" } { return }

    set RamDebugger::options(defaultdir) [file dirname $file]

    set project $file
    set debugreleasebefore ""
    set groups All
    set links Link
    set scripttabs Script
    set files ""
    fill_files_list $w
    NewData
    synctoUI
    
    set debugrelease debug
    set group All
}

proc cproject::NewData {} {
    variable project
    variable thisdataC
    variable dataC
    variable thisdataM
    variable dataM
    variable thisdataL
    variable dataL
    variable thisdataS
    variable dataS
    variable thisdataE
    variable dataE
    variable groups
    variable links
    variable scripttabs

    foreach i [list C L S E] {
	catch { unset data$i }
	catch { unset thisdata$i }
    }
    foreach i [list debug release both] {
	foreach group $groups {
	    set dataC($group,$i,includedirs) .
	    set dataC($group,$i,defines) ""
	    set dataC($group,$i,compiler) "gcc"
	    switch $i {
		debug {
		    set dataC($group,$i,flags) "-c -g"
		}
		release {
		    set dataC($group,$i,flags) "-c -O3"
		}
		both {
		    set dataC($group,$i,flags) "-c"
		}
	    }
	}

	set dataM($i,has_userdefined_makefile) 0
	set dataM($i,makefile_file) Makefile
	set dataM($i,makefile_arguments) ""
	
	foreach link $links {
	    set dataL($i,$link,librariesdirs) .
	    set dataL($i,$link,linkgroups) All
	    set dataL($i,$link,libraries) "libc libm"
	    set dataL($i,$link,linker) "gcc"
	    set dataL($i,$link,linkflags) ""
	    set dataL($i,$link,linkexe) [file root [file tail $project]]
	    
	    if { $i != "both" } {
		set dataE($i,execdir) [file join [file dirname $project] [file root $project]_$i]
		set dataE($i,exe) [file root [file tail $project]]
	    } else {
		set dataE($i,execdir) ""
		set dataE($i,exe) ""
		
	    }
	    set dataE($i,exeargs) ""
	}
	foreach scripttab $scripttabs {
	    set dataS($i,$scripttab,script) help
	    set dataS($i,$scripttab,executetime) "No automatic"
	}
    }
}

proc cproject::OpenProject { w { ask 1 } { raise_error 0 } } {
    variable project
    variable groupbefore
    variable group
    variable groups
    variable links
    variable scripttabs
    variable debugrelease
    variable files
    variable dataC
    variable dataM
    variable dataL
    variable dataE
    variable debugreleasebefore

    if { $ask } {
	set types {
	    {{Project files}      {.prj}   }
	    {{All Files}        *          }
	}        
	set dir $RamDebugger::options(defaultdir)
	
	set file [tk_getOpenFile -filetypes $types -initialdir $dir -parent $w \
	    -title [_ "Open existing project"] -defaultextension .prj]
	if { $file == "" } { return }
    } else {
	set file $project
    }
    set project $file
    if { $file ne "" } {
	set RamDebugger::options(defaultdir) [file dirname $file]
    }
    set debugreleasebefore ""
    set groups All
    set links Link
    set scripttabs Script
    set files ""

    NewData
    synctoUI
    
    if { $file eq "" } { return }

    trace vdelete ::cproject::group w "cproject::SetGroupActive;#"
    trace vdelete ::cproject::debugrelease w "cproject::SetDebugReleaseActive;#"
    trace vdelete ::cproject::links w "UpdateLinktabs ;#"
    trace vdelete ::cproject::scripttabs w "UpdateScripttabs ;#"    
    
    set err [catch {
	if { [interp exists cproject_tmp] } { interp delete cproject_tmp }
	interp create cproject_tmp
	cproject_tmp eval [list source $file]
	set groups [cproject_tmp eval set groups]
	if { [catch { set links [cproject_tmp eval set links] }]} {
	    set links Link
	}
	if { [catch { set scripttabs [cproject_tmp eval set scripttabs] }]} {
	    set scripttabs Script
	}
	interp delete cproject_tmp
	# trick because NewData needs to have groups and links defined
	NewData
	source $file
    } errstring]
    
    if { $err } {
	set txt "Error opening project '$file' ($errstring)"
	if { $raise_error } {
	    error $txt
	}
	WarnWin $txt $w
	return
    }
    if { [array exists data] } {
	# upgrade old versions
	set links Link
	foreach i [array names data] {
	    if { [regexp {(librariesdirs|libraries|linkflags)$} $i] } {
		regexp {^([^,]+),(.*)} $i {} dr r 
		set dataL($dr,Link,$r) $data($i)
	    } elseif { [regexp {(execdir|exe|exeargs)$} $i] } {
		set dataE($i) $data($i)
	    } else { set dataC($i) $data($i) }
	}
    }
    
   # fill_files_list $w
    
    # to activate the trace
    set groupbefore ""
    set debugreleasebefore ""

    trace var ::cproject::group w "cproject::SetGroupActive;#"
    trace var ::cproject::debugrelease w "cproject::SetDebugReleaseActive;#"
    trace var ::cproject::links w "UpdateLinktabs ;#"
    trace var ::cproject::scripttabs w "UpdateScripttabs ;#"
   
    set links $links
    set scripttabs $scripttabs

    set group $group

    set groupbefore $group
    set debugreleasebefore $debugrelease

    if { [RamDebugger::lsearchfile $RamDebugger::options(recentprojects) $project] != -1 } {
	set ipos [RamDebugger::lsearchfile $RamDebugger::options(recentprojects) $project]
	set RamDebugger::options(recentprojects) [lreplace $RamDebugger::options(recentprojects) \
		$ipos $ipos]
    }
    set RamDebugger::options(recentprojects) [linsert $RamDebugger::options(recentprojects) \
	    0 $project]
}

proc cproject::SaveProject { w } {
    variable project
    set types {
	{{Project files}      {.prj}   }
	{{All Files}        *          }
    }
    set dir $RamDebugger::options(defaultdir)

    set file [tk_getSaveFile -filetypes $types -initialdir $dir -parent $w \
	-title "Save project" -defaultextension .prj]
    if { $file == "" } { return }

    #set RamDebugger::options(defaultdir) [file dirname $file]
    
    set project $file
    SaveProjectC $w
}

proc cproject::SetGroupActive {} {
    variable groupbefore
    variable group
    variable debugrelease
    variable thisdata
    variable data

    set dir [IsProjectNameOk]

    syncfromUI

    set groupbefore $group
    synctoUI
}

proc cproject::SetDebugReleaseActive {} {
    variable debugreleasebefore
    variable debugrelease
    variable thisdata
    variable data

    set dir [IsProjectNameOk]
    syncfromUI
    set debugreleasebefore $debugrelease
    synctoUI
}

proc cproject::CreateModifyGroup { w what } {
    variable group
    variable groups
    variable files
    variable dataC
    variable groupbefore

    set dir [IsProjectNameOk]
    syncfromUI

    if { $what == "delete" } {
	if { $group == "All" } {
	    WarnWin [_ "Group 'All' cannot be deleted"] $w
	    return
	}
	set text [_ "Are you sure to delete group '%s'?" $group]
	set retval [snit_messageBox -default ok -icon question -message $text \
		-type okcancel -parent $w -title [_ "delete group"]]
	if { $retval eq "cancel" } { return }
	
	for { set i 0 } { $i < [llength $files] } { incr i } {
	    foreach "file type group_in path" [lindex $files $i] break
	    if { $group == $group_in } {
		set files [lreplace $files $i $i [list $file $type All $path]]
	    }
	}
	fill_files_list $w
	set ipos [lsearch $groups $group]
	set groups [lreplace $groups $ipos $ipos]
	foreach i [array names dataC $group,*] {
	    unset dataC($i)
	}
	set groupbefore ""
	set group All
	return
    }
    switch $what {
	create {
	    set title [_ "New group"]
	    set label [_ "Enter new group name"]
	}
	rename {
	    set title [_ "Rename group"]
	    set label [_ "Enter new name for group '%s'" $group]
	}
    }
    
    set wd [dialogwin_snit $w._ask -title $title -entrylabel $label: -entryvalue $group]
    set action [$wd createwindow]
    while 1 {
	if { $action <= 0 } { 
	    destroy $wd
	    return
	}
	set name [string trim [$wd giveentryvalue]]
	
	if { $name eq "" } {
	    snit_messageBox -message [_ "Group name cannot be void"] -parent $w
	} elseif { [lsearch -exact $groups $name] != -1 } {
	    snit_messageBox -message [_ "Group name already exists"] -parent $w
	} elseif { ![string is wordchar $name]  } {
	    snit_messageBox -message [_ "Group name is not OK"] -parent $w
	} else {
	    break
	}
	set action [$wd waitforwindow]
    }
    destroy $wd
    if { $action <= 0 } {  return }

    if { $what eq "rename" } {
	for { set i 0 } { $i < [llength $files] } { incr i } {
	    lassign [lindex $files $i] file type group_in path
	    if { $group eq $group_in } {
		set files [lreplace $files $i $i [list $file $type $name $path]]
	    }
	}
	fill_files_list $w
	set ipos [lsearch $groups $group]
	set groups [lreplace $groups $ipos $ipos $name]
	foreach i [array names dataC $group,*] {
	    regexp {,(.*)} $i {} rest
	    set dataC($name,$rest) $dataC($i)
	    unset dataC($i)
	}
    } else {
	lappend groups $name
	foreach i [array names dataC All,*] {
	    regexp {,(.*)} $i {} rest
	    set dataC($name,$rest) $dataC($i)
	}
    }
    set groupbefore ""
    set group $name
}

proc cproject::CreateDo { w } {
    
    if { [$w giveaction] < 1 } {
	destroy $w
	return
    }
    switch  [$w giveaction]  {
	1 {
	    SaveProjectC $w
	    destroy $w
	}
	2 {
	    SaveProjectC $w
	}
    }
}

proc cproject::Create { par } {
    variable notebook

    if { ![info exists RamDebugger::options(recentprojects)] } {
	set RamDebugger::options(recentprojects) ""
    }
    destroy $par.mpt
    set w [dialogwin_snit $par.mpt -title [_ "C++ compilation project"] -grab 0 \
	    -morebuttons [list [_ "Apply"]]  -callback [list cproject::CreateDo]]
    set f [$w giveframe]

    set f1 [ttk::frame $f.f1]
    
    set projects $RamDebugger::options(recentprojects) 
    lappend projects ""
    ttk::label $f1.l1 -text [_ "Project"]:
    ttk::combobox $f1.cb1  -textvariable cproject::project -width 80 -state readonly \
	-values $projects
    bind $f1.cb1 <<ComboboxSelected>> [list cproject::OpenProject $w 0]
    tooltip::tooltip $f1.cb1 [_ "A project includes all the compilation information. Create a project before entering data"]

    ttk::button $f1.b1 -image filenew16 -command [list cproject::NewProject $w] -style Toolbutton
    tooltip::tooltip $f1.b1 [_ "Create new project"]
    
    ttk::button $f1.b2 -image fileopen16 -command [list cproject::OpenProject $w] -style Toolbutton
    tooltip::tooltip $f1.b2 [_ "Open existing project"]
 
    ttk::button $f1.b3 -image filesave16 -command [list cproject::SaveProject $w] -style Toolbutton
    tooltip::tooltip $f1.b3 [_ "Save as project"]

    ttk::label $f1.l2 -text [_ "Group"]:
    cu::combobox $f1.cb2  -textvariable cproject::group -state readonly \
	-valuesvariable cproject::groups
    tooltip::tooltip $f1.cb2 [_ "A group is a set of files with common compilation options. The special group 'all' always exists and affects all files"]

    ttk::button $f1.b4 -image acttick16 -command [list cproject::CreateModifyGroup $w create] -style Toolbutton
    tooltip::tooltip $f1.b4 [_ "Create new group"]

    ttk::button $f1.b5 -image edit16 -command [list cproject::CreateModifyGroup $w rename] -style Toolbutton
    tooltip::tooltip $f1.b5 [_ "Rename group"]

    ttk::button $f1.b6 -image actcross16 -command [list cproject::CreateModifyGroup $w delete] -style Toolbutton
    tooltip::tooltip $f1.b6 [_ "Delete group"]

    set f11 [ttk::frame $f1.f1]
    
    ttk::radiobutton $f11.r1 -text [_ "Debug"] -variable cproject::debugrelease -value debug
    ttk::radiobutton $f11.r2 -text [_ "Release"] -variable cproject::debugrelease -value release
    ttk::radiobutton $f11.r3 -text [_ "Both"] -variable cproject::debugrelease -value both
    
    grid $f11.r1 $f11.r2 $f11.r3 -sticky w -padx 2
   
    set pw [panedwindow $f1.pw -orient horizontal]

    lassign [RamDebugger::ManagePanes $pw h "2 3"] weight1 weight2

    set pane1 [ttk::frame $pw.pane1]
    $pw add $pane1 -sticky nsew -width $weight1
    
    set columns [list \
	    [list 14 [_ "File"] left text 1] \
	    [list 5 [_ "Type"] center text 1] \
	    [list 11 [_ "Group"] left text 1] \
	    [list 15 [_ "Path"] left text 1] \
	    ]

    package require fulltktree
    fulltktree $pane1.list  \
	-columns $columns  \
	-contextualhandler [list cproject::contextual_files_list $w] \
	-selectmode extended -showlines 0 -indent 0
    $w set_uservar_value list $pane1.list
    
    set idx 1
    foreach "img cmd help" [list \
	    fileopen16 [list cproject::AddModFiles $w file] [_ "Add file to project"] \
	    folderopen16 [list cproject::AddModFiles $w dir] [_ "Add files from directory to project"] \
	    edit16 [list cproject::AddModFiles $w edit] [_ "Assign selected files to active group"] \
	    actcross16 [list cproject::AddModFiles $w delete] [_ "Delete files from project"] \
	    acttick16 [list cproject::AddModFiles $w view] [_ "View file"] \
	    ] {
	ttk::button $pane1.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $pane1.b$idx $help
	incr idx
    }
    
    grid $pane1.list  - - - - -sticky nsew -padx 2 -pady 2
    grid $pane1.b1 $pane1.b2 $pane1.b3 $pane1.b4 $pane1.b5 -sticky w
    grid columnconfigure $pane1 4 -weight 1
    grid rowconfigure $pane1 0 -weight 1

    set pane2 [ttk::frame $pw.pane2]
    $pw add $pane2 -sticky nsew -width $weight2

    set nb [ttk::notebook $pane2.nb]
    set notebook $nb
    
    set nf1 [ttk::frame $nb.f1]
    $nb add $nf1 -text [_ Compilation] -sticky nsew

    set nf11 [ttk::labelframe $nf1.f1 -text [_  "include directories"]]
    listbox $nf11.lb -listvariable cproject::thisdataC(includedirs) -yscrollcommand [list $nf11.sb set]
    ttk::scrollbar $nf11.sb -orient vertical -command [list $nf11.lb yview]

    set idx 1
    foreach "img cmd help" [list \
	    folderopen16 [list cproject::AddDelDirectories $nf11.lb add] [_ "Add include directory"] \
	    actcross16 [list cproject::AddDelDirectories $nf11.lb delete] [_ "Delete include directory"] \
	    ] {
	ttk::button $nf11.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $nf11.b$idx $help
	incr idx
    }

    grid $nf11.lb - - $nf11.sb -sticky nsew -padx 2 -pady 2
    grid $nf11.b1 $nf11.b2 -sticky w -padx 1
    
    grid rowconfigure $nf11 0 -weight 1
    grid columnconfigure $nf11 2 -weight 1
    
    set nf12 [ttk::labelframe $nf1.f2 -text [_  "compiler"]]

    set values [list "" gcc g++]
    ttk::combobox $nf12.cb1 -textvariable cproject::thisdataC(compiler) -values $values \
	-width 10

    grid $nf12.cb1 -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $nf12 0 -weight 1
    
    set nf13 [ttk::labelframe $nf1.f3 -text [_  "defines"]]

    ttk::entry $nf13.e -textvariable cproject::thisdataC(defines)
    
    grid $nf13.e -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $nf13 0 -weight 1

    set nf14 [ttk::labelframe $nf1.f4 -text [_  "additional compile flags"]]

    ttk::entry $nf14.e -textvariable cproject::thisdataC(flags)
    
    grid $nf14.e -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $nf14 0 -weight 1
    
    grid $nf11 -sticky nsew -padx 2 -pady 2
    grid $nf12 -sticky nsew -padx 2 -pady 2
    grid $nf13 -sticky nsew -padx 2 -pady 2
    grid $nf14 -sticky nsew -padx 2 -pady 2
    grid columnconfigure $nf1 0 -weight 1
    
    set nf2 [ttk::frame $nb.f2]
    $nb add $nf2 -text [_ Makefile] -sticky nsew

    ttk::checkbutton $nf2.cb1 -text [_ "User defined Makefile" ] -variable \
	cproject::thisdataM(has_userdefined_makefile)
    
    set nf21 [ttk::labelframe $nf2.f1 -text [_  "makefile file"]]

    ttk::label $nf21.l1 -text [_ "Makefile"]:
    ttk::combobox $nf21.cb1 -textvariable cproject::thisdataM(makefile_file)
    ttk::button $nf21.b -image [Bitmap::get file]  -style Toolbutton -command [list cproject::select_makefile $w]
    ttk::label $nf21.l2 -text [_ "Makefile arguments"]:
    ttk::entry $nf21.e -textvariable cproject::thisdataM(makefile_arguments)

    grid $nf21.l1 $nf21.cb1 $nf21.b -sticky nsew -padx 2 -pady 2
    grid  $nf21.l2 $nf21.e -sticky nsew -padx 2 -pady 2
    grid columnconfigure $nf21 1 -weight 1

    grid $nf2.cb1 -sticky nsew -padx 2 -pady 2
    grid  $nf2.f1 -sticky nsew -padx 2 -pady 2
    grid columnconfigure $nf2 0 -weight 1
    
    foreach "n v" [list has_userdefined_makefile 0 makefile_file Makefile makefile_arguments ""] {
	if { ![info exists cproject::thisdataM($n)] } {
	    set cproject::thisdataM($n) $v
	}
    }
    set cmdMake [list cproject::update_active_inactive_makefile $w [list $nf21.l1 $nf21.cb1 $nf21.b $nf21.l2 $nf21.e]]
    trace add variable cproject::thisdataM(has_userdefined_makefile) write "$cmdMake;#"
    bind $nf2.cb1 <Delete> [list trace remove variable cproject::thisdataM(has_userdefined_makefile) \
	    write "$cmdMake;#"]
    
    set nf3 [ttk::frame $nb.f3]
    $nb add $nf3 -text [_ Execute] -sticky nsew
    
    set nf31 [ttk::labelframe $nf3.f1 -text [_  "executable file"]]

    ttk::entry $nf31.e -textvariable cproject::thisdataE(exe)
    ttk::button $nf31.b -image [Bitmap::get file]  -style Toolbutton -command \
	[list cproject::select_executable_file $w]
    
    tooltip::tooltip $nf31.e [_ "Name of the executable relative to project directory"]
    
    grid $nf31.e $nf31.b -sticky ew -padx 2 -pady 2
    grid columnconfigure $nf31 0 -weight 1

    set nf32 [ttk::labelframe $nf3.f2 -text [_  "working directory"]]

    ttk::entry $nf32.e -textvariable cproject::thisdataE(execdir)
    ttk::button $nf32.b -image [Bitmap::get file]  -style Toolbutton -command \
	[list cproject::select_executable_dir $w]
    
    grid $nf32.e $nf32.b -sticky ew -padx 2 -pady 2
    grid columnconfigure $nf32 0 -weight 1

    set nf33 [ttk::labelframe $nf3.f3 -text [_  "arguments"]]

    ttk::entry $nf33.e -textvariable cproject::thisdataE(exeargs)
    
    grid $nf33.e -sticky ew -padx "2 22" -pady 2
    grid columnconfigure $nf33 0 -weight 1

    grid $nf31 -sticky nsew -padx 2 -pady 2
    grid $nf32 -sticky nsew -padx 2 -pady 2
    grid $nf33 -sticky nsew -padx 2 -pady 2
    grid columnconfigure $nf3 0 -weight 1

    UpdateLinktabs
    # if it exists from before, it will be deleted
    trace vdelete ::cproject::links w "UpdateLinktabs ;#"
    trace var cproject::links w "UpdateLinktabs ;#"

    UpdateScripttabs
    # if it exists from before, it will be deleted
    trace vdelete ::cproject::scripttabs w "UpdateScripttabs ;#"
    trace var cproject::scripttabs w "UpdateScripttabs ;#"
    
    eval $cmdMake

    grid $f1.l1 $f1.cb1     -           -           -       -       $f1.b1 $f1.b2 $f1.b3 -sticky w
    grid $f1.l2 $f1.cb2 $f1.b4 $f1.b5 $f1.b6 $f11 -       -          -   -   -sticky w
    grid $f1.pw     -             -           -         -           -    -       -          -   - -sticky nsew
    grid configure $f11 -padx "40 2"
    grid columnconfigure $f1 4 -weight 1
    grid rowconfigure $f1 2 -weight 1
    
    grid $pane2.nb -sticky nsew
    grid columnconfigure $pane2 0 -weight 1
    grid rowconfigure $pane2 0 -weight 1
    
    grid $f1 -sticky nsew
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1

    tk::TabToWindow $f1.cb1
    bind $w <Return> [list $w invokeok]
    $w createwindow
}

proc cproject::select_executable_file { w } {
    variable thisdataE
    variable project
    
    set file [tk_getOpenFile -filetypes {{{All Files} *}} \
	    -initialdir $RamDebugger::options(defaultdir) -initialfile \
	    [file tail $thisdataE(exe)] -parent $w -title [_ "Executable file"]]
    if { $file eq "" } { return }
    set file [ConvertToRelative [file dirname $project] $file]
    set thisdataE(exe) $file
}

proc cproject::select_executable_dir { w } {
    variable thisdataE
    variable project

    if { [file isdirectory [file dirname $thisdataE(exe)]] } {
	set initial [file dirname $thisdataE(exe)]
    } else {
	set initial $RamDebugger::options(defaultdir)
    }
    set dir [tk_chooseDirectory -initialdir $initial -parent $w -title [_ "Working directory"] \
	    -mustexist 1]
    if { $dir eq "" } { return }
    set dir [ConvertToRelative [file dirname $project] $dir]
    set thisdataE(execdir) $file
}

proc cproject::update_active_inactive_makefile { w wList } {
    variable thisdataM
    variable notebook
    
    if { $thisdataM(has_userdefined_makefile) } {
	set state !disabled
	set nstate disabled
    } else {
	set state disabled
	set nstate normal
    }
    foreach i $wList {
	$i state $state
    }
    foreach i [$notebook tabs] {
	set txt [$notebook tab $i -text]
	if { [string match Link* $txt] || $txt eq [_ "Compilation"]  } {
	    $notebook tab $i -state $nstate
	}
    }

}

proc cproject::fill_files_list { w } {
    variable files
    
    set list [$w give_uservar_value list]
    
    $list item delete all
    foreach i $files {
	$list insert end $i
    }    
}

proc cproject::contextual_files_list { w - items x y } {
    
    catch { destroy $w.menu }
    set menu [menu $w.menu -tearoff 0]
    
    $menu add command -label [_ "Assign group"] -command [list cproject::AddModFiles $w edit]
    $menu add command -label [_  "View file"] -command [list cproject::AddModFiles $w view]
    $menu add separator
    $menu add command -label [_ "Delete from project"] -command [list cproject::AddModFiles $w delete]
    tk_popup $menu $x $y
}

proc cproject::UpdateLinktabs {} {
    variable notebook
    variable links

    if { ![info exists notebook] || ![winfo exists $notebook] } { return }
    if { ![info exists links] } { set links Link }

    set pages [$notebook tabs]

    foreach i $pages {
	set txt [$notebook tab $i -text]
	if { [string match Link* $txt] } {
	    destroy $i
	}
    }
    set ipos 1
    foreach i $links {
	regsub -all {\W} $i {X} page
	set f [ttk::frame $notebook.lflink$ipos]
	$notebook insert $ipos $f -text $i
	AddLinkTab $f $i
	incr ipos
    }
}

proc cproject::AddGroupInLinkGroups { but entry } {
    variable groups

    set menu $but.menu
    catch { destroy $menu }

    menu $menu
    foreach i $groups {
	set comm {
	    set str [ENTRY get]
	    append str " GROUP"
	    ENTRY del 0 end
	    ENTRY insert end [string trim $str]
	}
	set comm [string map [list ENTRY $entry GROUP $i] $comm]
	$menu add command -label $i -command $comm
    }
    tk_popup $menu [winfo rootx $but] [winfo rooty $but]
}

proc cproject::AddLinkTab { f link } {

    set f0 [ttk::labelframe $f.f0 -text [_ "link groups"]]
    ttk::entry $f0.e -textvariable cproject::thisdataL($link,linkgroups)
    ttk::label $f0.l -image acttick16
    tooltip::tooltip $f0.l [_ "Add group"]
    bind $f0.l <ButtonPress-1> [list cproject::AddGroupInLinkGroups $f0.l $f0.e]

    grid $f0.e $f0.l -sticky ew -padx 2 -pady 2
    grid columnconfigure $f0 0 -weight 1
    
    set f1 [ttk::labelframe $f.f1 -text [_  "libraries directories"]]
    listbox $f1.lb -listvariable cproject::thisdataL($link,librariesdirs) -yscrollcommand [list $f1.sb set]
    ttk::scrollbar $f1.sb -orient vertical -command [list $f1.lb yview]

    set idx 1
    foreach "img cmd help" [list \
	    folderopen16 [list cproject::AddDelDirectories $f1.lb add] [_ "Add link directories"] \
	    actcross16 [list cproject::AddDelDirectories $f1.lb delete] [_ "Delete link directories"] \
	    ] {
	ttk::button $f1.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $f1.b$idx $help
	incr idx
    }

    grid $f1.lb - - $f1.sb -sticky nsew -padx 2 -pady 2
    grid $f1.b1 $f1.b2 -sticky w -padx 1
    
    grid rowconfigure $f1 0 -weight 1
    grid columnconfigure $f1 2 -weight 1
    
    set f2 [ttk::labelframe $f.f2 -text [_  "libraries"]]

    set values [list "" gcc g++ ar]
    if { $::tcl_platform(platform) == "windows" } {
	lappend values windres
    }
    ttk::combobox $f2.cb1 -textvariable cproject::thisdataL($link,linker) -values $values \
	-width 7
    
    entry $f2.e -textvariable cproject::thisdataL($link,libraries)

    grid $f2.cb1 -sticky ew -padx "2 20" -pady 2
    grid $f2.e -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $f2 0 -weight 1
    
    set f3 [ttk::labelframe $f.f3 -text [_  "additional link flags"]]

    entry $f3.e -textvariable cproject::thisdataL($link,linkflags)

    grid $f3.e -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $f3 0 -weight 1

    set f4 [ttk::labelframe $f.f4 -text [_  "output name"]]

    entry $f4.e -textvariable cproject::thisdataL($link,linkexe)

    grid $f4.e -sticky ew -padx "2 20" -pady 2
    grid columnconfigure $f4 0 -weight 1

    set f5 [ttk::frame $f.f5]
    set idx 1
    foreach "img cmd help" [list \
	    acttick16 [list cproject::CreateDeleteLinkTab $link create] [_ "Create new link tab"] \
	    actcross16 [list cproject::CreateDeleteLinkTab $link delete] [_ "Delete link tab"] \
	    edit16 [list cproject::CreateDeleteLinkTab $link rename] [_ "Rename link tab"] \
	    ] {
	ttk::button $f5.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $f5.b$idx $help
	incr idx
    }
    grid $f5.b1 $f5.b2 $f5.b3 -sticky w -padx 2 -pady 2
    
    grid $f.f0 -sticky new
    grid $f.f1 -sticky nsew
    grid $f.f2 -sticky new
    grid $f.f3 -sticky new
    grid $f.f4 -sticky new
    grid $f.f5 -sticky nw

    grid rowconfigure $f 1 -weight 1
    grid columnconfigure $f 0 -weight 1
}

proc cproject::CreateDeleteLinkTab { currentlink what } {
    variable links
    variable dataL
    variable notebook

    syncfromUI

    switch $what {
	create {
	    set num [expr [llength $links]+1]
	    set newlink Link$num
	    foreach i [array names dataL *,$currentlink,*] {
		regexp {([^,]+),[^,]+,([^,]+)} $i {} dr v
		set dataL($dr,$newlink,$v) $dataL($i)
	    }
	    lappend links $newlink
	    $notebook select $newlink
	}
	delete {
	    if { [llength $links] == 1 } {
		WarnWin "Error: There must be at least one link tab" $notebook
		return
	    }
	    set ret [DialogWin::messageBox -default ok -icon warning -message \
		"Are you sure to delete link tab '$currentlink'?" -parent $notebook \
		-title "delete link tab" -type okcancel]
	    if { $ret == "cancel" } { return }
	    
	    set delpos [lsearch $links $currentlink]

	    foreach i [array names dataL *,$currentlink,*] {
		unset dataL($i)
	    }
	    set links [lreplace $links $delpos $delpos]
	    if { $delpos >= [llength $links] } { set delpos 0 }
	    $notebook select [lindex $links $delpos]
	}
	rename {
	    CopyNamespace ::DialogWin ::DialogWinCR
	    set f [DialogWinCR::Init $notebook "Enter link name" separator ""]
	    set w [winfo toplevel $f]
	    
	    label $f.l -text "Enter new name for link tab '$currentlink'" -grid "0 px3 py3"
	    entry $f.e -textvariable DialogWinCR::user(name) -grid "0 px10 py3" -width 30
	    
	    set DialogWinCR::user(name) $currentlink
	    tkTabToWindow $f.e
	    supergrid::go $f
	    bind $w <Return> "DialogWinCR::InvokeOK"

	    set action [DialogWinCR::CreateWindow]
	    while 1 {
		switch $action {
		    0 {
		        DialogWinCR::DestroyWindow
		        namespace delete ::DialogWinCR
		        return
		    }
		    1 {
		        set newlink [string trim $DialogWinCR::user(name)]
		        if { $newlink == "" } {
		            set newlink "Link"
		        } elseif { ![string match "Link *" $newlink] } {
		            set newlink "Link $newlink"
		        }
		        if { $newlink == "" } {
		            WarnWin "Link tab name cannot be void" $w
		        } elseif { [lsearch $links $newlink] != -1 } {
		            WarnWin "Link tab name '$newlink' already exists" $w
		        } else {
		            DialogWinCR::DestroyWindow
		            namespace delete ::DialogWinCR
		            break
		        }
		    }
		}
		set action [DialogWinCR::WaitForWindow]
	    }
	    set pos [lsearch $links $currentlink]
	    foreach i [array names dataL *,$currentlink,*] {
		regexp {([^,]+),[^,]+,([^,]+)} $i {} dr v
		set dataL($dr,$newlink,$v) $dataL($i)
		unset dataL($i)
	    }
	    set links [lreplace $links $pos $pos $newlink]
	}
    }
    synctoUI
}


proc cproject::UpdateScripttabs {} {
    variable notebook
    variable scripttabs

    if { ![info exists notebook] || ![winfo exists $notebook] } { return }
    if { ![info exists scripttabs] } { set scripttabs Script }

    set pages [$notebook tabs]

    foreach i $pages {
	set txt [$notebook tab $i -text]
	if { [string match Script* $txt] } {
	    destroy $i
	}
    }
    set ipos [expr {[llength [$notebook tabs]]-1}]
    foreach i $scripttabs {
	regsub -all {\W} $i {X} page
	set f [ttk::frame $notebook.lf$ipos]
	$notebook insert $ipos $f -text $i
	AddScripttab $f $i
	incr ipos
    }
}

proc cproject::ScriptTabColorize { txt command args } {
    
    if { [regexp {^(ins|del)} $command] } {
	RamDebugger::ColorizeSlow $txt
    }
}

proc cproject::AddScripttab { f scripttab } {
    variable links
    variable debugrelease

    set f0 [ttk::labelframe $f.f0 -text [_ "contents"]]

    set helptext ""
    append helptext "# It is possible to include a TCL script here\n"
    append helptext "# it will be executed either automatically or\n"
    append helptext "# manually, depending on configuration. In this\n"
    append helptext "# latter case, it is possible to use it as a NOTES\n"
    append helptext "# storage place\n\n"
    append helptext "# AVAILABLE VARIABLES\n"
    append helptext "# \$ProjectDir: the directory where the project is\n"
    append helptext "# \$ObjectsDir: the directory where the object files are\n"
    
    supertext::text $f0.text -wrap none -syncvar cproject::thisdataS($scripttab,script) \
	-height 4 -bg white -postproc [list cproject::ScriptTabColorize $f0.text] \
	-yscrollcommand [list $f0.sb set]
    ttk::scrollbar $f0.sb -orient vertical -command [list $f0.text yview]

    bind $f0.text <Return> "[bind Text <Return>] ;break"

    foreach i [array names cproject::dataS *,script] {
	if { $cproject::dataS($i) == "help" } {
	    set cproject::dataS($i) $helptext
	}
    }
    foreach i [array names cproject::dataS $debugrelease,*,script] {
	regexp {^[^,]+,(.*)} $i {} prop
	set cproject::thisdataS($prop) $cproject::dataS($i)
    }
    if { $cproject::thisdataS($scripttab,script) == "help" } {
	set cproject::thisdataS($scripttab,script) $helptext
    }
    
    set idx 1
    foreach "img cmd help" [list \
	    acttick16  "[list $f0.text del 1.0 end] ; [list $f0.text ins end $helptext]" [_ "Clear text and add help"] \
	    actcross16 [list $f0.text del 1.0 end] [_ "Clear text"] \
	    ] {
	ttk::button $f0.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $f0.b$idx $help
	incr idx
    }

    grid $f0.text - -sticky nsew -padx 2 -pady 2
    grid $f0.b1 $f0.b2 -sticky w -padx 2 -pady 2
    grid rowconfigure $f0 0 -weight 1
    grid columnconfigure $f0 1 -weight 1

    set f1 [ttk::labelframe $f.f1 -text [_ "execute when"]]

    set values [list "Before compile" "After compile"]
    set dict [dict create \
	    "Before compile" [_ "Before compile"] \
	    "After compile" [_ "After compile"] \
	    "No automatic" [_ "No automatic"] \
	    ]
    foreach i $links {
	lappend values "After $i"
	dict set dict "After $i" [_ "After %s" $i]
    }
    lappend values --- "No automatic"

    ttk::label $f1.l -text [_ "Execute script"]:
    cu::combobox $f1.cb -state readonly -textvariable cproject::thisdataS($scripttab,executetime) \
	    -values $values -dict $dict
    ttk::button $f1.b -text [_ "Execute now"] -width 15 -command \
	"cproject::syncfromUI; cproject::EvalScript $f \$cproject::debugrelease \
	[list $scripttab] 1"
    
    grid $f1.l $f1.cb -sticky ew  -padx 2 -pady 2
    grid $f1.b  - -padx 2 -pady 2
    grid columnconfigure $f1 1 -weight 1

    set f2 [ttk::frame $f.f2]
    set idx 1
    foreach "img cmd help" [list \
	    acttick16 [list cproject::CreateDeleteScriptTab $scripttab create] [_ "Create new script tab"] \
	    actcross16 [list cproject::CreateDeleteScriptTab $scripttab delete] [_ "Delete script tab"] \
	    edit16 [list cproject::CreateDeleteScriptTab $scripttab rename] [_ "Rename script tab"] \
	    ] {
	ttk::button $f2.b$idx -image $img -command $cmd -style Toolbutton
	tooltip::tooltip $f2.b$idx $help
	incr idx
    }
    grid $f2.b1 $f2.b2 $f2.b3 -sticky w -padx 2 -pady 2
    
    grid $f.f0 -sticky nsew
    grid $f.f1 -sticky nsew
    grid $f.f2 -sticky nw

    grid rowconfigure $f 0 -weight 1
    grid columnconfigure $f 0 -weight 1
}

proc cproject::CreateDeleteScriptTab { currentscripttab what } {
    variable scripttabs
    variable dataS
    variable notebook

    syncfromUI

    switch $what {
	create {
	    set num [expr [llength $scripttabs]+1]
	    set newscripttab Script$num
	    foreach i [array names dataS *,$currentscripttab,*] {
		regexp {([^,]+),[^,]+,([^,]+)} $i {} dr v
		set dataS($dr,$newscripttab,$v) $dataS($i)
	    }
	    lappend scripttabs $newscripttab
	    $notebook select $newscripttab
	}
	delete {
	    if { [llength $scripttabs] == 1 } {
		WarnWin "Error: There must be at least one Script tab" $notebook
		return
	    }
	    set ret [DialogWin::messageBox -default ok -icon warning -message \
		"Are you sure to delete script tab '$currentscripttab'?" -parent $notebook \
		-title "delete script tab" -type okcancel]
	    if { $ret == "cancel" } { return }
	    
	    set delpos [lsearch $scripttabs $currentscripttab]

	    foreach i [array names dataS *,$currentscripttab,*] {
		unset dataS($i)
	    }
	    set scripttabs [lreplace $scripttabs $delpos $delpos]
	    if { $delpos >= [llength $scripttabs] } { set delpos 0 }
	    $notebook select [lindex $$scripttabsd $delpos]
	}
	rename {
	    CopyNamespace ::DialogWin ::DialogWinCR
	    set f [DialogWinCR::Init $notebook "Enter script tab name" separator ""]
	    set w [winfo toplevel $f]
	    
	    label $f.l -text "Enter new name for script tab '$currentscripttab'" -grid "0 px3 py3"
	    entry $f.e -textvariable DialogWinCR::user(name) -grid "0 px10 py3" -width 30
	    
	    set DialogWinCR::user(name) $currentscripttab
	    tkTabToWindow $f.e
	    supergrid::go $f
	    bind $w <Return> "DialogWinCR::InvokeOK"

	    set action [DialogWinCR::CreateWindow]
	    while 1 {
		switch $action {
		    0 {
		        DialogWinCR::DestroyWindow
		        namespace delete ::DialogWinCR
		        return
		    }
		    1 {
		        set newscripttab [string trim $DialogWinCR::user(name)]
		        if { $newscripttab == "" } {
		            set newscripttab "Script"
		        } elseif { ![string match "Script *" $newscripttab] } {
		            set newscripttab "Script $newscripttab"
		        }
		        if { $newscripttab == "" } {
		            WarnWin "Script tab name cannot be void" $w
		        } elseif { [lsearch $scripttabs $newscripttab] != -1 } {
		            WarnWin "Script tab name '$newscripttab' already exists" $w
		        } else {
		            DialogWinCR::DestroyWindow
		            namespace delete ::DialogWinCR
		            break
		        }
		    }
		}
		set action [DialogWinCR::WaitForWindow]
	    }
	    set pos [lsearch $scripttabs $currentscripttab]
	    foreach i [array names dataS *,$currentscripttab,*] {
		regexp {([^,]+),[^,]+,([^,]+)} $i {} dr v
		set dataS($dr,$newscripttab,$v) $dataS($i)
		unset dataS($i)
	    }
	    set scripttabs [lreplace $scripttabs $pos $pos $newscripttab]
	}
    }
    synctoUI
}

proc cproject::AreFilesEqual { file1 file2 } {
    
    if { $::tcl_platform(platform) == "windows" } {
	return [string equal -nocase $file1 $file2]
    } else {
	return [string equal $file1 $file2]
    }
}

proc cproject::ConvertToRelative { dir file } {

    set list1 [file split $dir]
    set list2 [file split $file]

    for { set i 0 } { $i < [llength $list2] } { incr i } {
	if { ![AreFilesEqual [lindex $list1 $i] [lindex $list2 $i]] } {
	    break
	}
    }
    set listres ""
    for { set j $i } { $j < [llength $list1] } { incr j } {
	lappend listres ..
    }
    for { set j $i } { $j < [llength $list2] } { incr j } {
	lappend listres [lindex $list2 $j]
    }
    if { $listres == "" } { return . }
    return [eval file join $listres]
}

proc cproject::IsProjectNameOk {} {
    variable project
    variable notebook

    if { [info exists notebook] && [winfo exists notebook] } {
	set w [winfo toplevel $notebook]
    } else { set w . }

    if { [string trim $project] == "" } {
	WarnWin [_ "Define a project name before entering data"] $w
	return -code return
    }
    if { [file pathtype $project] != "absolute" } {
	set project [file join [pwd] $project]
	if { ![file isdir [file dirname $project]] } {
	    WarnWin "Project pathname is not correct" $w
	    return -code return
	}
    }
    return [file dirname $project]
}

proc cproject::select_makefile { parent } {
    variable thisdataM
    
    set projectdir [IsProjectNameOk]
    
    set file [tk_getOpenFile -filetypes {{{All Files} *}} \
	    -initialdir $RamDebugger::options(defaultdir) -initialfile \
	    [file tail $thisdataM(makefile_file)] -parent $parent -title [_ "Makefile file"]]
    if { $file eq "" } { return }
    set RamDebugger::options(defaultdir) [file dirname $file]
    set file [ConvertToRelative $projectdir $file]
    set thisdataM(makefile_file)  $file
}

proc cproject::AddModFiles { listbox what } {
    variable project
    variable files
    variable group

    set projectdir [IsProjectNameOk]

    switch $what {
	"view" {
	    if { [llength [$listbox curselection]] != 1 } {
		WarnWin "Error: Select just one file to see it" $listbox
		return
	    }
	    foreach "file_in type group_in path" [lindex $files [$listbox curselection]] break
	    set file [file join [file dirname $project] $path $file_in]
	    RamDebugger::OpenFileF [RamDebugger::filenormalize $file]
	}
	"file" {
	    set types {
		{{C Source Files} {.c .cc .h} }
		{{All Files} * }
	    }
	    set file [tk_getOpenFile -filetypes $types -initialdir $projectdir -parent $listbox \
		-title "Insert file into project"]
	    if { $file == "" } { return }
	    set file [ConvertToRelative $projectdir $file]

	    foreach i $files {
		foreach "file_in type group_in path" $i break
		if { [AreFilesEqual $file $file_in] } {
		    WarnWin "Error: file '$file' is already in the project" $listbox
		    return
		}
	    }
	    lappend files [list [file tail $file] [string trimleft [file ext $file] .] $group \
		[file dirname $file]]
	    fill_files_list $w
	}
	"dir" {
	    set dir [RamDebugger::filenormalize [tk_chooseDirectory -initialdir $projectdir \
		-parent $listbox \
		-title "Insert files from directory into project" -mustexist 1]]
	    if { $dir == "" } { return }
	    
	    set fileslist ""
	    foreach i $files {
		foreach "file_in type group_in path" $i break
		lappend fileslist $file_in
	    }
	    set num 0
	    foreach i [glob -nocomplain -dir $dir *.c *.cc] {
		set file [ConvertToRelative $projectdir $i]
		if { [RamDebugger::lsearchfile $fileslist $file] != -1 } { continue }
		lappend files [list [file tail $file] [string trimleft [file ext $file] .] $group \
		     [file dirname $file]]
		incr num
	    }
	    fill_files_list $w
	    WarnWin [_ "Inserted %d new files" $num] $listbox
	}
	edit {
	    set num 0
	    set numdiff 0
	    foreach i [$listbox curselection] {
		foreach "file_in type group_in path" [lindex $files $i] break
		if { $group != $group_in } { incr numdiff }
		set files [lreplace $files $i $i [list $file_in $type $group $path]]
		incr num
	    }
	    fill_files_list $w
	    WarnWin [_ "Replaced group to %d files (%d new)" $num $numdiff]
	}
	delete {
	    set num 0
	    foreach i [$listbox curselection] {
		set ipos [expr $i-$num]
		set files [lreplace $files $ipos $ipos]
		incr num
	    }
	    $listbox selection clear 0 end
	    fill_files_list $w
	    WarnWin [_ "Deleted from project $num files" $num]
	}
    }
}

proc cproject::AddDelDirectories { listbox what } {

    set projectdir [IsProjectNameOk]

    set dir [IsProjectNameOk]
    switch $what {
	add {
	    set dir [RamDebugger::filenormalize [tk_chooseDirectory -initialdir $dir \
		    -parent $listbox -title "Add directories" -mustexist 1]]
	    if { $dir == "" } { return }
	    $listbox insert end [ConvertToRelative $projectdir $dir]
	}
	delete {
	    set num 0
	    foreach i [$listbox curselection] {
		set ipos [expr $i-$num]
		$listbox delete $ipos
		incr num
	    }
	    $listbox selection clear 0 end
	}
    }
}


################################################################################
#   execution
################################################################################


proc cproject::GiveDebugData {} {
    variable project
    variable dataM
    variable dataE

    set dr $RamDebugger::options(debugrelease)

    if { [info exists dataE($dr,exe)] } {
	if { $dataM($dr,has_userdefined_makefile)  } {
	    set base_dir [file dirname $project]
	    set exe [file join $base_dir $dataE($dr,exe)]
	} else {
	    set objdir [file root $project]_$dr
	    set exe [file join $objdir $dataE($dr,exe)]
	}
	return [list $exe $dataE($dr,execdir) $dataE($dr,exeargs)]
    }
    return ""
}


proc cproject::TryToFindPath { file } {
    variable project
    variable files

    set last_path ""

    set base_dir [file dirname $project]

    if { [file exists [file join $base_dir $file]] } {
	return [file join $base_dir $file]
    }

    foreach i $files {
	foreach "file_in type group_in path" $i break
	if { $path == $last_path } { continue }

	if { [file exists [file join $base_dir $path $file]] } {
	    return [file join $base_dir $path $file]
	}
	set last_path $path
    }
    return ""
}

proc cproject::ScanHeaders { file } {

    set fin [open $file r]
    set aa [read $fin]
    close $fin

    set headers ""
    foreach "- header" [regexp -inline -all {\#include\s+\"([^\"]+)\"} $aa] {
	set file [file join [file dirname $file] $header]
	if { [file exist $file] } {
	    lappend headers [file join [file dirname $file] $header]
	}
    }
    return $headers
}

proc cproject::CleanCompiledFiles { w } {
    variable project
    variable files
    variable dataC
    variable dataM
    variable dataL
    variable dataE

    RamDebugger::SetMessage [_ "Cleaning compilation files..."]
    RamDebugger::WaitState 1
    

    if { $project eq "" } {
	if { [info exists RamDebugger::options(recentprojects)] && \
		[llength $RamDebugger::options(recentprojects)] > 0 } {
	    set project [lindex $RamDebugger::options(recentprojects) 0]
	    set err [catch { cproject::OpenProject $w 0 }]
	    if { $err } { set project "" }
	}
	if { $project == "" } {
	    cproject::Create $w
	    return
	}
    }
    set debrel $RamDebugger::options(debugrelease)

    if { $debrel eq "both" } {
	WarnWin [_ "error: program must be in debug or in release mode"]
	RamDebugger::WaitState 0
	return
    }
    set pwd [pwd]
    cd [file dirname $project]

    if { $dataM($debrel,has_userdefined_makefile)  } {
	set make $dataM($debrel,makefile_file)
	set make_args $dataM($debrel,makefile_arguments)
	set err [catch { exec make -f $make {*}$make_args clean } ret]
    } else { 
	set objdir [file join [file dirname $project] [file root $project]_$debrel]
	
	foreach i [glob -nocomplain -dir $objdir *] {
	    file delete $i
	}
	set ret ""
    }
    cd $pwd
    
    RamDebugger::TextCompClear
    RamDebugger::TextCompRaise
    RamDebugger::TextCompInsert [_ "Compilation files deleted"]
    if { $ret ne "" } {
	RamDebugger::TextCompInsert $ret
    }
    RamDebugger::WaitState 0
    RamDebugger::SetMessage [_ "Cleaning compilation files...done"]
}

proc cproject::printfilename { filename } {
    regsub -all " " $filename {\\ } filename
    return $filename
}

proc cproject::TouchFiles { w } {
    variable project
    variable files
    variable dataC
    variable dataM
    variable dataL
    variable dataE

    set debrel $RamDebugger::options(debugrelease)

    RamDebugger::SetMessage [_ "Actualizing date for compilation files..."]
    RamDebugger::WaitState 1

    if { $project == "" } {
	if { [info exists RamDebugger::options(recentprojects)] && \
		[llength $RamDebugger::options(recentprojects)] > 0 } {
	    set project [lindex $RamDebugger::options(recentprojects) 0]
	    set err [catch { cproject::OpenProject $w 0 }]
	    if { $err } { set project "" }
	}
	if { $project == "" } {
	    cproject::Create $w
	    return
	}
    }
    if { $debrel eq "both" } {
	WarnWin [_ "error: program must be in debug or in release mode"]
	RamDebugger::WaitState 0
	return
    }
    set pwd [pwd]
    cd [file dirname $project]

    if { $dataM($debrel,has_userdefined_makefile)  } {
	set make $dataM($debrel,makefile_file)
	set make_args $dataM($debrel,makefile_arguments)
	set err [catch { exec make -f $make -t {*}$make_args } ret]
    } else {    
	set objdir [file join [file dirname $project] [file root $project]_$debrel]
	
	set time [clock seconds]
	foreach i [glob -nocomplain -dir $objdir *] {
	    file mtime $i $time
	}
	set ret ""
    }
    cd $pwd
    RamDebugger::TextCompClear
    RamDebugger::TextCompRaise
    RamDebugger::SetMessage [_ "Actualizing date for compilation files..."]
    if { $ret ne "" } {
	RamDebugger::TextCompInsert $ret
    }
    RamDebugger::WaitState 0
    RamDebugger::SetMessage [_ "Actualizing date for compilation files...done"]
}

proc cproject::CompileAll { w } {

    RamDebugger::TextCompClear
    foreach i [list debug release] {
	set retval [CompileDo $w $i 1 ""]
	if { $retval == -1 } { break }
    }
}

proc cproject::Compile { w { unique_file "" } } {

    set dr $RamDebugger::options(debugrelease)

    RamDebugger::TextCompClear
    CompileDo $w $dr 0 $unique_file
}

proc cproject::CompileNoStop { w } {

    set dr $RamDebugger::options(debugrelease)

    RamDebugger::TextCompClear
    CompileDo $w $dr 1 ""
}

proc cproject::create_auto_makefile { debrel unique_file } {
    variable project
    variable files
    variable dataC
    variable dataL
    variable dataE
    variable links

    if { $unique_file ne "" } {
	set found 0
	set unique_file [RamDebugger::filenormalize $unique_file]
	foreach i $files {
	    lassign $i file_in type group_in path
	    set file_in2 [RamDebugger::filenormalize [file join [file dirname $project] \
		        $path $file_in]]
	    if { [string equal $file_in2 $unique_file] } {
		set compfiles [list $i]
		set found 1
		break
	    }
	}
	if { !$found } {
	    error [_ "error: file '%s' is not included in the compile project" $unique_file]
	}
	set forcecompile 1
	set project_short "$file_in $debrel"
    } else {
	set compfiles $files
	set forcecompile 0
	set project_short "[file root [file tail $project]] $debrel"
    }

    set objdir [file tail [file root $project]]_$debrel
    if { ![file exists $objdir] } { file mkdir $objdir }
    
    set make [file join $objdir Makefile.ramdebugger]
    if { $unique_file ne "" } { append make 1 }

    set fout [open $make w]
    
    puts -nonewline  $fout "\n# Makefile  -*- makefile -*- "
    puts $fout "Created: [clock format [clock seconds] -format {%Y-%m-%d %H:%M:%S}]"
    puts $fout "\n[string repeat # 80]"
    puts $fout "# Makefile automatically made by RamDebugger"
    puts $fout "#     execute it from the upper directory"
    puts $fout "[string repeat # 80]\n"
    
    if { $unique_file == "" } {
	puts -nonewline $fout "all: "
	foreach link $links {
	    puts -nonewline $fout "[file join $objdir $dataL($debrel,$link,linkexe)] "
	}
	puts $fout "\n"
    }
    foreach i $compfiles {
	foreach "file_in type group_in path" $i break
	
	if { [string trim $dataC($group_in,$debrel,compiler)] == "" } { continue }
	
	set file [file join $path $file_in]
	set objfile [file join $objdir [file root $file_in].o]
	
	if { $forcecompile && [file exists $objfile] } {
	    file delete $objfile
	}
	set dependencies [ScanHeaders $file]
	puts -nonewline $fout "[printfilename $objfile]: [printfilename $file]"
	foreach i $dependencies {
	    puts -nonewline $fout " [printfilename $i]"
	}
	puts -nonewline $fout "\n\t$dataC($group_in,$debrel,compiler) "
	foreach j $dataC($group_in,$debrel,flags) {
	    puts -nonewline $fout "$j "
	}
	foreach j $dataC($group_in,$debrel,includedirs) {
	    if { [string first " " $j] == -1 } {
		puts -nonewline $fout "-I$j "
	    } else {
		puts -nonewline $fout "-I\"$j\" "
	    }
	}
	foreach j $dataC($group_in,$debrel,defines) {
	    puts -nonewline $fout "-D$j "
	}
	puts -nonewline $fout "\\\n\t\t-o [printfilename $objfile] "
	puts $fout "[printfilename $file]\n"
    }
    if { $unique_file eq "" } {
	foreach link $links {
	    set objfiles ""
	    foreach i $files {
		foreach "file_in type group_in path" $i break
		if { [lsearch $dataL($debrel,$link,linkgroups) $group_in] != -1 || \
		    [lsearch $dataL($debrel,$link,linkgroups) All] != -1 } {
		    if { [file ext $file_in] == ".rc" } {
		        lappend objfiles [file join $path $file_in]
		    } else {
		        lappend objfiles [file join $objdir [file root $file_in].o]
		    }
		}
	    }
	    if { [string trim $dataL($debrel,$link,linkexe)] == "" } {
		set tt "WARNING: no output name for linking target '$link'. "
		append tt "assuming 'program_$link.exe'\n"
		RamDebugger::TextCompInsertRed $tt
		set outputname [file join $objdir program_$link.exe]
	    } else {
		set outputname [file join $objdir $dataL($debrel,$link,linkexe)]
	    }
	    
	    set target [string toupper OBJFILES_$link]
	    set string "$target = "
	    foreach i $objfiles {
		append string "$i "
		if { [string length $string] > 70 } {
		    puts $fout "$string \\"
		    set string ""
		}
	    }
	    puts $fout "$string\n"
	    
	    puts $fout "$outputname: \$($target)"
	    puts -nonewline $fout "\t$dataL($debrel,$link,linker) "
	    foreach j $dataL($debrel,$link,linkflags) {
		puts -nonewline $fout  "$j "
	    }
	    puts -nonewline $fout "-o $outputname \$($target) "
	    foreach j $dataL($debrel,$link,librariesdirs) {
		if { $dataL($debrel,$link,linker) != "windres" } {
		    if { [string first " " $j] == -1 } {
		        puts -nonewline $fout "-L$j "
		    } else {
		        puts -nonewline $fout "-L\"$j\" "
		    }
		} else {
		    if { [string first " " $j] == -1 } {
		        puts -nonewline $fout "--include $j "
		    } else {
		        puts -nonewline $fout "--include \"$j\" "
		    }
		}
	    }
	    if { $dataL($debrel,$link,linker) != "windres" } {
		puts -nonewline $fout "-L$objdir "
	    }
	    foreach j $dataL($debrel,$link,libraries) {
		if { [regexp {^lib(.*)(\.a|\.lib|\.so)$} $j {} j2] } {
		    puts -nonewline $fout "-l$j2 "
		} elseif { [regexp {^lib(.*)} $j {} j2] } {
		    puts -nonewline $fout "-l$j2 "
		} elseif { [file exists $j] } {
		    puts -nonewline $fout "$j "
		} elseif { [regexp {(?i)(.*)\.lib} $j {} j2] } {
		    puts -nonewline $fout "-l$j2 "
		} else {
		    puts -nonewline $fout "[file join $objdir $j] "
		}
	    }
	    puts $fout "\n"
	}
    }
    close $fout
    
    return $make
}

proc cproject::CompileDo { w debrel nostop { unique_file "" } } {
    variable project
    variable dataM
    variable compilationstatus

    if { $project eq "" } {
	if { [info exists RamDebugger::options(recentprojects)] && \
		[llength $RamDebugger::options(recentprojects)] > 0 } {
	    set project [lindex $RamDebugger::options(recentprojects) 0]
	    set err [catch { cproject::OpenProject $w 0 }]
	    if { $err } { set project "" }
	}
	if { $project == "" } {
	    cproject::Create $w
	    return -1
	}
    }

    if { $debrel != "debug" && $debrel != "release" } {
	WarnWin [_ "error: program must be in debug or in release mode"]
	return -1
    }
    if { [auto_execok gcc] == "" } {
	set ret [snit_messageBox -default yes -icon question -message \
		[_ "Could not find command 'gcc'. Do you want to see the help?"] -parent $w \
		-title [_ "Command not found"] -type yesno]
	if { $ret eq "yes" } {
	    RamDebugger::ViewHelpForWord "Debugging c++"
	    #RamDebugger::ViewHelpFile "01RamDebugger/RamDebugger_12.html"
	}
	return -1
    }
    
    RamDebugger::SaveFile -only_if_modified 1 auto_save
    RamDebugger::ViewOnlyTextOrAll -force_all
    
    $RamDebugger::mainframe setmenustate debugentry disabled
    $RamDebugger::mainframe setmenustate c++entry disabled
    $RamDebugger::mainframe setmenustate activeconfiguration disabled

    set menu [$RamDebugger::mainframe getmenu c++]
    $menu add separator
    $menu add command -label [_ "Stop compiling"] -command \
	"set ::cproject::compilationstatus 2"
    
    set pwd [pwd]
    cd [file dirname $project]

    set cproject::compilationstatus -1

    set catch_err [catch {
	    if { $unique_file ne "" } {
		set project_short "$unique_file $debrel"
	    } else {
		set project_short "[file root [file tail $project]] $debrel"
	    }
	    RamDebugger::TextCompRaise
	    RamDebugger::TextCompInsert "[string repeat - 20]Compiling $project_short"
	    RamDebugger::TextCompInsert "[string repeat - 20]\n"
	    update
	    
	    if { $dataM($debrel,has_userdefined_makefile)  } {
		set make $dataM($debrel,makefile_file)
		set make_args $dataM($debrel,makefile_arguments)
		if { $unique_file ne "" } {
		    set rel_unique_file [ConvertToRelative [file dirname $project] $unique_file]
		    set ret [exec make -W $rel_unique_file -n -p | grep [format {:[[:space:]]*%s} $rel_unique_file]]
		    regexp {^\s*(.*\S)\s*:} $ret {} target
		    lappend make_args -W $rel_unique_file $target
		}
	    } else {
		set make [create_auto_makefile $debrel $unique_file]
		set make_args ""
	    }
	    if { $::tcl_platform(platform) eq "windows" } {
		set comm ""
		#set comm [auto_execok start]
		#lappend comm  /w /m
		#lappend comm  /WAIT /MIN
	    } else {
		set comm ""
	    }
	    lappend comm make
	    if { $nostop } {
		lappend comm -k
	    }
	    lappend comm -f $make {*}$make_args
	    
	    if { $nostop } {
		RamDebugger::TextCompInsert "make -k -f $make $make_args\n"
	    } else {
		RamDebugger::TextCompInsert "make -f $make $make_args\n"
	    }
	    set fin [open "|$comm |& cat" r]
	    
	    fconfigure $fin -blocking 0
	    fileevent $fin readable [list cproject::CompileFeedback $fin]
	    
	    vwait cproject::compilationstatus
	
	    if { $compilationstatus == 2 } {
		if { $::tcl_platform(platform) eq "windows" } {
		    # maybe it kills also other compilations from other RamDebugger's or manual make's
		    # but that's live and that's windows
		    foreach i [split [exec tlist] \n] {
		        if { [string match -nocase "*make.exe*" $i] } {
		            catch { exec kill /f [scan $i %d] }
		        }
		    }
		}
		catch { close $fin }
	    }
	} catch_string]
	
    if { $catch_err } {
	WarnWin $catch_string
	set compilationstatus 1
    }
    switch -- $compilationstatus {
	-1 {
	    RamDebugger::TextCompInsert "Project '$project_short' is up to date"
	}
	0 {
	    RamDebugger::TextCompInsert "[string repeat - 20]Ending compilation of "
	    RamDebugger::TextCompInsert "$project_short"
	    RamDebugger::TextCompInsert "[string repeat - 20]\n"
	}
	1 {
	    RamDebugger::TextCompInsert "[string repeat - 20]Failing compilation of $project_short"
	    RamDebugger::TextCompInsert "[string repeat - 20]\n"
	    update
	}
	2 {
	    RamDebugger::TextCompInsert "[string repeat - 20]compilation of $project_short stopped "
	    RamDebugger::TextCompInsert "at user demand[string repeat - 20]\n"
	    
	    update
	}
    }
    cd $pwd

    $menu delete end
    $menu delete end
    $RamDebugger::mainframe setmenustate c++entry normal
    $RamDebugger::mainframe setmenustate debugentry normal
    $RamDebugger::mainframe setmenustate activeconfiguration normal

    if { $compilationstatus == 2 } {
	return -1
    } else { return 0 }
}

proc cproject::CompileFeedback { fin } {
    variable compilationstatus

    if { [catch { eof $fin } ret] || $ret } {
	set err [catch { close $fin } errstring]
	if { $err } {
	    RamDebugger::TextCompInsert $errstring\n
	    set compilationstatus 1
	} else {
	    set compilationstatus 0
	}
	return
    }
    set ret [gets $fin aa]

    if { $aa != "" } {
	RamDebugger::TextCompInsert $aa\n
	update
    }
}

proc cproject::EvalScript { w debrel scripttab { show 0 } } {
    variable project
    variable dataS

    set script $dataS($debrel,$scripttab,script)
    regsub -all {(?n)^\s*\#.*$} $script "" res
    if { [string trim $res] == "" } {
	if { $show } {
	    WarnWin "Nothing to execute in Script '$scripttab'" $w
	}
	return
    }

    set pwd [pwd]
    cd [file dirname $project]

    set ProjectDir [file dirname $project]
    set ObjectsDir [file tail [file root $project]]_$debrel

    rename ::puts ::___puts
    proc ::puts args {
	set argsN $args
	set hasnewline 1
	if { [lindex $argsN 0] == "-nonewline" } {
	    set hasnewline 0
	    set argsN [lrange $argsN 1 end]
	}
	set channelId stdout
	if { [llength $argsN] == 2 } {
	    set channelId [lindex $argsN 0]
	    set argsN [lrange $argsN 1 end]
	}
	if { [llength $argsN] == 1 && [regexp {stdout|stderr} $channelId] } {
	    RamDebugger::TextCompInsert [lindex $argsN 0]
	    if { $hasnewline } { RamDebugger::TextCompInsert \n }
	} else {
	    uplevel ___puts $args
	}
    }

    set err [catch $script errstring]

    rename ::puts ""
    rename ::___puts ::puts

    if { $err } {
	RamDebugger::TextCompRaise
	RamDebugger::TextCompInsertRed "-----Error executing script '$scripttab'-----\n"
	RamDebugger::TextCompInsertRed $::errorInfo\n
	if { $show } {
	    WarnWin "error executing script '$scripttab' ($errstring)" $w
	}
    } else {
	RamDebugger::TextCompRaise
	RamDebugger::TextCompInsert "Executed script '$scripttab'. Result: \n$errstring\n"
	if { $show } {
	    WarnWin "Executed script '$scripttab'. Result: $errstring" $w
	}
    }
    cd $pwd
}

################################################################################
# DebugCplusPlus
################################################################################

proc RamDebugger::DebugCplusPlusWindow { { tryautomatic 0 } } {
    variable text
    variable options

    if { ![info exists options(debugcplusplus)] } {
	set options(debugcplusplus) ""
    }

    set exes ""
    set dirs ""
    set args ""
    foreach "exe dir arg" $options(debugcplusplus) {
	lappend exes $exe
	lappend dirs $dir
	lappend args $arg
    }
    lassign [cproject::GiveDebugData] exe dir arg

    if { $tryautomatic && $exe != "" } {
	set found 0
	set ipos 0
	foreach "exe_in dir_in arg_in" $options(debugcplusplus) {
	    if { $exe == $exe_in && $dir == $dir_in && $arg == $arg_in } {
		set found 1
		break
	    }
	    incr ipos 3
	}
	if { $found && $ipos != 0 } {
	    set options(debugcplusplus) [lreplace $options(debugcplusplus) $ipos \
		    [expr $ipos+2]]
	}
	if { !$found || $ipos != 0 } {
	    set options(debugcplusplus) [linsert $options(debugcplusplus) 0 \
		    $exe $dir $arg]
	}

	rdebug -debugcplusplus [list $exe $dir $arg]
	return
    }
    if { $tryautomatic && $options(debugcplusplus) != "" } {
	rdebug -debugcplusplus [lrange $options(debugcplusplus) 0 2]
	return
    }
    
    destroy $text.cmp
    set w [dialogwin_snit $text.cmp -title [_ "Debug c++"]]
    set f [$w giveframe]
    
    ttk::label $f.l1 -text [_ "Program to debug"];
    ttk::combobox $f.cb1 -textvariable [$w give_uservar executable ""] -width 40 -values \
	$exes
    ttk::button $f.b1 -image [Bitmap::get file] -style Toolbutton -command \
	[list RamDebugger::DebugCplusPlusWindow_getexe $w]
    
    ttk::label $f.l2 -text [_ "Directory"];
    ttk::combobox $f.cb2 -textvariable [$w give_uservar directory ""] -width 40 -values \
	$dirs
    ttk::button $f.b2 -image [Bitmap::get folder] -style Toolbutton -command \
	[list RamDebugger::DebugCplusPlusWindow_getdir $w]
  
    ttk::label $f.l3 -text [_ "Arguments"];
    ttk::combobox $f.cb3 -textvariable [$w give_uservar arguments ""] -width 40 -values \
	$args

    grid $f.l1 $f.cb1 $f.b1 -sticky w -padx 2 -pady 2
    grid $f.l2 $f.cb2 $f.b2 -sticky w -padx 2 -pady 2
    grid $f.l3 $f.cb3           -sticky w -padx 2 -pady 2
    grid configure $f.cb1 $f.cb2 $f.cb3 -sticky ew
    grid columnconfigure $f 1 -weight 1
    
    if { [info exists options(debugcplusplus)] } {
	$w set_uservar_value executable [lindex $options(debugcplusplus) 0]
	$w set_uservar_value directory [lindex $options(debugcplusplus) 1]
	$w set_uservar_value arguments [lindex $options(debugcplusplus) 2]
    }
    tk::TabToWindow $f.cb1
    bind $w <Return> [list $w invokeok]
    set action [$w createwindow]
    while 1 {
	switch -- $action {
	    -1 -- 0 {
		destroy $w
		return
	    }
	    1 {
		set found 0
		set ipos 0
		foreach "exe dir args" $options(debugcplusplus) {
		    if { $exe eq [$w give_uservar_value executable] && \
		        $dir eq [$w give_uservar_value directory]  && \
		            $args eq [$w give_uservar_value arguments]  } {
		        set found 1
		        break
		    }
		    incr ipos 3
		}
		if { $found && $ipos != 0 } {
		    set options(debugcplusplus) [lreplace $options(debugcplusplus) $ipos \
		            [expr $ipos+2]]
		}
		if { !$found || $ipos != 0 } {
		    set options(debugcplusplus) [linsert $options(debugcplusplus) 0 \
		            [$w give_uservar_value executable] [$w give_uservar_value directory] \
		            [$w give_uservar_value arguments]]
		}
		rdebug -debugcplusplus [list  [$w give_uservar_value executable]  \
		        [$w give_uservar_value directory]   [$w give_uservar_value arguments]]
		destroy $w
		return
	    }
	}
	set action [$w waitforwindow]
    }
}

proc RamDebugger::DebugCplusPlusWindow_getexe { w } {

    set file [tk_getOpenFile -filetypes {{{All Files} *}} \
	    -initialdir $RamDebugger::options(defaultdir) -initialfile \
	    [$w give_uservar_value executable] -parent $w -title [_ "Debug executable"]]
    if { $file eq "" } { return }
    set RamDebugger::options(defaultdir) [file dirname $file]
    $w set_uservar_value executable $file
}

proc RamDebugger::DebugCplusPlusWindow_getdir { w } {

    if { [file isdirectory [file dirname [$w give_uservar_value executable]]] } {
	set initial [file dirname [$w give_uservar_value executable]]
    } else {
	set initial $RamDebugger::options(defaultdir)
    }
    set dir [tk_chooseDirectory -initialdir $initial -parent $w -mustexist 1 -title [_ "Debug directory"]]
    if { $dir eq "" } { return }
    set RamDebugger::options(defaultdir) $dir
    $w set_uservar_value directory $dir
}

proc RamDebugger::DebugCplusPlusWindowAttach {} {
    variable text
    
    set w $text.debugatt
    destroy $w
    dialogwin_snit $w -title [_ "Select program to attach the debugger"]
    set f [$w giveframe]
    
    set f1 [ttk::labelframe $f.f1 -text [_ "programs"]]
	
    set columns [list \
	    [list 25 [_ "Program"] left text 1] \
	    [list 10 [_ "PID"] right text 1] \
	    [list 8 [_ "Create time"] left text 1] \
	    [list 8 [_ "CPU time"] left text 1] \
	    [list 10 [_ "Memory (KB)"] right text 1] \
	    ]
    
    package require fulltktree
    fulltktree $f1.tree -selecthandler2 \
	"[list $w invokeok];#" \
	-contextualhandler_menu [list RamDebugger::DebugCplusPlusWindowAttach_contextual $w] \
	-columns $columns -expand 1 \
	-selectmode browse -showlines 0 -indent 0 -width 650
    $w set_uservar_value tree $f1.tree

    set searchList [RamDebugger::GetPreference debug_cplus_attach_search_list]
    
    $w set_uservar_value search [lindex $searchList 0]
    ttk::combobox $f1.e1 -textvariable [$w give_uservar search] -width 12 -values $searchList
	
    $w add_trace_to_uservar search [list RamDebugger::DebugCplusPlusWindowAttach_search $w]
       
    ttk::button $f1.b1 -image actcross16 -style Toolbutton \
	-command "[list $w set_uservar_value search ""] ; [list focus $f1.e1]"
    
    tooltip::tooltip $f1.e1 [_ "Search in list"]
    tooltip::tooltip $f1.b1 [_ "Clear search entry"]
    
    grid $f1.tree  -      -     -   -sticky nsew -padx 2 
    grid   x     x    $f1.e1 $f1.b1 -sticky e -padx 2 -pady "1 1"
    
    grid columnconfigure $f1 1 -weight 1
    grid rowconfigure $f1 0 -weight 1
    
    grid $f1 -sticky nsew
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1
    
    DebugCplusPlusWindowAttach_update $w
    
    tk::TabToWindow $f1.tree
    bind [winfo toplevel $f1] <Return> [list $w invokeok]
    set action [$w createwindow]
    
    while 1 {
	if { $action <= 0 } { 
	    destroy $w
	    return
	}
	set item [$f1.tree selection get]
	if { [llength $item] == 1 } {
	    lassign [$f1.tree item text $item] cmd pid
	    rdebug -debugcplusplus [list  $pid "" $cmd]
	    set search [string trim [$w give_uservar_value search]]
	    if { $search ne "" } {
		set searchList [linsert0 -max_len 10 $searchList $search]
		RamDebugger::SetPreference debug_cplus_attach_search_list $searchList
	    }
	    break
	}
	set action [$w waitforwindow]
    }
    destroy $w
}

proc RamDebugger::DebugCplusPlusWindowAttach_contextual { w tree menu item itemList } {

    set pid [$tree item text $item 1]
    $menu add command -label [_ "Kill process"] -command \
	[list RamDebugger::DebugCplusPlusWindowAttach_kill $w $pid]
}

proc RamDebugger::DebugCplusPlusWindowAttach_kill { w pid } {
    cu::kill $pid
    DebugCplusPlusWindowAttach_update $w
}

proc RamDebugger::DebugCplusPlusWindowAttach_update { w } {
    variable DebugCplusPlusWindowAttach_search_id
    
    unset -nocomplain DebugCplusPlusWindowAttach_search_id
    
    set tree [$w give_uservar_value tree]
    set search [string trim [$w give_uservar_value search]]
    
    $tree item delete all
    foreach i [cu::ps $search] {
	$tree insert end $i
    }
}

proc RamDebugger::DebugCplusPlusWindowAttach_search { w } {
    variable DebugCplusPlusWindowAttach_search_id

    if { [info exists DebugCplusPlusWindowAttach_search_id] } {
	after cancel $DebugCplusPlusWindowAttach_search_id
    }
    set DebugCplusPlusWindowAttach_search_id [after 500 \
	    [list RamDebugger::DebugCplusPlusWindowAttach_update $w]]
}













