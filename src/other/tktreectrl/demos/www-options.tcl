# Copyright (c) 2002-2011 Tim Baker

namespace eval DemoInternetOptions {}
proc DemoInternetOptions::Init {T} {

    variable Priv

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode browse

    $T configure -canvaspadx {4 0} -canvaspady {2 0}

    InitPics internet-*

    #
    # Create columns
    #

    $T column create -text "Internet Options" -tags C0

    $T configure -treecolumn C0

    #
    # Create elements
    #

    $T item state define check
    $T item state define radio
    $T item state define on

    $T element create elemImg image -image {
	internet-check-on {check on}
	internet-check-off {check}
	internet-radio-on {radio on}
	internet-radio-off {radio}
    }
    $T element create elemTxt text \
	-fill [list $::SystemHighlightText {selected focus}] -lines 1
    $T element create elemRectSel rect \
	-fill [list $::SystemHighlight {selected focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    set S [$T style create STYLE]
    $T style elements $S {elemRectSel elemImg elemTxt}
    $T style layout $S elemImg -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns -squeeze x
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    #
    # Create items and assign styles
    #

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth setting text option group} {
	0 print "Printing" "" ""
	    1 off "Print background colors and images" "o1" ""
	0 search "Search from Address bar" "" ""
	    1 search "When searching" "" ""
		2 off "Display results, and go to the most likely sites" "o2" "r1"
		2 off "Do not search from the Address bar" "o3" "r1"
		2 off "Just display the results in the main window" "o4" "r1"
		2 on "Just go to the most likely site" "o5" "r1"
	0 security "Security" "" ""
	    1 on "Check for publisher's certificate revocation" "o5" ""
	    1 off "Check for server certificate revocation (requires restart)" "o6" ""
    } {
	set item [$T item create]
	$T item style set $item C0 STYLE
	$T item element configure $item C0 elemTxt -text $text
	set Priv(option,$item) $option
	set Priv(group,$item) $group
	if {($setting eq "on") || ($setting eq "off")} {
	    set Priv(setting,$item) $setting
	    if {$group eq ""} {
		$T item state set $item check
		if {$setting eq "on"} {
		    $T item state set $item on
		}
	    } else {
		if {$setting eq "on"} {
		    set Priv(current,$group) $item
		    $T item state set $item on
		}
		$T item state set $item radio
	    }
	} else {
	    $T item element configure $item C0 elemImg -image internet-$setting
	}
	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    bind DemoInternetOptions <Double-ButtonPress-1> {
	TreeCtrl::DoubleButton1 %W %x %y
    }
    bind DemoInternetOptions <ButtonPress-1> {
	DemoInternetOptions::Button1 %W %x %y
	break
    }

    bindtags $T [list $T DemoInternetOptions TreeCtrl [winfo toplevel $T] all]

    return
}

proc DemoInternetOptions::Button1 {T x y} {
    variable Priv
    focus $T
    set id [$T identify $x $y]
    if {[lindex $id 0] eq "header"} {
	TreeCtrl::ButtonPress1 $T $x $y
    } elseif {$id eq ""} {
	set ::TreeCtrl::Priv(buttonMode) ""
    } else {
	set ::TreeCtrl::Priv(buttonMode) ""
	set item [lindex $id 1]
	$T selection modify $item all
	$T activate $item
	if {$Priv(option,$item) eq ""} return
	set group $Priv(group,$item)
	# a checkbutton
	if {$group eq ""} {
	    $T item state set $item ~on
	    if {$Priv(setting,$item) eq "on"} {
		set setting off
	    } else {
		set setting on
	    }
	    set Priv(setting,$item) $setting
	# a radiobutton
	} else {
	    set current $Priv(current,$group)
	    if {$current eq $item} return
	    $T item state set $current !on
	    $T item state set $item on
	    set Priv(setting,$item) on
	    set Priv(current,$group) $item
	}
    }
    return
}


# Alternate implementation that does not rely on run-time states
proc DemoInternetOptions::Init_2 {T} {

    variable Priv

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode browse

    InitPics internet-*

    #
    # Create columns
    #

    $T column create -text "Internet Options"

    #
    # Create elements
    #

    $T element create elemImg image
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}]
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    set S [$T style create STYLE]
    $T style elements $S {elemRectSel elemImg elemTxt}
    $T style layout $S elemImg -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    #
    # Create items and assign styles
    #

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth setting text option group} {
	0 print "Printing" "" ""
	    1 off "Print background colors and images" "o1" ""
	0 search "Search from Address bar" "" ""
	    1 search "When searching" "" ""
		2 off "Display results, and go to the most likely sites" "o2" "r1"
		2 off "Do not search from the Address bar" "o3" "r1"
		2 off "Just display the results in the main window" "o4" "r1"
		2 on "Just go to the most likely site" "o5" "r1"
	0 security "Security" "" ""
	    1 on "Check for publisher's certificate revocation" "o5" ""
	    1 off "Check for server certificate revocation (requires restart)" "o6" ""
    } {
	set item [$T item create]
	$T item style set $item 0 STYLE
	$T item element configure $item 0 elemTxt -text $text
	set Priv(option,$item) $option
	set Priv(group,$item) $group
	if {$setting eq "on" || $setting eq "off"} {
	    set Priv(setting,$item) $setting
	    if {$group eq ""} {
		set img internet-check-$setting
		$T item element configure $item 0 elemImg -image $img
	    } else {
		if {$setting eq "on"} {
		    set Priv(current,$group) $item
		}
		set img internet-radio-$setting
		$T item element configure $item 0 elemImg -image $img
	    }
	} else {
	    $T item element configure $item 0 elemImg -image internet-$setting
	}
	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    bind DemoInternetOptions <Double-ButtonPress-1> {
	TreeCtrl::DoubleButton1 %W %x %y
    }
    bind DemoInternetOptions <ButtonPress-1> {
	DemoInternetOptions::Button1 %W %x %y
	break
    }

    bindtags $T [list $T DemoInternetOptions TreeCtrl [winfo toplevel $T] all]

    return
}

# Alternate implementation that does not rely on run-time states
proc DemoInternetOptions::Button1_2 {T x y} {
    variable Priv
    focus $T
    set id [$T identify $x $y]
    if {[lindex $id 0] eq "header"} {
	TreeCtrl::ButtonPress1 $T $x $y
    } elseif {$id eq ""} {
	set ::TreeCtrl::Priv(buttonMode) ""
    } else {
	set ::TreeCtrl::Priv(buttonMode) ""
	set item [lindex $id 1]
	$T selection modify $item all
	$T activate $item
	if {$Priv(option,$item) eq ""} return
	set group $Priv(group,$item)
	# a checkbutton
	if {$group eq ""} {
	    if {$Priv(setting,$item) eq "on"} {
		set setting off
	    } else {
		set setting on
	    }
	    $T item element configure $item 0 elemImg -image internet-check-$setting
	    set Priv(setting,$item) $setting
	# a radiobutton
	} else {
	    set current $Priv(current,$group)
	    if {$current eq $item} return
	    $T item element configure $current 0 elemImg -image internet-radio-off
	    $T item element configure $item 0 elemImg -image internet-radio-on
	    set Priv(setting,$item) on
	    set Priv(current,$group) $item
	}
    }
    return
}

