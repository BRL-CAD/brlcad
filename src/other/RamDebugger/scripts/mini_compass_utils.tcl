
package require snit

proc info_fullargs { procname } {
    set ret ""
    foreach arg [uplevel 1 [list info args $procname]] {
	if { [uplevel 1 [list info default $procname $arg value]] } {
	    upvar 1 value value
	    lappend ret [list $arg $value]
	} else {
	    lappend ret $arg
	}
    }
    return $ret
}

namespace eval cu {}

# for tclIndex to work 
proc cu::menubutton_button { args } {}


snit::widgetadaptor cu::menubutton_button {
    option -command ""
    option -image ""
    option -text ""

    delegate method * to hull
    delegate option * to hull
    delegate option -_image to hull as -image
    delegate option -_text to hull as -text

    variable is_button_active 1
    
    constructor args {
	installhull using ttk::menubutton -style Toolbutton
	bind $win <ButtonPress-1> [mymethod BP1 %x %y]
	bind $win <ButtonRelease-1> [mymethod BR1 %x %y]

	$self configurelist $args
    }
    onconfigure -image {img} {
	set options(-image) $img

	if { $options(-text) ne "" } {
	    $self configure -_image $img
	    return
	} elseif { $img ne "" } {
	    set width [image width $img]
	    set height [image height $img]
	} else { foreach "width height" [list 0 16] break }

	set new_img [image create photo -width [expr {$width+7}] -height $height]
	if { $img ne "" } { $new_img copy $img -to 0 0 }
	set coords {
	    -3 -1
	    -4 -2 -3 -2 -2 -2
	    -5 -3 -4 -3 -3 -3 -2 -3 -1 -3
	}
	foreach "x y" $coords {
	    $new_img put black -to [expr {$width+7+$x}] [expr {$height+$y}]
	}
	$self configure -_image $new_img
	bind $win <Destroy> +[list image delete $new_img]
    }
    onconfigure -text {value} {
	set options(-text) $value

	if { $options(-text) ne "" } {
	    $self configure -style ""
	    if { $options(-image) ne "" } {
		$self configure -_image $options(-image)
	    }
	}
	$self configure -_text $value
    }
    method give_is_button_active_var {} {
	return [myvar is_button_active]
    }
    method BP1 { x y } {
	if { !$is_button_active } { return }
	if { $x < [winfo width $win]-10 && $options(-command) ne "" } {
	    $win instate !disabled {
		catch { tile::clickToFocus $win }
		catch { ttk::clickToFocus $win }
		$win state pressed
	    }
	    return -code break
	}
    }
    method BR1 { x y } {
	if { !$is_button_active } { return }
	if { $x < [winfo width $win]-10 && $options(-command) ne "" } {
	    $win instate {pressed !disabled} {
		$win state !pressed
		uplevel #0 $options(-command)
	    } 
	    return -code break
	}
    }
}

