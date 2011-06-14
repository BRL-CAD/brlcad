
proc RamDebugger::UpdateNumbersInGiDFiles { { dowarn 1 } } {
    variable text

    set inum 0
    set inumdiff 0
    set idx 1.0
    while 1 {
	set idx [$text search -forwards -regexp {(?i)^\s*NUMBER:\s*\d+.*} $idx end]
	if { $idx == "" } { break }
	set txt [$text get "$idx linestart" "$idx lineend"]
	incr inum
	regexp {(?i)^(\s*NUMBER:\s*)(\d+)(.*)} $txt {} {} old_num
	if { $old_num != $inum } {
	    regsub {(?i)^(\s*NUMBER:\s*)(\d+)(.*)} $txt "\\1$inum\\3" txt
	    $text delete "$idx linestart" "$idx lineend"
	    $text insert "$idx linestart" $txt
	    incr inumdiff
	}
	set idx [$text index "$idx lineend"]
    }
    if { $dowarn } { WarnWin "There were $inum numbers. $inumdiff changed" }
}

proc RamDebugger::SelectBasLoop {} {
    variable text

    set line [string tolower [$text get "insert linestart" "insert lineend"]]

    if { [regexp {^\*(if|for|loop)\M} $line] } {
	set level 0
	set idx "insert lineend"
	set reverse 0
	while 1 {
	    set idxA [$text search -forwards -regexp {(?i)^\*(if|for|loop)\M} $idx end]
	    set idxB [$text search -forwards -regexp {(?i)^\*(end|endif|endfor)\M} $idx end]
	    if { $idxA != "" && ($idxB == "" || [$text compare $idxA < $idxB]) } {
		incr level
		set idx "$idxA lineend"
	    } elseif { $idxB != "" && ($idxA == "" || [$text compare $idxA > $idxB]) } {
		if { $level == 0 } {
		    set idxend "$idxB lineend"
		    break
		}
		incr level -1
		set idx "$idxB lineend"
	    } else { 
		WarnWin "Command *end not found"
		return
	    }
	}
    } elseif { [regexp {^\*(end|endif|endfor)\M} $line] } {
	set level 0
	set idx "insert linestart"
	set reverse 1
	while 1 {
	    set idxA [$text search -backwards -regexp {(?i)^\*(if|for|loop)\M} $idx 1.0]
	    set idxB [$text search -backwards -regexp {(?i)^\*(end|endif|endfor)\M} $idx 1.0]
	    if { $idxB != "" && ($idxA == "" || [$text compare $idxA < $idxB]) } {
		incr level
		set idx "$idxB linestart"
	    } elseif { $idxA != "" && ($idxB == "" || [$text compare $idxA > $idxB]) } {
		if { $level == 0 } {
		    set idxend $idxA
		    break
		}
		incr level -1
		set idx "$idxA linestart"
	    } else { 
		WarnWin "Couldn't find beginning of loop"
		return
	    }
	}
    } else {
	WarnWin "This commands requires the cursor in a line with: *loop, *for *if or *end"
	return
    }
    $text tag remove sel 1.0 end
    if { !$reverse } {
	$text tag add sel "insert linestart" $idxend
    } else {
	$text tag add sel $idxend "insert lineend" 
    }
    $text see $idxend
}

namespace eval RamDebugger::Wizard {
    variable parentwidget
    variable image
    variable title
    variable basetext
}

proc RamDebugger::Wizard::EnterInitialData { _parentwidget _image _title _basetext } {
    variable parentwidget $_parentwidget image $_image title $_title basetext $_basetext
}

