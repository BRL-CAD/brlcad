# RCS: @(#) $Id$

#
# Demo: Help contents
#
proc DemoHelpContents {} {

    global HelpContents

    set T [DemoList]

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

    InitPics help-*

    #
    # Create columns
    #

    $T column create -text "Help Contents" -tags C0

    $T configure -treecolumn C0

    #
    # Create elements
    #

    # Define a new item state
    $T state define mouseover

    $T element create elemImgPage image -image help-page
    $T element create elemImgBook image -image {help-book-open {open} help-book-closed {}}
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus} blue {mouseover}] \
	-font [list DemoFontUnderline {mouseover}]
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes

    #
    # Create styles using the elements
    #

    # book
    set S [$T style create styBook]
    $T style elements $S {elemRectSel elemImgBook elemTxt}
    $T style layout $S elemImgBook -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    # page
    set S [$T style create styPage]
    $T style elements $S {elemRectSel elemImgPage elemTxt}
    $T style layout $S elemImgPage -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    #
    # Create items and assign styles
    #

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth style text} {
	0 styPage "Welcome to Help"
	0 styBook "Introducing Windows 98"
	    1 styBook "How to Use Help"
		2 styPage "Find a topic"
		2 styPage "Get more out of help"
	    1 styBook "Register Your Software"
		2 styPage "Registering Windows 98 online"
	    1 styBook "What's New in Windows 98"
		2 styPage "Innovative, easy-to-use features"
		2 styPage "Improved reliability"
		2 styPage "A faster operating system"
		2 styPage "True Web integration"
		2 styPage "More entertaining and fun"
	    1 styBook "If You're New to Windows 98"
		2 styBook "Tips for Macintosh Users"
		    3 styPage "Why does the mouse have two buttons?"
    } {
	set item [$T item create -open no]
	$T item style set $item C0 $style
	$T item element configure $item C0 elemTxt -text $text
	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    bind DemoHelpContents <Double-ButtonPress-1> {
	if {[lindex [%W identify %x %y] 0] eq "header"} {
	    TreeCtrl::DoubleButton1 %W %x %y
	} else {
	    HelpButton1 %W %x %y
	}
	break
    }
    bind DemoHelpContents <ButtonPress-1> {
	HelpButton1 %W %x %y
	break
    }
    bind DemoHelpContents <Button1-Motion> {
	# noop
    }
    bind DemoHelpContents <Button1-Leave> {
	# noop
    }
    bind DemoHelpContents <Motion> {
	HelpMotion %W %x %y
    }
    bind DemoHelpContents <Leave> {
	HelpMotion %W %x %y
    }
    bind DemoHelpContents <KeyPress-Return> {
	if {[%W selection count] == 1} {
	    %W item toggle [%W selection get 0]
	}
	break
    }

    set HelpContents(prev) ""
    bindtags $T [list $T DemoHelpContents TreeCtrl [winfo toplevel $T] all]

    return
}