snit::widgetadaptor cu::combobox {
    option -valuesvariable ""
    option -textvariable ""
    option -statevariable ""
    option -values ""
    option -dict ""
    option -dictvariable ""

    variable _translated_textvariable ""

    delegate method * to hull
    delegate option * to hull
    delegate option -_values to hull as -values
    delegate option -_textvariable to hull as -textvariable

    constructor args {
	installhull using ttk::combobox

	cu::add_contextual_menu_to_entry $win init
	bind $win <<ComboboxSelected>> [mymethod combobox_selected]
	$self configurelist $args
    }
    destructor {
	catch {
	    if { $options(-valuesvariable) ne "" } {
		upvar #0 $options(-valuesvariable) v
		trace remove variable v write "[mymethod _changed_values_var];#"
	    }
	    if { $options(-dictvariable) ne "" } {
		upvar #0 $options(-dictvariable) v
		trace remove variable v write "[mymethod _changed_values_var];#"
	    }
	    if { $options(-textvariable) ne "" } {
		upvar #0 $options(-textvariable) v
		trace remove variable v write "[mymethod _written_textvariable];#"
	    }
	    if { $options(-statevariable) ne "" } {
		upvar #0 $options(-statevariable) v
		trace remove variable v write "[mymethod _written_statevariable];#"
		trace remove variable v read "[mymethod _read_statevariable];#"
	    }
	}
    }
    onconfigure -textvariable {value} {
	set options(-textvariable) $value
	$self configure -_textvariable [myvar _translated_textvariable]

	upvar #0 $options(-textvariable) v
	trace add variable v write "[mymethod _written_textvariable];#"
	trace add variable [myvar _translated_textvariable] write \
	    "[mymethod _read_textvariable];#"
	if { [info exists v] } {
	    $self _written_textvariable
	}
    }
    onconfigure -dictvariable {value} {
	set options(-dictvariable) $value
	$self _changed_values_var
	upvar #0 $options(-dictvariable) v
	trace add variable v write "[mymethod _changed_values_var];#"
    }
    onconfigure -statevariable {value} {
	set options(-statevariable) $value

	upvar #0 $options(-statevariable) v
	trace add variable v write "[mymethod _written_statevariable];#"
	trace add variable v read "[mymethod _read_statevariable];#"
	if { [info exists v] } {
	    set v $v
	}
    }
    onconfigure -valuesvariable {value} {
	set options(-valuesvariable) $value

	upvar #0 $options(-valuesvariable) v

	if { $options(-dictvariable) ne "" } {
	    upvar #0 $options(-dictvariable) vd
	    if { [info exists vd] } {
		set dict $vd
	    } else {
		set dict ""
	    }
	} else {
	    set dict $options(-dict)
	}
	if { ![info exists v] } {
	    set v ""
	    foreach value [$self cget -_values] {
		catch { 
		    set value [dict get [dict_inverse $dict] $value]
		}
		lappend v $value
	    }
	} else {
	    set vtrans ""
	    foreach value $v {
		catch { set value [dict get $dict $value] }
		lappend vtrans $value
	    }
	    $self configure -_values $vtrans
	}
	trace add variable v write "[mymethod _changed_values_var];#"
    }
    onconfigure -dict {value} {
	set options(-dict) $value
	$self _changed_values_var
    }
    onconfigure -values {values} {
	if { $options(-valuesvariable) ne "" } {
	    upvar #0 $options(-valuesvariable) v
	    set v $values
	} else {
	    if { $options(-dictvariable) ne "" } {
		upvar #0 $options(-dictvariable) vd
		if { [info exists vd] } {
		    set dict $vd
		} else {
		    set dict ""
		}
	    } else {
		set dict $options(-dict)
	    }
	    set vtrans ""
	    foreach value $values {
		catch { set value [dict get $dict $value] }
		lappend vtrans $value
	    }
	    $self configure -_values $vtrans
	}
    }
    oncget -values {
	set v ""
	foreach value [$self cget -_values] {
#             catch {
#                 set value [dict get [dict_inverse $options(-dict)] $value]
#             }
	    lappend v $value
	}
	return $v
    }
    method _changed_values_var {} {
	if { $options(-valuesvariable) ne "" } {
	    upvar #0 $options(-valuesvariable) v
	} else {
	    set v [$self cget -values]
	}
	if { $options(-dictvariable) ne "" } {
	    upvar #0 $options(-dictvariable) vd
	    if { [info exists vd] } {
		set dict $vd
	    } else {
		set dict ""
	    }
	} else {
	    set dict $options(-dict)
	}
	set vtrans ""
	foreach value $v {
	    catch { set value [dict get $dict $value] }
	    lappend vtrans $value
	}
	$self configure -_values $vtrans
	$self _written_textvariable
    }
    method _written_textvariable { args } {

	set optional {
	    { -force_dict "" 0 }
	}
	set compulsory ""
	parse_args $optional $compulsory $args

	upvar #0 $options(-textvariable) v
	if { ![info exists v] } { return }
	set value $v
	if { $options(-dictvariable) ne "" } {
	    upvar #0 $options(-dictvariable) vd
	    if { [info exists vd] } {
		set dict $vd
	    } else {
		set dict ""
	    }
	} else {
	    set dict $options(-dict)
	}
	if { $force_dict || [$self instate readonly] } {
	    catch { set value [dict get $dict $value] }
	}
	if { $_translated_textvariable ne $value } {
	    set _translated_textvariable $value
	}
    }
    method _read_textvariable {} {
	upvar #0 $options(-textvariable) v
	set value $_translated_textvariable
	if { $options(-dictvariable) ne "" } {
	    upvar #0 $options(-dictvariable) vd
	    if { [info exists vd] } {
		set dict $vd
	    } else {
		set dict ""
	    }
	} else {
	    set dict $options(-dict)
	}
	catch {
	    set value [dict get [dict_inverse $dict] $value]
	}
	if { ![info exists v] || $v ne $value } {
	    set v $value
	}
    }
    method _written_statevariable {} {
	upvar #0 $options(-statevariable) v
	$self state $v
    }
    method _read_statevariable {} {
	upvar #0 $options(-statevariable) v
	set v [$self state]
    }
    method combobox_selected {} {
	if { ![$self instate readonly] } {
	    $self _written_textvariable -force_dict
	}
    }
}