proc RamDebugger::Wizard::OpenPage { FillCmd hasprevious hasnext } {
    variable image
    variable parentwidget
    variable title
    variable basetext
    
    catch { DialogWin::DestroyWindow }
    if { $hasprevious && $hasnext } {
	set f [DialogWin::Init $parentwidget $title separator [list "Next >>"] "<< Prev" Close]
    } elseif {!$hasprevious && $hasnext } {
	set f [DialogWin::Init $parentwidget $title separator "" "Next >>" Close]
    } elseif {$hasprevious && !$hasnext } {
	set f [DialogWin::Init $parentwidget $title separator [list "Finish"] "<< Prev" Close]
    } else {
	set f [DialogWin::Init $parentwidget $title separator "" "Finish" Close]
    }
    set wf [winfo toplevel $f]

    frame $f.f1 -grid 0
    label $f.f1.l1 -text $basetext -wraplength [image width $image] -justify left -grid 0
    label $f.f1.l2 -image $image -grid 0

    frame $f.f2 -grid "1 px3 py3"

    eval $FillCmd $f.f2

    bind $wf <Return> "DialogWin::InvokeOK"

    supergrid::go $f

    set action [DialogWin::CreateWindow "" 450 320]

    switch $action {
	0 {
	    DialogWin::DestroyWindow
	    return cancel
	}
	1 {
	    if { $hasprevious } {
		return prev
	    } else {
		if { !$hasnext } { DialogWin::DestroyWindow }
		return next
	    }
	}
	2 {
	    if { !$hasnext } { DialogWin::DestroyWindow }
	    return next
	}
    }
}

proc RamDebugger::Wizard::CondMatPage1 { what f } {
    
    switch $what {
	condition { set names $DialogWin::user(CONDNAMES) }
	material { set names $DialogWin::user(MATNAMES) }
    }
    for { set i 0 } { $i < 100 } { incr i } {
	if { [lsearch $names $what$i] == -1 } {
	    set DialogWin::user(NAME) $what$i
	    break
	}
    }
    if { ![info exists DialogWin::user(CONDTYPE)] } {
	set DialogWin::user(CONDTYPE) "over points"
    }
    if { ![info exists DialogWin::user(CONDMESHTYPE)] } {
	set DialogWin::user(CONDMESHTYPE) "over nodes"
    }

    labelframe $f.f1 -text "$what basics" -grid 0
    label $f.f1.l0 -text "[string totitle $what] name:" -grid "0 e"
    ComboBox $f.f1.cb0 -textvariable DialogWin::user(NAME) \
	-values $names -grid "1 px2"
    if { $what == "condition" } {
	label $f.f1.l1 -text "Geometry assign:" -grid "0 e py2"
	ComboBox $f.f1.cb1 -textvariable DialogWin::user(CONDTYPE) \
	    -values [list "over points" "over lines" "over surface" "over volumes" \
		         "over layers"] -grid "1 px2" -editable 0
	label $f.f1.l2 -text "Mesh assign:" -grid "0 e py2"
	ComboBox $f.f1.cb12 -textvariable DialogWin::user(CONDMESHTYPE) \
	    -values [list "over nodes" "over body elements" "over face elements"] \
	    -grid "1 px2" -editable 0
    }
    ::tk::TabToWindow $f.f1.cb0
}

