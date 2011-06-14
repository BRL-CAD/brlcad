
package require snit

namespace eval profileprocs {
    variable lastcommandList ""
}

proc profileprocs::StartProfiling { procs } {
    variable lastcommandList

    if { ![llength $procs] } {
	WarnWin [_ "There are no procs selected"]
	return
    }
    RamDebugger::rtime -start
    update idletasks
    set comm {
	namespace eval ::RDC {}
	proc ::RDC::profileEnterCommand { name op } {
	    variable datatimesStack
	    variable datatimes
	    lappend datatimesStack [clock clicks -milliseconds]
	}
	proc ::RDC::profileLeaveCommand { name code result op } {
	    variable datatimesStack
	    variable datatimes
	    
	    
	    if { ![llength $datatimesStack] } { return }
	    set time [expr {[clock clicks -milliseconds]-[lindex $datatimesStack end]}]
	    set datatimesStack [lrange $datatimesStack 0 end-1]
	    
	    set sname [uplevel [list namespace which -command [lindex $name 0]]]
	    
	    if { [string match *namespace $sname] } { continue }
	    if { [string match ::profileprocs::* $sname] } { return }
	    
	    set fname ""
	    for { set i 1 } { $i <= [info level]-1 } { incr i } {
		set cmd_s [lindex [info level $i] 0]
		set cmd [uplevel \#$i [list namespace which -command $cmd_s]]
		if { [string match *namespace $cmd] } { continue }
		if { [string match ::profileprocs::* $cmd] } { continue }
		if { $cmd eq "" } {
		    lappend fname [lindex [info level $i] 0]
		} else {
		    lappend fname $cmd
		}
	    }
	    lappend fname $sname

	    if { [info exists datatimes($fname)] } {
		set time [expr {$time+[lindex $datatimes($fname) 0]}]
		set rep [expr {1+[lindex $datatimes($fname) 1]}]
		set datatimes($fname) [list $time $rep]
	    } else {
		set datatimes($fname) [list $time 1]  
	    }
	}
	set ::RDC::datatimesStack ""
	array unset ::RDC::datatimes
    }
    RamDebugger::EvalRemoteAndReturn $comm

    # not necessary, as we restart
    set commList ""
    foreach i $lastcommandList {
	set cmd {
	    if { [info procs %i] ne "" } {
		trace remove execution %i enter ::RDC::profileEnterCommand
		trace remove execution %i leave ::RDC::profileLeaveCommand
	    }
	}
	lappend commList [string map [list %i [list $i]] $cmd]
    }
    foreach i $procs {
	set cmd {
	    if { [info procs %i] eq "" } {
		auto_load %i
	    }
	    trace add execution %i enter ::RDC::profileEnterCommand
	    trace add execution %i leave ::RDC::profileLeaveCommand
	}
	lappend commList [string map [list %i [list $i]] $cmd]
    }
    RamDebugger::EvalRemoteAndReturn [join $commList \n]
    set lastcommandList $procs
}

proc profileprocs::EndProfiling {} {
    variable lastcommandList

    set commList ""
    foreach i $lastcommandList {
	lappend commList [list trace remove execution $i enter ::RDC::profileEnterCommand]
	lappend commList [list trace remove execution $i leave ::RDC::profileLeaveCommand]
    }
    lappend commList [list set ::RDC::datatimesStack ""]
    lappend commList [list array unset ::RDC::datatimes]
    RamDebugger::EvalRemoteAndReturn [join $commList \n]
    set lastcommandList ""

    RamDebugger::rtime -stop
}

snit::widget tablestree {
    option -selecthandler

    variable tree
    variable vscrollbar
    variable hscrollbar

    variable SystemButtonFace
    variable SystemHighlight
    variable SystemHighlightText
    variable SystemFont
    variable searchstring
    variable searchstring_reached_end

    delegate method * to tree
    delegate option * to tree

    constructor { args } {

	$self createimages_colors
	
	set height [font metrics $SystemFont -linespace]
	if {$height < 18} { set height 18 }

	install tree as treectrl $win.t -highlightthickness 0 -borderwidth 0 \
	    -xscrollincrement 20 -yscrollincrement 20 -showheader 1 -indent 5 \
	    -font $SystemFont -itemheight $height -selectmode browse -showroot no \
	    -showrootbutton no -showbuttons yes -showlines yes \
	    -scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50" \
	    -showlines yes
	install vscrollbar as scrollbar $win.sv -orient vertical -command [list $win.t yview]
	install hscrollbar as scrollbar $win.sh -orient horizontal -command [list $win.t xview]

	set err [catch {
		$tree configure -openbuttonimage mac-collapse \
		    -closedbuttonimage mac-expand
	    }]
	if { $err } {
	    $tree configure -buttonimage [list mac-collapse open mac-expand !open ]
	}

	grid $win.t $win.sv -sticky ns
	grid $win.sh -sticky ew
	grid configure $win.t -sticky nsew
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure $win 0 -weight 1

	$tree column create
	$tree column create
	$tree column create
	$tree column create
	catch { $tree configure -treecolumn 0 }
	$tree column configure 0 -width 285 -text Proc -itembackground {\#e0e8f0 {}}
	$tree column configure 1 -text Time -width 70 -justify left \
	    -itembackground {linen {}}
	$tree column configure 2 -text # -width 30 -justify left \
	    -itembackground {linen {}}
	$tree column configure 3 -text % -width 30 -justify left \
	    -itembackground {linen {}}

	$tree element create e1 image -image {contents-16 {open} appbook16 {}}
	$tree element create e2 image -image appbook16
	$tree element create e3 text \
	    -fill [list $SystemHighlightText {selected focus}]
	$tree element create e4 text -fill blue
	$tree element create e5 rect -showfocus yes \
	    -fill [list $SystemHighlight {selected focus} gray {selected !focus}]
	$tree element create e6 text

	$tree style create s1
	$tree style elements s1 {e5 e1 e3 e4}
	$tree style layout s1 e1 -padx {0 4} -expand ns
	$tree style layout s1 e3 -padx {0 4} -expand ns
	$tree style layout s1 e5 -union [list e3] -iexpand ns -ipadx 2 -ipadx 2

	$tree style create s2
	$tree style elements s2 {e5 e2 e3 e4}
	$tree style layout s2 e2 -padx {0 4} -expand ns
	$tree style layout s2 e3 -padx {0 4} -expand ns
	$tree style layout s2 e5 -union [list e3] -iexpand ns -ipadx 2 -ipadx 2

	$tree style create s3
	$tree style elements s3 {e6}
	$tree style layout s3 e6 -padx 6 -padx 6 -expand ns

	$self createbindings
	$self configurelist $args
    }


    method createbindings {} {
	bind TreeCtrl <KeyPress-Left> {
	    #TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W -1]
	    if { [%W item numchildren [%W index active]] } {
		%W collapse [%W index active]
	    } else {
		%W activate "active parent"
		%W selection clear all
		%W selection add active
	    }
	}
	bind TreeCtrl <KeyPress-Right> {
	    #TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W 1]
	    %W expand [%W index active]
	}
#         bind TreeCtrl <Double-ButtonPress-1> {+
#             set id [%W identify %x %y]
#             if {[lindex $id 0] eq "item"} {
#                 %W toggle [%W index active]
#             }
#         }

	$tree notify bind $vscrollbar <Scroll-y> { %W set %l %u }
	bind $vscrollbar <ButtonPress-1> [list focus $tree]

	$tree notify bind $hscrollbar <Scroll-x> { %W set %l %u }
	bind $hscrollbar <ButtonPress-1> [list focus $tree]

	$tree notify install event Header
	$tree notify install detail Header invoke

	$tree notify install event Drag
	$tree notify install detail Drag begin
	$tree notify install detail Drag end
	$tree notify install detail Drag receive

	$tree notify install event Edit
	$tree notify install detail Edit accept

	bind $tree <KeyPress> [mymethod search_text %A]
	bind $tree <Return> "[mymethod execute_select]"
	bind $tree <ButtonRelease-1> "[bind TreeCtrl <ButtonRelease-1>]
		[mymethod execute_select %x %y] ; break"
	bind $tree <<ContextualPress>> {
	    set id ""
	    foreach "type id" [%W identify %x %y] break
	    if { $type != "item" || $id == "" } { return }
	    %W selection clear all
	    %W selection add $id
	}
    }
    method search_text { char } {

	if { $char eq "\t" } { return }
	if { [string is wordchar -strict $char] || [string is punct -strict $char] \
		 || [string is space -strict $char] } {
	    if { ![info exists searchstring] || [string index $searchstring end] != $char } {
		append searchstring $char
	    }
	    if { [info exists searchstring_reached_end] && $searchstring_reached_end ne "" \
		&& $searchstring_reached_end eq $searchstring } {
		set id "first visible"
	    } elseif { [$tree compare active == "active bottom"] } {
		set id "first visible"
	    } else { set id "active below" }
	    set found 0
	    while { $id != "" } {
		set txt [$tree item text $id 0]
		if { [string match -nocase $searchstring* $txt] } {
		    set found 1
		    break
		}
		set id [$tree index "$id below"]
	    }
	    if { !$found } {
		bell
		set searchstring_reached_end $searchstring
		set searchstring ""
		after 300 [list set [varname searchstring_reached_end] ""]
	    } else {
		$tree activate $id
		$tree selection clear all
		$tree selection add active
		after 300 [list set [varname searchstring] ""]
	    }
	}
    }
    method activate_select { name { parent 0 } } {
	foreach id [$tree item children $parent] {
	    if { [$tree item text $id 0] eq $name } {
		$tree activate $id
		$tree selection clear all
		$tree selection add active

		if { [$tree item numchildren $id] == 0 } {
		    $tree collapse all
		    foreach i [$tree item ancestors $id] { $tree expand $i }
		    $tree expand -recurse $id
		}
		return
	    }
	    $self activate_select $name $id
	}
    }
    method execute_select { { x "" } { y "" } } {
	set id [lindex [$tree selection get] 0]
	set identify [$tree identify $x $y]
	if { $x ne "" && ([lindex $identify 0] ne "item" || \
	    [lindex $identify 2] ne "column") } { return }

	set isopen [$tree item state get $id open]
	if { $isopen } {
	    $tree collapse -recurse $id
	} else {
	    $tree collapse all
	    foreach i [$tree item ancestors $id] { $tree expand $i }
	    $tree expand -recurse $id
	}
	set path ""
	while { $id != 0 } {
	    set path [linsert $path 0 [$tree item element cget $id 0 e3 -text]]
	    set id [$tree item parent $id]
	}
	if { $options(-selecthandler) ne "" } {
	    uplevel #0 [list $options(-selecthandler) $path]
	}
    }
    method add { parent proc time num { before "" } } {
	set treenode [$tree item create]
	if { $before eq "" } {
	    $tree item lastchild $parent $treenode
	} else {
	    $tree item prevsibling $before $treenode
	}
	
	$tree item style set $treenode 0 s2 1 s3 2 s3 3 s3
	$tree item complex $treenode \
	    [list [list e3 -text $proc]] \
	    [list [list e6 -text $time]] \
	    [list [list e6 -text $num]]
	
	if { $parent != 0 && ![$tree item cget $parent -button] } {
	    set pproc [$tree item text $parent 0]
	    set ptime [$tree item text $parent 1]
	    set pnum [$tree item text $parent 2]
	    $tree item configure $parent -button yes
	    $tree item style set $parent 0 s1 1 s3 2 s3 3 s3
	    $tree item complex $parent \
		[list [list e3 -text $pproc]] \
		[list [list e6 -text $ptime]] \
		[list [list e6 -text $pnum]]
	    $tree item collapse $parent
	}
	return $treenode
    }
    method addpercent { item percent } {
	$tree item element configure $item 3 e6 -text $percent
    }
    method createimages_colors {} {

	set w [listbox .listbox]
	set SystemButtonFace [$w cget -highlightbackground]
	set SystemHighlight [$w cget -selectbackground]
	set SystemHighlightText [$w cget -selectforeground]
	set SystemFont [$w cget -font]
	destroy $w

	if { [lsearch -exact [image names] appbook16] != -1 } { return }

	image create photo mac-collapse  -data {
	    R0lGODlhEAAQALIAAAAAAAAAMwAAZgAAmQAAzAAA/wAzAAAzMyH5BAUAAAYA
	    LAAAAAAQABAAggAAAGZmzIiIiLu7u5mZ/8zM/////wAAAAMlaLrc/jDKSRm4
	    OAMHiv8EIAwcYRKBSD6AmY4S8K4xXNFVru9SAgAh/oBUaGlzIGFuaW1hdGVk
	    IEdJRiBmaWxlIHdhcyBjb25zdHJ1Y3RlZCB1c2luZyBVbGVhZCBHSUYgQW5p
	    bWF0b3IgTGl0ZSwgdmlzaXQgdXMgYXQgaHR0cDovL3d3dy51bGVhZC5jb20g
	    dG8gZmluZCBvdXQgbW9yZS4BVVNTUENNVAAh/wtQSUFOWUdJRjIuMAdJbWFn
	    ZQEBADs=
	}
	image create photo mac-expand -data {
	    R0lGODlhEAAQALIAAAAAAAAAMwAAZgAAmQAAzAAA/wAzAAAzMyH5BAUAAAYA
	    LAAAAAAQABAAggAAAGZmzIiIiLu7u5mZ/8zM/////wAAAAMnaLrc/lCB6MCk
	    C5SLNeGR93UFQQRgVaLCEBasG35tB9Qdjhny7vsJACH+gFRoaXMgYW5pbWF0
	    ZWQgR0lGIGZpbGUgd2FzIGNvbnN0cnVjdGVkIHVzaW5nIFVsZWFkIEdJRiBB
	    bmltYXRvciBMaXRlLCB2aXNpdCB1cyBhdCBodHRwOi8vd3d3LnVsZWFkLmNv
	    bSB0byBmaW5kIG91dCBtb3JlLgFVU1NQQ01UACH/C1BJQU5ZR0lGMi4wB0lt
	    YWdlAQEAOw==
	}
	image create photo appbook16 -data {
	    R0lGODlhEAAQAIQAAPwCBAQCBDyKhDSChGSinFSWlEySjCx+fHSqrGSipESO
	    jCR6dKTGxISytIy6vFSalBxydAQeHHyurAxubARmZCR+fBx2dDyKjPz+/MzK
	    zLTS1IyOjAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAVkICCOZGmK
	    QXCWqTCoa0oUxnDAZIrsSaEMCxwgwGggHI3E47eA4AKRogQxcy0mFFhgEW3M
	    CoOKBZsdUrhFxSUMyT7P3bAlhcnk4BoHvb4RBuABGHwpJn+BGX1CLAGJKzmK
	    jpF+IQAh/mhDcmVhdGVkIGJ5IEJNUFRvR0lGIFBybyB2ZXJzaW9uIDIuNQ0K
	    qSBEZXZlbENvciAxOTk3LDE5OTguIEFsbCByaWdodHMgcmVzZXJ2ZWQuDQpo
	    dHRwOi8vd3d3LmRldmVsY29yLmNvbQA7
	}
	image create photo contents-16 -data {
	    R0lGODlhEAAQAIUAAPwCBAQCBExCNGSenHRmVCwqJPTq1GxeTHRqXPz+/Dwy
	    JPTq3Ny+lOzexPzy5HRuVFSWlNzClPTexIR2ZOzevPz29AxqbPz6/IR+ZDyK
	    jPTy5IyCZPz27ESOjJySfDSGhPTm1PTizJSKdDSChNzWxMS2nIR6ZKyijNzO
	    rOzWtIx+bLSifNTGrMy6lIx+ZCRWRAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAae
	    QEAAQCwWBYJiYEAoGAFIw0E5QCScAIVikUgQqNargtFwdB9KSDhxiEjMiUlg
	    HlB3E48IpdKdLCxzEAQJFxUTblwJGH9zGQgVGhUbbhxdG4wBHQQaCwaTb10e
	    mB8EBiAhInp8CSKYIw8kDRSfDiUmJ4xCIxMoKSoRJRMrJyy5uhMtLisTLCQk
	    C8bHGBMj1daARgEjLyN03kPZc09FfkEAIf5oQ3JlYXRlZCBieSBCTVBUb0dJ
	    RiBQcm8gdmVyc2lvbiAyLjUNCqkgRGV2ZWxDb3IgMTk5NywxOTk4LiBBbGwg
	    cmlnaHRzIHJlc2VydmVkLg0KaHR0cDovL3d3dy5kZXZlbGNvci5jb20AOw==
	}
	image create photo folder-open -data {
	    R0lGODlhEAANANIAAAAAAISEhMbGxv//AP////8AAAAAAAAAACH5BAkZAAUA
	    LAAAAAAQAA0AAAM8WBrM+rAEQmmIb4qxBWnNQnCkV32ARHRlGQBgDA7vdN6v
	    UK8tC78qlrCWmvRKsJTquHkpZTKAsiCtWq0JAAA7
	}
	image create photo folder-closed -data {
	    R0lGODlhDwANANIAAAAAAISEhMbGxv//AP////8AAAAAAAAAACH5BAkZAAUA
	    LAAAAAAPAA0AAAMzWBXM+jCIMUWAT9JtmwtEKI5hAIBcOplgpVIs8bqxa8On
	    fNP5zsWzDctD9AAKgKRyuUwAAAA7
	} -format GIF
    }

}

snit::widget procstree {
    option -selecthandler

    variable tree
    variable vscrollbar
    variable hscrollbar

    variable SystemButtonFace
    variable SystemHighlight
    variable SystemHighlightText
    variable SystemFont
    variable searchstring
    variable searchstring_reached_end

    delegate method * to tree
    delegate option * to tree

    constructor { args } {

	$self createimages_colors
	
	set height [font metrics $SystemFont -linespace]
	if {$height < 18} { set height 18 }

	install tree as treectrl $win.t -highlightthickness 0 -borderwidth 0 \
	    -xscrollincrement 20 -showheader 1 -indent 5 \
	    -font $SystemFont -itemheight $height -selectmode browse -showroot no \
	    -showrootbutton no -showbuttons yes -showlines yes \
	    -scrollmargin 16 -xscrolldelay "500 50" -yscrolldelay "500 50" \
	    -showlines yes
	install vscrollbar as scrollbar $win.sv -orient vertical -command [list $win.t yview]
	install hscrollbar as scrollbar $win.sh -orient horizontal -command [list $win.t xview]

	set err [catch {
		$tree configure -openbuttonimage mac-collapse \
		    -closedbuttonimage mac-expand
	    }]
	if { $err } {
	    $tree configure -buttonimage [list mac-collapse open mac-expand !open ]
	}

	grid $win.t $win.sv -sticky ns
	grid $win.sh -sticky ew
	grid configure $win.t -sticky nsew
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure $win 0 -weight 1

	$tree state define semichosen
	$tree state define chosen

	$tree column create
	$tree column create
	$tree column configure 0 -width 285 -text Proc -itembackground {\#e0e8f0 {}}
	$tree column configure 1 -text Selection -width 70 -justify left \
	    -itembackground {linen {}}

	$tree element create e1 image -image {contents-16 {open} appbook16 {}}
	$tree element create e2 image -image appbook16
	$tree element create e3 text \
	    -fill [list $SystemHighlightText {selected focus}]
	$tree element create e4 text -fill blue
	$tree element create e5 rect -showfocus yes \
	    -fill [list $SystemHighlight {selected focus} gray {selected !focus}]
	$tree element create e6 text
	$tree element create e7 image -image [list img-check-grey {semichosen} \
		img-check-on {chosen} img-check-off {}]
	$tree element create e8 image -image document2-16

	$tree style create s1
	$tree style elements s1 {e5 e1 e3 e4}
	$tree style layout s1 e1 -padx {0 4} -expand ns
	$tree style layout s1 e3 -padx {0 4} -expand ns
	$tree style layout s1 e5 -union [list e3] -iexpand ns -ipadx 2 -ipadx 2

	$tree style create s2
	$tree style elements s2 {e5 e8 e3 e4}
	$tree style layout s2 e8 -padx {0 4} -expand ns
	$tree style layout s2 e3 -padx {0 4} -expand ns
	$tree style layout s2 e5 -union [list e3] -iexpand ns -ipadx 2 -ipadx 2

	$tree style create s3
	$tree style elements s3 {e7}
	$tree style layout s3 e7 -padx {0 0} -expand ns

	$self createbindings
	$self configurelist $args

	$self addnamespace 0 ::
    }
    method addnamespace { parent ns } {
	set nss ""
	foreach i [RamDebugger::EvalRemoteAndReturn [list namespace children $ns]] {
	    lappend nss [namespace tail $i]
	}
	foreach i [lsort -dictionary $nss] {
	    $self add $parent $i asparent
	}
	set procs ""
	if { $ns eq "::" } { set pattern ::* } else { set pattern ${ns}::* }
	foreach i [RamDebugger::EvalRemoteAndReturn [list info procs $pattern]] {
	    lappend procs [namespace tail $i]
	}
	foreach i [lsort -dictionary $procs] {
	    $self add $parent $i
	}
    }
    method createbindings {} {
	bind TreeCtrl <KeyPress-Left> {
	    #TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W -1]
	    if { [%W item numchildren [%W index active]] } {
		%W collapse [%W index active]
	    } else {
		%W activate "active parent"
		%W selection clear all
		%W selection add active
	    }
	}
	bind TreeCtrl <KeyPress-Right> {
	    #TreeCtrl::SetActiveItem %W [TreeCtrl::LeftRight %W 1]
	    %W expand [%W index active]
	}
#         bind TreeCtrl <Double-ButtonPress-1> {+
#             set id [%W identify %x %y]
#             if {[lindex $id 0] eq "item"} {
#                 %W toggle [%W index active]
#             }
#         }

	$tree notify bind $vscrollbar <Scroll-y> { %W set %l %u }
	bind $vscrollbar <ButtonPress-1> [list focus $tree]

	$tree notify bind $hscrollbar <Scroll-x> { %W set %l %u }
	bind $hscrollbar <ButtonPress-1> [list focus $tree]

	$tree notify bind $tree <Expand-before> {
	    set w [winfo parent %T]
	    foreach i [$w item children %I] {
		$w item delete $i
	    }
	    set ns ::[$w item text %I 0]
	    set i %I
	    while 1 {
		set i [$w item parent $i]
		if { $i == 0 } { break }
		set ns ::[$w item text $i 0]$ns
	    }
	    $w addnamespace %I $ns
	}
	$tree notify install event Header
	$tree notify install detail Header invoke

	$tree notify install event Drag
	$tree notify install detail Drag begin
	$tree notify install detail Drag end
	$tree notify install detail Drag receive

	$tree notify install event Edit
	$tree notify install detail Edit accept

	bind $tree <KeyPress> [mymethod search_text %A]
	bind $tree <Return> "[mymethod execute_select] ; break"
	bind $tree <ButtonRelease-1> "[bind TreeCtrl <ButtonRelease-1>]
		[mymethod execute_select %x %y] ; break"
	bind $tree <Double-1> [mymethod execute_select %x %y double]
	bind $tree <<ContextualPress>> {
	    set id ""
	    foreach "type id" [%W identify %x %y] break
	    if { $type != "item" || $id == "" } { return }
	    %W selection clear all
	    %W selection add $id
	}
    }
    method search_text { char } {

	if { $char eq "\t" } { return }
	if { [string is wordchar -strict $char] || [string is punct -strict $char] \
		 || [string is space -strict $char] } {
	    if { ![info exists searchstring] || [string index $searchstring end] != $char } {
		append searchstring $char
	    }
	    if { [info exists searchstring_reached_end] && $searchstring_reached_end ne "" \
		&& $searchstring_reached_end eq $searchstring } {
		set id "first visible"
	    } elseif { [$tree compare active == "active bottom"] } {
		set id "first visible"
	    } else { set id "active below" }
	    set found 0
	    while { $id != "" } {
		set txt [$tree item text $id 0]
		if { [string match -nocase $searchstring* $txt] } {
		    set found 1
		    break
		}
		set id [$tree index "$id below"]
	    }
	    if { !$found } {
		bell
		set searchstring_reached_end $searchstring
		set searchstring ""
		after 300 [list set [varname searchstring_reached_end] ""]
	    } else {
		$tree activate $id
		$tree selection clear all
		$tree selection add active
		after 300 [list set [varname searchstring] ""]
	    }
	}
    }
    method activate_select { name { parent 0 } } {
	foreach id [$tree item children $parent] {
	    if { [$tree item text $id 0] eq $name } {
		$tree activate $id
		$tree selection clear all
		$tree selection add active

		if { [$tree item numchildren $id] == 0 } {
		    $tree collapse all
		    foreach i [$tree item ancestors $id] { $tree expand $i }
		    $tree expand -recurse $id
		}
		return
	    }
	    $self activate_select $name $id
	}
    }
    method set_state_recursive { item stateList } {
	$tree item state set $item $stateList
	foreach i [$tree item children $item] {
	    $self set_state_recursive $i $stateList
	}
    }
    method set_state_up { item } {
	set item [$tree item parent $item]
	if { $item == 0 } { return }
	foreach "haschosen hasnochosen hasemichosen" [list 0 0 0] break
	foreach i [$tree item children $item] {
	    if { [$tree item state get $i semichosen] } {
		set hasemichosen 1
		break
	    } elseif { [$tree item state get $i chosen] } {
		set haschosen 1
	    } else {
		set hasnochosen 1
	    }
	}
	if { $hasemichosen || ($haschosen && $hasnochosen) } {
	    $tree item state set $item "semichosen !chosen"
	} elseif { $haschosen } {
	    $tree item state set $item "chosen !semichosen"
	} else {
	    $tree item state set $item "!chosen !semichosen"
	}
	$self set_state_up $item
    }
    method execute_select { { x "" } { y "" } { simple_or_double simple } } {
	set id [lindex [$tree selection get] 0]
	if { $x ne "" } {
	    set identify [$tree identify $x $y]
	} else { set identify "" }

	if { [lindex $identify 0] eq "item" && [lindex $identify 2] eq "column" && \
	    [lindex $identify 3] == 1 } {
	    set item [lindex $identify 1]
	    if { [$tree item state get $item chosen] } {
		$self set_state_recursive $item "!chosen !semichosen"
	    } else {
		$self set_state_recursive $item "chosen !semichosen"
	    }
	    $self set_state_up $item
	    return
	}

	if { $x ne "" && ([lindex $identify 0] ne "item" || \
	    [lindex $identify 2] ne "column") } { return }

	if { $simple_or_double eq "double" && \
	    [$tree item cget $id -button] } {

	    set isopen [$tree item state get $id open]
	    if { $isopen } {
		$tree collapse -recurse $id
	    } else {
		$tree collapse all
		foreach i [$tree item ancestors $id] {
		    if { $i != 0 } { $tree item expand $i }
		}
		$tree item expand $id -recurse
	    }
	}
	if { $options(-selecthandler) ne "" } {
	    set path ""
	    while { $id != 0 } {
		set path [linsert $path 0 [$tree item element cget $id 0 e3 -text]]
		set id [$tree item parent $id]
	    }
	    uplevel #0 [list $options(-selecthandler) $path]
	}
    }
    method add { parent proc { asparent "" } } {
	set treenode [$tree item create]

	$tree item lastchild $parent $treenode
	
	if { $asparent eq "" } {
	    $tree item style set $treenode 0 s2 1 s3
	    $tree item complex $treenode \
		[list [list e3 -text $proc]]
	} else {
	    $tree item style set $treenode 0 s1 1 s3
	    $tree item complex $treenode \
		[list [list e3 -text $proc]]
	    $tree item configure $treenode -button yes
	    $tree item collapse $treenode
	}

	if { $parent != 0 && ![$tree item cget $parent -button] } {
	    $tree item configure $parent -button yes
	    set name [$tree item text $parent 0]
	    $tree item style set $parent 0 s1 1 s3 2 s3
	    $tree item complex $parent \
		[list [list e3 -text $name]]
	    $tree item collapse $parent
	}
	return $treenode
    }
    method give_chosen_procs { { item 0 } { ns :: } } {
	set ret ""
	foreach i [$tree item children $item] {
	    if { [$tree item style set $i 0] eq "s2" } {
		if { [$tree item state get $i chosen] } {
		    lappend ret $ns[$tree item text $i 0] 
		}
	    } else {
		set newns $ns[$tree item text $i 0]::
		eval lappend ret [$self give_chosen_procs $i $newns]
	    }
	}
	return $ret
    }
    method createimages_colors {} {

	set w [listbox .listbox]
	set SystemButtonFace [$w cget -highlightbackground]
	set SystemHighlight [$w cget -selectbackground]
	set SystemHighlightText [$w cget -selectforeground]
	set SystemFont [$w cget -font]
	destroy $w

	if { [lsearch -exact [image names] img-check-on] != -1 } { return }

	image create photo mac-collapse  -data {
	    R0lGODlhEAAQALIAAAAAAAAAMwAAZgAAmQAAzAAA/wAzAAAzMyH5BAUAAAYA
	    LAAAAAAQABAAggAAAGZmzIiIiLu7u5mZ/8zM/////wAAAAMlaLrc/jDKSRm4
	    OAMHiv8EIAwcYRKBSD6AmY4S8K4xXNFVru9SAgAh/oBUaGlzIGFuaW1hdGVk
	    IEdJRiBmaWxlIHdhcyBjb25zdHJ1Y3RlZCB1c2luZyBVbGVhZCBHSUYgQW5p
	    bWF0b3IgTGl0ZSwgdmlzaXQgdXMgYXQgaHR0cDovL3d3dy51bGVhZC5jb20g
	    dG8gZmluZCBvdXQgbW9yZS4BVVNTUENNVAAh/wtQSUFOWUdJRjIuMAdJbWFn
	    ZQEBADs=
	}
	image create photo mac-expand -data {
	    R0lGODlhEAAQALIAAAAAAAAAMwAAZgAAmQAAzAAA/wAzAAAzMyH5BAUAAAYA
	    LAAAAAAQABAAggAAAGZmzIiIiLu7u5mZ/8zM/////wAAAAMnaLrc/lCB6MCk
	    C5SLNeGR93UFQQRgVaLCEBasG35tB9Qdjhny7vsJACH+gFRoaXMgYW5pbWF0
	    ZWQgR0lGIGZpbGUgd2FzIGNvbnN0cnVjdGVkIHVzaW5nIFVsZWFkIEdJRiBB
	    bmltYXRvciBMaXRlLCB2aXNpdCB1cyBhdCBodHRwOi8vd3d3LnVsZWFkLmNv
	    bSB0byBmaW5kIG91dCBtb3JlLgFVU1NQQ01UACH/C1BJQU5ZR0lGMi4wB0lt
	    YWdlAQEAOw==
	}
	image create photo img-check-on -data {
	    R0lGODlhEAAQAMQAANnZ2YKCgrxdXe4AANnZ2fePj+0BAdVOWdl6fugGBuSgpeimpu0AANvW
	    1up8fOwAANympv///+ttbe0fH+0UFNyrq+TAwOa1tf//////////////////////////////
	    /yH5BAEAAAAALAAAAAAQABAAAAVyICCOZGmeJhiIIxmAgCiCgTiSwgAAACCChDgShXEAAAAE
	    ASkiiQICohgExMI0hPNA0SgGAcEwjfQsUTSKARgQwzBRFRFFICCKQUAMA2MRRBSNYhAQF3MR
	    BBFFoxiAARiJ4wgCohiAkTiSICCOZGmeaGqGADs=
	}
	image create photo img-check-off -data {
	    R0lGODlhEAAQAJEAANnZ2f///4KCgv///yH5BAEAAAAALAAAAAAQABAAAAJAhI+py50UH4Ng
	    U3wUgh1xB0Ii7Ig7EBJhR9yBkAg74g6ERNgRdyAkwo64AyERdsQdCImwI0j+Eewg+UjwMXW3
	    AAA7
	}
	image create photo img-check-grey -data {
	    R0lGODlhEAAQAMQAANnZ2YKCgnl5eUdHR9nZ2a6urkhISHh4eJeXl0pKSrW1tbq6utfX152d
	    nba2tv///5OTk11dXVVVVcvLy8TExP//////////////////////////////////////////
	    /yH5BAEAAAAALAAAAAAQABAAAAVyICCOZGmeJhiIIxmAgCiCgTiSwgAAACCChDgShXEAAAAE
	    ASkiiQICohgExDIwRDM4zygGATEMDDQszzOKARgQwxBJC/E8ICCKQUAMwzARxPOMYhAQ1EAR
	    BPE8oxiAAfiI4wgCohiAjziSICCOZGmeaGqGADs=
	}
	image create photo appbook16 -data {
	    R0lGODlhEAAQAIQAAPwCBAQCBDyKhDSChGSinFSWlEySjCx+fHSqrGSipESO
	    jCR6dKTGxISytIy6vFSalBxydAQeHHyurAxubARmZCR+fBx2dDyKjPz+/MzK
	    zLTS1IyOjAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAVkICCOZGmK
	    QXCWqTCoa0oUxnDAZIrsSaEMCxwgwGggHI3E47eA4AKRogQxcy0mFFhgEW3M
	    CoOKBZsdUrhFxSUMyT7P3bAlhcnk4BoHvb4RBuABGHwpJn+BGX1CLAGJKzmK
	    jpF+IQAh/mhDcmVhdGVkIGJ5IEJNUFRvR0lGIFBybyB2ZXJzaW9uIDIuNQ0K
	    qSBEZXZlbENvciAxOTk3LDE5OTguIEFsbCByaWdodHMgcmVzZXJ2ZWQuDQpo
	    dHRwOi8vd3d3LmRldmVsY29yLmNvbQA7
	}
	image create photo contents-16 -data {
	    R0lGODlhEAAQAIUAAPwCBAQCBExCNGSenHRmVCwqJPTq1GxeTHRqXPz+/Dwy
	    JPTq3Ny+lOzexPzy5HRuVFSWlNzClPTexIR2ZOzevPz29AxqbPz6/IR+ZDyK
	    jPTy5IyCZPz27ESOjJySfDSGhPTm1PTizJSKdDSChNzWxMS2nIR6ZKyijNzO
	    rOzWtIx+bLSifNTGrMy6lIx+ZCRWRAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAae
	    QEAAQCwWBYJiYEAoGAFIw0E5QCScAIVikUgQqNargtFwdB9KSDhxiEjMiUlg
	    HlB3E48IpdKdLCxzEAQJFxUTblwJGH9zGQgVGhUbbhxdG4wBHQQaCwaTb10e
	    mB8EBiAhInp8CSKYIw8kDRSfDiUmJ4xCIxMoKSoRJRMrJyy5uhMtLisTLCQk
	    C8bHGBMj1daARgEjLyN03kPZc09FfkEAIf5oQ3JlYXRlZCBieSBCTVBUb0dJ
	    RiBQcm8gdmVyc2lvbiAyLjUNCqkgRGV2ZWxDb3IgMTk5NywxOTk4LiBBbGwg
	    cmlnaHRzIHJlc2VydmVkLg0KaHR0cDovL3d3dy5kZXZlbGNvci5jb20AOw==
	}
	image create photo document2-16 -data {
	    R0lGODlhEAAQAIUAAPwCBFxaXNze3Ly2rJSWjPz+/Ozq7GxqbJyanPT29HRy
	    dMzOzDQyNIyKjERCROTi3Pz69PTy7Pzy7PTu5Ozm3LyqlJyWlJSSjJSOhOzi
	    1LyulPz27PTq3PTm1OzezLyqjIyKhJSKfOzaxPz29OzizLyidIyGdIyCdOTO
	    pLymhOzavOTStMTCtMS+rMS6pMSynMSulLyedAAAAAAAAAAAAAAAAAAAAAAA
	    AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACH5BAEAAAAALAAAAAAQABAAAAaQ
	    QIAQECgajcNkQMBkDgKEQFK4LFgLhkMBIVUKroWEYlEgMLxbBKLQUBwc52Hg
	    AQ4LBo049atWQyIPA3pEdFcQEhMUFYNVagQWFxgZGoxfYRsTHB0eH5UJCJAY
	    ICEinUoPIxIcHCQkIiIllQYEGCEhJicoKYwPmiQeKisrKLFKLCwtLi8wHyUl
	    MYwM0tPUDH5BACH+aENyZWF0ZWQgYnkgQk1QVG9HSUYgUHJvIHZlcnNpb24g
	    Mi41DQqpIERldmVsQ29yIDE5OTcsMTk5OC4gQWxsIHJpZ2h0cyByZXNlcnZl
	    ZC4NCmh0dHA6Ly93d3cuZGV2ZWxjb3IuY29tADs=
	}
    }

}

proc profileprocs::OpenGUI { { w .profileprocs } } {

    set err [catch { package require treectrl }]

    if { $err } {
	WarnWin [_ "It is necessary to install package 'treectrl' in order to use this function"]
	return
    }

    catch { destroy $w }
    set w [dialogwin_snit $w -title [_ "Profile procs"] \
	    -okname - -morebuttons [list [_ "Start"] [_ "Update"] [_ "Stop"]] \
	    -grab 0 -callback profileprocs::OpenGUI_do]
    set f [$w giveframe]

    NoteBook $f.n1
    $w set_uservar_value notebook $f.n1

    set p1 [$f.n1 insert end procs -text [_ "Select procs"]]

    procstree $p1.t -width 500 -height 400
    $w set_uservar_value procstree $p1.t

    grid $p1.t -sticky nsew -pady 2
    grid columnconfigure $p1 0 -weight 1
    grid rowconfigure $p1 0 -weight 1

    set p2 [$f.n1 insert end results -text [_ "Results"]]

    tablestree $p2.t -width 500 -height 400
    $w set_uservar_value tablestree $p2.t

    grid $p2.t -sticky nsew -pady 2
    grid columnconfigure $p2 0 -weight 1
    grid rowconfigure $p2 0 -weight 1

    grid $f.n1 -sticky nsew -pady 2
    grid columnconfigure $f 0 -weight 1
    grid rowconfigure $f 0 -weight 1

    $f.n1 compute_size
    $f.n1 raise procs

    $w focusok
    bind $w <Return> [list $w invokebutton 3]

    set action [$w createwindow]
}

proc profileprocs::OpenGUI_do { w } {

    switch -- [$w giveaction] {
	-1 - 0 { destroy $w }
	2 {
	    set nt [$w give_uservar_value notebook]
	    $nt raise procs
	    set t [$w give_uservar_value procstree]
	    StartProfiling [$t give_chosen_procs]
	    set tt [$w give_uservar_value tablestree]
	    $tt item delete all
	}
	3 {
	    set nt [$w give_uservar_value notebook]
	    $nt raise results
	    FillTree [$w give_uservar_value tablestree]
	}
	4 {
	    profileprocs::EndProfiling
	}
    }
}

proc profileprocs::_addrecursivepercent { t parent } {

    if { $parent != 0 && [set totaltime [$t item text $parent 1]] != 0 } {
	foreach i [$t item children $parent] {
	    set time [$t item text $i 1]
	    set percent [format %.3g [expr {$time*100.0/$totaltime}]]
	    $t addpercent $i $percent
	}
    }
    foreach i [$t item children $parent] {
	_addrecursivepercent $t $i
    }
}

proc profileprocs::FillTree { t } {

    array set datatimes [RamDebugger::EvalRemoteAndReturn [list array get ::RDC::datatimes]]

    $t item delete all
    set dt ""
    foreach fname [array names datatimes] {
	lappend dt [list [llength $fname] $fname]
    }
    foreach i [lsort -index 0 -integer $dt] {
	set fname [lindex $i 1]
	set parentnames [lrange $fname 0 end-1]
	set sname [lindex $fname end]
	if { ![info exists ids($parentnames)] } {
	    set parent 0
	} else {
	    set parent $ids($parentnames)
	}
	foreach "time num" $datatimes($fname) break
	set ids($fname) [$t add $parent [string trim $sname :] $time $num]
    }
    _addrecursivepercent $t 0
}



