################################################################################
# cu::multiline_entry
################################################################################

snit::widget cu::multiline_entry {
    option -textvariable ""
    option -takefocus 0 ;# option used by the tab standard bindings
    option -values ""
    option -valuesvariable ""

    hulltype frame

    variable text

    delegate method * to text
    delegate option * to text

    constructor args {

	$hull configure -background #a4b97f -bd 0
	install text using text $win.t -wrap word -bd 0 -width 40 -height 3
	
	cu::add_contextual_menu_to_entry $text init

	grid $text -padx 1 -pady 1 -sticky nsew
	grid columnconfigure $win 0 -weight 1
	grid rowconfigure $win 0 -weight 1

	bind $text <Tab> "[bind all <Tab>] ; break"
	bind $text <<PrevWindow>> "[bind all <<PrevWindow>>] ; break"
	bindtags $text [list $win $text [winfo class $win] [winfo class $text] [winfo toplevel $text] all]
	bind $win <FocusIn> [list focus $text]
	$self configurelist $args
    }
    destructor {
	$self _clean_traces
    }
    onconfigure -textvariable {value} {
	$self _clean_traces
	set options(-textvariable) $value

	set cmd "[mymethod _check_textvariable_read] ;#"
	trace add variable $options(-textvariable) read $cmd
	set cmd "[mymethod _check_textvariable_write] ;#"
	trace add variable $options(-textvariable) write $cmd
    }
    onconfigure -values {value} {
	set options(-values) $value
	
	if { $options(-values) ne "" || $options(-valuesvariable) ne "" } {
	    if { ![winfo exists $win.b] } {
		image create photo cu::multiline_entry::nav1downarrow16 -data {
		    R0lGODlhEAAQAIAAAPwCBAQCBCH5BAEAAAAALAAAAAAQABAAAAIYhI+py+0PUZi0zmTtypflV0Vd
		    RJbm6fgFACH+aENyZWF0ZWQgYnkgQk1QVG9HSUYgUHJvIHZlcnNpb24gMi41DQqpIERldmVsQ29y
		    IDE5OTcsMTk5OC4gQWxsIHJpZ2h0cyByZXNlcnZlZC4NCmh0dHA6Ly93d3cuZGV2ZWxjb3IuY29t
		    ADs=
		}
		ttk::menubutton $win.b -image cu::multiline_entry::nav1downarrow16 -style Toolbutton -menu $win.b.m
		menu $win.b.m -tearoff 0
		grid $win.b -row 0 -column 1 -padx "0 1" -pady 1 -sticky wns
	    } else {
		$win.b.m delete 0 end
	    }
	    $win.b.m add command -label [_ "(Clear)"] -command [mymethod set_text ""]
	    $win.b.m add separator
	    foreach v $value {
		if { [string length $v] > 60 } {
		    set l [string range $v 0 56]...
		} else {
		    set l $v
		}
		$win.b.m add command -label $l -command [mymethod set_text $v]
	    }
	} elseif { ![winfo exists $win.b] } {
	    destroy $win.b
	}
    }
    onconfigure -valuesvariable {value} {
	set options(-valuesvariable) $value

	upvar #0 $options(-valuesvariable) v

	if { [info exists v] } {
	    $self configure -values $v
	}
	trace add variable v write "[mymethod _changed_values_var];#"
    }
    method set_text { txt } {
	$text delete 1.0 end
	$text insert end $txt
	$text tag add sel 1.0 end-1c
	focus $text
    }
    method _clean_traces {} {
	if { $options(-textvariable) ne "" } {
	    set cmd "[mymethod _check_textvariable_read] ;#"
	    trace remove variable $options(-textvariable) read $cmd
	    set cmd "[mymethod _check_textvariable_write] ;#"
	    trace remove variable $options(-textvariable) write $cmd
	}
	if { $options(-valuesvariable) ne "" } {
	    upvar #0 $options(-valuesvariable) v
	    trace remove variable v write "[mymethod _changed_values_var];#"
	}
    }
    method _check_textvariable_read {} {
	upvar #0 $options(-textvariable) v
	set v [$text get 1.0 end-1c]
    }
    method _check_textvariable_write {} {
	upvar #0 $options(-textvariable) v
	$text delete 1.0 end
	$text insert end $v
    }
    method _changed_values_var {} {
	if { $options(-valuesvariable) ne "" } {
	    upvar #0 $options(-valuesvariable) v
	    $self configure -values $v
	}
    }
}