proc RamDebugger::Wizard::ModCondMatPage2 { what f } {

    foreach i [winfo children $f] { destroy $i }
    set n $DialogWin::user(FIELDNUM)
    switch $DialogWin::user(FIELDTYPE,$n) {
	normal {
	    label $f.l1 -text "Default value:" -grid 0
	    entry $f.e1 -textvariable DialogWin::user(DEFAULTVALUE,$n) -grid 1
	}
	combobox - matrix {
	    if { $DialogWin::user(FIELDTYPE,$n) eq "combobox" } {
		label $f.l1 -text "Default value:" -grid 0
	    } else {
		label $f.l1 -text "Column names:" -grid 0
	    }
	    ComboBox $f.cb1 -textvariable DialogWin::user(DEFAULTVALUE,$n) -grid 1
	    set DialogWin::user(FIELDVALUES,$n) ""
	    set bbox [ButtonBox $f.bbox1 -spacing 0 -padx 1 -pady 1 -homogeneous 1 -grid "2 w"]
	    $bbox add -image acttick16 \
		-highlightthickness 0 -takefocus 0 -relief link -borderwidth 1 -padx 1 -pady 1 \
		-helptext "Add new entry" \
		-command [string map [list %W $f.cb1 %n $n] {
		    if { [lsearch $DialogWin::user(FIELDVALUES,%n) $DialogWin::user(DEFAULTVALUE,%n)] != -1 } {
		        WarnWin "Entry to enter already inside"
		    } elseif { [regexp {[,()]} $DialogWin::user(DEFAULTVALUE,%n)] } {
		        WarnWin "Entry cannot contain characters: ,()"
		    } else {
		        lappend DialogWin::user(FIELDVALUES,%n) $DialogWin::user(DEFAULTVALUE,%n)
		        %W configure -values $DialogWin::user(FIELDVALUES,%n)
		    }
		}]
	    $bbox add -image actcross16 \
		-highlightthickness 0 -takefocus 0 -relief link -borderwidth 1 -padx 1 -pady 1 \
		-helptext [_ "Delete active entry"] \
		-command [string map [list %W $f.cb1 %n $n] {
		    set ipos [lsearch $DialogWin::user(FIELDVALUES,%n) $DialogWin::user(DEFAULTVALUE,%n)]
		    if { $ipos == -1 } {
		        WarnWin "Entry to delete not valid"
		    } else {
		        set DialogWin::user(FIELDVALUES,%n) [lreplace $DialogWin::user(FIELDVALUES,%n)\
		                $ipos $ipos]
		        %W configure -values $DialogWin::user(FIELDVALUES,%n)
		    }
		}]
	}
	localaxes {
	    set ic 0
	    foreach i [list global automatic automatic_alt] {
		checkbutton $f.l$ic -text "$i axes:" -grid "0 w" -variable DialogWin::user(AXESTYPE,$i,$n) \
		    -command [string map [list %i $i %n $n %e $f.e$ic %c $f.cb4] {
		        if { $DialogWin::user(AXESTYPE,%i,%n) } {
		            %e configure -state normal -fg black
		        } else { %e configure -state disabled -fg grey }

		        set values ""
		        foreach i [list global automatic automatic_alt] {
		            if { $DialogWin::user(AXESTYPE,$i,%n) } { lappend values $i }
		        }
		        %c configure -values $values
		        set DialogWin::user(DEFAULTVALUE,%n) [lindex $values 0]
		    }]
		if { ![info exists DialogWin::user(AXESNAME,$i,$n)] } {
		    set DialogWin::user(AXESNAME,$i,$n) $i
		}
		entry $f.e$ic -textvariable DialogWin::user(AXESNAME,$i,$n) -grid 1
		incr ic
	    }
	    label $f.l4 -text "Default value:" -grid 0
	    set values ""
	    foreach i [list global automatic automatic_alt] {
		if { $DialogWin::user(AXESTYPE,$i,$n) } { lappend values $i }
	    }
	    ComboBox $f.cb4 -textvariable DialogWin::user(DEFAULTVALUE,$n) -values $values -grid 1
	    set DialogWin::user(DEFAULTVALUE,$n) [lindex $values 0]
	}
    }
    supergrid::go $f
}