# This is an alternate implementation that does not define a new item state
# to change the appearance of the item under the cursor.
proc DemoHelpContents_2 {} {

    global HelpContents

    set T [DemoList]

    set height [font metrics [$T cget -font] -linespace]
    if {$height < 18} {
	set height 18
    }

    #
    # Configure the treectrl widget
    #

    $T configure -showroot no -showbuttons no -showlines no -itemheight $height \
	-selectmode browse

    InitPics help-*

    #
    # Create columns
    #

    $T column create -text "Help Contents"

    #
    # Create elements
    #

    $T element create elemImgPage image -image help-page
    $T element create elemImgBook image -image {help-book-open {open} help-book-closed {}}
    $T element create elemTxt text -fill [list $::SystemHighlightText {selected focus}]
    $T element create elemRectSel rect -fill [list $::SystemHighlight {selected focus}] -showfocus yes
    $T element create elemTxtOver text -fill [list $::SystemHighlightText {selected focus} blue {}] \
	-font "[$T cget -font] underline"

    #
    # Create styles using the elements
    #

    # book
    set S [$T style create styBook]
    $T style elements $S {elemRectSel elemImgBook elemTxt}
    $T style layout $S elemImgBook -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    # page
    set S [$T style create styPage]
    $T style elements $S {elemRectSel elemImgPage elemTxt}
    $T style layout $S elemImgPage -padx {0 4} -expand ns
    $T style layout $S elemTxt -expand ns
    $T style layout $S elemRectSel -union [list elemTxt] -iexpand ns -ipadx 2

    # book (focus)
    set S [$T style create styBook.f]
    $T style elements $S {elemRectSel elemImgBook elemTxtOver}
    $T style layout $S elemImgBook -padx {0 4} -expand ns
    $T style layout $S elemTxtOver -expand ns
    $T style layout $S elemRectSel -union [list elemTxtOver] -iexpand ns -ipadx {1 2}

    # page (focus)
    set S [$T style create styPage.f]
    $T style elements $S {elemRectSel elemImgPage elemTxtOver}
    $T style layout $S elemImgPage -padx {0 4} -expand ns
    $T style layout $S elemTxtOver -expand ns
    $T style layout $S elemRectSel -union [list elemTxtOver] -iexpand ns -ipadx {1 2}

    #
    # Create items and assign styles
    #

    set parentList [list root {} {} {} {} {} {}]
    set parent root
    foreach {depth style text} {
	0 styPage "Welcome to Help"
	0 styBook "Introducing Windows 98"
	    1 styBook "How to Use Help"
		2 styPage "Find a topic"
		2 styPage "Get more out of help"
	    1 styBook "Register Your Software"
		2 styPage "Registering Windows 98 online"
	    1 styBook "What's New in Windows 98"
		2 styPage "Innovative, easy-to-use features"
		2 styPage "Improved reliability"
		2 styPage "A faster operating system"
		2 styPage "True Web integration"
		2 styPage "More entertaining and fun"
	    1 styBook "If You're New to Windows 98"
		2 styBook "Tips for Macintosh Users"
		    3 styPage "Why does the mouse have two buttons?"
    } {
	set item [$T item create -open no]
	$T item style set $item 0 $style
	$T item element configure $item 0 elemTxt -text $text
	$T item lastchild [lindex $parentList $depth] $item
	incr depth
	set parentList [lreplace $parentList $depth $depth $item]
    }

    bind DemoHelpContents <Double-ButtonPress-1> {
	if {[lindex [%W identify %x %y] 0] eq "header"} {
	    TreeCtrl::DoubleButton1 %W %x %y
	} else {
	    HelpButton1 %W %x %y
	}
	break
    }
    bind DemoHelpContents <ButtonPress-1> {
	HelpButton1 %W %x %y
	break
    }
    bind DemoHelpContents <Button1-Motion> {
	# noop
    }
    bind DemoHelpContents <Button1-Leave> {
	# noop
    }
    bind DemoHelpContents <Motion> {
	HelpMotion_2 %W %x %y
    }
    bind DemoHelpContents <Leave> {
	HelpMotion_2 %W %x %y
    }
    bind DemoHelpContents <KeyPress-Return> {
	if {[%W selection count] == 1} {
	    %W item toggle [%W selection get 0]
	}
	break
    }

    set HelpContents(prev) ""
    bindtags $T [list $T DemoHelpContents TreeCtrl [winfo toplevel $T] all]

    return
}

proc HelpButton1 {w x y} {
    variable TreeCtrl::Priv
    focus $w
    set id [$w identify $x $y]
    set Priv(buttonMode) ""
    if {[lindex $id 0] eq "header"} {
	TreeCtrl::ButtonPress1 $w $x $y
    } elseif {[lindex $id 0] eq "item"} {
	set item [lindex $id 1]
	# didn't click an element
	if {[llength $id] != 6} return
	if {[$w selection includes $item]} {
	    $w item toggle $item
	    return
	}
	if {[$w selection count]} {
	    set item2 [$w selection get 0]
	    $w item collapse $item2
	    foreach item2 [$w item ancestors $item2] {
		if {[$w item compare $item != $item2]} {
		    $w item collapse $item2
		}
	    }
	}
	$w activate $item
	$w item expand [list $item ancestors]
	$w item toggle $item
	$w selection modify $item all
    }
    return
}

proc HelpMotion {w x y} {
    global HelpContents
    set id [$w identify $x $y]
    if {$id eq ""} {
    } elseif {[lindex $id 0] eq "header"} {
    } elseif {[lindex $id 0] eq "item"} {
	set item [lindex $id 1]
	if {[llength $id] == 6} {
	    if {$item ne $HelpContents(prev)} {
		if {$HelpContents(prev) ne ""} {
		    $w item state set $HelpContents(prev) !mouseover
		}
		$w item state set $item mouseover
		$w configure -cursor hand2
		set HelpContents(prev) $item
	    }
	    return
	}
    }
    if {$HelpContents(prev) ne ""} {
	$w item state set $HelpContents(prev) !mouseover
	$w configure -cursor ""
	set HelpContents(prev) ""
    }
    return
}

# Alternate implementation that does not rely on run-time states
proc HelpMotion_2 {w x y} {
    variable TreeCtrl::Priv
    global HelpContents
    set id [$w identify $x $y]
    if {[lindex $id 0] eq "header"} {
    } elseif {$id ne ""} {
	set item [lindex $id 1]
	if {[llength $id] == 6} {
	    if {$item ne $HelpContents(prev)} {
		if {$HelpContents(prev) ne ""} {
		    set style [$w item style set $HelpContents(prev) 0]
		    set style [string trim $style .f]
		    $w item style map $HelpContents(prev) 0 $style {elemTxtOver elemTxt}
		}
		set style [$w item style set $item 0]
		$w item style map $item 0 $style.f {elemTxt elemTxtOver}
		set HelpContents(prev) $item
	    }
	    return
	}
    }
    if {$HelpContents(prev) ne ""} {
	set style [$w item style set $HelpContents(prev) 0]
	set style [string trim $style .f]
	$w item style map $HelpContents(prev) 0 $style {elemTxtOver elemTxt}
	set HelpContents(prev) ""
    }
    return
}