################################################################################
#    add_contextual_menu_to_entry
################################################################################

proc cu::add_contextual_menu_to_entry { w what args } {
    switch $what {
	init {
	    bind $w <ButtonRelease-3> [list cu::add_contextual_menu_to_entry $w post %X %Y]
	}
	post {
	    lassign $args x y
	    set menu $w.menu
	    catch { destroy $menu }
	    menu $menu -tearoff 0
	    foreach i [list cut copy paste --- select_all --- clear] \
		txt [list [_ "Cut"] [_ "Copy"] [_ "Paste"] --- [_ "Select all"] --- [_ "Clear"]] {
		if { $i eq "---" } {
		    $menu add separator
		} else {
		    $menu add command -label $txt -command [list cu::add_contextual_menu_to_entry $w $i]
		}
	    }
	    tk_popup $menu $x $y
	}
	clear {
	    if { [winfo class $w] eq "Text" } {
		$w delete 1.0 end
	    } else {
		$w delete 0 end
	    }
	}
	cut {
	    event generate $w <<Cut>>
	}
	copy {
	    event generate $w <<Copy>>
	}
	paste {
	    event generate $w <<Paste>>
	}
	select_all {
	    if { [winfo class $w] eq "Text" } {
		$w tag add sel 1.0 end-1c
	    } else {
		$w selection range 0 end
	    }
	}
    }
}

################################################################################
#    store preferences
################################################################################