proc RamDebugger::Wizard::CondMatPage2 { what f } {
    
    set n $DialogWin::user(FIELDNUM)
    if { ![info exists DialogWin::user(FIELDNAME,$n)] || $n > [llength $DialogWin::user(FIELDNAMES)] } {
	for { set i 0 } { $i < 100 } { incr i } {
	    if { [lsearch $DialogWin::user(FIELDNAMES) field_name$i] == -1 } {
		set DialogWin::user(FIELDNAME,$n) field_name$i
		break
	    }
	}
    }
    if { ![info exists DialogWin::user(DEFAULTVALUE,$n)] } {
	set DialogWin::user(DEFAULTVALUE,$n) default_value
    }
    if { ![info exists DialogWin::user(FIELDTYPE,$n)] } {
	set DialogWin::user(FIELDTYPE,$n) normal
    }

    labelframe $f.f1 -text "field definition" -grid 0
    label $f.f1.l0 -text "Field name:" -grid "0 e"
    ComboBox $f.f1.cb0 -textvariable DialogWin::user(FIELDNAME,$n) \
	-values $DialogWin::user(FIELDNAMES) -grid "1 px2"
    label $f.f1.l1 -text "Field type:" -grid 0
    frame $f.f1.f -grid "0 2 w px20"
    radiobutton $f.f1.f.r1 -text Normal -variable DialogWin::user(FIELDTYPE,$n) \
	-value normal -grid "0 w" -command [list RamDebugger::Wizard::ModCondMatPage2 $what $f.f2]
    radiobutton $f.f1.f.r2 -text Combobox -variable DialogWin::user(FIELDTYPE,$n) \
	-value combobox -grid "0 w" -command [list RamDebugger::Wizard::ModCondMatPage2 $what $f.f2]
    if { $what == "condition" } {
	radiobutton $f.f1.f.r3 -text "Local Axes" -variable DialogWin::user(FIELDTYPE,$n) \
	    -value localaxes -grid "0 w" -command [list RamDebugger::Wizard::ModCondPage2 $what $f.f2]
    }
    radiobutton $f.f1.f.r4 -text Matrix -variable DialogWin::user(FIELDTYPE,$n) \
	-value matrix -grid "0 w" -command [list RamDebugger::Wizard::ModCondMatPage2 $what $f.f2]

    labelframe $f.f2 -text "default value" -grid 0
    RamDebugger::Wizard::ModCondMatPage2 $what $f.f2

    checkbutton $f.l0 -text "Enter another field" -variable DialogWin::user(anotherfield) -grid "0 w"
    set DialogWin::user(anotherfield) 0

}
proc RamDebugger::Wizard::CondMatPage3 { what f } {

    switch $what {
	condition { set txt "NUMBER: $DialogWin::user(MAXCONDNUM) " }
	material { set txt "NUMBER: $DialogWin::user(MAXMATNUM) " }
    }
    switch $what {
	condition {
	    append txt "CONDITION: $DialogWin::user(NAME)\n"
	    append txt "CONDTYPE: $DialogWin::user(CONDTYPE)\n"
	    append txt "CONDMESHTYPE: $DialogWin::user(CONDMESHTYPE)\n"
	}
	material {
	    append txt "MATERIAL: $DialogWin::user(NAME)\n"
	}
    }
    for { set i 1 } { $i <= $DialogWin::user(FIELDNUM) } { incr i } {
	switch $DialogWin::user(FIELDTYPE,$i) {
	    normal {
		append txt "QUESTION: $DialogWin::user(FIELDNAME,$i)\n"
	    }
	    combobox {
		set fo [join $DialogWin::user(FIELDVALUES,$i) ","]
		append txt "QUESTION: $DialogWin::user(FIELDNAME,$i)#CB#($fo)\n"
	    }
	    localaxes {
		set values ""
		foreach "j post" [list global "#G#" automatic "#A#" automatic_alt "#L#"] {
		    if { $DialogWin::user(AXESTYPE,$j,$i) } {
		        lappend values $DialogWin::user(AXESNAME,$j,$i)$post
		    }
		}
		set fo [join $values ","]
		append txt "QUESTION: $DialogWin::user(FIELDNAME,$i)#LA#($fo)\n"
	    }
	    matrix {
		set fo [join $DialogWin::user(FIELDVALUES,$i) ","]
		append txt "QUESTION: $DialogWin::user(FIELDNAME,$i)($fo)\n"
	    }
	}
	if { $DialogWin::user(FIELDTYPE,$i) != "matrix" } {
	    append txt "VALUE: $DialogWin::user(DEFAULTVALUE,$i)\n"
	} else {
	    set len_fo [llength $DialogWin::user(FIELDVALUES,$i)]
	    append txt "VALUE: #N# $len_fo [lrepeat $len_fo 0.0]\n"
	}
    }
    switch $what {
	condition { append txt "END CONDITION" }
	material { append txt "END MATERIAL" }
    }
    set DialogWin::user(CONTTXT) $txt

    labelframe $f.f1 -text "generated $what" -grid 0
    label $f.f1.l0 -text "The generated $what is the following. Proceed?" -grid 0
    set sw [ScrolledWindow $f.f1.lf -relief sunken -borderwidth 0 -grid 0]
    text $sw.t -background white -width 30 -height 6 -wrap none
    $sw setwidget $sw.t
    $sw.t insert end $txt
    $sw.t configure -state disabled
    bind $sw.t <1> "focus $sw.t"
}

proc RamDebugger::Wizard::CondMatWizard { text filename } {
    variable nice_image

    switch -- [file extension $filename] {
	.cnd { set what condition }
	.mat { set what material }
	default {
	    WarnWin "File '$filename' must be a GiD CND or a MAT file"
	    return
	}
    }

    set idx [$text index insert]
    if { [string trim [$text get "$idx linestart" "$idx lineend"]] != "" } {
	WarnWin "Insertion cursor must be in a blank line"
	return
    }
    if { ![regexp {(END\s+(CONDITION|MATERIAL)\s+|^\s*)$} [$text get 1.0 $idx]] } {
	WarnWin "Insertion cursor must be at the beginning of the file or after one $what"
	return
    }

    set txt [$text get 1.0 end-1c]

    switch $what {
	condition {
	    set DialogWin::user(CONDNAMES) ""
	    set DialogWin::user(MAXCONDNUM) 0
	    foreach "- num cnd" [regexp -inline -all {(?in)^\s*NUMBER:\s*(\d+)\s+CONDITION:\s*(\S+)} $txt] {
		lappend DialogWin::user(CONDNAMES) $cnd
		if { $num > $DialogWin::user(MAXCONDNUM) } { set DialogWin::user(MAXCONDNUM) $num }
	    }
	    incr DialogWin::user(MAXCONDNUM)
	    set names $DialogWin::user(CONDNAMES)
	}
	material {
	    set DialogWin::user(MATNAMES) ""
	    set DialogWin::user(MAXMATNUM) 0
	    foreach "- num mat" [regexp -inline -all {(?in)^\s*NUMBER:\s*(\d+)\s+MATERIAL:\s*(\S+)} $txt] {
		lappend DialogWin::user(MATNAMES) $mat
		if { $num > $DialogWin::user(MAXMATNUM) } { set DialogWin::user(MAXMATNUM) $num }
	    }
	    incr DialogWin::user(MAXMATNUM)
	    set names $DialogWin::user(MATNAMES)
	}
    }
    set DialogWin::user(FIELDNAMES) ""
    foreach "- f" [regexp -inline -all {(?in)^\s*QUESTION:\s*([-\w]+)} $txt] {
	lappend DialogWin::user(FIELDNAMES) $f
    }
    EnterInitialData $text $nice_image "Create $what" "Enter data to define $what:"
    set numlevels [llength [info commands ::RamDebugger::Wizard::CondMatPage*]]
    set level 1
    while 1 {
	switch $level 1 { set hasprevious 0 } default { set hasprevious 1 }
	switch $level $numlevels { set hasnext 0 } default { set hasnext 1 }
   
	set retval [RamDebugger::Wizard::OpenPage [list RamDebugger::Wizard::CondMatPage$level $what] \
		$hasprevious $hasnext]
	switch $retval {
	    cancel { return }
	    prev {
		if { $level == 2 && $DialogWin::user(FIELDNUM) > 1 } {
		    incr DialogWin::user(FIELDNUM) -1
		} else {
		    incr level -1
		}
	    }
	    next {
		switch $level {
		    1 {
		        set DialogWin::user(NAME) [string trim $DialogWin::user(NAME)]
		        regsub -all {\s+} $DialogWin::user(NAME) {_} DialogWin::user(NAME)
		        if { $DialogWin::user(NAME) == "" } {
		            WarnWin "[string totitle $what] name is void. Fill it, please"
		            continue
		        }
		        if { [lsearch $names $DialogWin::user(NAME)] != -1 } {
		            WarnWin "Repeated $what name '$DialogWin::user(NAME)'"
		            continue
		        }
		        set DialogWin::user(FIELDNUM) 1
		        set DialogWin::user(FIELDNAMES) ""
		        incr level
		    }
		    2 {
		        set n $DialogWin::user(FIELDNUM)
		        set DialogWin::user(FIELDNAME,$n) [string trim $DialogWin::user(FIELDNAME,$n)]
		        regsub -all {\s+} $DialogWin::user(FIELDNAME,$n) {_} DialogWin::user(FIELDNAME,$n)
		        if { $DialogWin::user(FIELDNAME,$n) == "" } {
		            WarnWin "[string totitle $what] field is void. Fill it, please"
		            continue
		        }
		        set DialogWin::user(DEFAULTVALUE,$n) [string trim $DialogWin::user(DEFAULTVALUE,$n)]
		        regsub -all {\s+} $DialogWin::user(DEFAULTVALUE,$n) {_} DialogWin::user(DEFAULTVALUE,$n)
		        if { $DialogWin::user(DEFAULTVALUE,$n) == "" && $DialogWin::user(FIELDTYPE,$n) != "matrix" } {
		            WarnWin "[string totitle $what] default value is void. Fill it, please"
		            continue
		        }
		        if { [llength $DialogWin::user(FIELDNAMES)] < $n && \
		            [lsearch $DialogWin::user(FIELDNAMES) $DialogWin::user(FIELDNAME,$n)] != -1 } {
		            WarnWin "Repeated field name '$DialogWin::user(FIELDNAME,$n)'"
		            continue
		        }
		        switch $DialogWin::user(FIELDTYPE,$n) {
		            combobox {
		                if { [lsearch $DialogWin::user(FIELDVALUES,$n) $DialogWin::user(DEFAULTVALUE,$n)] \
		                    == -1 } {
		                    set val $DialogWin::user(DEFAULTVALUE,$n)
		                    WarnWin "Default value: '$val' is not contained in options"
		                    continue
		                }
		            }
		            matrix {
		                if { [llength $DialogWin::user(FIELDVALUES,$n)] < 1 } {
		                    WarnWin "There are no column names for matrix field type"
		                    continue
		                }
		            }
		            localaxes {
		                set values ""
		                foreach i [list global automatic automatic_alt] {
		                    if { $DialogWin::user(AXESTYPE,$i,$n) } { lappend values $i }
		                }
		                if { [llength $values] == 0 } {
		                    WarnWin "At least an option in local axes must be selected"
		                    continue
		                }
		            }
		        }
		        if { [llength $DialogWin::user(FIELDNAMES)] < $n } {
		            lappend DialogWin::user(FIELDNAMES) $DialogWin::user(FIELDNAME,$n)
		        }
		        if { $DialogWin::user(anotherfield) } {
		            incr DialogWin::user(FIELDNUM)
		        } else { incr level }
		    }
		    3 {
		        incr level
		    }
		    default {
		        error "field num $level unknown"
		    }
		}
	    }
	}
	if { $level > $numlevels } { break }
    }
    catch { DialogWin::DestroyWindow }

    $text insert insert $DialogWin::user(CONTTXT)
    UpdateNumbersInGiDFiles 0
}