proc cu::store_program_preferences { args } {

    set optional {
	{ -valueName name "" }
    }
    set compulsory "program_name data"

    parse_args $optional $compulsory $args

    if { $valueName eq "" } {
	set valueNameF IniData
    } else {
	set valueNameF IniData_$valueName
    }

    if { $::tcl_platform(platform) eq "windows" && $::tcl_platform(os) ne "Windows CE" } {
	set key "HKEY_CURRENT_USER\\Software\\Compass\\$program_name"
	package require registry
	registry set $key $valueNameF $data
    } else {
	package require tdom
	if { $::tcl_platform(os) eq "Windows CE" } {
	    set dir [file join / "Application Data" Compass $program_name]
	    file mkdir $dir
	    set file [file join $dir prefs]
	} elseif { [info exists ::env(HOME)] } {
	    set file [file normalize ~/.compass_${program_name}_prefs]
	} else {
	    set file [file normalize [file join /tmp compass_${program_name}_prefs]]
	}
	set err [catch { tDOM::xmlReadFile $file } xml]
	if { $err } { set xml "<preferences/>" }
	set doc [dom parse $xml]
	set root [$doc documentElement]
	set domNode [$root selectNodes "pref\[@n=[xpath_str $valueNameF]\]"]
	if { $domNode ne "" } { $domNode delete }
	set p [$root appendChildTag pref]
	$p setAttribute n $valueNameF
	$p appendChildText $data

	set fout [open $file w]
	fconfigure $fout -encoding utf-8
	puts $fout [$doc asXML]
	close $fout
    }
}
proc cu::get_program_preferences { args } {

    set optional {
	{ -valueName name "" }
	{ -default default_value "" }
    }
    set compulsory "program_name"

    parse_args $optional $compulsory $args

    if { $valueName eq "" } {
	set valueNameF IniData
    } else {
	set valueNameF IniData_$valueName
    }

    set data $default
    if { $::tcl_platform(platform) eq "windows" && $::tcl_platform(os) ne "Windows CE" } {
	set key "HKEY_CURRENT_USER\\Software\\Compass\\$program_name"
	package require registry
	set err [catch { registry get $key $valueNameF } data]
	if { $err } {
	    set data $default
	}
    } else {
	package require tdom
	if { $::tcl_platform(os) eq "Windows CE" } {
	    set dir [file join / "Application Data" Compass $program_name]
	    file mkdir $dir
	    set file [file join $dir prefs]
	} elseif { [info exists ::env(HOME)] } {
	    set file [file normalize ~/.compass_${program_name}_prefs]
	} else {
	    set file [file normalize [file join /tmp compass_${program_name}_prefs]]
	}
	set err [catch { tDOM::xmlReadFile $file } xml]
	if { !$err } {
	    set doc [dom parse $xml]
	    set root [$doc documentElement]
	    set domNode [$root selectNodes "pref\[@n=[xpath_str $valueNameF]\]"]
	    if { $domNode ne "" } {
		set data [$domNode text]
	    }
	}
    }
    return $data
}

################################################################################
#    cu::set_window_geometry u::give_window_geometry
################################################################################

proc cu::give_window_geometry { w } {

    regexp {(\d+)x(\d+)([-+])([-\d]\d*)([-+])([-\d]+)} [wm geometry $w] {} width height m1 x m2 y
    if { $::tcl_platform(platform) eq "unix" } {
	# note: this work in ubuntu 9.04
	incr x -4
	incr y -24
    }
    return ${width}x$height$m1$x$m2$y
}

proc cu::set_window_geometry { w geometry } {

    regexp {(\d+)x(\d+)([-+])([-\d]\d*)([-+])([-\d]+)} $geometry {} width height m1 x m2 y
    if { $x < 0 } { set x 0 }
    if { $y < 0 } { set y 0 }
    if { [ tk windowingsystem] eq "aqua" } {
	if { $y < 20 } { set y 20 }
    }
    if { $x > [winfo screenwidth $w]-100 } { set x [expr {[winfo screenwidth $w]-100}] }
    if { $y > [winfo screenheight $w]-100 } { set y [expr {[winfo screenheight $w]-100}] }
    wm geometry $w ${width}x$height$m1$x$m2$y
}

################################################################################
#    XML & xpath utilities
################################################################################