namespace eval RamDebugger::Wizard {
    package require img::png
    package require img::jpeg
    package require img::gif
    
    set nice_image [image create photo -data {
/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRof
Hh0aHBwgJC4nICIsIxwcKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwh
MjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAAR
CACrAKsDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAA
AgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkK
FhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWG
h4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl
5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREA
AgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYk
NOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOE
hYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk
5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDvKKKKYgooooAKKKKADtRR2ooAKKKKACii
koAWiiigAooooAKKKKADtRR2ooAKSlpKAFooooAVVLMFUZY9BUv2K6z/AKhqLL/j/h+tdWzN
uPNAHKfY7r/ng1H2O6/54NXVbm9aNzetIZyv2O6/54NR9juv+eDV1W5vWjc3rQByv2O6/wCe
DUfY7r/ng1dVub1o3N60Acr9juv+eDUfY7r/AJ4NXVbm9aNzetAHK/Y7r/ng1H2O6/54NXVb
m9aNzetAHK/Y7r/ng1H2O6/54NXVbm9aNzetAHK/Y7r/AJ4NR9juv+eDV1W5qMt7/lQByMkU
kTbZEKt6Gm1o64Sb9c/3azqYgpKWkoAWiiigCaz/AOP6H611TfeNcrZ/8f0P1rpbi6gtjmaV
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
	}]
}