proc xpath_str { str } {
    
    foreach "strList type pos" [list "" "" 0] break
    while 1 {
	switch $type {
	    "" {
		set ret [regexp -start $pos -indices {['"]} $str idxs]
		if { !$ret } {
		    lappend strList "\"[string range $str $pos end]\""
		    break
		}
		set idx [lindex $idxs 0]
		switch -- [string index $str $idx] {
		    ' { set type apostrophe }
		    \" { set type quote }
		}
	    }
	    apostrophe {
		set ret [regexp -start $pos -indices {["]} $str idxs]
		if { !$ret } {
		    lappend strList "\"[string range $str $pos end]\""
		    break
		}
		set idx [lindex $idxs 0]
		lappend strList "\"[string range $str $pos [expr {$idx-1}]]\""
		set type quote
		set pos $idx
	    }
	    quote {
		set ret [regexp -start $pos -indices {[']} $str idxs]
		if { !$ret } {
		    lappend strList "'[string range $str $pos end]'"
		    break
		}
		set idx [lindex $idxs 0]
		lappend strList "'[string range $str $pos [expr {$idx-1}]]'"
		set type apostrophe
		set pos $idx
	    }
	}
    }
    if { [llength $strList] > 1 } {
	return "concat([join $strList ,])"
    } else {
	return [lindex $strList 0]
    }
}

proc format_xpath { string args } {
    set cmd [list format $string]
    foreach i $args {
	lappend cmd [xpath_str $i]
    }
    return [eval $cmd]
}

namespace eval ::dom::domNode {}

# args can be one or more tags
proc ::dom::domNode::appendChildTag { node args } {
    if { [::llength $args] == 0 } {
	error "error in appendChildTag. At list one tag"
    }
    ::set doc [$node ownerDocument]
    foreach tag $args {
	if { [string match "text() *" $tag] } {
	    ::set newnode [$doc createTextNode [lindex $tag 1]]
	    $node appendChild $newnode
	    ::set node $newnode
	} elseif { [string match "attributes() *" $tag] } {
	    foreach "n v" [lrange $tag 1 end] {
		$node setAttribute $n $v
	    }
	} else {
	    ::set newnode [$doc createElement $tag]
	    $node appendChild $newnode
	    ::set node $newnode
	}
    }
    return $newnode
}

proc ::dom::domNode::appendChildText { node text } {
    ::set doc [$node ownerDocument]
    foreach child [$node selectNodes text()] { $child delete }
    ::set newnode [$doc createTextNode $text]
    $node appendChild $newnode
    return $newnode
}

proc dict_getd { args } {
    
    set dictionaryValue [lindex $args 0]
    set keys [lrange $args 1 end-1]
    set default [lindex $args end]
    if { [dict exists $dictionaryValue {*}$keys] } {
	return [dict get $dictionaryValue {*}$keys]
    }
    return $default
}

proc linsert0 { args } {
    set optional {
	{ -max_len len "" }
    }
    set compulsory "list element"
    parse_args $optional $compulsory $args

    set ipos [lsearch -exact $list $element]
    if { $ipos != -1 } {
	set list [lreplace $list $ipos $ipos]
    }
    set list [linsert $list 0 $element]
    if { $max_len ne "" } {
	set list [lrange $list 0 $max_len]
    }
    return $list
}

################################################################################
#    cu::kill and cu::ps
################################################################################

proc cu::kill { pid } {

    if { $::tcl_platform(platform) eq "windows" } {
	package require compass_utils::c
	return [cu::_kill_win $pid]
    } else {
	exec kill $pid 
    }
}

proc cu::ps { args } {

    if { $::tcl_platform(platform) eq "windows" } {
	package require compass_utils::c
	return [cu::_ps_win {*}$args]
    } else {
	# does not do exactly the same than in Windows
	#set err [catch { exec pgrep -l -f [lindex $args 0] } ret]
	#set retList  [split $ret \n]
	lassign $args pattern
	if { $pattern eq "" } {
	    set err [catch { exec ps -u $::env(USER) --no-headers -o pid,stime,time,size,cmd } ret]
	} else {
	    set err [catch { exec ps -u $::env(USER) --no-headers -o pid,stime,time,size,cmd | grep -i $pattern } ret]
	}        
	if { $err } {
	    return ""
	} else {
	    set retList ""
	    foreach line [split $ret \n] {
		regexp {(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)} $line {} pid stime cputime size cmd
		lappend retList [list $cmd $pid $stime $cputime $size]
	    }
	    return $retList
	}
    }
}
